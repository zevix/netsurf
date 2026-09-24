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
 * The one instance of the core callback table.
 *
 * Added 2026-09-25 (netsurf_upy, GPLv2 section 2(a) change notice).
 *
 * Compiled into the browser whichever engine is built in, because the
 * table is what a *loaded* engine is given and a loader exists in every
 * build.  Its address is passed to `ns_script_engine_v1_get()` by
 * `javascript/engine.c`.
 */

#include "javascript/core.h"

/* exported interface documented in javascript/core.h */
const struct ns_script_core_v1 ns_script_core_v1 = {
#define NS_CORE_FN(n)   .n = &n,
#define NS_CORE_DATA(n) .n = &n,
#define NS_CORE_RAW(n)  .n = &n,
#include "javascript/core.def"
#undef NS_CORE_FN
#undef NS_CORE_DATA
#undef NS_CORE_RAW
};
