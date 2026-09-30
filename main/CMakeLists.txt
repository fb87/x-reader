idf_component_register(
    SRCS
        "main.cpp"
        "board/m5paper/m5paper_board.cpp"
        "board/xteink/xteink_board.cpp"
        "board/xteink/xteink_buttons.cpp"
        "drivers/gt911/gt911.cpp"
        "drivers/it8951e/it8951e.cpp"
        "epub/zip.cpp"
        "epub/inflate.cpp"
        "epub/book.cpp"
        "epub/css.cpp"
        "epub/xml.cpp"
        "epub/image.cpp"
        "gfx/framebuffer.cpp"
        "gfx/font.cpp"
        "gfx/unicode_font.cpp"
        "gfx/reader_font.cpp"
        "gfx/icon_font.cpp"
        "gfx/m5epd_font16.c"
        "input/input.cpp"
        "input/mapper.cpp"
        "storage/sdcard/sdcard.cpp"
        "storage/persistence.cpp"
        "storage/book_loader.cpp"
        "services/connectivity.cpp"
        "services/ota.cpp"
        "services/book_manager.cpp"
        "services/file_browser.cpp"
        "services/library_index.cpp"
        "services/library_scan.cpp"
        "services/book_sync.cpp"
        "services/debug_console.cpp"
        "ui/layout/layout.cpp"
        "ui/widgets.cpp"
        "ui/navigation/focus.cpp"
        "ui/library.cpp"
        "ui/library_details.cpp"
        "ui/navigation.cpp"
        "ui/chrome.cpp"
        "ui/home.cpp"
        "ui/settings.cpp"
        "ui/settings_panel.cpp"
        "ui/display_settings.cpp"
        "ui/reading_settings.cpp"
        "ui/quick_settings.cpp"
        "ui/book_info.cpp"
        "ui/contents.cpp"
        "ui/bookmarks.cpp"
        "ui/connectivity.cpp"
        "ui/wifi_networks.cpp"
        "ui/keyboard.cpp"
        "ui/ota.cpp"
        "ui/book_manager.cpp"
        "ui/file_browser.cpp"
        "ui/book_actions.cpp"
        "ui/book_sync.cpp"
        "ui/screen.cpp"
        "ui/reader.cpp"
        "ui/hardware_test.cpp"
        "ui/dialog.cpp"
        "ui/storage_screen.cpp"
        "ui/about.cpp"
        "ui/splash.cpp"
    INCLUDE_DIRS
        "."
        "board/m5paper"
        "board/xteink"
        "drivers/gt911"
        "drivers/it8951e"
        "epub"
        "gfx"
        "input"
        "storage/sdcard"
        "services"
        "ui"
    REQUIRES
        driver
        esp_adc
        esp_timer
        fatfs
        vfs
        esp_wifi
        esp_event
        esp_netif
        esp_http_client
        esp_https_ota
        app_update
        nvs_flash
        json
        mbedtls
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wconversion
    -Wshadow
    -Wno-error=missing-field-initializers
)

if(XREADER_SIMULATE_NAVIGATION)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE XREADER_SIMULATE_NAVIGATION=1)
endif()

if(XREADER_DIAGNOSTICS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE XREADER_DIAGNOSTICS=1)
endif()

# Serial test-harness console.  Injects synthetic input events so a host can
# drive the UI without touching the panel.  Never enable for a release image.
if(XREADER_DEBUG_CONSOLE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE XREADER_DEBUG_CONSOLE=1)
endif()
