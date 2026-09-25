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
 * The script-engine seam: one vtable, in plain C.
 *
 * Added 2026-09-25 (netsurf_upy, GPLv2 section 2(a) change notice).
 *
 * Until now the engine was a build-time choice and nothing else: a
 * single `ifeq` on `NETSURF_USE_DUKTAPE` in
 * `content/handlers/javascript/Makefile` decided which `.c` defined the
 * `js_*` externs of `javascript/js.h`.  This header adds the *other*
 * way of choosing one -- a shared object named by the
 * `script_engine_path` option, loaded at start-up -- without taking the
 * first away.  `javascript/engine.c` is the loader and the router;
 * `javascript/js.h` is unchanged and the rest of the browser does not
 * know which of the two answered.
 *
 * Every type here is a C fundamental type or a pointer to one.  Nothing
 * a script engine defines -- not `jsheap`, not `jsthread`, not a
 * `duk_context` -- and nothing libdom defines appears in it, so a
 * browser and a plugin agree on this header without agreeing on each
 * other's internals.  `jsheap *` and `jsthread *` cross it as `void *`;
 * `js.h`'s own signatures keep the opaque typedefs and `engine.c` casts
 * at the one boundary.
 *
 * The engine reaches *back* into the browser through
 * `struct ns_script_core_v1` (`javascript/core.h`), which the browser
 * passes in.  It is deliberately opaque here: a plugin needs the full
 * declaration, the browser's other 400-odd translation units do not.
 *
 * Changed 2026-09-25 for netsurf_upy (GPLv2 section 2(a), a dated
 * notice of change), a second time (G5).  **The vtable is unchanged --
 * the same eleven functions, the same ABI token.**  What is new is that
 * a browser can hold *two* engines at once: the one that answers for
 * `text/javascript` and a second, named by `script_engine_upy_path`,
 * that answers for `text/x-upy` and for nothing else.  Both are the
 * same `struct ns_script_engine_v1`, loaded by the same loader and
 * refused by the same checks; `js_exec_upy()` below is the only new
 * entry point, and `html/script.c::select_script_handler()` is its one
 * caller.  A plugin cannot tell which slot it was loaded into, which is
 * what keeps this a change to the *browser* and not to the seam.
 */

#ifndef NETSURF_JAVASCRIPT_ENGINE_H_
#define NETSURF_JAVASCRIPT_ENGINE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "javascript/js.h"

/**
 * The ABI token of the table below.
 *
 * Bump it for *any* change to `struct ns_script_engine_v1` -- a new
 * member, a changed signature, a changed meaning.  A plugin built
 * against a different one is refused with a log line and the browser
 * keeps the engine it was built with; see `engine.c::engine_load()`.
 *
 * R3.3: MicroPython has no stable cross-build C ABI, so a version check
 * is the only thing standing between a stale plugin and a crash inside
 * it.
 */
#define NS_SCRIPT_ENGINE_ABI_V1 ((uint32_t) 0x6e734531)	/* "nsE1" */

/** The one symbol a plugin must export. */
#define NS_SCRIPT_ENGINE_V1_GET "ns_script_engine_v1_get"

struct ns_script_core_v1;

/**
 * A script engine, as the browser sees it.
 *
 * The eleven live entry points of `javascript/js.h`, in its order.
 * `js.h` declares a twelfth, `js_dom_event_add_listener`, which nothing
 * in the tree defines and nothing calls; it is not here, and it is a
 * candidate for deletion upstream rather than for implementation.
 *
 * `int` stands for `nserror` in the three that return one and for
 * `bool` in the two that return one, because neither type is a C
 * fundamental type and this table must not need NetSurf's headers.
 */
struct ns_script_engine_v1 {
	/** NS_SCRIPT_ENGINE_ABI_V1, checked before anything else is. */
	uint32_t abi;
	/** For the log line, e.g. "duktape".  Never NULL. */
	const char *name;

	void (*initialise)(void);
	void (*finalise)(void);

	/** nserror; `heap` is a `jsheap *` the browser only ever passes back. */
	int (*newheap)(int timeout, void **heap);
	void (*destroyheap)(void *heap);

	/** nserror; `thread` is a `jsthread *`, likewise opaque. */
	int (*newthread)(void *heap, void *win_priv, void *doc_priv,
			 void **thread);
	/** nserror */
	int (*closethread)(void *thread);
	void (*destroythread)(void *thread);

	/** bool */
	int (*exec)(void *thread, const uint8_t *txt, size_t txtlen,
		    const char *name);
	/** bool; `doc` is a `dom_document *`, `target` a `dom_node *`. */
	int (*fire_event)(void *thread, const char *type, void *doc,
			  void *target);
	/** `node` is a `dom_element *`. */
	void (*handle_new_element)(void *thread, void *node);
	/** `evt` is a `dom_event *`. */
	void (*event_cleanup)(void *thread, void *evt);
};

/**
 * The plugin's one entry point.
 *
 * It is a getter and a setter in one call, deliberately: a plugin must
 * be given the core table before it can do anything at all, and the
 * browser must be able to refuse it before it has done anything at all.
 * So one `dlsym` hands the engine its callbacks and takes its vtable
 * back, and nothing in between can be observed half-done.
 *
 * \param core the browser's callback table; valid for the process
 * \return the engine's vtable, or NULL to refuse to load
 */
typedef const struct ns_script_engine_v1 *
(ns_script_engine_v1_get_fn)(const struct ns_script_core_v1 *core);

/**
 * The engine this browser was *built* with -- duktape, or none.
 *
 * Defined by whichever arm of `javascript/Makefile` compiled, and
 * always available: it is what a refused or missing plugin falls back
 * to, so it can never be NULL.
 */
const struct ns_script_engine_v1 *ns_builtin_script_engine(void);

/**
 * The engine that is actually answering, for a caller that wants to say
 * which.  Never NULL after `js_initialise()`.
 */
const struct ns_script_engine_v1 *ns_active_script_engine(void);

/**
 * The engine `text/x-upy` scripts are run by, or NULL (netsurf_upy G5).
 *
 * Loaded from `script_engine_upy_path` by the same `engine_load()` that
 * loads `script_engine_path`, and refused by the same checks.  NULL
 * means the option was unset or the plugin was refused; a `text/x-upy`
 * script is then not executed at all, which is exactly what stock
 * NetSurf does with any script type it does not know.
 */
const struct ns_script_engine_v1 *ns_upy_script_engine(void);

/**
 * Run a `text/x-upy` script (netsurf_upy G5).
 *
 * Same signature as `js_exec()`, because `html/script.c` picks between
 * the two by MIME type and calls whichever it picked through one
 * `script_handler_t *`.  `thread` is the browser's own `jsthread *` --
 * the router's, not an engine's -- and the upy engine's heap and thread
 * are created inside it, lazily, the first time a page actually carries
 * a upy script.  A page that carries none pays nothing.
 *
 * \param thread the content's jsthread
 * \param txt the script source
 * \param txtlen its length in bytes
 * \param name what to call it in a message
 * \return true if the engine ran it
 */
bool js_exec_upy(jsthread *thread, const uint8_t *txt, size_t txtlen,
		 const char *name);

#endif /* NETSURF_JAVASCRIPT_ENGINE_H_ */
