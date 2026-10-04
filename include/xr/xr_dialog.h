/*
 * xr_dialog.h - modal popups on the dialog layer.
 *
 * A dialog sizes itself to its content in `layout`, is centred by the
 * shell, takes all input while on top, and reports a result through a
 * callback when closed. Subclass by embedding `xr_dialog_t base` first.
 */
#ifndef XR_DIALOG_H
#define XR_DIALOG_H

#include "xr_widget.h"

#define XR_DIALOG_SHADOW 6

enum { XR_RESULT_CANCEL = -1, XR_RESULT_NO = 0, XR_RESULT_YES = 1 };

typedef struct xr_dialog xr_dialog_t;
typedef void (*xr_dialog_result_fn)(xr_dialog_t *d, int result, void *user);

typedef struct xr_dialog_vtbl {
    /* Set d->rect (size from content, placed inside `bounds`) and build
     * widgets. The shell centres nothing for you: use xr_dialog_place(). */
    void (*layout)(xr_dialog_t *d, xr_rect_t bounds);
    void (*render)(xr_dialog_t *d, xr_canvas_t *c); /* NULL = default */
    bool (*on_event)(xr_dialog_t *d, const xr_event_t *ev);
} xr_dialog_vtbl_t;

struct xr_dialog {
    const xr_dialog_vtbl_t *vt;
    struct xr_shell *shell;
    const char *title;           /* NULL = no title bar */
    xr_button_icon_fn icon;      /* optional title-bar icon */
    xr_rect_t rect;
    xr_scope_t scope;
    xr_dialog_result_fn on_result;
    void *user;
};

void xr_dialog_init(xr_dialog_t *d, const xr_dialog_vtbl_t *vt, const char *title);
void xr_dialog_set_icon(xr_dialog_t *d, xr_button_icon_fn icon);
/* Centre a w x h dialog in bounds and reset its widget tree to that rect. */
void xr_dialog_place(xr_dialog_t *d, xr_rect_t bounds, int w, int h);
int xr_dialog_header_height(const xr_dialog_t *d);
xr_rect_t xr_dialog_dirty_rect(const xr_dialog_t *d); /* incl. shadow */
void xr_dialog_render_frame(xr_dialog_t *d, xr_canvas_t *c);
void xr_dialog_default_render(xr_dialog_t *d, xr_canvas_t *c);
void xr_dialog_close(xr_dialog_t *d, int result);

/* ------------------------------------------------------- confirm (Yes/No) */

typedef struct xr_confirm {
    xr_dialog_t base;
    const char *message, *yes_text, *no_text;
    xr_label_t label;
    xr_button_t yes, no;
} xr_confirm_t;

/* Focus starts on "No": a stray button press must not be destructive. */
void xr_confirm_show(struct xr_shell *s, xr_confirm_t *cd, const char *title,
                     const char *message, const char *yes_text, const char *no_text,
                     xr_dialog_result_fn cb, void *user);

#endif /* XR_DIALOG_H */
