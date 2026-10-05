#pragma once

/**
 * @brief E-ink refresh intent, ported from `xr_refresh_t` (include/xr/xr_types.h).
 *
 * Widgets and pages state *what* they need; the display port maps it onto
 * real waveforms (A2/DU/GC16/...). The three-level meaning below is load-
 * bearing: `core/refresh.hpp`'s ghosting-mitigation counter (promote to
 * `full` after N `quality` updates) and every display backend depend on
 * exactly these semantics, not a richer waveform enum.
 */
namespace refresh {

/** @brief Refresh intent requested by application code. */
enum class mode {
  none = 0,
  fast,     ///< partial, 1-bit-ish, no flash: focus moves, menus.
  quality,  ///< partial, full grayscale: text pages, dialogs.
  full,     ///< full screen with flash: clears ghosting.
};

}  // namespace refresh
