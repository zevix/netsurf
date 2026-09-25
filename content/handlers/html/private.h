/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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

/*
 * Changed 2026-09-24 for netsurf_upy (GPLv2 section 2(a), a dated
 * notice of change).  This file gained:
 * declared html_relayout(); html_content gained late_css_retries
 * (VitaSurf) and deferred_links (netsurf_upy).
 *
 * Changed 2026-09-24 for netsurf_upy (GPLv2 section 2(a), a dated
 * notice of change), a second time: declared html_mark_dom_dirty() and
 * html_mark_dom_dirty_node(), and html_content gained the dirty flag and
 * the rebuild-cost bookkeeping they drive (dom_dirty,
 * dom_dirty_retries, relayout_count, relayout_elements,
 * relayout_last_ms, relayout_quiet_until).
 *
 * Changed 2026-09-25 for netsurf_upy (GPLv2 section 2(a), a dated
 * notice of change), a third time: declared
 * html_content_get_document(), the accessor a loaded script engine
 * reaches an html_content's document through.  A plugin is a separate
 * build and may not dereference an html_content -- see
 * javascript/core.def, where this is the 587th name.
 *
 * Adapted from VitaSurf <https://github.com/Breezyslasher/VitaSurf>,
 * patches/0022-netsurf-relayout-after-script-changes.patch, by Breezyslasher.
 * Adapted from VitaSurf <https://github.com/Breezyslasher/VitaSurf>,
 * patches/0105-netsurf-late-stylesheets.patch, by Breezyslasher.
 * GPL-2.0, same as NetSurf.
 *
 * The fork and the rest of its changes: netsurf_upy/ in the ubitron
 * repository; see netsurf_upy/README.md.
 */

/**
 * \file
 * Private data for text/html content.
 */

#ifndef NETSURF_HTML_PRIVATE_H
#define NETSURF_HTML_PRIVATE_H

#include <stdint.h>

#include <dom/bindings/hubbub/parser.h>

#include "netsurf/types.h"
#include "content/content_protected.h"
#include "content/handlers/css/utils.h"


struct gui_layout_table;
struct scrollbar_msg_data;
struct content_redraw_data;
struct selection;

typedef enum {
	HTML_DRAG_NONE,			/** No drag */
	HTML_DRAG_SELECTION,		/** Own; Text selection */
	HTML_DRAG_SCROLLBAR,		/** Not own; drag in scrollbar widget */
	HTML_DRAG_TEXTAREA_SELECTION,	/** Not own; drag in textarea widget */
	HTML_DRAG_TEXTAREA_SCROLLBAR,	/** Not own; drag in textarea widget */
	HTML_DRAG_CONTENT_SELECTION,	/** Not own; drag in child content */
	HTML_DRAG_CONTENT_SCROLL	/** Not own; drag in child content */
} html_drag_type;

/**
 * For drags we don't own
 */
union html_drag_owner {
	bool no_owner;
	struct box *content;
	struct scrollbar *scrollbar;
	struct box *textarea;
};

typedef enum {
	HTML_SELECTION_NONE,		/** No selection */
	HTML_SELECTION_TEXTAREA,	/** Selection in one of our textareas */
	HTML_SELECTION_SELF,		/** Selection in this html content */
	HTML_SELECTION_CONTENT		/** Selection in child content */
} html_selection_type;

/**
 * For getting at selections in this content or things in this content
 */
union html_selection_owner {
	bool none;
	struct box *textarea;
	struct box *content;
};

typedef enum {
	HTML_FOCUS_SELF,		/**< Focus is our own */
	HTML_FOCUS_CONTENT,		/**< Focus belongs to child content */
	HTML_FOCUS_TEXTAREA		/**< Focus belongs to textarea */
} html_focus_type;

/**
 * For directing input
 */
union html_focus_owner {
	bool self;
	struct box *textarea;
	struct box *content;
};

/**
 * Data specific to CONTENT_HTML.
 */
typedef struct html_content {
	struct content base;

	dom_hubbub_parser *parser; /**< Parser object handle */
	bool parse_completed; /**< Whether the parse has been completed */
	bool conversion_begun; /**< Whether or not the conversion has begun */

	/** Document tree */
	dom_document *document;
	/** Quirkyness of document */
	dom_document_quirks_mode quirks;

	/** Encoding of source, NULL if unknown. */
	char *encoding;
	/** Source of encoding information. */
	dom_hubbub_encoding_source encoding_source;

	/** Base URL (may be a copy of content->url). */
	struct nsurl *base_url;
	/** Base target */
	char *base_target;

	/** Content has been aborted in the LOADING state */
	bool aborted;

	/** Whether a meta refresh has been handled */
	bool refresh;

	/** Whether a layout (reflow) is in progress */
	bool reflowing;

	/** Whether an initial layout has been done */
	bool had_initial_layout;

	/** Whether scripts are enabled for this content */
	bool enable_scripting;

	/* Title element node */
	dom_node *title;

	/** A talloc context purely for the render box tree */
	int *bctx;
	/** A context pointer for the box conversion, NULL if no conversion
	 * is in progress.
	 */
	void *box_conversion_context;
	/** Box tree, or NULL. */
	struct box *layout;
	/** Document background colour. */
	colour background_colour;

	/** Font callback table */
	const struct gui_layout_table *font_func;

	/** Number of entries in scripts */
	unsigned int scripts_count;
	/** Scripts */
	struct html_script *scripts;
	/** javascript thread in use */
	struct jsthread *jsthread;

	/** Number of entries in stylesheet_content. */
	unsigned int stylesheet_count;
	/** Stylesheets. Each may be NULL. */
	struct html_stylesheet *stylesheets;
	/**< Style selection context */
	css_select_ctx *select_ctx;
	/** retries of the rebuild a late stylesheet asks for (VitaSurf) */
	unsigned late_css_retries;
	/** <link> stylesheets whose fetch was held back past conversion
	 *  (netsurf_upy, nsoption defer_author_stylesheets) */
	struct html_deferred_link *deferred_links;

	/** The document has been mutated since the box tree was built and
	 *  a rebuild is owed (netsurf_upy; VitaSurf's vita/js/qjs.c calls
	 *  the same thing mark_dirty()).  Set by html_mark_dom_dirty(),
	 *  cleared when the rebuild has run or been refused. */
	bool dom_dirty;
	/** retries of the rebuild a DOM mutation asks for (netsurf_upy) */
	unsigned dom_dirty_retries;
	/** how many rebuilds a DOM mutation has asked for and got */
	unsigned relayout_count;
	/** elements counted for the last rebuild, and what it cost in ms:
	 *  together they are the per-element cost the budget estimates the
	 *  next rebuild with (netsurf_upy) */
	unsigned relayout_elements;
	uint64_t relayout_last_ms;
	/** monotonic ms before which the next rebuild may not start -- the
	 *  quiet period, HTML_RELAYOUT_QUIET_FACTOR times the last
	 *  rebuild's cost (netsurf_upy, VitaSurf's policy) */
	uint64_t relayout_quiet_until;
	/**< Style selection media specification */
	css_media media;
	/** CSS length conversion context for document. */
	css_unit_ctx unit_len_ctx;
	/**< Universal selector */
	lwc_string *universal;

	/** Number of entries in object_list. */
	unsigned int num_objects;
	/** List of objects. */
	struct content_html_object *object_list;
	/** Forms, in reverse order to document. */
	struct form *forms;
	/** Hash table of imagemaps. */
	struct imagemap **imagemaps;

	/** Browser window containing this document, or NULL if not open. */
	struct browser_window *bw;

	/** Frameset information */
	struct content_html_frames *frameset;

	/** Inline frame information */
	struct content_html_iframe *iframe;

	/** Content of type CONTENT_HTML containing this, or NULL if not an
	 * object within a page. */
	struct html_content *page;

	/** Current drag type */
	html_drag_type drag_type;
	/** Widget capturing all mouse events */
	union html_drag_owner drag_owner;

	/** Current selection state */
	html_selection_type selection_type;
	/** Current selection owner */
	union html_selection_owner selection_owner;

	/** Current input focus target type */
	html_focus_type focus_type;
	/** Current input focus target */
	union html_focus_owner focus_owner;

	/** HTML content's own text selection object */
	struct selection *sel;

	/**
	 * Open core-handled form SELECT menu, or NULL if none
	 *  currently open.
	 */
	struct form_control *visible_select_menu;

} html_content;

/**
 * Render padding and margin box outlines in html_redraw().
 */
extern bool html_redraw_debug;


/* in html/html.c */

/**
 * redraw a box
 *
 * \param htmlc HTML content
 * \param box The box to redraw.
 */
void html__redraw_a_box(html_content *htmlc, struct box *box);


/**
 * Complete conversion of an HTML document
 *
 * \param htmlc Content to convert
 */
void html_finish_conversion(html_content *htmlc);


/**
 * Test if an HTML content conversion can begin
 *
 * \param htmlc		html content to test
 * \return true iff the html content conversion can begin
 */
bool html_can_begin_conversion(html_content *htmlc);


/**
 * Begin conversion of an HTML document
 *
 * \param htmlc Content to convert
 */
bool html_begin_conversion(html_content *htmlc);


/**
 * execute some text as a script element
 */
bool html_exec(struct content *c, const char *src, size_t srclen);


/**
 * Attempt script execution for defer and async scripts
 *
 * execute scripts using algorithm found in:
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/scripting-1.html#the-script-element
 *
 * \param htmlc html content.
 * \param allow_defer allow deferred execution, if not, only async scripts.
 * \return NSERROR_OK error code.
 */
nserror html_script_exec(html_content *htmlc, bool allow_defer);


/**
 * Free all script resources and references for a html content.
 *
 * \param htmlc html content.
 * \return NSERROR_OK or error code.
 */
nserror html_script_free(html_content *htmlc);


/**
 * Check if any of the scripts loaded were insecure
 */
bool html_saw_insecure_scripts(html_content *htmlc);


/**
 * Complete the HTML content state machine *iff* all scripts are finished
 */
nserror html_proceed_to_done(html_content *html);


/* in html/redraw.c */
bool html_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip, const struct redraw_context *ctx);


/* in html/redraw_border.c */
bool html_redraw_borders(struct box *box, int x_parent, int y_parent,
		int p_width, int p_height, const struct rect *clip, float scale,
		const struct redraw_context *ctx);


bool html_redraw_inline_borders(struct box *box, struct rect b,
		const struct rect *clip, float scale, bool first, bool last,
		const struct redraw_context *ctx);


/* in html/script.c */
dom_hubbub_error html_process_script(void *ctx, dom_node *node);


/**
 * Rebuild the box tree from the (script-modified) document and lay it
 * out again (VitaSurf).
 *
 * \param htmlc html content whose layout is stale
 * \return NSERROR_OK if the layout was rebuilt, NSERROR_INVALID if the
 *         content is not in a state where that is possible right now
 *         (still converting, being dragged, typed into) and the caller
 *         should try again later.
 */
nserror html_relayout(html_content *htmlc);

/**
 * Note that the document has been mutated and ask for a rebuild
 * (netsurf_upy).
 *
 * The policy is VitaSurf's, ported rather than copied: a dirty flag set
 * by each mutating binding, coalesced onto one timer, gated on
 * CONTENT_STATUS_DONE with no outstanding fetches, held off by an
 * element and cost budget, and followed by a quiet period of about four
 * times the last rebuild.  It is a no-op unless nsoption
 * enable_dynamic_relayout is set.
 *
 * \param htmlc html content whose document has changed
 */
void html_mark_dom_dirty(html_content *htmlc);

/**
 * html_mark_dom_dirty() for a caller that has a node and not a content
 * (netsurf_upy).
 *
 * Finds the content through the node's owner document and the
 * __ns_key_html_content_data user data html_create_html_data() put
 * there -- the hop CanvasRenderingContext2D.bnd::redraw_node() already
 * makes.  Safe on a node that has no box and on one detached from the
 * document; both still carry an owner document.
 *
 * \param node the node that was mutated, or whose children were
 */
void html_mark_dom_dirty_node(dom_node *node);

/**
 * The document of an html content, for a caller that holds the content
 * as an opaque pointer (netsurf_upy).
 *
 * `html_get_document()` takes an `hlcache_handle`; a script engine is
 * given the html content itself, as `doc_priv` of js_newthread(), and a
 * *loaded* engine is a separate build that must not dereference the
 * struct (R3.4: no NetSurf type layout crosses the plugin boundary).
 * This is the one hop it needs, and it is in javascript/core.def so the
 * plugin reaches it through the callback table like everything else.
 *
 * \param htmlc html content, or NULL
 * \return the content's document, or NULL
 */
dom_document *html_content_get_document(html_content *htmlc);

/* in html/forms.c */
struct form *html_forms_get_forms(const char *docenc, dom_html_document *doc);
struct form_control *html_forms_get_control_for_node(struct form *forms,
		dom_node *node);


/* in html/css_fetcher.c */
/**
 * Register the fetcher for the pseudo x-ns-css scheme.
 *
 * \return NSERROR_OK on successful registration or error code on failure.
 */
nserror html_css_fetcher_register(void);
nserror html_css_fetcher_add_item(dom_string *data, struct nsurl *base_url,
		uint32_t *key);


/* Events */
/**
 * Construct an event and fire it at the DOM
 *
 */
bool fire_generic_dom_event(dom_string *type, dom_node *target,
		    bool bubbles, bool cancelable);

/**
 * Construct a keyboard event and fire it at the DOM
 */
bool fire_dom_keyboard_event(dom_string *type, dom_node *target,
		bool bubbles, bool cancelable, uint32_t key);

/* Useful dom_string pointers */
struct dom_string;

extern struct dom_string *html_dom_string_map;
extern struct dom_string *html_dom_string_id;
extern struct dom_string *html_dom_string_name;
extern struct dom_string *html_dom_string_area;
extern struct dom_string *html_dom_string_a;
extern struct dom_string *html_dom_string_nohref;
extern struct dom_string *html_dom_string_href;
extern struct dom_string *html_dom_string_target;
extern struct dom_string *html_dom_string_shape;
extern struct dom_string *html_dom_string_default;
extern struct dom_string *html_dom_string_rect;
extern struct dom_string *html_dom_string_rectangle;
extern struct dom_string *html_dom_string_coords;
extern struct dom_string *html_dom_string_circle;
extern struct dom_string *html_dom_string_poly;
extern struct dom_string *html_dom_string_polygon;
extern struct dom_string *html_dom_string_text_javascript;
extern struct dom_string *html_dom_string_type;
extern struct dom_string *html_dom_string_src;

#endif
