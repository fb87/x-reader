#include "xr/xr_page.h"
#include "xr/xr_shell.h"

#include <string.h>

void xr_page_init(xr_page_t *p, const xr_page_vtbl_t *vt, const char *title, uint8_t chrome)
{
    memset(p, 0, sizeof(*p));
    p->vt = vt;
    p->title = title;
    p->chrome = chrome;
    p->enter_refresh = XR_REFRESH_FULL; /* new screen: flash once, no ghosts */
    xr_scope_init(&p->scope, xr_rect(0, 0, 0, 0));
}

void xr_page_set_actions(xr_page_t *p, const xr_action_t *actions, uint8_t count)
{
    p->actions = actions;
    p->action_count = count;
    if (p->shell && xr_shell_top(p->shell) == p && (p->chrome & XR_CHROME_DOCK))
        xr_shell_invalidate(p->shell, p->shell->dock_rect, XR_REFRESH_QUALITY);
}

void xr_page_set_title(xr_page_t *p, const char *title)
{
    p->title = title;
    if (p->shell && xr_shell_top(p->shell) == p && (p->chrome & XR_CHROME_STATUS))
        xr_shell_invalidate(p->shell, p->shell->status_rect, XR_REFRESH_QUALITY);
}

void xr_page_add(xr_page_t *p, xr_widget_t *w)
{
    xr_widget_add(&p->scope.root, w);
}

void xr_page_invalidate(xr_page_t *p, xr_refresh_t mode)
{
    if (p->shell) xr_shell_invalidate(p->shell, p->area, mode);
}
