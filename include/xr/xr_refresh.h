/*
 * xr_refresh.h - dirty-rectangle refresh scheduler.
 *
 * Invalidations are merged into a few rectangles, each carrying the
 * strongest refresh mode requested for it. After `full_every` QUALITY
 * updates the next update is promoted to a full flashing refresh to
 * clear accumulated ghosting (0 disables).
 */
#ifndef XR_REFRESH_H
#define XR_REFRESH_H

#include "xr_types.h"

#ifndef XR_REFRESH_MAX_RECTS
#define XR_REFRESH_MAX_RECTS 6
#endif

typedef struct xr_dirty {
    xr_rect_t rect;
    xr_refresh_t mode;
} xr_dirty_t;

typedef struct xr_refresh_sched {
    xr_dirty_t dirty[XR_REFRESH_MAX_RECTS];
    uint8_t count;
    uint8_t align;
    uint16_t full_every;
    uint16_t quality_since_full;
    xr_rect_t screen;
} xr_refresh_sched_t;

void xr_refresh_init(xr_refresh_sched_t *s, xr_rect_t screen, uint8_t align);
void xr_refresh_invalidate(xr_refresh_sched_t *s, xr_rect_t r, xr_refresh_t mode);
bool xr_refresh_pending(const xr_refresh_sched_t *s);
/* Removes the next region to compose + push. False when nothing is dirty. */
bool xr_refresh_take(xr_refresh_sched_t *s, xr_dirty_t *out);

#endif /* XR_REFRESH_H */
