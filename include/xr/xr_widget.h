/*
 * xr_widget.h - widget base class, focus scope, and the stock widgets.
 *
 * OOP in C: every widget struct starts with `xr_widget_t base`, and
 * behaviour comes from a const vtable. Subclasses recover themselves
 * with XR_CONTAINER_OF(w, my_type_t, base).
 *
 * Widgets are statically allocated by their owner (page / dialog); the
 * tree is intrusive (parent / first_child / next_sibling), so the
 * framework never mallocs.
 */
#ifndef XR_WIDGET_H
#define XR_WIDGET_H

#include "xr_hal.h"
#include "xr_text.h"

struct xr_shell;
typedef struct xr_widget xr_widget_t;

typedef struct xr_widget_vtbl {
    const char *type_name;
    void (*render)(xr_widget_t *self, xr_canvas_t *c);
    /* Key events reach the focused widget; taps reach the hit widget. */
    bool (*on_event)(xr_widget_t *self, const xr_event_t *ev);
    void (*on_focus)(xr_widget_t *self, bool focused); /* optional */
} xr_widget_vtbl_t;

enum {
    XR_WF_VISIBLE   = 1u << 0,
    XR_WF_FOCUSABLE = 1u << 1,
    XR_WF_FOCUSED   = 1u << 2,
    XR_WF_DISABLED  = 1u << 3,
};

typedef void (*xr_activate_fn)(xr_widget_t *w, void *user);

struct xr_widget {
    const xr_widget_vtbl_t *vt;
    xr_rect_t rect;              /* absolute screen coordinates */
    uint8_t flags;
    xr_refresh_t refresh_hint;   /* mode used when it invalidates itself */
    xr_widget_t *parent, *first_child, *next_sibling;
    struct xr_shell *shell;      /* set on tree roots by the shell */
    xr_activate_fn on_activate;  /* "clicked" */
    void *user;
};

extern const xr_widget_vtbl_t xr_container_vtbl;

void xr_widget_init(xr_widget_t *w, const xr_widget_vtbl_t *vt, xr_rect_t rect);
void xr_widget_add(xr_widget_t *parent, xr_widget_t *child);
void xr_widget_set_visible(xr_widget_t *w, bool visible);
void xr_widget_render_tree(xr_widget_t *w, xr_canvas_t *c);
void xr_widget_invalidate(xr_widget_t *w);
void xr_widget_invalidate_rect(xr_widget_t *w, xr_rect_t r, xr_refresh_t mode);
xr_widget_t *xr_widget_hit_test(xr_widget_t *root, int x, int y);
struct xr_shell *xr_widget_shell(xr_widget_t *w);

static inline bool xr_widget_has_focus(const xr_widget_t *w)
{
    return (w->flags & XR_WF_FOCUSED) != 0;
}

/* ------------------------------------------------------------ focus scope */
/* A widget tree plus its focused widget. Pages and dialogs each own one. */

typedef struct xr_scope {
    xr_widget_t root;
    xr_widget_t *focus;
} xr_scope_t;

void xr_scope_init(xr_scope_t *s, xr_rect_t rect);
void xr_scope_set_focus(xr_scope_t *s, xr_widget_t *w);
void xr_scope_focus_first(xr_scope_t *s);
bool xr_scope_move_focus(xr_scope_t *s, int dir); /* +1 next, -1 prev */
/* Default routing: focused widget, then arrow-key focus moves, then taps. */
bool xr_scope_handle_event(xr_scope_t *s, const xr_event_t *ev);

/* ------------------------------------------------------------------ label */

typedef struct xr_label {
    xr_widget_t base;
    const char *text;
    const xr_font_t *font;
    xr_align_t align;
    uint8_t gray;
    bool wrap;
} xr_label_t;

void xr_label_init(xr_label_t *l, xr_rect_t r, const char *text,
                   const xr_font_t *font, xr_align_t align);
void xr_label_set_text(xr_label_t *l, const char *text);

/* ----------------------------------------------------------------- button */

typedef void (*xr_button_icon_fn)(xr_canvas_t *c, xr_rect_t r, uint8_t gray);

typedef struct xr_button {
    xr_widget_t base;
    const char *text;
    const xr_font_t *font;
    xr_button_icon_fn icon;
} xr_button_t;

void xr_button_init(xr_button_t *b, xr_rect_t r, const char *text,
                    const xr_font_t *font, xr_activate_fn fn, void *user);
void xr_button_set_icon(xr_button_t *b, xr_button_icon_fn icon);

/* ------------------------------------------------------------------- list */

typedef struct xr_list xr_list_t;

typedef enum xr_list_style {
    XR_LIST_TWO_LINE, /* primary on top, secondary below (library)   */
    XR_LIST_VALUE     /* primary left, secondary right (settings)     */
} xr_list_style_t;

typedef void (*xr_list_row_fn)(xr_list_t *l, int index,
                               const char **primary, const char **secondary);

struct xr_list {
    xr_widget_t base;
    xr_list_style_t style;
    int count, selected, top;
    int16_t row_h;
    const xr_font_t *font, *font_secondary;
    xr_list_row_fn get_row;
    void (*on_select)(xr_list_t *l, int index); /* OK / tap */
    void *user;
};

void xr_list_init(xr_list_t *l, xr_rect_t r, xr_list_style_t style, int16_t row_h,
                  const xr_font_t *font, const xr_font_t *font_secondary,
                  xr_list_row_fn get_row, void *user);
void xr_list_set_count(xr_list_t *l, int count);
void xr_list_select(xr_list_t *l, int index);
void xr_list_invalidate_row(xr_list_t *l, int index, xr_refresh_t mode);
int xr_list_visible_rows(const xr_list_t *l);

#endif /* XR_WIDGET_H */
