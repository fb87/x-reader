cmake_minimum_required(VERSION 3.16)
project(xreader_host_tests CXX)

add_executable(xreader_epub_tests
    test_epub.cpp
    ../main/epub/inflate.cpp
    ../main/epub/image.cpp
    ../main/epub/xml.cpp
    ../main/epub/zip.cpp
    ../main/gfx/framebuffer.cpp
)
target_include_directories(xreader_epub_tests PRIVATE . ../main)
target_compile_features(xreader_epub_tests PRIVATE cxx_std_17)
target_compile_options(xreader_epub_tests PRIVATE -Wall -Wextra -Werror)
enable_testing()
add_test(NAME xreader_epub_tests COMMAND xreader_epub_tests)
