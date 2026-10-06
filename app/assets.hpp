#pragma once

#include "../core/canvas.hpp"
#include "../core/geometry.hpp"
#include "../core/text.hpp"

#include <cstdint>

/**
 * @brief Bridges the existing C-compiled bitmap font/icon tables
 * (fonts/xr_font_*.c, fonts/xr_icons.c) into the new `text::font` type,
 * instead of waiting on the font-generator-tooling phase to regenerate
 * them.
 *
 * `xr_font_t` and `text::font` have byte-identical layout (verified by
 * the Phase 4/5 differential tests against real font data), and
 * `extern "C"` linkage means the symbol names aren't mangled, so these
 * declarations bind directly to the real, already-compiled C objects.
 * The font-regeneration phase later points `tools/gen_font.py` at this
 * same struct shape; until then, this bridge lets real fonts (full
 * Vietnamese coverage included) flow through real app pages without
 * a placeholder detour.
 */
extern "C" {
extern const text::font xr_font_alegreya_14;
extern const text::font xr_font_alegreya_17;
extern const text::font xr_font_alegreya_18;
extern const text::font xr_font_alegreya_20;
extern const text::font xr_font_alegreya_24;
extern const text::font xr_font_alegreya_bold_18;
extern const text::font xr_font_alegreya_bold_26;

/** @brief One 24x24 1bpp Material-Symbols-style icon glyph, ported from `xr_icon_glyph_t`. */
struct xr_icon_glyph_bridge {
  std::uint8_t width;
  std::uint8_t bitmap[72];
};
extern const xr_icon_glyph_bridge xr_icons[14];
}

namespace app {

/** @brief Stable icon identifiers, matching `enum xr_icon` (fonts/xr_icons.h) by position. */
enum class icon {
  none = 0,
  book,
  folder,
  description,
  settings,
  del,  // "delete" is a keyword-adjacent name best avoided.
  arrow_back,
  search,
  info,
  text_fields,
  remove,
  sd_card,
  close,
  add,
  count,
};

/** @brief The three body-text sizes selectable from Settings. */
inline const text::font* const body_fonts[3] = {&xr_font_alegreya_17, &xr_font_alegreya_20,
                                                &xr_font_alegreya_24};
inline const char* const font_names[3] = {"Small", "Medium", "Large"};

/** @brief Draws one icon glyph at `r`'s origin in the given gray, ported from `app_draw_icon`. */
inline void draw_icon(canvas::surface& c, geometry::rect r, icon value, std::uint8_t gray) {
  const auto index = static_cast<int>(value);
  if (index < 0 || index >= static_cast<int>(icon::count)) return;
  const xr_icon_glyph_bridge& glyph = xr_icons[index];
  for (int y = 0; y < 24; ++y) {
    for (int x = 0; x < 24; ++x) {
      if ((glyph.bitmap[y * 3 + x / 8] & static_cast<std::uint8_t>(0x80u >> (x % 8))) != 0) {
        canvas::fill(c, geometry::make(r.x + x, r.y + y, 1, 1), gray);
      }
    }
  }
}

/** @brief A bordered progress bar, ported from `app_draw_progress`. */
inline void draw_progress(canvas::surface& c, geometry::rect r, int percent) {
  canvas::border(c, r, 2, canvas::gray::black);
  const geometry::rect in = geometry::inset(r, 3);
  canvas::fill(c, geometry::make(in.x, in.y, in.w * percent / 100, in.h), canvas::gray::dark);
}

/** @brief The "back" chevron, shared by the Library/Favorites and Settings action bars. */
inline void back_icon(canvas::surface& c, geometry::rect r, std::uint8_t g) {
  draw_icon(c, r, icon::arrow_back, g);
}

}  // namespace app
