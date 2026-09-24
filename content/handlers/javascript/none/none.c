/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
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

/** \file
 * Dummy implementation of javascript engine functions.
 *
 * Changed 2026-09-25 (netsurf_upy, GPLv2 section 2(a) change notice):
 * these no longer *are* the `js_*` entry points of `javascript/js.h`.
 * `javascript/engine.c` owns those in every build and forwards them to
 * an engine vtable; this file is the vtable a build made with
 * `NETSURF_USE_DUKTAPE := NO` falls back to, and it is what a refused
 * plugin leaves running.  The bodies are unchanged.
 *
 * `none_js_initialise()` clearing `enable_javascript` is unchanged too,
 * and it is why the loader runs *before* the active engine's
 * `initialise`: a build with no engine compiled in but a good plugin
 * named in `script_engine_path` never reaches this file at all, so the
 * option is not cleared under a working engine.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "utils/errors.h"
#include "content/content.h"
#include "utils/nsoption.h"

#include "javascript/js.h"
#include "javascript/engine.h"
#include "utils/log.h"

static void none_js_initialise(void)
{
	nsoption_set_bool(enable_javascript, false);
}

static void none_js_finalise(void)
{
}

static int none_js_newheap(int timeout, void **heap)
{
	*heap = NULL;
	return (int) NSERROR_OK;
}

static void none_js_destroyheap(void *heap)
{
}

static int none_js_newthread(void *heap, void *win_priv, void *doc_priv,
			     void **thread)
{
	*thread = NULL;
	return (int) NSERROR_NOT_IMPLEMENTED;
}

static int none_js_closethread(void *thread)
{
	return (int) NSERROR_OK;
}

static void none_js_destroythread(void *thread)
{
}

static int none_js_exec(void *thread, const uint8_t *txt, size_t txtlen,
			const char *name)
{
	return 1;
}

static int none_js_fire_event(void *thread, const char *type, void *doc,
			      void *target)
{
	return 1;
}

static void none_js_handle_new_element(void *thread, void *node)
{
}

static void none_js_event_cleanup(void *thread, void *evt)
{
}

static const struct ns_script_engine_v1 none_engine_v1 = {
	.abi = NS_SCRIPT_ENGINE_ABI_V1,
	.name = "none",

	.initialise = none_js_initialise,
	.finalise = none_js_finalise,

	.newheap = none_js_newheap,
	.destroyheap = none_js_destroyheap,

	.newthread = none_js_newthread,
	.closethread = none_js_closethread,
	.destroythread = none_js_destroythread,

	.exec = none_js_exec,
	.fire_event = none_js_fire_event,
	.handle_new_element = none_js_handle_new_element,
	.event_cleanup = none_js_event_cleanup,
};

/* exported interface documented in javascript/engine.h */
const struct ns_script_engine_v1 *ns_builtin_script_engine(void)
{
	return &none_engine_v1;
}
