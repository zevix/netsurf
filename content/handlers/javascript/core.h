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
 * The core callback table a loaded script engine is given.
 *
 * Added 2026-09-25 (netsurf_upy, GPLv2 section 2(a) change notice).
 *
 * A script engine built as a shared object cannot see the browser's own
 * symbols: an executable exports nothing to a `dlopen`ed object unless
 * it is linked `-rdynamic`, and measured on 2026-09-24 a plugin that
 * leaves its host symbols undefined fails at `dlopen` with
 * `undefined symbol:` rather than at build time.  So the browser hands
 * the engine a table of everything it may call, and the engine's shared
 * object imports nothing but the C library.
 *
 * `javascript/core.def` is the list, and the only place it is written.
 * This header reads it to declare `struct ns_script_core_v1`, whose
 * members are typed by `__typeof__` of the browser's own declarations,
 * so a prototype cannot drift between the two sides.
 *
 * In a plugin (`NS_ENGINE_PLUGIN`) it also pulls in
 * `javascript/core_plugin.h`, which the build derives from the same
 * `core.def` with two `sed` expressions and which `#define`s each name
 * onto its table slot -- so engine source compiles **unchanged**.  The
 * `#define`s must land after every header that declares those names,
 * which is why this file includes those headers itself and why it is
 * force-included (`-include`) into the plugin's translation units.
 *
 * `javascript/core.c` defines the one instance, `ns_script_core_v1`.
 */

#ifndef NETSURF_JAVASCRIPT_CORE_H_
#define NETSURF_JAVASCRIPT_CORE_H_

#include <stdint.h>

#include <nsutils/time.h>

#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>

#include "utils/errors.h"
#include "utils/log.h"
#include "utils/nsurl.h"
#include "utils/corestrings.h"
#include "utils/useragent.h"
#include "utils/nsoption.h"
#include "utils/utils.h"

#include "netsurf/inttypes.h"
#include "netsurf/browser_window.h"
#include "netsurf/misc.h"
#include "netsurf/bitmap.h"

#include "desktop/gui_internal.h"
#include "desktop/gui_table.h"

#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"

#include "html/private.h"

/**
 * Everything a script engine may reach back into the browser for.
 *
 * Every member is a pointer -- a function pointer for a function, the
 * address of the object for a global -- so the whole table is a
 * constant initialiser and needs no run-time fill.  That matters for
 * the globals: `corestring_dom_click` and its 96 siblings are
 * `dom_string *` variables that `corestrings_init()` writes long after
 * static initialisation, so the table holds `&corestring_dom_click` and
 * not its value.
 */
struct ns_script_core_v1 {
#define NS_CORE_FN(n)   __typeof__(n) *n;
#define NS_CORE_DATA(n) __typeof__(n) *n;
#define NS_CORE_RAW(n)  __typeof__(n) *n;
#include "javascript/core.def"
#undef NS_CORE_FN
#undef NS_CORE_DATA
#undef NS_CORE_RAW
};

#ifndef NS_ENGINE_PLUGIN

/** The browser's table, defined in `javascript/core.c`. */
extern const struct ns_script_core_v1 ns_script_core_v1;

#else /* NS_ENGINE_PLUGIN */

/**
 * The table this plugin was handed.  `ns_script_engine_v1_get()` sets
 * it before it returns the engine, and the browser calls nothing in the
 * engine before that returns, so every other entry point may rely on it.
 */
extern const struct ns_script_core_v1 *ns_script_core;

/*
 * The redirections: `#define <name> (ns_script_core-><name>)` for a
 * function, `(*ns_script_core-><name>)` for a global.  Derived from
 * `core.def` by the `core_plugin.h` rule in
 * `content/handlers/javascript/duktape/Makefile`.
 */
#include "javascript/core_plugin.h"

/*
 * `NSLOG()` builds a `static nslog_entry_context_t` holding
 * `&__nslog_category_<name>`.  In a plugin that address is not a
 * constant expression, so no `#define` can redirect it -- which is what
 * `NS_CORE_RAW` marks in `core.def`, and why `core_plugin.h` emits
 * nothing for those three names.  Instead the context moves onto the
 * stack and the category comes out of the table, so a plugin's log
 * lines go through the *browser's* category objects and obey the same
 * `log_filter` as everything else.  The condition, the levels and the
 * argument list are libnslog's own.
 */
#ifdef WITH_NSLOG
#undef NSLOG
#define NSLOG(catname, level, logmsg, args...)				\
	do {								\
		if (NSLOG_LEVEL_##level >= NSLOG_COMPILED_MIN_LEVEL) {	\
			nslog_entry_context_t _nslog_ctx = {		\
				ns_script_core->__nslog_category_##catname, \
				NSLOG_LEVEL_##level,			\
				__FILE__,				\
				sizeof(__FILE__) - 1,			\
				__PRETTY_FUNCTION__,			\
				sizeof(__PRETTY_FUNCTION__) - 1,	\
				__LINE__,				\
			};						\
			nslog__log(&_nslog_ctx, logmsg, ##args);	\
		}							\
	} while(0)
#endif

#endif /* NS_ENGINE_PLUGIN */

#endif /* NETSURF_JAVASCRIPT_CORE_H_ */
