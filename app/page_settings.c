/* Settings: a value list; OK cycles the selected option. */
#include "app_internal.h"

#include <stdio.h>

enum { ROW_WIFI, ROW_BLUETOOTH, ROW_FONT, ROW_REFRESH, ROW_PROGRESS, ROW_ABOUT, ROW_COUNT };

static void back_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray);

static const int k_refresh_choices[] = { 1, 3, 6, 10, 0 };
static const xr_action_t k_settings_actions[] = { { 1, "Back", XR_KEY_NONE, back_icon } };

typedef struct settings_page {
    xr_page_t base;
    xr_list_t list;
    char value[32];
} settings_page_t;

static settings_page_t s_settings;

static void settings_row(xr_list_t *l, int i, const char **primary, const char **secondary)
{
    settings_page_t *sp = (settings_page_t *)l->user;
    switch (i) {
    case ROW_WIFI:
        *primary = "Wi-Fi";
        *secondary = g_app.wifi_connected ? "Connected" : "Off";
        break;
    case ROW_BLUETOOTH:
        *primary = "Bluetooth";
        *secondary = g_app.bluetooth_connected ? "Connected" : "Off";
        break;
    case ROW_FONT:
        *primary = "Font size";
        *secondary = app_font_names[g_app.font_idx];
        break;
    case ROW_REFRESH:
        *primary = "Full refresh";
        if (g_app.full_refresh_every == 0) *secondary = "Never";
        else if (g_app.full_refresh_every == 1) *secondary = "Every page";
        else {
            snprintf(sp->value, sizeof sp->value, "Every %d pages", g_app.full_refresh_every);
            *secondary = sp->value;
        }
        break;
    case ROW_PROGRESS:
        *primary = "Progress bar";
        *secondary = g_app.show_progress ? "On" : "Off";
        break;
    default:
        *primary = "About";
        *secondary = "X-Reader 0.1";
        break;
    }
}

static void settings_selected(xr_list_t *l, int i)
{
    switch (i) {
    case ROW_WIFI:
        g_app.wifi_connected = !g_app.wifi_connected;
        break;
    case ROW_BLUETOOTH:
        g_app.bluetooth_connected = !g_app.bluetooth_connected;
        break;
    case ROW_FONT:
        g_app.font_idx = (g_app.font_idx + 1) % 3;
        break;
    case ROW_REFRESH: {
        int n = (int)XR_ARRAY_LEN(k_refresh_choices), k = 0;
        for (int j = 0; j < n; j++)
            if (k_refresh_choices[j] == g_app.full_refresh_every) k = j;
        g_app.full_refresh_every = k_refresh_choices[(k + 1) % n];
        xr_shell_set_full_refresh_every(g_app.shell, (uint16_t)g_app.full_refresh_every);
        break;
    }
    case ROW_PROGRESS:
        g_app.show_progress = !g_app.show_progress;
        break;
    default:
        return;
    }
    /* Only the value text changed: refresh just that row. */
    xr_list_invalidate_row(l, i, XR_REFRESH_QUALITY);
}

static void back_icon(xr_canvas_t *c, xr_rect_t r, uint8_t gray)
{
    xr_canvas_hline(c, r.x + 3, r.y + r.h / 2, r.w - 6, gray);
    xr_canvas_vline(c, r.x + 3, r.y + r.h / 2 - 5, 11, gray);
    xr_canvas_hline(c, r.x + 3, r.y + r.h / 2 - 5, 6, gray);
    xr_canvas_hline(c, r.x + 3, r.y + r.h / 2 + 5, 6, gray);
}

static void settings_create(xr_page_t *p)
{
    settings_page_t *sp = XR_CONTAINER_OF(p, settings_page_t, base);
    const xr_theme_t *t = p->shell->theme;
    xr_rect_t a = p->area;
    xr_list_init(&sp->list, xr_rect(a.x + 4, a.y + 4, a.w - 8, a.h - 8), XR_LIST_VALUE, t->row_h - 8,
                 t->font_normal, t->font_normal, settings_row, sp);
    sp->list.on_select = settings_selected;
    xr_list_set_count(&sp->list, ROW_COUNT);
    xr_page_add(p, &sp->list.base);
}

static void settings_action(xr_page_t *p, uint16_t id)
{
    (void)id;
    xr_shell_pop(p->shell);
}

static const xr_page_vtbl_t k_settings_vtbl = {
    .on_create = settings_create,
    .on_action = settings_action,
};

xr_page_t *app_page_settings(void)
{
    xr_page_init(&s_settings.base, &k_settings_vtbl, "Settings", XR_CHROME_ALL);
    xr_page_set_actions(&s_settings.base, k_settings_actions, XR_ARRAY_LEN(k_settings_actions));
    return &s_settings.base;
}
