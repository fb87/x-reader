#include "settings.hpp"

namespace xreader
{
namespace ui
{

namespace
{
static void build_rows(settings_row_t* rows)
{
    rows[settings_display] = {"Display",
                              gfx::icon_light_mode,
                              settings_control_link,
                              "Refresh, orientation",
                              false,
                              0,
                              0,
                              nullptr,
                              0,
                              0};
    rows[settings_reading] = {"Reading",
                              gfx::icon_text_fields,
                              settings_control_link,
                              "Font, spacing",
                              false,
                              0,
                              0,
                              nullptr,
                              0,
                              0};
    rows[settings_connectivity] = {
        "Connectivity", gfx::icon_wifi, settings_control_link, "Wi-Fi", false, 0, 0, nullptr, 0, 0};
    rows[settings_book_manager] = {"Book Manager",
                                   gfx::icon_folder,
                                   settings_control_link,
                                   "Import, files",
                                   false,
                                   0,
                                   0,
                                   nullptr,
                                   0,
                                   0};
    rows[settings_book_sync] = {"Book Sync",
                                gfx::icon_sync,
                                settings_control_link,
                                "Server, progress",
                                false,
                                0,
                                0,
                                nullptr,
                                0,
                                0};
    rows[settings_ota] = {"System Update",
                          gfx::icon_system_update,
                          settings_control_link,
                          "OTA",
                          false,
                          0,
                          0,
                          nullptr,
                          0,
                          0};
    rows[settings_storage] = {
        "Storage", gfx::icon_storage, settings_control_link, "SD card", false, 0, 0, nullptr, 0, 0};
    rows[settings_about] = {
        "About", gfx::icon_info, settings_control_link, "Device info", false, 0, 0, nullptr, 0, 0};
    rows[settings_back] = {
        "Back", gfx::icon_arrow_back, settings_control_link, nullptr, false, 0, 0, nullptr, 0, 0};
}
} // namespace

void draw_settings(gfx::framebuffer_t* framebuffer, settings_item_t focus,
                   const quick_settings_values_t* values)
{
    (void)values;
    settings_row_t rows[settings_item_count] = {};
    build_rows(rows);
    draw_settings_panel(framebuffer, "Settings", rows, settings_item_count,
                        static_cast<uint8_t>(focus));
}

bool settings_touch_item(uint16_t display_width, uint16_t display_height, settings_item_t focus,
                         uint16_t x, uint16_t y, settings_item_t* item)
{
    (void)focus;
    if (item == nullptr)
        return false;
    settings_row_t rows[settings_item_count] = {};
    build_rows(rows);
    settings_hit_t hit = {};
    if (!settings_panel_hit({display_width, display_height}, rows, settings_item_count, x, y, &hit))
        return false;
    *item = static_cast<settings_item_t>(hit.index);
    return true;
}

} // namespace ui
} // namespace xreader
