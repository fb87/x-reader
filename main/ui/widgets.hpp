#pragma once

#include <stdint.h>

#include "gfx/font.hpp"
#include "gfx/framebuffer.hpp"
#include "ui/layout/layout.hpp"

namespace xreader
{
namespace ui
{
namespace widgets
{

// Each widget exposes its hit-test next to its draw call.  Keeping the two in
// one place is what stops a control from being drawn somewhere it cannot be
// touched, which is how the bottom action bar became decorative.

// Horizontal slider: a track with a knob at value/max_value.
void draw_slider(gfx::framebuffer_t* framebuffer, layout::rect_t bounds, uint8_t value,
                 uint8_t max_value);
bool slider_value_at(layout::rect_t bounds, uint16_t x, uint16_t y, uint8_t max_value,
                     uint8_t* value);

// Pill toggle, on the right-hand side of a settings row.
void draw_toggle(gfx::framebuffer_t* framebuffer, layout::rect_t bounds, bool on);
layout::rect_t toggle_bounds(layout::rect_t row, uint16_t padding);

// Equal-width cells, one selected.  Cells are icon-only, as in the mockup's
// text-alignment control.
void draw_segmented(gfx::framebuffer_t* framebuffer, layout::rect_t bounds,
                    const gfx::icon_t* icons, uint8_t count, uint8_t selected);
bool segmented_index_at(layout::rect_t bounds, uint8_t count, uint16_t x, uint16_t y,
                        uint8_t* index);

// Reading position / download progress.
void draw_progress(gfx::framebuffer_t* framebuffer, layout::rect_t bounds, uint32_t value,
                   uint32_t total);

// "SD Card > Books" trail.  Returns the width consumed.
uint16_t draw_breadcrumb(gfx::framebuffer_t* framebuffer, layout::rect_t bounds,
                         const char* const* parts, uint8_t count);

// Filled call-to-action, e.g. "Open Book".
void draw_button(gfx::framebuffer_t* framebuffer, layout::rect_t bounds, const char* label,
                 gfx::icon_t icon, bool focused);
bool button_contains(layout::rect_t bounds, uint16_t x, uint16_t y);

} // namespace widgets
} // namespace ui
} // namespace xreader
