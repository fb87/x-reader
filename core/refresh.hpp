#pragma once

#include "geometry.hpp"

#include <array>
#include <cstdint>

/**
 * @brief E-ink refresh intent and the dirty-rect scheduler that merges
 * invalidations, ported from `xr_refresh_t`/`xr_refresh_sched_t`
 * (include/xr/xr_types.h, include/xr/xr_refresh.h, src/xr_refresh.c).
 *
 * Widgets and pages state *what* they need; the display port maps it onto
 * real waveforms (A2/DU/GC16/...). The three-level meaning below is load-
 * bearing: the ghosting-mitigation counter (promote to `full` after N
 * `quality` updates) and every display backend depend on exactly these
 * semantics, not a richer waveform enum.
 */
namespace refresh {

/** @brief Refresh intent requested by application code. */
enum class mode {
  none = 0,
  fast,     ///< partial, 1-bit-ish, no flash: focus moves, menus.
  reader_quality,  ///< partial GL16 grayscale: reader page turns with reduced flash.
  quality,  ///< partial GC16 grayscale: dialogs and general UI content.
  full,     ///< full screen with flash: clears ghosting.
};

inline constexpr int max_rects = 6;

/** @brief One pending region plus the strongest refresh mode requested for it. */
struct dirty {
  geometry::rect rect{};
  mode kind = mode::none;
};

/**
 * @brief Dirty-rectangle scheduler. Invalidations are merged into a few
 * rectangles, each carrying the strongest mode requested for it. After
 * `full_every` QUALITY updates the next update is promoted to a full
 * flashing refresh to clear accumulated ghosting (0 disables).
 */
struct scheduler {
  std::array<dirty, max_rects> entries{};
  int count = 0;
  std::uint8_t align = 1;
  std::uint16_t full_every = 0;
  std::uint16_t quality_since_full = 0;
  geometry::rect screen{};
};

/** @brief Resets the scheduler for a `screen`-sized surface with the given x-alignment. */
inline void init(scheduler& s, geometry::rect screen, std::uint8_t align) {
  s = scheduler{};
  s.screen = screen;
  s.align = align != 0 ? align : 1;
}

namespace detail {

inline geometry::rect align_x(geometry::rect r, int a) {
  if (a <= 1) return r;
  const int x0 = (r.x / a) * a;
  const int x1 = ((r.x + r.w + a - 1) / a) * a;
  return geometry::make(x0, r.y, x1 - x0, r.h);
}

/** @brief Merge when overlapping, or when the union wastes <= 25% extra area. */
inline bool worth_merging(geometry::rect a, geometry::rect b) {
  if (geometry::intersects(a, b)) return true;
  const std::int32_t u = geometry::area(geometry::merge(a, b));
  return u * 4 <= (geometry::area(a) + geometry::area(b)) * 5;
}

inline void remove_at(scheduler& s, int i) {
  for (int k = i; k < s.count - 1; ++k) s.entries[k] = s.entries[k + 1];
  --s.count;
}

}  // namespace detail

/** @brief Merges one invalidation into the pending set. */
inline void invalidate(scheduler& s, geometry::rect r, mode kind) {
  if (kind == mode::none) return;
  if (kind == mode::full) {
    s.entries[0] = dirty{s.screen, mode::full};
    s.count = 1;
    return;
  }
  if (s.count == 1 && s.entries[0].kind == mode::full) return;

  r = geometry::intersect(detail::align_x(geometry::intersect(r, s.screen), s.align), s.screen);
  if (geometry::empty(r)) return;

  dirty next{r, kind};
  bool merged = true;
  while (merged) {
    merged = false;
    for (int i = 0; i < s.count; ++i) {
      if (detail::worth_merging(s.entries[i].rect, next.rect)) {
        next.rect = geometry::merge(s.entries[i].rect, next.rect);
        if (s.entries[i].kind > next.kind) next.kind = s.entries[i].kind;
        detail::remove_at(s, i);
        merged = true;
        break;
      }
    }
  }
  if (s.count == max_rects) {  // out of slots: collapse.
    for (int i = 0; i < s.count; ++i) {
      next.rect = geometry::merge(next.rect, s.entries[i].rect);
      if (s.entries[i].kind > next.kind) next.kind = s.entries[i].kind;
    }
    s.count = 0;
  }
  s.entries[s.count++] = next;
}

/** @brief True when at least one region is pending. */
inline bool pending(const scheduler& s) { return s.count > 0; }

/** @brief Removes and returns the next region to compose + push; false when nothing is dirty. */
inline bool take(scheduler& s, dirty& out) {
  if (s.count == 0) return false;
  out = s.entries[0];
  detail::remove_at(s, 0);

  if (out.kind == mode::full) {
    s.quality_since_full = 0;
  } else if (out.kind == mode::reader_quality || out.kind == mode::quality) {
    ++s.quality_since_full;
    if (s.full_every != 0 && s.quality_since_full >= s.full_every) {
      // Ghost budget spent: one full flash covers everything pending.
      out.rect = s.screen;
      out.kind = mode::full;
      s.count = 0;
      s.quality_since_full = 0;
    }
  }
  return true;
}

}  // namespace refresh
