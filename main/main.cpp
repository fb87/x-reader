#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "board/m5paper/m5paper_board.hpp"
#include "board/m5paper/m5paper_pins.hpp"
#include "drivers/gt911/gt911.hpp"
#include "drivers/it8951e/it8951e.hpp"
#include "epub/book.hpp"
#include "gfx/framebuffer.hpp"
#include "input/input.hpp"
#include "storage/book_loader.hpp"
#include "storage/persistence.hpp"
#include "storage/sdcard/sdcard.hpp"
#include "ui/hardware_test.hpp"
#include "ui/library.hpp"
#include "ui/reader.hpp"

namespace xreader
{
namespace app
{

static const char* const tag = "xreader";

static esp_err_t show(gfx::framebuffer_t* framebuffer, drivers::it8951e::device_t* display,
                      const epub::book_t* book, const epub::document_t* document, uint8_t page,
                      uint8_t page_count)
{
    ui::draw_reader(framebuffer, book, document, page, page_count);
    esp_err_t error = drivers::it8951e::write_image_4bpp(display, framebuffer->pixels, 0, 0,
                                                         framebuffer->width, framebuffer->height);
    uint16_t dirty_x = 0;
    uint16_t dirty_y = 0;
    uint16_t dirty_width = 0;
    uint16_t dirty_height = 0;
    if (error == ESP_OK &&
        gfx::take_dirty(framebuffer, &dirty_x, &dirty_y, &dirty_width, &dirty_height))
        error = drivers::it8951e::refresh(display, dirty_x, dirty_y, dirty_width, dirty_height,
                                          drivers::it8951e::refresh_gc16);
    return error;
}

static void run()
{
    esp_err_t error = storage::persistence::init();
    if (error != ESP_OK)
        ESP_LOGW(tag, "NVS initialization failed: %s", esp_err_to_name(error));
    error = board::m5paper::power_on();
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "M5Paper power initialization failed: %s", esp_err_to_name(error));
        return;
    }

    const drivers::it8951e::config_t display_config = {
        .spi_host = board::m5paper::epd_spi_host,
        .sck_pin = board::m5paper::epd_sck_pin,
        .mosi_pin = board::m5paper::epd_mosi_pin,
        .miso_pin = board::m5paper::epd_miso_pin,
        .cs_pin = board::m5paper::epd_cs_pin,
        .busy_pin = board::m5paper::epd_busy_pin,
        .width = board::m5paper::display_width,
        .height = board::m5paper::display_height,
        .rotation = board::m5paper::display_rotation,
        .spi_frequency_hz = 10000000,
    };
    drivers::it8951e::device_t display = {};
    error = drivers::it8951e::init(&display, &display_config);
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "Display initialization failed: %s", esp_err_to_name(error));
        return;
    }

    const drivers::gt911::config_t touch_config = {
        .port = I2C_NUM_0,
        .sda_pin = board::m5paper::touch_sda_pin,
        .scl_pin = board::m5paper::touch_scl_pin,
        .frequency_hz = 100000,
    };
    static drivers::gt911::device_t touch = {};
    error = drivers::gt911::init(&touch, &touch_config);
    if (error != ESP_OK)
    {
        ESP_LOGW(tag, "Touch initialization failed: %s", esp_err_to_name(error));
    }

    const storage::sdcard::config_t sd_config = {
        .spi_host = board::m5paper::epd_spi_host,
        .cs_pin = board::m5paper::sd_cs_pin,
        .mount_path = "/sdcard",
        .max_files = 4,
    };
    storage::sdcard::device_t sd_card = {};
    error = storage::sdcard::mount(&sd_card, &sd_config);
    if (error != ESP_OK)
    {
        ESP_LOGW(tag, "SD-card initialization failed: %s", esp_err_to_name(error));
    }

    char first_book_path[256] = {};
    if (sd_card.mounted &&
        ui::find_first_book(sd_config.mount_path, first_book_path, sizeof(first_book_path)))
    {
        storage::book_loader::start(first_book_path);
    }

    gfx::framebuffer_t framebuffer = {};
    error = gfx::create(&framebuffer, display_config.width, display_config.height);
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "Unable to allocate display framebuffer: %s", esp_err_to_name(error));
        return;
    }

    ui::draw_library(&framebuffer, sd_card.mounted, sd_config.mount_path);
    error = drivers::it8951e::write_image_4bpp(&display, framebuffer.pixels, 0, 0,
                                               display_config.width, display_config.height);
    if (error == ESP_OK)
        error = drivers::it8951e::refresh(&display, 0, 0, display_config.width,
                                          display_config.height, drivers::it8951e::refresh_gc16);
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "Library display update failed: %s", esp_err_to_name(error));
        return;
    }
    ESP_LOGI(tag, "Library display update complete");

    QueueHandle_t events = xQueueCreate(8, sizeof(input::event_t));
    if (events != nullptr && touch.device != nullptr)
    {
        const input::config_t input_config = {
            .rotary_right_pin = board::m5paper::rotary_right_pin,
            .rotary_press_pin = board::m5paper::rotary_press_pin,
            .rotary_left_pin = board::m5paper::rotary_left_pin,
            .touch = &touch,
            .poll_interval_ms = 30,
        };
        input::start(&input_config, events);
    }

    epub::book_t* book = static_cast<epub::book_t*>(
        heap_caps_calloc(1, sizeof(epub::book_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    epub::document_t* document = static_cast<epub::document_t*>(
        heap_caps_calloc(1, sizeof(epub::document_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (book == nullptr || document == nullptr)
    {
        heap_caps_free(book);
        heap_caps_free(document);
        return;
    }
    esp_err_t load_result = ESP_ERR_INVALID_STATE;
    bool have_book = false;
    for (uint32_t wait_ms = 0; wait_ms < 15000 && !have_book; wait_ms += 50)
    {
        if (storage::book_loader::poll(book, document, &load_result))
            have_book = load_result == ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (!have_book)
    {
        ESP_LOGW(tag, "first book metadata unavailable: %s", esp_err_to_name(load_result));
        heap_caps_free(book);
        heap_caps_free(document);
        gfx::destroy(&framebuffer);
        return;
    }
    const uint8_t total_pages = ui::page_count(document);
    uint32_t saved_page = 0;
    storage::persistence::load_page_for_book(first_book_path, &saved_page);
    uint8_t page = saved_page < total_pages ? static_cast<uint8_t>(saved_page) : 0;
    show(&framebuffer, &display, book, document, page, total_pages);
    while (events != nullptr && touch.device != nullptr)
    {
        input::event_t event = {};
        if (xQueueReceive(events, &event, pdMS_TO_TICKS(600000)) != pdTRUE)
        {
            board::m5paper::enter_deep_sleep(1000ULL * 60ULL * 60ULL);
            continue;
        }
        bool changed = false;
        if (event.type == input::event_rotary_clockwise ||
            (event.type == input::event_touch_up && event.x > display_config.width / 2))
        {
            if (page + 1 < total_pages)
            {
                ++page;
                changed = true;
            }
        }
        else if (event.type == input::event_rotary_counterclockwise ||
                 (event.type == input::event_touch_up && event.x <= display_config.width / 2))
        {
            if (page > 0)
            {
                --page;
                changed = true;
            }
        }
        if (changed)
        {
            storage::persistence::save_page_for_book(first_book_path, page);
            show(&framebuffer, &display, book, document, page, total_pages);
        }
    }
}

} // namespace app
} // namespace xreader

extern "C" void app_main(void)
{
    xreader::app::run();
}
