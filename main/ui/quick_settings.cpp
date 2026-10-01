#include "quick_settings.hpp"

#include "ui/settings_panel.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static void build_rows(settings_row_t* rows)
{
    rows[quick_setting_contents] = {"Table of Contents",
                                    gfx::icon_toc,
                                    settings_control_link,
                                    nullptr,
                                    false,
                                    0,
                                    0,
                                    nullptr,
                                    0,
                                    0};
    rows[quick_setting_bookmarks] = {"Bookmarks",
                                     gfx::icon_bookmark,
                                     settings_control_link,
                                     nullptr,
                                     false,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     0};
    rows[quick_setting_add_bookmark] = {
        "Add bookmark", gfx::icon_add, settings_control_link, nullptr, false, 0, 0, nullptr, 0, 0};
    rows[quick_setting_display_settings] = {"Display Settings",
                                            gfx::icon_light_mode,
                                            settings_control_link,
                                            nullptr,
                                            false,
                                            0,
                                            0,
                                            nullptr,
                                            0,
                                            0};
    rows[quick_setting_reading_settings] = {"Reading Settings",
                                            gfx::icon_text_fields,
                                            settings_control_link,
                                            nullptr,
                                            false,
                                            0,
                                            0,
                                            nullptr,
                                            0,
                                            0};
    rows[quick_setting_search] = {"Search in book",
                                  gfx::icon_search,
                                  settings_control_link,
                                  nullptr,
                                  false,
                                  0,
                                  0,
                                  nullptr,
                                  0,
                                  0};
    rows[quick_setting_book_info] = {"Book Information",
                                     gfx::icon_info,
                                     settings_control_link,
                                     nullptr,
                                     false,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     0};
    rows[quick_setting_exit_to_library] = {"Exit to Library",
                                           gfx::icon_open_in_new,
                                           settings_control_link,
                                           nullptr,
                                           false,
                                           0,
                                           0,
                                           nullptr,
                                           0,
                                           0};
}
} // namespace

void draw_quick_settings(gfx::framebuffer_t* framebuffer, quick_setting_t focus,
                         const quick_settings_values_t* values, int8_t footer_focus)
{
    (void)values;
    settings_row_t rows[quick_setting_count] = {};
    build_rows(rows);
    draw_settings_panel(framebuffer, "Reader Menu", rows, quick_setting_count,
                        static_cast<uint8_t>(focus), footer_focus);
}

bool quick_settings_touch(uint16_t display_width, uint16_t display_height, quick_setting_t focus,
                          uint16_t x, uint16_t y, quick_setting_t* item)
{
    (void)focus;
    if (item == nullptr)
        return false;
    settings_row_t rows[quick_setting_count] = {};
    build_rows(rows);
    settings_hit_t hit = {};
    if (!settings_panel_hit({display_width, display_height}, rows, quick_setting_count, x, y, &hit))
        return false;
    *item = static_cast<quick_setting_t>(hit.index);
    return true;
}

bool quick_settings_contains(uint16_t display_width, uint16_t display_height, uint16_t x,
                             uint16_t y)
{
    // The menu is now a full screen, so every point inside the viewport belongs
    // to it; there is no "tap outside to dismiss" region any more.
    (void)x;
    (void)y;
    (void)display_width;
    (void)display_height;
    return true;
}

} // namespace ui
} // namespace xreader
