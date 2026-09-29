#pragma once

#include <stdint.h>

namespace xreader
{
namespace ui
{
namespace layout
{

enum display_class_t : uint8_t
{
    display_compact,
    display_medium,
    display_large,
};

struct viewport_t
{
    uint16_t width;
    uint16_t height;
};

struct rect_t
{
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
};

enum footer_zone_t : uint8_t
{
    footer_none,
    footer_left,
    footer_center,
    footer_right,
};

struct metrics_t
{
    display_class_t display_class;
    uint16_t margin;
    uint16_t gap;
    uint16_t status_height;
    uint16_t footer_height;
    uint16_t row_height;
    uint16_t panel_padding;
};

viewport_t viewport(uint16_t width, uint16_t height);
display_class_t classify(viewport_t viewport);
metrics_t metrics(viewport_t viewport);
bool contains(rect_t rect, uint16_t x, uint16_t y);
rect_t inset(rect_t rect, uint16_t amount);
rect_t content(viewport_t viewport);
rect_t row(rect_t area, uint8_t index, uint8_t count, uint16_t preferred_height,
           uint16_t preferred_gap);
// Top-aligned row of fixed height.  `row()` centres a short list inside a tall
// area, which leaves a list stranded in the middle of a portrait screen; list
// screens want their rows to start at the top instead.
rect_t stacked_row(rect_t area, uint8_t index, uint8_t count, uint16_t height, uint16_t gap);
rect_t centered_panel(viewport_t viewport, uint8_t width_percent, uint8_t height_percent);
footer_zone_t footer_hit(viewport_t viewport, uint16_t x, uint16_t y);

} // namespace layout
} // namespace ui
} // namespace xreader
