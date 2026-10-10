cmake_minimum_required(VERSION 3.24)

project(x_reader VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Wformat=2
    -Wundef
    -Wvla
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wcast-align
    -Wwrite-strings
)

find_package(PkgConfig REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(Lua 5.4 REQUIRED)

add_library(core STATIC
    core/src/font.c
    core/src/runtime.c
    core/src/zip.c
    core/src/epub.c
)
target_include_directories(core PUBLIC core/include)
target_include_directories(core PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(core PRIVATE ${LUA_LIBRARIES})

add_executable(x-reader-sim
    simulator/main.c
    platform/sdl/platform.c
)
target_include_directories(x-reader-sim PRIVATE . core/include ${LUA_INCLUDE_DIR})
target_link_libraries(x-reader-sim PRIVATE core SDL2::SDL2 ${LUA_LIBRARIES} z)

add_executable(zip-probe simulator/zip_probe.c)
target_include_directories(zip-probe PRIVATE core/include)
target_link_libraries(zip-probe PRIVATE core z)

install(TARGETS x-reader-sim RUNTIME DESTINATION bin)
install(DIRECTORY lua/ DESTINATION share/x-reader/lua)
