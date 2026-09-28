#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "epub/book.hpp"
#include "epub/image.hpp"
#include "epub/inflate.hpp"
#include "epub/xml.hpp"
#include "epub/zip.hpp"
#include "gfx/font.hpp"
#include "gfx/framebuffer.hpp"
#include "gfx/unicode_font.hpp"
#include "ui/navigation.hpp"
#include "ui/screen.hpp"

static void append_u16(uint8_t* data, size_t* size, uint16_t value)
{
    data[(*size)++] = static_cast<uint8_t>(value);
    data[(*size)++] = static_cast<uint8_t>(value >> 8);
}

static void append_u32(uint8_t* data, size_t* size, uint32_t value)
{
    append_u16(data, size, static_cast<uint16_t>(value));
    append_u16(data, size, static_cast<uint16_t>(value >> 16));
}

struct zip_fixture_entry_t
{
    const char* name;
    const char* data;
};

static void append_bytes(uint8_t* data, size_t* size, const void* source, size_t length)
{
    memcpy(data + *size, source, length);
    *size += length;
}

static void write_stored_zip(const char* path, const zip_fixture_entry_t* entries, size_t count)
{
    uint8_t data[8192] = {};
    uint32_t offsets[8] = {};
    uint32_t sizes[8] = {};
    size_t size = 0;
    for (size_t index = 0; index < count; ++index)
    {
        const uint16_t name_length = static_cast<uint16_t>(strlen(entries[index].name));
        const uint32_t content_size = static_cast<uint32_t>(strlen(entries[index].data));
        offsets[index] = static_cast<uint32_t>(size);
        sizes[index] = content_size;
        append_u32(data, &size, 0x04034b50);
        append_u16(data, &size, 20);
        append_u16(data, &size, 0);
        append_u16(data, &size, 0);
        append_u16(data, &size, 0);
        append_u16(data, &size, 0);
        append_u32(data, &size, 0);
        append_u32(data, &size, content_size);
        append_u32(data, &size, content_size);
        append_u16(data, &size, name_length);
        append_u16(data, &size, 0);
        append_bytes(data, &size, entries[index].name, name_length);
        append_bytes(data, &size, entries[index].data, content_size);
    }
    const uint32_t central_offset = static_cast<uint32_t>(size);
    for (size_t index = 0; index < count; ++index)
    {
        const uint16_t name_length = static_cast<uint16_t>(strlen(entries[index].name));
        append_u32(data, &size, 0x02014b50);
        append_u16(data, &size, 20);
        append_u16(data, &size, 20);
        append_u16(data, &size, 0);
        append_u16(data, &size, 0);
        append_u16(data, &size, 0);
        append_u16(data, &size, 0);
        append_u32(data, &size, 0);
        append_u32(data, &size, sizes[index]);
        append_u32(data, &size, sizes[index]);
        append_u16(data, &size, name_length);
        append_u16(data, &size, 0);
        append_u16(data, &size, 0);
        append_u16(data, &size, 0);
        append_u16(data, &size, 0);
        append_u32(data, &size, 0);
        append_u32(data, &size, offsets[index]);
        append_bytes(data, &size, entries[index].name, name_length);
    }
    const uint32_t central_size = static_cast<uint32_t>(size) - central_offset;
    append_u32(data, &size, 0x06054b50);
    append_u16(data, &size, 0);
    append_u16(data, &size, 0);
    append_u16(data, &size, static_cast<uint16_t>(count));
    append_u16(data, &size, static_cast<uint16_t>(count));
    append_u32(data, &size, central_size);
    append_u32(data, &size, central_offset);
    append_u16(data, &size, 0);
    FILE* file = fopen(path, "wb");
    assert(file != nullptr);
    assert(fwrite(data, 1, size, file) == size);
    fclose(file);
}

int main()
{
    const uint32_t required_glyph_ranges[][2] = {
        {0x0020U, 0x007eU}, {0x00a0U, 0x00ffU}, {0x0100U, 0x017fU},
        {0x0180U, 0x024fU}, {0x0300U, 0x036fU}, {0x1e00U, 0x1effU},
    };
    for (const auto& range : required_glyph_ranges)
    {
        for (uint32_t codepoint = range[0]; codepoint <= range[1]; ++codepoint)
        {
            bool found = false;
            for (size_t index = 0; index < unicode_glyph_count; ++index)
                found = found || unicode_glyphs[index].codepoint == codepoint;
            assert(found);
        }
    }
    uint32_t composed = 0;
    size_t consumed_codepoints = 0;
    assert(xreader::gfx::compose_unicode('o', 0x031bU, 'n', &composed, &consumed_codepoints));
    assert(composed == 0x01a1U && consumed_codepoints == 2);
    assert(xreader::gfx::compose_unicode('a', 0x0306U, 0x0300U, &composed, &consumed_codepoints));
    assert(composed == 0x1eb1U && consumed_codepoints == 3);

    const uint8_t png[] = {
        0x89, 'P',  'N',  'G',  0x0d, 0x0a, 0x1a, 0x0a, 0,    0,    0,    13,   'I',  'H',
        'D',  'R',  0,    0,    0,    2,    0,    0,    0,    1,    8,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,    14,   'I',  'D',  'A',  'T',  0x78,
        0x01, 0x01, 0x03, 0x00, 0xfc, 0xff, 0x00, 0x00, 0xff, 0x00, 0x01, 0x01, 0x00, 0,
        0,    0,    0,    'I',  'E',  'N',  'D',  0,    0,    0,    0,
    };
    xreader::epub::image::info_t image_info = {};
    assert(xreader::epub::image::inspect(png, sizeof(png), &image_info) == ESP_OK);
    assert(image_info.width == 2 && image_info.height == 1 && image_info.supported);
    uint8_t image_pixels[1] = {};
    assert(xreader::epub::image::decode_mono(png, sizeof(png), image_pixels,
                                             sizeof(image_pixels)) == ESP_OK);
    assert(image_pixels[0] == 0xf0);

    xreader::gfx::framebuffer_t framebuffer = {};
    assert(xreader::gfx::create(&framebuffer, 8, 2) == ESP_OK);
    for (uint16_t x = 0; x < 8; ++x)
        xreader::gfx::set_pixel(&framebuffer, x, 0, static_cast<uint8_t>(x));
    uint8_t region[4] = {};
    assert(xreader::gfx::copy_region_4bpp(&framebuffer, 2, 0, 4, 1, region, sizeof(region)) ==
           ESP_OK);
    assert(region[0] == 0x23 && region[1] == 0x45);
    xreader::gfx::destroy(&framebuffer);

    xreader::gfx::framebuffer_t double_buffer = {};
    assert(xreader::gfx::create(&double_buffer, 8, 2) == ESP_OK);
    xreader::gfx::clear(&double_buffer, 0x0f);
    assert(xreader::gfx::present(&double_buffer) == ESP_OK);
    uint16_t dirty_x = 0;
    uint16_t dirty_y = 0;
    uint16_t dirty_width = 0;
    uint16_t dirty_height = 0;
    assert(
        xreader::gfx::take_dirty(&double_buffer, &dirty_x, &dirty_y, &dirty_width, &dirty_height));
    assert(
        !xreader::gfx::take_dirty(&double_buffer, &dirty_x, &dirty_y, &dirty_width, &dirty_height));
    xreader::gfx::set_pixel(&double_buffer, 3, 1, 0x00);
    assert(xreader::gfx::present(&double_buffer) == ESP_OK);
    assert(
        xreader::gfx::take_dirty(&double_buffer, &dirty_x, &dirty_y, &dirty_width, &dirty_height));
    assert(dirty_x == 2 && dirty_y == 1 && dirty_width == 2 && dirty_height == 1);
    xreader::gfx::destroy(&double_buffer);

    uint8_t zip_data[128] = {};
    size_t zip_size = 0;
    append_u32(zip_data, &zip_size, 0x04034b50);
    append_u16(zip_data, &zip_size, 20);
    append_u16(zip_data, &zip_size, 0);
    append_u16(zip_data, &zip_size, 0);
    append_u16(zip_data, &zip_size, 0);
    append_u16(zip_data, &zip_size, 0);
    append_u32(zip_data, &zip_size, 0);
    append_u32(zip_data, &zip_size, 5);
    append_u32(zip_data, &zip_size, 5);
    append_u16(zip_data, &zip_size, 5);
    append_u16(zip_data, &zip_size, 0);
    memcpy(zip_data + zip_size, "hello", 5);
    zip_size += 5;
    memcpy(zip_data + zip_size, "hello", 5);
    zip_size += 5;
    const uint32_t central_offset = static_cast<uint32_t>(zip_size);
    append_u32(zip_data, &zip_size, 0x02014b50);
    append_u16(zip_data, &zip_size, 20);
    append_u16(zip_data, &zip_size, 20);
    append_u16(zip_data, &zip_size, 0);
    append_u16(zip_data, &zip_size, 0);
    append_u16(zip_data, &zip_size, 0);
    append_u16(zip_data, &zip_size, 0);
    append_u32(zip_data, &zip_size, 0);
    append_u32(zip_data, &zip_size, 5);
    append_u32(zip_data, &zip_size, 5);
    append_u16(zip_data, &zip_size, 5);
    append_u16(zip_data, &zip_size, 0);
    append_u16(zip_data, &zip_size, 0);
    append_u16(zip_data, &zip_size, 0);
    append_u16(zip_data, &zip_size, 0);
    append_u32(zip_data, &zip_size, 0);
    append_u32(zip_data, &zip_size, 0);
    memcpy(zip_data + zip_size, "hello", 5);
    zip_size += 5;
    const uint32_t central_size = static_cast<uint32_t>(zip_size) - central_offset;
    append_u32(zip_data, &zip_size, 0x06054b50);
    append_u16(zip_data, &zip_size, 0);
    append_u16(zip_data, &zip_size, 0);
    append_u16(zip_data, &zip_size, 1);
    append_u16(zip_data, &zip_size, 1);
    append_u32(zip_data, &zip_size, central_size);
    append_u32(zip_data, &zip_size, central_offset);
    append_u16(zip_data, &zip_size, 0);
    FILE* zip_file = fopen("/tmp/xreader-test.zip", "wb");
    assert(zip_file != nullptr);
    assert(fwrite(zip_data, 1, zip_size, zip_file) == zip_size);
    fclose(zip_file);
    xreader::epub::zip::archive_t archive = {};
    assert(xreader::epub::zip::open(&archive, "/tmp/xreader-test.zip") == ESP_OK);
    xreader::epub::zip::entry_t entry = {};
    assert(xreader::epub::zip::find(&archive, "hello", &entry) == ESP_OK);
    assert(entry.compressed_size == 5 && entry.uncompressed_size == 5 &&
           entry.local_header_offset == 0);
    uint8_t stored[8] = {};
    size_t stored_size = 0;
    assert(xreader::epub::zip::read(&archive, &entry, stored, sizeof(stored), &stored_size) ==
           ESP_OK);
    assert(stored_size == 5 && memcmp(stored, "hello", 5) == 0);
    xreader::epub::zip::close(&archive);

    const uint8_t compressed[] = {0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00};
    uint8_t output[16] = {};
    size_t output_size = 0;
    assert(xreader::epub::inflate::decode(compressed, sizeof(compressed), output, sizeof(output),
                                          &output_size) == ESP_OK);
    assert(output_size == 5);
    assert(memcmp(output, "hello", 5) == 0);

    const char xml_data[] = "<root><item id=\"one\">text</item><!-- ignored --></root>";
    xreader::epub::xml::reader_t reader = {};
    xreader::epub::xml::init(&reader, xml_data, sizeof(xml_data) - 1);
    xreader::epub::xml::token_t token = {};
    assert(xreader::epub::xml::next(&reader, &token) == ESP_OK);
    assert(xreader::epub::xml::name_is(&token, "root"));
    assert(xreader::epub::xml::next(&reader, &token) == ESP_OK);
    assert(xreader::epub::xml::name_is(&token, "item"));
    assert(xreader::epub::xml::next(&reader, &token) == ESP_OK);
    assert(token.type == xreader::epub::xml::token_text);
    assert(token.value_length == 4);

    const zip_fixture_entry_t epub_entries[] = {
        {"META-INF/container.xml", "<container><rootfiles><rootfile "
                                   "full-path=\"OEBPS/content.opf\"/></rootfiles></container>"},
        {"OEBPS/content.opf",
         "<package><metadata><dc:title>Fixture</dc:title><dc:creator>Author</dc:creator></metadata>"
         "<manifest><item id=\"one\" href=\"ch1.xhtml\" media-type=\"application/xhtml+xml\"/>"
         "<item id=\"two\" href=\"ch2.xhtml\" media-type=\"application/xhtml+xml\"/></manifest>"
         "<spine><itemref idref=\"one\"/><itemref idref=\"two\"/></spine></package>"},
        {"OEBPS/ch1.xhtml", "<html><body><p>Chapter one.</p></body></html>"},
        {"OEBPS/ch2.xhtml", "<html><body><p>Chapter two.</p></body></html>"},
    };
    const char* epub_path = "/tmp/xreader-test.epub";
    write_stored_zip(epub_path, epub_entries, sizeof(epub_entries) / sizeof(epub_entries[0]));
    xreader::epub::book_t book = {};
    xreader::epub::document_t document = {};
    assert(xreader::epub::load_metadata(epub_path, &book) == ESP_OK);
    assert(book.spine_count == 2);
    assert(strcmp(book.spine[0].href, "ch1.xhtml") == 0);
    assert(strcmp(book.spine[1].href, "ch2.xhtml") == 0);
    assert(xreader::epub::load_document(epub_path, &book, 0, &document) == ESP_OK);
    assert(strstr(document.text, "Chapter one") != nullptr);
    assert(xreader::epub::load_document(epub_path, &book, 1, &document) == ESP_OK);
    assert(strstr(document.text, "Chapter two") != nullptr);

    using xreader::ui::navigation_rotary_clockwise;
    using xreader::ui::navigation_rotary_counterclockwise;
    using xreader::ui::navigation_touch_up;
    assert(xreader::ui::page_delta(navigation_touch_up, 721, 960, 0, 3) == 1);
    assert(xreader::ui::page_delta(navigation_touch_up, 240, 960, 1, 3) == -1);
    assert(xreader::ui::page_delta(navigation_touch_up, 480, 960, 1, 3) == -1);
    assert(xreader::ui::page_delta(navigation_rotary_clockwise, 0, 960, 0, 3) == 1);
    assert(xreader::ui::page_delta(navigation_rotary_counterclockwise, 0, 960, 2, 3) == -1);
    assert(xreader::ui::page_delta(navigation_touch_up, 721, 960, 2, 3) == 0);
    assert(xreader::ui::page_delta(navigation_touch_up, 240, 960, 0, 3) == 0);
    assert(xreader::ui::navigation_result(navigation_touch_up, 721, 960, 0, 2, 0, 3) ==
           xreader::ui::navigation_page_forward);
    assert(xreader::ui::navigation_result(navigation_touch_up, 721, 960, 1, 2, 0, 3) ==
           xreader::ui::navigation_chapter_forward);
    assert(xreader::ui::navigation_result(navigation_touch_up, 240, 960, 0, 2, 1, 3) ==
           xreader::ui::navigation_chapter_backward);
    assert(xreader::ui::navigation_result(navigation_touch_up, 240, 960, 0, 2, 0, 3) ==
           xreader::ui::navigation_none);
    xreader::ui::screen_state_t screen = {};
    const xreader::ui::logical_event_t rotate_right = {xreader::ui::logical_rotary_clockwise, 0, 0};
    const xreader::ui::logical_event_t press = {xreader::ui::logical_button_up, 0, 0};
    xreader::ui::initialize(&screen);
    assert(screen.screen == xreader::ui::screen_home);
    assert(xreader::ui::dispatch(&screen, &rotate_right, 960, 540, 0, 1, 0, 2) ==
           xreader::ui::screen_command_redraw);
    assert(screen.home_focus == xreader::ui::home_library);
    assert(xreader::ui::dispatch(&screen, &press, 960, 540, 0, 1, 0, 2) ==
           xreader::ui::screen_command_show_library);
    assert(screen.screen == xreader::ui::screen_library);
    assert(xreader::ui::dispatch(&screen, &press, 960, 540, 0, 1, 0, 2) ==
           xreader::ui::screen_command_open_reader);
    assert(screen.screen == xreader::ui::screen_reader);
    assert(xreader::ui::dispatch(&screen, &press, 960, 540, 0, 1, 0, 2) ==
           xreader::ui::screen_command_open_quick_settings);
    assert(screen.screen == xreader::ui::screen_quick_settings);
    return 0;
}
