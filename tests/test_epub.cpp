#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "epub/inflate.hpp"
#include "epub/xml.hpp"
#include "epub/zip.hpp"

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
    return 0;
}
