/*
 * Copyright 2026 The netsurf_upy contributors
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 * The script-engine loader, and the router in front of it.
 *
 * Added 2026-09-25 (netsurf_upy, GPLv2 section 2(a) change notice).
 *
 * This file defines the `js_*` entry points of `javascript/js.h` --
 * every one of them, in every build -- and forwards each to whichever
 * `struct ns_script_engine_v1` is active.  The active engine is
 * `ns_builtin_script_engine()` unless the `script_engine_path` option
 * names a shared object that loads, exports
 * `ns_script_engine_v1_get`, accepts the core table and declares
 * `NS_SCRIPT_ENGINE_ABI_V1`.
 *
 * There is no dynamic loading anywhere else in NetSurf: `dlopen`,
 * `lt_dlopen` and `LoadLibrary` are otherwise absent from the tree, and
 * the three `dlsym`/`RTLD_`/`dlfcn` hits are one line of
 * `test/malloc_fig.c`, an LD_PRELOAD malloc interposer.  So everything
 * about the failure modes here is written out rather than assumed: a
 * plugin that cannot be loaded, cannot be read, refuses the core table
 * or declares the wrong ABI leaves the browser running the engine it
 * was built with, and says so at WARNING.
 *
 * MIME registration is *not* the engine's job and does not depend on
 * one being compiled in.  `javascript_init()` used to be called from
 * `duktape/dukky.c::js_initialise()`, and `javascript/content.c` was
 * listed only in the duktape arm of the Makefile, so a
 * `NETSURF_USE_DUKTAPE := NO` build registered no `CONTENT_JS` handler
 * at all and `content_factory.c::content_factory_register_handler()`
 * never heard of a script's MIME type.  The registration is here now,
 * and `content.c` is in the always-compiled list.
 *
 * Changed 2026-09-25 for netsurf_upy (GPLv2 section 2(a), a dated
 * notice of change), a second time (G5): a **second engine slot**.
 * `script_engine_upy_path` names a plugin that answers for
 * `text/x-upy` and for nothing else, loaded by the same `engine_load()`
 * and refused by the same checks as the first.  The `js_*` entry points
 * below are untouched and still forward to `active`; the new
 * `js_exec_upy()` is the only way into the second slot, and
 * `html/script.c::select_script_handler()` is its only caller.
 *
 * **One shared object is one interpreter (R4.4).**  MicroPython's embed
 * API carries no interpreter handle -- its state is a single file-scope
 * `mp_state_ctx` -- so the upy heap and thread here are *singletons*,
 * not one per content.  A second document that runs a upy script takes
 * the interpreter from the first, and says so at WARNING.  That is the
 * honest shape of the limitation; `netsurf_upy/plugin/README.md`
 * records it, and pretending otherwise would mean N copies of the `.so`.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <inttypes.h>
#include <dlfcn.h>

#include "utils/errors.h"
#include "utils/log.h"
#include "utils/nsoption.h"

#include "javascript/js.h"
#include "javascript/content.h"
#include "javascript/core.h"
#include "javascript/engine.h"

/** The engine every `js_*` below forwards to.  Never NULL once set. */
static const struct ns_script_engine_v1 *active;

/**
 * The engine `text/x-upy` scripts go to, or NULL (netsurf_upy G5).
 *
 * A *second slot*, not a second seam: the same
 * `struct ns_script_engine_v1`, the same loader, the same ABI check.
 * NULL means no plugin was named or the one named was refused, and a
 * `text/x-upy` script is then not executed -- which is what stock
 * NetSurf does with every script type it does not know.
 */
static const struct ns_script_engine_v1 *upy;

/** The one upy heap and the one upy thread there can be (R4.4). */
static void *upy_heap;
static void *upy_thread;
/** Which of the browser's threads the upy thread is bound to. */
static jsthread *upy_owner;
/** `js_newheap()`'s timeout, for the upy heap when it is finally made. */
static int upy_timeout;

/**
 * What `js_newthread()` was told, kept for `js_exec_upy()`.
 *
 * The upy engine needs the html content (`doc_priv`) to resolve ids in
 * and to mark dirty, and `html/script.c`'s `script_handler_t` hands a
 * handler only the `jsthread *`.  A record per live thread is the whole
 * of that mapping.  Re-typing `jsthread` into a wrapper struct would
 * have worked too, and would have changed what every existing `js_*`
 * caller holds for the sake of one pointer.
 */
struct js_thread_record {
	struct js_thread_record *next;
	jsthread *thread;
	void *win_priv;
	void *doc_priv;
};

static struct js_thread_record *thread_records;

static struct js_thread_record *thread_record(jsthread *thread)
{
	struct js_thread_record *rec;

	for (rec = thread_records; rec != NULL; rec = rec->next) {
		if (rec->thread == thread) {
			return rec;
		}
	}

	return NULL;
}

static void thread_record_add(jsthread *thread, void *win_priv,
			      void *doc_priv)
{
	struct js_thread_record *rec;

	rec = calloc(1, sizeof(*rec));
	if (rec == NULL) {
		/* Not fatal, and not silent: JavaScript is unaffected
		 * and only `text/x-upy` on this document is lost. */
		NSLOG(netsurf, WARNING,
		      "no memory for the upy binding of thread %p", thread);
		return;
	}

	rec->thread = thread;
	rec->win_priv = win_priv;
	rec->doc_priv = doc_priv;
	rec->next = thread_records;
	thread_records = rec;
}

static void thread_record_remove(jsthread *thread)
{
	struct js_thread_record **link = &thread_records;
	struct js_thread_record *rec;

	while ((rec = *link) != NULL) {
		if (rec->thread == thread) {
			*link = rec->next;
			free(rec);
			return;
		}
		link = &rec->next;
	}
}

/**
 * Drop the one upy thread, if it belongs to `thread` (or to anybody,
 * when `thread` is NULL).
 */
static void upy_release(jsthread *thread)
{
	if (upy_thread == NULL) {
		return;
	}
	if (thread != NULL && upy_owner != thread) {
		return;
	}

	upy->closethread(upy_thread);
	upy->destroythread(upy_thread);
	upy_thread = NULL;
	upy_owner = NULL;
}

/**
 * Make the upy engine's thread the one for `thread`'s document.
 *
 * R4.4 in one function: there is one interpreter, so binding it to a
 * second document takes it away from the first, loudly.
 */
static bool upy_bind(jsthread *thread)
{
	struct js_thread_record *rec;

	if (upy == NULL) {
		return false;
	}

	if (upy_thread != NULL && upy_owner == thread) {
		return true;
	}

	rec = thread_record(thread);
	if (rec == NULL) {
		NSLOG(netsurf, WARNING,
		      "upy engine: no record of thread %p; not running the "
		      "script", thread);
		return false;
	}

	if (upy_thread != NULL) {
		NSLOG(netsurf, WARNING,
		      "upy engine \"%s\": one shared object is one "
		      "interpreter, so this document takes it from the "
		      "previous one", upy->name);
		upy_release(NULL);
	}

	if (upy_heap == NULL) {
		if ((nserror) upy->newheap(upy_timeout, &upy_heap) !=
				NSERROR_OK) {
			upy_heap = NULL;
			NSLOG(netsurf, WARNING,
			      "upy engine \"%s\": no heap", upy->name);
			return false;
		}
	}

	if ((nserror) upy->newthread(upy_heap, rec->win_priv, rec->doc_priv,
				     &upy_thread) != NSERROR_OK) {
		upy_thread = NULL;
		NSLOG(netsurf, WARNING,
		      "upy engine \"%s\": no thread", upy->name);
		return false;
	}

	upy_owner = thread;

	return true;
}

/**
 * The loaded plugins, kept for the life of the process.
 *
 * Never `dlclose`d once one is an engine: DOM wrappers, event
 * listeners and the heap itself live inside it, and `js_finalise()`
 * runs while browser windows still hold them.  `upy_object` is the
 * same thing for the second slot (netsurf_upy G5).
 */
static void *engine_object;
static void *upy_object;

/**
 * Every member of the table, so a short plugin cannot be called into.
 *
 * A plugin is a separate build; a missing entry point here is a NULL
 * call in `js_exec()` later, which is a crash with no explanation.  The
 * ABI token catches a *known* revision; this catches a plugin that got
 * the token right and the table wrong.
 */
static bool engine_is_whole(const struct ns_script_engine_v1 *engine)
{
	return engine->name != NULL &&
		engine->initialise != NULL &&
		engine->finalise != NULL &&
		engine->newheap != NULL &&
		engine->destroyheap != NULL &&
		engine->newthread != NULL &&
		engine->closethread != NULL &&
		engine->destroythread != NULL &&
		engine->exec != NULL &&
		engine->fire_event != NULL &&
		engine->handle_new_element != NULL &&
		engine->event_cleanup != NULL;
}

/**
 * Load `path` and return the engine it publishes, or NULL.
 *
 * Never fails the caller: every refusal is a log line naming what was
 * kept instead (`whatever`, the caller's words) and a NULL return, so
 * the browser goes on running whatever it was running.
 *
 * > **Changed 2026-09-25 (netsurf_upy G5).**  It used to assign
 * > `active` itself and spell "keeping the built-in engine" into every
 * > one of its messages.  There are two slots now -- the engine for
 * > `text/javascript` and the engine for `text/x-upy` -- and only the
 * > first has a built-in engine to keep, so the consequence of a
 * > refusal is the caller's to say.
 */
static const struct ns_script_engine_v1 *
engine_load(const char *path, const char *whatever, void **object_out)
{
	void *object;
	ns_script_engine_v1_get_fn *get;
	const struct ns_script_engine_v1 *engine;
	uint32_t abi;
	const char *name;

	object = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (object == NULL) {
		NSLOG(netsurf, WARNING,
		      "script engine %s: dlopen failed: %s; %s",
		      path, dlerror(), whatever);
		return NULL;
	}

	/* The ISO C cast an object pointer to a function pointer needs. */
	*(void **) (&get) = dlsym(object, NS_SCRIPT_ENGINE_V1_GET);
	if (get == NULL) {
		NSLOG(netsurf, WARNING,
		      "script engine %s: no %s; %s",
		      path, NS_SCRIPT_ENGINE_V1_GET, whatever);
		dlclose(object);
		return NULL;
	}

	engine = get(&ns_script_core_v1);
	if (engine == NULL) {
		NSLOG(netsurf, WARNING,
		      "script engine %s: %s refused the core table; %s",
		      path, NS_SCRIPT_ENGINE_V1_GET, whatever);
		dlclose(object);
		return NULL;
	}

	/* Read everything that is wanted for the log line *before* the
	 * object can be unmapped: `engine` points into it. */
	abi = engine->abi;
	name = engine->name;

	if (abi != NS_SCRIPT_ENGINE_ABI_V1) {
		NSLOG(netsurf, WARNING,
		      "script engine %s: ABI 0x%08" PRIx32 ", this browser "
		      "wants 0x%08" PRIx32 "; %s",
		      path, abi, (uint32_t) NS_SCRIPT_ENGINE_ABI_V1,
		      whatever);
		dlclose(object);
		return NULL;
	}

	if (engine_is_whole(engine) == false) {
		NSLOG(netsurf, WARNING,
		      "script engine %s: the vtable has a hole in it; %s",
		      path, whatever);
		dlclose(object);
		return NULL;
	}

	*object_out = object;

	NSLOG(netsurf, INFO, "script engine %s: loaded \"%s\"", path, name);

	return engine;
}

/* exported interface documented in javascript/engine.h */
const struct ns_script_engine_v1 *ns_active_script_engine(void)
{
	return active;
}

/* exported interface documented in javascript/engine.h */
const struct ns_script_engine_v1 *ns_upy_script_engine(void)
{
	return upy;
}

/* exported interface documented in js.h */
void js_initialise(void)
{
	const char *path;
	const struct ns_script_engine_v1 *loaded;
	char kept[128];

	active = ns_builtin_script_engine();

	/* The accepted-MIME list, whatever engine answers.  It used to
	 * be the duktape engine's own first act, which made a
	 * `NETSURF_USE_DUKTAPE := NO` build one that could not classify
	 * a script at all. */
	javascript_init();

	path = nsoption_charp(script_engine_path);
	if (path != NULL && path[0] != '\0') {
		snprintf(kept, sizeof(kept),
			 "keeping the built-in engine \"%s\"", active->name);
		loaded = engine_load(path, kept, &engine_object);
		if (loaded != NULL) {
			active = loaded;
		}
	}

	/* The second slot (netsurf_upy G5).  It has no built-in engine
	 * to fall back to, so a refusal means `text/x-upy` scripts are
	 * simply not run -- the behaviour of every NetSurf there has
	 * ever been for a script type it does not know. */
	path = nsoption_charp(script_engine_upy_path);
	if (path != NULL && path[0] != '\0') {
		upy = engine_load(path,
				  "text/x-upy scripts will not be run",
				  &upy_object);
		if (upy != NULL) {
			upy->initialise();
		}
	}

	active->initialise();
}

/* exported interface documented in js.h */
void js_finalise(void)
{
	if (upy != NULL) {
		upy_release(NULL);
		if (upy_heap != NULL) {
			upy->destroyheap(upy_heap);
			upy_heap = NULL;
		}
		upy->finalise();
	}

	active->finalise();
}

/* exported interface documented in js.h */
nserror js_newheap(int timeout, jsheap **heap)
{
	void *opaque = NULL;
	nserror ret;

	/* The upy heap is made lazily, by the first document that runs
	 * a upy script, and there is one of it (R4.4).  This is where
	 * the browser's timeout is heard, so this is where it is kept. */
	upy_timeout = timeout;

	ret = (nserror) active->newheap(timeout, &opaque);
	*heap = (jsheap *) opaque;

	return ret;
}

/* exported interface documented in js.h */
void js_destroyheap(jsheap *heap)
{
	active->destroyheap(heap);
}

/* exported interface documented in js.h */
nserror
js_newthread(jsheap *heap, void *win_priv, void *doc_priv, jsthread **thread)
{
	void *opaque = NULL;
	nserror ret;

	ret = (nserror) active->newthread(heap, win_priv, doc_priv, &opaque);
	*thread = (jsthread *) opaque;

	/* What the upy engine will need if this document turns out to
	 * carry a `text/x-upy` script (netsurf_upy G5). */
	if (ret == NSERROR_OK && *thread != NULL) {
		thread_record_add(*thread, win_priv, doc_priv);
	}

	return ret;
}

/* exported interface documented in js.h */
nserror js_closethread(jsthread *thread)
{
	upy_release(thread);

	return (nserror) active->closethread(thread);
}

/* exported interface documented in js.h */
void js_destroythread(jsthread *thread)
{
	upy_release(thread);
	thread_record_remove(thread);

	active->destroythread(thread);
}

/* exported interface documented in js.h */
bool
js_exec(jsthread *thread, const uint8_t *txt, size_t txtlen, const char *name)
{
	return active->exec(thread, txt, txtlen, name) != 0;
}

/* exported interface documented in javascript/engine.h */
bool
js_exec_upy(jsthread *thread, const uint8_t *txt, size_t txtlen,
	    const char *name)
{
	if (upy == NULL) {
		/* Unreachable through `select_script_handler()`, which
		 * asks first; here because an engine slot that can be
		 * empty must have one place that says what empty means. */
		return false;
	}

	if (upy_bind(thread) == false) {
		return false;
	}

	return upy->exec(upy_thread, txt, txtlen, name) != 0;
}

/* exported interface documented in js.h */
bool
js_fire_event(jsthread *thread, const char *type, struct dom_document *doc,
	      struct dom_node *target)
{
	return active->fire_event(thread, type, doc, target) != 0;
}

/* exported interface documented in js.h */
void js_handle_new_element(jsthread *thread, struct dom_element *node)
{
	active->handle_new_element(thread, node);
}

/* exported interface documented in js.h */
void js_event_cleanup(jsthread *thread, struct dom_event *evt)
{
	active->event_cleanup(thread, evt);
}
