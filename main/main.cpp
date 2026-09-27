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
#include "ui/home.hpp"
#include "ui/library.hpp"
#include "ui/navigation.hpp"
#include "ui/reader.hpp"

namespace xreader
{
namespace app
{

static const char* const tag = "xreader";

static esp_err_t transfer_dirty(gfx::framebuffer_t* framebuffer,
                                drivers::it8951e::device_t* display)
{
    uint16_t dirty_x = 0;
    uint16_t dirty_y = 0;
    uint16_t dirty_width = 0;
    uint16_t dirty_height = 0;
    if (!gfx::take_dirty(framebuffer, &dirty_x, &dirty_y, &dirty_width, &dirty_height))
        return ESP_OK;
    const uint16_t right =
        static_cast<uint16_t>(((dirty_x + dirty_width + 3U) & ~3U) > framebuffer->width
                                  ? framebuffer->width
                                  : (dirty_x + dirty_width + 3U) & ~3U);
    dirty_x = static_cast<uint16_t>(dirty_x & ~3U);
    dirty_width = static_cast<uint16_t>(right - dirty_x);
    if ((dirty_width & 3U) != 0)
        return ESP_ERR_INVALID_SIZE;
    const size_t transfer_size = gfx::size(dirty_width, dirty_height);
    uint8_t* transfer =
        static_cast<uint8_t*>(heap_caps_malloc(transfer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (transfer == nullptr)
        return ESP_ERR_NO_MEM;
    esp_err_t error = gfx::copy_region_4bpp(framebuffer, dirty_x, dirty_y, dirty_width,
                                            dirty_height, transfer, transfer_size);
    if (error == ESP_OK)
        error = drivers::it8951e::write_image_4bpp(display, transfer, dirty_x, dirty_y, dirty_width,
                                                   dirty_height);
    if (error == ESP_OK)
        error = drivers::it8951e::refresh(display, dirty_x, dirty_y, dirty_width, dirty_height,
                                          drivers::it8951e::refresh_gc16);
    heap_caps_free(transfer);
    return error;
}

static esp_err_t show(gfx::framebuffer_t* framebuffer, drivers::it8951e::device_t* display,
                      const epub::book_t* book, const epub::document_t* document, uint8_t page,
                      uint8_t page_count)
{
    ui::draw_reader(framebuffer, book, document, page, page_count);
    return transfer_dirty(framebuffer, display);
}

static bool load_spine(const char* path, const epub::book_t* book, uint8_t spine_index,
                       epub::book_t* loaded_book, epub::document_t* document)
{
    if (storage::book_loader::start_document(path, book, spine_index) != ESP_OK)
        return false;
    esp_err_t result = ESP_ERR_INVALID_STATE;
    for (uint32_t wait_ms = 0; wait_ms < 15000; wait_ms += 50)
    {
        if (storage::book_loader::poll(loaded_book, document, &result))
            return result == ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return false;
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
        board::m5paper::power_off();
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

    gfx::framebuffer_t framebuffer = {};
    error = gfx::create(&framebuffer, display_config.width, display_config.height);
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "Unable to allocate display framebuffer: %s", esp_err_to_name(error));
        board::m5paper::power_off();
        return;
    }

    ui::home_action_t home_focus = ui::home_continue_reading;
    ui::draw_home(&framebuffer, sd_card.mounted, nullptr, home_focus);
    error = transfer_dirty(&framebuffer, &display);
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "Library display update failed: %s", esp_err_to_name(error));
        gfx::destroy(&framebuffer);
        board::m5paper::power_off();
        return;
    }
    ESP_LOGI(tag, "Home display update complete");

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
    char book_paths[8][ui::book_path_length] = {};
    const size_t book_count =
        sd_card.mounted ? ui::find_books(sd_config.mount_path, book_paths, 8) : 0;
    char book_path[ui::book_path_length] = {};
    esp_err_t load_result = ESP_ERR_NOT_FOUND;
    bool have_book = false;
    for (size_t book_index = 0; book_index < book_count && !have_book; ++book_index)
    {
        if (storage::book_loader::start(book_paths[book_index]) != ESP_OK)
            continue;
        for (uint32_t wait_ms = 0; wait_ms < 15000 && !have_book; wait_ms += 50)
        {
            if (storage::book_loader::poll(book, document, &load_result))
                have_book = load_result == ESP_OK;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (have_book)
            strcpy(book_path, book_paths[book_index]);
        else
            ESP_LOGW(tag, "book unavailable: %s: %s", book_paths[book_index],
                     esp_err_to_name(load_result));
    }
    if (!have_book)
    {
        ESP_LOGW(tag, "no valid EPUB book available: %s", esp_err_to_name(load_result));
        heap_caps_free(book);
        heap_caps_free(document);
        gfx::destroy(&framebuffer);
        board::m5paper::power_off();
        return;
    }
    ui::draw_home(&framebuffer, sd_card.mounted, book->title, home_focus);
    error = transfer_dirty(&framebuffer, &display);
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "Home display refresh failed: %s", esp_err_to_name(error));
        heap_caps_free(book);
        heap_caps_free(document);
        gfx::destroy(&framebuffer);
        board::m5paper::power_off();
        return;
    }
    bool open_reader = events == nullptr || touch.device == nullptr;
    while (!open_reader)
    {
        input::event_t event = {};
        if (xQueueReceive(events, &event, pdMS_TO_TICKS(600000)) != pdTRUE)
        {
            board::m5paper::enter_deep_sleep(1000ULL * 60ULL * 60ULL);
            continue;
        }
        bool redraw = false;
        if (event.type == input::event_rotary_clockwise)
        {
            home_focus = static_cast<ui::home_action_t>((static_cast<uint8_t>(home_focus) + 1) %
                                                        ui::home_action_count);
            redraw = true;
        }
        else if (event.type == input::event_rotary_counterclockwise)
        {
            home_focus = static_cast<ui::home_action_t>(
                (static_cast<uint8_t>(home_focus) + ui::home_action_count - 1) %
                ui::home_action_count);
            redraw = true;
        }
        else if (event.type == input::event_touch_up)
        {
            ui::home_action_t touched = home_focus;
            if (ui::home_touch_action(event.y, &touched))
            {
                home_focus = touched;
                redraw = true;
                if (touched == ui::home_continue_reading || touched == ui::home_library ||
                    touched == ui::home_recent_books)
                    open_reader = true;
                else if (touched == ui::home_sleep)
                    board::m5paper::enter_deep_sleep(1000ULL * 60ULL * 60ULL);
            }
        }
        else if (event.type == input::event_button_up)
        {
            if (home_focus == ui::home_continue_reading || home_focus == ui::home_library ||
                home_focus == ui::home_recent_books)
                open_reader = true;
            else if (home_focus == ui::home_sleep)
                board::m5paper::enter_deep_sleep(1000ULL * 60ULL * 60ULL);
        }
        if (redraw && !open_reader)
        {
            ui::draw_home(&framebuffer, sd_card.mounted, book->title, home_focus);
            transfer_dirty(&framebuffer, &display);
        }
    }
    uint8_t spine_index = 0;
    uint8_t total_pages = ui::page_count(document);
    uint32_t saved_spine = 0;
    uint32_t saved_page = 0;
    storage::persistence::load_position_for_book(book_path, &saved_spine, &saved_page);
    if (saved_spine < book->spine_count)
        spine_index = static_cast<uint8_t>(saved_spine);
    if (spine_index != 0 && !load_spine(book_path, book, spine_index, book, document))
    {
        spine_index = 0;
    }
    total_pages = ui::page_count(document);
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
        bool navigation_event = false;
        ui::navigation_input_t navigation_input = ui::navigation_touch_up;
        if (event.type == input::event_rotary_clockwise)
        {
            navigation_input = ui::navigation_rotary_clockwise;
            navigation_event = true;
        }
        else if (event.type == input::event_rotary_counterclockwise)
        {
            navigation_input = ui::navigation_rotary_counterclockwise;
            navigation_event = true;
        }
        else if (event.type == input::event_touch_up)
        {
            navigation_event = true;
        }
        const ui::navigation_result_t result =
            navigation_event
                ? ui::navigation_result(navigation_input, event.x, display_config.width, page,
                                        total_pages, spine_index, book->spine_count)
                : ui::navigation_none;
        if (result != ui::navigation_none)
        {
            if (result == ui::navigation_chapter_forward)
            {
                if (!load_spine(book_path, book, static_cast<uint8_t>(spine_index + 1), book,
                                document))
                    continue;
                ++spine_index;
                page = 0;
                total_pages = ui::page_count(document);
            }
            else if (result == ui::navigation_chapter_backward)
            {
                if (!load_spine(book_path, book, static_cast<uint8_t>(spine_index - 1), book,
                                document))
                    continue;
                --spine_index;
                total_pages = ui::page_count(document);
                page = static_cast<uint8_t>(total_pages - 1);
            }
            else
            {
                page = static_cast<uint8_t>(static_cast<int16_t>(page) +
                                            (result == ui::navigation_page_forward
                                                 ? static_cast<int16_t>(1)
                                                 : static_cast<int16_t>(-1)));
            }
            storage::persistence::save_position_for_book(book_path, spine_index, page);
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
