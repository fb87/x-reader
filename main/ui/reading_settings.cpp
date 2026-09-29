#include "reading_settings.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static const gfx::icon_t alignment_icons[3] = {
    gfx::icon_format_align_left,
    gfx::icon_format_align_center,
    gfx::icon_format_align_right,
};

static void build_rows(const quick_settings_values_t* values, settings_row_t* rows)
{
    // text_scale is 1 or 2, so the slider has two stops rather than a free range.
    rows[reading_setting_font_size] = {"Font size",
                                       gfx::icon_text_fields,
                                       settings_control_slider,
                                       nullptr,
                                       false,
                                       static_cast<uint8_t>(values->text_scale > 1U ? 1U : 0U),
                                       1,
                                       nullptr, 0, 0};
    rows[reading_setting_line_spacing] = {"Line spacing",
                                          gfx::icon_format_line_spacing,
                                          settings_control_slider,
                                          nullptr,
                                          false,
                                          static_cast<uint8_t>(values->line_spacing != 0U ? 1U : 0U),
                                          1,
                                          nullptr, 0, 0};
    rows[reading_setting_margins] = {"Margins",
                                     gfx::icon_margin,
                                     settings_control_slider,
                                     nullptr,
                                     false,
                                     static_cast<uint8_t>(values->margin_mode > 2U ? 2U : values->margin_mode),
                                     2,
                                     nullptr, 0, 0};
    rows[reading_setting_paragraph_gap] = {"Paragraph gap",
                                           gfx::icon_format_line_spacing,
                                           settings_control_toggle,
                                           nullptr,
                                           values->paragraph_spacing != 0,
                                           0, 0, nullptr, 0, 0};
    rows[reading_setting_alignment] = {"Text alignment",
                                       gfx::icon_format_align_left,
                                       settings_control_segmented,
                                       nullptr,
                                       false,
                                       0, 0,
                                       alignment_icons, 3,
                                       static_cast<uint8_t>(values->text_alignment > 2U ? 0U : values->text_alignment)};
    rows[reading_setting_page_turn] = {"Page turn area",
                                       gfx::icon_swap_horiz,
                                       settings_control_value,
                                       values->reverse_page_turn != 0 ? "Right / left" : "Left / right",
                                       false, 0, 0, nullptr, 0, 0};
    rows[reading_setting_back] = {"Back", gfx::icon_arrow_back, settings_control_link,
                                  nullptr, false, 0, 0, nullptr, 0, 0};
}

static const quick_settings_values_t fallback = {2, 0, 1, 0, 0, 0, 0, 1, 0, 1, 60};
} // namespace

void draw_reading_settings(gfx::framebuffer_t* framebuffer, reading_setting_t focus,
                           const quick_settings_values_t* values)
{
    settings_row_t rows[reading_setting_count] = {};
    build_rows(values == nullptr ? &fallback : values, rows);
    draw_settings_panel(framebuffer, "Reading Settings", rows, reading_setting_count,
                        static_cast<uint8_t>(focus));
}

bool reading_settings_hit(layout::viewport_t viewport, uint16_t x, uint16_t y, settings_hit_t* hit)
{
    settings_row_t rows[reading_setting_count] = {};
    build_rows(&fallback, rows);
    return settings_panel_hit(viewport, rows, reading_setting_count, x, y, hit);
}

} // namespace ui
} // namespace xreader
