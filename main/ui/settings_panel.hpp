#pragma once

#include <stdint.h>

#include "gfx/font.hpp"
#include "gfx/framebuffer.hpp"
#include "ui/layout/layout.hpp"

namespace xreader
{
namespace ui
{

// A settings screen is a list of labelled rows, each carrying one control.  The
// row table is built by the screen and rendered generically, so Display Settings
// and Reading Settings share both their drawing and their hit-testing and cannot
// drift apart.

enum settings_control_t : uint8_t
{
    settings_control_value,     // right-aligned text, e.g. "Portrait"
    settings_control_toggle,    // pill switch
    settings_control_slider,    // level within max_level
    settings_control_segmented, // icon cells, one selected
    settings_control_link,      // opens another screen; shows a chevron
};

struct settings_row_t
{
    const char* label;
    gfx::icon_t icon;
    settings_control_t control;
    const char* value;
    bool on;
    uint8_t level;
    uint8_t max_level;
    const gfx::icon_t* segments;
    uint8_t segment_count;
    uint8_t segment_selected;
};

void draw_settings_panel(gfx::framebuffer_t* framebuffer, const char* title,
                         const settings_row_t* rows, uint8_t count, uint8_t focus,
                         int8_t footer_focus);

// Resolves a touch to a row.  For sliders and segmented controls it also reports
// the value the touch selected, so the caller can set rather than cycle.
struct settings_hit_t
{
    uint8_t index;
    bool has_value;
    uint8_t value;
};

bool settings_panel_hit(layout::viewport_t viewport, const settings_row_t* rows, uint8_t count,
                        uint16_t x, uint16_t y, settings_hit_t* hit);

} // namespace ui
} // namespace xreader
