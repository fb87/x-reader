file(GLOB XR_CORE_SRCS "../src/*.c")
file(GLOB XR_APP_SRCS "../app/*.c")
file(GLOB XR_FONT_SRCS "../fonts/*.c")

idf_component_register(
    SRCS
        "../port/m5paper/app_main.cpp"
        "../port/m5paper/inflate.cpp"
        ${XR_CORE_SRCS}
        ${XR_APP_SRCS}
        ${XR_FONT_SRCS}
        "../port/m5paper/board/m5paper/m5paper_board.cpp"
        "../port/m5paper/drivers/gt911/gt911.cpp"
        "../port/m5paper/drivers/it8951e/it8951e.cpp"
    INCLUDE_DIRS
        "."
        "../include"
        "../app"
        "../fonts"
        "../port/m5paper/board/m5paper"
        "../port/m5paper/drivers/gt911"
        "../port/m5paper/drivers/it8951e"
    REQUIRES
        driver
        esp_adc
        esp_timer
        fatfs
        vfs
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wshadow)
