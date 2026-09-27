#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "epub/inflate.hpp"
#include "epub/image.hpp"
#include "epub/xml.hpp"
#include "epub/zip.hpp"
#include "gfx/framebuffer.hpp"
#include "ui/navigation.hpp"

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

int main()
{
    const uint8_t png[] = {
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a,
        0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 0, 2, 0, 0, 0, 1, 8, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 14, 'I', 'D', 'A', 'T', 0x78, 0x01, 0x01, 0x03, 0x00, 0xfc, 0xff, 0x00, 0x00,
        0xff, 0x00, 0x01, 0x01, 0x00, 0, 0, 0, 0, 'I', 'E', 'N', 'D', 0, 0, 0, 0,
    };
    xreader::epub::image::info_t image_info = {};
    assert(xreader::epub::image::inspect(png, sizeof(png), &image_info) == ESP_OK);
    assert(image_info.width == 2 && image_info.height == 1 && image_info.supported);
    uint8_t image_pixels[1] = {};
    assert(xreader::epub::image::decode_mono(png, sizeof(png), image_pixels, sizeof(image_pixels)) == ESP_OK);
    assert(image_pixels[0] == 0xf0);

    xreader::gfx::framebuffer_t framebuffer = {};
    assert(xreader::gfx::create(&framebuffer, 8, 2) == ESP_OK);
    for (uint16_t x = 0; x < 8; ++x)
        xreader::gfx::set_pixel(&framebuffer, x, 0, static_cast<uint8_t>(x));
    uint8_t region[4] = {};
    assert(xreader::gfx::copy_region_4bpp(&framebuffer, 2, 0, 4, 1, region, sizeof(region)) == ESP_OK);
    assert(region[0] == 0x23 && region[1] == 0x45);
    xreader::gfx::destroy(&framebuffer);

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
    return 0;
}
