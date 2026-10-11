idf_component_register(
    SRCS
        "main.c"
        "platform.c"
        "app_platform.c"
        "buses.c"
        "input_gt911.c"
        "display_it8951.c"
        "storage.c"
        "../../core/src/runtime.c"
        "../../core/src/font.c"
        "../../core/src/zip.c"
        "../../core/src/epub.c"
    INCLUDE_DIRS "." "../../core/include"
    REQUIRES driver esp_timer lua spiffs fatfs esp_rom
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE LUA_32BITS=1)
