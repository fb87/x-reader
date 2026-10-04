#include "xr/xr_refresh.h"

#include <string.h>

void xr_refresh_init(xr_refresh_sched_t *s, xr_rect_t screen, uint8_t align)
{
    memset(s, 0, sizeof(*s));
    s->screen = screen;
    s->align = align ? align : 1;
}

static xr_rect_t align_x(xr_rect_t r, int a)
{
    if (a <= 1) return r;
    int x0 = (r.x / a) * a;
    int x1 = ((r.x + r.w + a - 1) / a) * a;
    return xr_rect(x0, r.y, x1 - x0, r.h);
}

/* Merge when overlapping, or when the union wastes <= 25% extra area. */
static bool worth_merging(xr_rect_t a, xr_rect_t b)
{
    if (xr_rect_intersects(a, b)) return true;
    int32_t u = xr_rect_area(xr_rect_union(a, b));
    return u * 4 <= (xr_rect_area(a) + xr_rect_area(b)) * 5;
}

static void remove_at(xr_refresh_sched_t *s, int i)
{
    memmove(&s->dirty[i], &s->dirty[i + 1], (size_t)(s->count - i - 1) * sizeof(xr_dirty_t));
    s->count--;
}

void xr_refresh_invalidate(xr_refresh_sched_t *s, xr_rect_t r, xr_refresh_t mode)
{
    if (mode == XR_REFRESH_NONE) return;
    if (mode == XR_REFRESH_FULL) {
        s->dirty[0].rect = s->screen;
        s->dirty[0].mode = XR_REFRESH_FULL;
        s->count = 1;
        return;
    }
    if (s->count == 1 && s->dirty[0].mode == XR_REFRESH_FULL) return;

    r = xr_rect_intersect(align_x(xr_rect_intersect(r, s->screen), s->align), s->screen);
    if (xr_rect_empty(r)) return;

    xr_dirty_t nd = { r, mode };
    bool merged = true;
    while (merged) {
        merged = false;
        for (int i = 0; i < s->count; i++) {
            if (worth_merging(s->dirty[i].rect, nd.rect)) {
                nd.rect = xr_rect_union(s->dirty[i].rect, nd.rect);
                if (s->dirty[i].mode > nd.mode) nd.mode = s->dirty[i].mode;
                remove_at(s, i);
                merged = true;
                break;
            }
        }
    }
    if (s->count == XR_REFRESH_MAX_RECTS) { /* out of slots: collapse */
        for (int i = 0; i < s->count; i++) {
            nd.rect = xr_rect_union(nd.rect, s->dirty[i].rect);
            if (s->dirty[i].mode > nd.mode) nd.mode = s->dirty[i].mode;
        }
        s->count = 0;
    }
    s->dirty[s->count++] = nd;
}

bool xr_refresh_pending(const xr_refresh_sched_t *s) { return s->count > 0; }

bool xr_refresh_take(xr_refresh_sched_t *s, xr_dirty_t *out)
{
    if (s->count == 0) return false;
    *out = s->dirty[0];
    remove_at(s, 0);

    if (out->mode == XR_REFRESH_FULL) {
        s->quality_since_full = 0;
    } else if (out->mode == XR_REFRESH_QUALITY) {
        s->quality_since_full++;
        if (s->full_every && s->quality_since_full >= s->full_every) {
            /* Ghost budget spent: one full flash covers everything pending. */
            out->rect = s->screen;
            out->mode = XR_REFRESH_FULL;
            s->count = 0;
            s->quality_since_full = 0;
        }
    }
    return true;
}
