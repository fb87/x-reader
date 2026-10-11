cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../platform/esp"
    "$ENV{XREADER_LUA_COMPONENT_DIR}/lua"
)
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/../sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(x_reader_firmware)

spiffs_create_partition_image(lua "${CMAKE_CURRENT_LIST_DIR}/../lua" FLASH_IN_PROJECT)
