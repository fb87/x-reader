cmake_minimum_required(VERSION 3.16)
project(xreader_host_tests C CXX)

add_executable(xreader_epub_tests
    test_epub.cpp
    ../main/epub/inflate.cpp
    ../main/epub/image.cpp
    ../main/epub/book.cpp
    ../main/epub/chapter_stream.cpp
    ../main/epub/css.cpp
    ../main/epub/xml.cpp
    ../main/epub/zip.cpp
    ../main/gfx/font.cpp
    ../main/gfx/framebuffer.cpp
    ../main/gfx/m5epd_font16.c
    ../main/gfx/unicode_font.cpp
    ../main/gfx/reader_font.cpp
    ../main/gfx/icon_font.cpp
    ../main/ui/layout/layout.cpp
    ../main/ui/navigation/focus.cpp
    ../main/ui/navigation.cpp
    ../main/ui/home.cpp
    ../main/ui/library.cpp
    ../main/ui/library_details.cpp
    ../main/ui/settings.cpp
    ../main/ui/settings_panel.cpp
    ../main/ui/display_settings.cpp
    ../main/ui/reading_settings.cpp
    ../main/ui/widgets.cpp
    ../main/ui/quick_settings.cpp
    ../main/ui/contents.cpp
    ../main/ui/bookmarks.cpp
    ../main/ui/book_info.cpp
    ../main/ui/connectivity.cpp
    ../main/ui/wifi_networks.cpp
    ../main/ui/keyboard.cpp
    ../main/ui/ota.cpp
    ../main/ui/book_manager.cpp
    ../main/ui/file_browser.cpp
    ../main/ui/book_actions.cpp
    ../main/ui/book_sync.cpp
    ../main/ui/screen.cpp
    ../main/ui/chrome.cpp
    ../main/ui/dialog.cpp
    ../main/ui/storage_screen.cpp
    ../main/ui/about.cpp
    ../main/ui/reader.cpp
    ../main/services/library_index.cpp
    ../main/services/book_manager.cpp
    ../main/services/file_browser.cpp
)
target_include_directories(xreader_epub_tests PRIVATE . ../main)
target_compile_features(xreader_epub_tests PRIVATE cxx_std_17)
target_compile_options(xreader_epub_tests PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow)
enable_testing()
add_test(NAME xreader_epub_tests COMMAND xreader_epub_tests)
