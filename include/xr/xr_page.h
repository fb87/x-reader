/*
 * xr_page.h - a full screen of UI (Home, Library, Reader, ...).
 *
 * Pages live on the shell's navigation stack. Each declares which shell
 * chrome it wants (status bar / dock) and gets `area`, the rect left
 * over for its content. Subclass by embedding `xr_page_t base` first.
 *
 * Lifecycle:
 *   push:    on_create -> on_enter          (covered page gets on_exit)
 *   pop:     on_exit -> on_destroy          (revealed page gets on_enter)
 *   replace: old on_exit/on_destroy, new on_create/on_enter
 *   chrome or area change: on_layout
 */
#ifndef XR_PAGE_H
#define XR_PAGE_H

#include "xr_widget.h"

enum {
    XR_CHROME_NONE   = 0,
    XR_CHROME_STATUS = 1u << 0,
    XR_CHROME_DOCK   = 1u << 1,
    XR_CHROME_ALL    = XR_CHROME_STATUS | XR_CHROME_DOCK,
};

/* A dock button. `key` is an optional hardware shortcut. */
typedef struct xr_action {
    uint16_t id;
    const char *label;
    xr_key_t key;
    xr_button_icon_fn icon;
} xr_action_t;

typedef struct xr_page xr_page_t;

typedef struct xr_page_vtbl {
    void (*on_create)(xr_page_t *p);   /* build widgets inside p->area   */
    void (*on_layout)(xr_page_t *p);   /* p->area changed (chrome toggle) */
    void (*on_enter)(xr_page_t *p);
    void (*on_exit)(xr_page_t *p);
    void (*on_destroy)(xr_page_t *p);
    /* NULL = render the widget tree. Custom pages can draw directly. */
    void (*render)(xr_page_t *p, xr_canvas_t *c);
    /* Sees events before the default focus routing. Return true if used. */
    bool (*on_event)(xr_page_t *p, const xr_event_t *ev);
    void (*on_action)(xr_page_t *p, uint16_t action_id);
    void (*on_tick)(xr_page_t *p, uint32_t now_ms);
} xr_page_vtbl_t;

struct xr_page {
    const xr_page_vtbl_t *vt;
    struct xr_shell *shell;
    const char *title;           /* shown in the status bar */
    uint8_t chrome;              /* XR_CHROME_* */
    xr_refresh_t enter_refresh;  /* how the screen refreshes on navigation */
    const xr_action_t *actions;
    uint8_t action_count;
    xr_rect_t area;              /* content rect, set by the shell */
    xr_scope_t scope;            /* widget tree + focus */
};

void xr_page_init(xr_page_t *p, const xr_page_vtbl_t *vt, const char *title, uint8_t chrome);
void xr_page_set_actions(xr_page_t *p, const xr_action_t *actions, uint8_t count);
void xr_page_set_title(xr_page_t *p, const char *title);
void xr_page_add(xr_page_t *p, xr_widget_t *w);
void xr_page_invalidate(xr_page_t *p, xr_refresh_t mode);

#endif /* XR_PAGE_H */
