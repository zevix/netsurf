/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 * Copyright 2015 Daniel Dilverstone <dsilvers@netsurf-browser.org>
 * Copyright 2016 Michael Drake <tlsa@netsurf-browser.org>
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
 * Duktapeish implementation of javascript engine functions, prototypes.
 *
 * Changed 2026-09-25 for netsurf_upy (GPLv2 section 2(a), a dated
 * notice of change): the eleven engine entry points are declared here.
 * They used to be `js.h`'s `js_*` externs; `javascript/engine.c` owns
 * those now and reaches these through `struct ns_script_engine_v1`, so
 * they need a header of the engine's own.  See `javascript/engine.h`.
 *
 * The fork and the rest of its changes: netsurf_upy/ in the ubitron
 * repository; see netsurf_upy/README.md.
 */

#ifndef DUKKY_H
#define DUKKY_H

#include <stdint.h>
#include <stddef.h>

#include "javascript/js.h"
#include "javascript/engine.h"

/**
 * The duktape engine, as `javascript/js.h` used to spell it.
 *
 * Bodies unchanged in `dukky.c`; only the names moved, so that one
 * browser can hold this engine *and* a loaded one without two
 * definitions of `js_exec`.  `jsheap` and `jsthread` are still this
 * file's own opaque types -- the vtable in `javascript/engine.h` sees
 * them as `void *` and never looks inside.
 */
void dukky_js_initialise(void);
void dukky_js_finalise(void);
nserror dukky_js_newheap(int timeout, jsheap **heap);
void dukky_js_destroyheap(jsheap *heap);
nserror dukky_js_newthread(jsheap *heap, void *win_priv, void *doc_priv,
			   jsthread **thread);
nserror dukky_js_closethread(jsthread *thread);
void dukky_js_destroythread(jsthread *thread);
bool dukky_js_exec(jsthread *thread, const uint8_t *txt, size_t txtlen,
		   const char *name);
bool dukky_js_fire_event(jsthread *thread, const char *type,
			 struct dom_document *doc, struct dom_node *target);
void dukky_js_handle_new_element(jsthread *thread, struct dom_element *node);
void dukky_js_event_cleanup(jsthread *thread, struct dom_event *evt);

#ifdef NS_ENGINE_PLUGIN
/** This shared object's one export; see `javascript/engine.h`. */
const struct ns_script_engine_v1 *
ns_script_engine_v1_get(const struct ns_script_core_v1 *core);
#endif

duk_ret_t dukky_create_object(duk_context *ctx, const char *name, int args);
duk_bool_t dukky_push_node_stacked(duk_context *ctx);
duk_bool_t dukky_push_node(duk_context *ctx, struct dom_node *node);
void dukky_inject_not_ctr(duk_context *ctx, int idx, const char *name);
void dukky_register_event_listener_for(duk_context *ctx,
				       struct dom_element *ele,
				       dom_string *name,
				       bool capture);
bool dukky_get_current_value_of_event_handler(duk_context *ctx,
					      dom_string *name,
					      dom_event_target *et);
void dukky_push_event(duk_context *ctx, dom_event *evt);
bool dukky_event_target_push_listeners(duk_context *ctx, bool dont_create);

typedef enum {
	ELF_CAPTURE = 1 << 0,
	ELF_PASSIVE = 1 << 1,
	ELF_ONCE    = 1 << 2,
	ELF_NONE    = 0
} event_listener_flags;

void dukky_shuffle_array(duk_context *ctx, duk_uarridx_t idx);

/* pcall something, and if it errored, also dump the error to the log */
duk_int_t dukky_pcall(duk_context *ctx, duk_size_t argc, bool reset_timeout);

/* Push a generics function onto the stack */
void dukky_push_generics(duk_context *ctx, const char *generic);

/* Log the current stack frame if possible */
void dukky_log_stack_frame(duk_context *ctx, const char * reason);

#endif
