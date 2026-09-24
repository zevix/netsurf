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
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

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
 * The loaded plugin, kept for the life of the process.
 *
 * Never `dlclose`d once it is the active engine: DOM wrappers, event
 * listeners and the heap itself live inside it, and `js_finalise()`
 * runs while browser windows still hold them.
 */
static void *engine_object;

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
 * Try to make `path` the active engine.  Never fails the caller: on any
 * refusal `active` is left as it was, which is the built-in engine.
 */
static void engine_load(const char *path)
{
	void *object;
	ns_script_engine_v1_get_fn *get;
	const struct ns_script_engine_v1 *engine;
	uint32_t abi;
	const char *name;

	object = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (object == NULL) {
		NSLOG(netsurf, WARNING,
		      "script engine %s: dlopen failed: %s; "
		      "keeping the built-in engine \"%s\"",
		      path, dlerror(), active->name);
		return;
	}

	/* The ISO C cast an object pointer to a function pointer needs. */
	*(void **) (&get) = dlsym(object, NS_SCRIPT_ENGINE_V1_GET);
	if (get == NULL) {
		NSLOG(netsurf, WARNING,
		      "script engine %s: no %s; "
		      "keeping the built-in engine \"%s\"",
		      path, NS_SCRIPT_ENGINE_V1_GET, active->name);
		dlclose(object);
		return;
	}

	engine = get(&ns_script_core_v1);
	if (engine == NULL) {
		NSLOG(netsurf, WARNING,
		      "script engine %s: %s refused the core table; "
		      "keeping the built-in engine \"%s\"",
		      path, NS_SCRIPT_ENGINE_V1_GET, active->name);
		dlclose(object);
		return;
	}

	/* Read everything that is wanted for the log line *before* the
	 * object can be unmapped: `engine` points into it. */
	abi = engine->abi;
	name = engine->name;

	if (abi != NS_SCRIPT_ENGINE_ABI_V1) {
		NSLOG(netsurf, WARNING,
		      "script engine %s: ABI 0x%08" PRIx32 ", this browser "
		      "wants 0x%08" PRIx32 "; keeping the built-in engine "
		      "\"%s\"",
		      path, abi, (uint32_t) NS_SCRIPT_ENGINE_ABI_V1,
		      active->name);
		dlclose(object);
		return;
	}

	if (engine_is_whole(engine) == false) {
		NSLOG(netsurf, WARNING,
		      "script engine %s: the vtable has a hole in it; "
		      "keeping the built-in engine \"%s\"",
		      path, active->name);
		dlclose(object);
		return;
	}

	active = engine;
	engine_object = object;

	NSLOG(netsurf, INFO, "script engine %s: loaded \"%s\"", path, name);
}

/* exported interface documented in javascript/engine.h */
const struct ns_script_engine_v1 *ns_active_script_engine(void)
{
	return active;
}

/* exported interface documented in js.h */
void js_initialise(void)
{
	const char *path;

	active = ns_builtin_script_engine();

	/* The accepted-MIME list, whatever engine answers.  It used to
	 * be the duktape engine's own first act, which made a
	 * `NETSURF_USE_DUKTAPE := NO` build one that could not classify
	 * a script at all. */
	javascript_init();

	path = nsoption_charp(script_engine_path);
	if (path != NULL && path[0] != '\0') {
		engine_load(path);
	}

	active->initialise();
}

/* exported interface documented in js.h */
void js_finalise(void)
{
	active->finalise();
}

/* exported interface documented in js.h */
nserror js_newheap(int timeout, jsheap **heap)
{
	void *opaque = NULL;
	nserror ret;

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

	return ret;
}

/* exported interface documented in js.h */
nserror js_closethread(jsthread *thread)
{
	return (nserror) active->closethread(thread);
}

/* exported interface documented in js.h */
void js_destroythread(jsthread *thread)
{
	active->destroythread(thread);
}

/* exported interface documented in js.h */
bool
js_exec(jsthread *thread, const uint8_t *txt, size_t txtlen, const char *name)
{
	return active->exec(thread, txt, txtlen, name) != 0;
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
