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
 * The one core symbol a `#define` cannot redirect.
 *
 * Added 2026-09-25 (netsurf_upy, GPLv2 section 2(a) change notice).
 *
 * `javascript/core.h`'s redirections are `#define`s, and a `#define`
 * only reaches source the preprocessor has not read yet.  Almost all of
 * the engine's 586 core symbols are called from engine source, which is
 * exactly that; **one** is called from inside a library header, by a
 * `static inline` that has already been compiled into every translation
 * unit before the macros exist:
 *
 *     libdom include/dom/core/string.h::dom_string_unref()
 *             -> dom_string_destroy()
 *
 * So the plugin defines that symbol itself, as a thunk onto the table
 * slot.  `-Wl,--no-undefined` on the plugin link is what found it and
 * is what will find the next one: measured 2026-09-25, this file closed
 * the entire remainder of the link, and the finished shared object's
 * dynamic imports are the C library and nothing else.
 *
 * Only compiled into the plugin.  The browser's own copy of this symbol
 * is libdom's.
 */

#include "javascript/core.h"

#ifndef NS_ENGINE_PLUGIN
#error "core_thunks.c belongs to the plugin build only"
#endif

/* The macro is in the way of the *definition*, which is the whole
 * reason this file exists. */
#undef dom_string_destroy

/* declared in libdom's include/dom/core/string.h */
void dom_string_destroy(dom_string *str)
{
	ns_script_core->dom_string_destroy(str);
}
