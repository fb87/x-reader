#include "display_settings.hpp"

#include <stdio.h>

namespace xreader
{
namespace ui
{

namespace
{
static void build_rows(const quick_settings_values_t* values, settings_row_t* rows,
                       char* sleep_text, size_t sleep_capacity)
{
    snprintf(sleep_text, sleep_capacity, "%u min",
             static_cast<unsigned>(values->sleep_timeout_minutes));

    rows[display_setting_refresh_mode] = {"Refresh mode",
                                          gfx::icon_light_mode,
                                          settings_control_value,
                                          values->refresh_mode != 0 ? "Fast (DU)" : "Quality (GC16)",
                                          false, 0, 0, nullptr, 0, 0};
    rows[display_setting_orientation] = {"Orientation",
                                         gfx::icon_swap_horiz,
                                         settings_control_value,
                                         values->orientation != 0 ? "Portrait" : "Landscape",
                                         false, 0, 0, nullptr, 0, 0};
    rows[display_setting_invert] = {"Invert colours",
                                    gfx::icon_light_mode,
                                    settings_control_toggle,
                                    nullptr,
                                    values->invert_colors != 0,
                                    0, 0, nullptr, 0, 0};
    rows[display_setting_show_clock] = {"Show clock",
                                        gfx::icon_schedule,
                                        settings_control_toggle,
                                        nullptr,
                                        values->show_clock != 0,
                                        0, 0, nullptr, 0, 0};
    rows[display_setting_sleep_timeout] = {"Sleep timeout",
                                           gfx::icon_bedtime,
                                           settings_control_value,
                                           sleep_text,
                                           false, 0, 0, nullptr, 0, 0};
    rows[display_setting_back] = {"Back",        gfx::icon_arrow_back, settings_control_link,
                                  nullptr,       false,                0,
                                  0,             nullptr,              0,
                                  0};
}

static const quick_settings_values_t fallback = {2, 0, 1, 0, 0, 0, 0, 1, 0, 1, 60};
} // namespace

void draw_display_settings(gfx::framebuffer_t* framebuffer, display_setting_t focus,
                           const quick_settings_values_t* values)
{
    settings_row_t rows[display_setting_count] = {};
    char sleep_text[24] = {};
    build_rows(values == nullptr ? &fallback : values, rows, sleep_text, sizeof(sleep_text));
    draw_settings_panel(framebuffer, "Display Settings", rows, display_setting_count,
                        static_cast<uint8_t>(focus));
}

bool display_settings_hit(layout::viewport_t viewport, uint16_t x, uint16_t y, settings_hit_t* hit)
{
    settings_row_t rows[display_setting_count] = {};
    char sleep_text[24] = {};
    build_rows(&fallback, rows, sleep_text, sizeof(sleep_text));
    return settings_panel_hit(viewport, rows, display_setting_count, x, y, hit);
}

} // namespace ui
} // namespace xreader
