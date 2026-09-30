#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "epub/book.hpp"
#include "epub/css.hpp"
#include "epub/image.hpp"
#include "epub/inflate.hpp"
#include "epub/xml.hpp"
#include "epub/zip.hpp"
#include "gfx/font.hpp"
#include "gfx/framebuffer.hpp"
#include "gfx/unicode_font.hpp"
#include "services/library_index.hpp"
#include "services/book_manager.hpp"
#include "services/file_browser.hpp"
#include "services/version.hpp"
#include "ui/layout/layout.hpp"
#include "ui/navigation.hpp"
#include "ui/reader.hpp"
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

    assert(xreader::services::version::compare("1.2.3", "1.2.4") < 0);
    assert(xreader::services::version::compare("v2.0.0", "1.99.99") > 0);
    assert(xreader::services::version::compare("1.2.3", "1.2.3") == 0);
    assert(xreader::services::version::compare("1.2.3-rc1", "1.2.3") < 0);

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
    const uint8_t expected_narrow_metrics[][3] = {
        {' ', 1, 4}, {'i', 4, 4}, {'l', 4, 4}, {'r', 7, 7}, {'t', 7, 7},
    };
    for (const auto& expected : expected_narrow_metrics)
    {
        bool found = false;
        for (size_t index = 0; index < unicode_glyph_count; ++index)
        {
            if (unicode_glyphs[index].codepoint == expected[0])
            {
                assert(unicode_glyphs[index].width == expected[1]);
                assert(unicode_glyphs[index].advance == expected[2]);
                found = true;
                break;
            }
        }
        assert(found);
    }
    assert(xreader::gfx::measure_text("title", 1) == 31);
    assert(xreader::gfx::measure_text("title", 2) == 62);
    uint32_t composed = 0;
    size_t consumed_codepoints = 0;
    assert(xreader::gfx::compose_unicode('o', 0x031bU, 'n', &composed, &consumed_codepoints));
    assert(composed == 0x01a1U && consumed_codepoints == 2);
    assert(xreader::gfx::compose_unicode('a', 0x0306U, 0x0300U, &composed, &consumed_codepoints));
    assert(composed == 0x1eb1U && consumed_codepoints == 3);
    // NFC and canonical NFD Vietnamese must have identical layout metrics.
    assert(xreader::gfx::measure_text("\xe1\xba\xaf", 1) ==
           xreader::gfx::measure_text("a\xcc\x86\xcc\x81", 1)); // ắ
    assert(xreader::gfx::measure_text("\xe1\xbb\xa9", 1) ==
           xreader::gfx::measure_text("u\xcc\x9b\xcc\x81", 1)); // ứ

    // Pagination is built once and page starts are cached. Verify stable page count and
    // rendering for a multi-page Vietnamese document.
    xreader::epub::document_t paged_document = {};
    const char* sample_line =
        "Tiếng Việt thử nghiệm: Trường, người, đường, những, nước và chương. ";
    while (paged_document.length + strlen(sample_line) + 2 < sizeof(paged_document.text))
    {
        memcpy(paged_document.text + paged_document.length, sample_line, strlen(sample_line));
        paged_document.length += strlen(sample_line);
        paged_document.text[paged_document.length++] = '\n';
    }
    paged_document.text[paged_document.length] = '\0';
    xreader::ui::reader_settings_t reader_settings = {2, 0, 0, 1, 0, 0};
    const uint8_t cached_pages = xreader::ui::page_count(&paged_document, &reader_settings, 960, 540);
    assert(cached_pages > 1);
    assert(xreader::ui::page_count(&paged_document, &reader_settings, 960, 540) == cached_pages);
    uint8_t search_page = 0;
    size_t search_offset = 0;
    assert(xreader::ui::find_page(&paged_document, "Trường", &reader_settings, 960, 540, 0,
                                  &search_page, &search_offset));
    assert(search_page < cached_pages && search_offset < paged_document.length);

    const uint8_t png[] = {
        0x89, 'P',  'N',  'G',  0x0d, 0x0a, 0x1a, 0x0a, 0,    0,    0,    13,   'I',  'H',
        'D',  'R',  0,    0,    0,    2,    0,    0,    0,    1,    8,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,    14,   'I',  'D',  'A',  'T',  0x78,
        0x01, 0x01, 0x03, 0x00, 0xfc, 0xff, 0x00, 0x00, 0xff, 0x00, 0x01, 0x01, 0x00, 0,
        0,    0,    0,    'I',  'E',  'N',  'D',  0,    0,    0,    0,
    };
    const xreader::epub::css::style_t hidden_css =
        xreader::epub::css::parse_inline("display: none; text-align:center; white-space: pre-wrap");
    assert(hidden_css.hidden);
    assert(hidden_css.preformatted);
    assert(hidden_css.text_align == xreader::epub::css::align_center);
    const xreader::epub::css::style_t break_css =
        xreader::epub::css::parse_inline("break-before: page; display:block");
    assert(break_css.page_break_before && break_css.block);

    xreader::epub::image::info_t image_info = {};
    assert(xreader::epub::image::inspect(png, sizeof(png), &image_info) == ESP_OK);
    assert(image_info.width == 2 && image_info.height == 1 && image_info.supported);
    uint8_t image_pixels[1] = {};
    assert(xreader::epub::image::decode_mono(png, sizeof(png), image_pixels,
                                             sizeof(image_pixels)) == ESP_OK);
    assert(image_pixels[0] == 0xf0);
    const uint8_t jpeg_header[] = {0xff, 0xd8, 0xff, 0xc0, 0x00, 0x11, 0x08,
                                   0x01, 0xe0, 0x03, 0x20, 0x03, 0x01, 0x11,
                                   0x00, 0x02, 0x11, 0x00, 0x03, 0x11, 0x00};
    xreader::epub::image::info_t jpeg_info = {};
    assert(xreader::epub::image::inspect(jpeg_header, sizeof(jpeg_header), &jpeg_info) == ESP_OK);
    assert(jpeg_info.width == 800U && jpeg_info.height == 480U && !jpeg_info.supported);

    xreader::gfx::framebuffer_t framebuffer = {};
    assert(xreader::gfx::create(&framebuffer, 8, 2) == ESP_OK);
    for (uint16_t x = 0; x < 8; ++x)
        xreader::gfx::set_pixel(&framebuffer, x, 0, static_cast<uint8_t>(x));
    uint8_t region[4] = {};
    assert(xreader::gfx::copy_region_4bpp(&framebuffer, 2, 0, 4, 1, region, sizeof(region)) ==
           ESP_OK);
    assert(region[0] == 0x23 && region[1] == 0x45);
    const uint8_t scaled_source[2] = {0x0f, 0xf0}; // 2x2: black/white, white/black
    xreader::gfx::blit_4bpp_scaled(&framebuffer, 0, 0, 4, 2, scaled_source, 2, 2);
    uint8_t scaled_region[4] = {};
    assert(xreader::gfx::copy_region_4bpp(&framebuffer, 0, 0, 4, 2, scaled_region,
                                          sizeof(scaled_region)) == ESP_OK);
    assert(scaled_region[0] == 0x00 && scaled_region[1] == 0xff);
    assert(scaled_region[2] == 0xff && scaled_region[3] == 0x00);
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
         "<item id=\"two\" href=\"ch2.xhtml\" media-type=\"application/xhtml+xml\"/>"
         "<item id=\"cover\" href=\"images/cover.png\" media-type=\"image/png\" properties=\"cover-image\"/></manifest>"
         "<spine><itemref idref=\"one\"/><itemref idref=\"two\"/></spine></package>"},
        {"OEBPS/ch1.xhtml", "<html><body><p>Chapter one. a\xcc\x86\xcc\x81 &#x1EB1;</p></body></html>"},
        {"OEBPS/ch2.xhtml", "<html><body><p>Chapter two.</p><span style=\"display:none\">SECRET</span><pre>A  B</pre></body></html>"},
    };
    const char* epub_path = "/tmp/xreader-test.epub";
    write_stored_zip(epub_path, epub_entries, sizeof(epub_entries) / sizeof(epub_entries[0]));
    xreader::epub::book_t book = {};
    xreader::epub::document_t document = {};
    assert(xreader::epub::load_metadata(epub_path, &book) == ESP_OK);
    assert(book.spine_count == 2);
    assert(strcmp(book.spine[0].href, "ch1.xhtml") == 0);
    assert(strcmp(book.spine[1].href, "ch2.xhtml") == 0);
    assert(strcmp(book.cover_href, "OEBPS/images/cover.png") == 0);
    assert(xreader::epub::load_document(epub_path, &book, 0, &document) == ESP_OK);
    assert(strstr(document.text, "Chapter one") != nullptr);
    assert(strstr(document.text, "\xe1\xba\xaf") != nullptr); // NFC ắ
    assert(strstr(document.text, "\xe1\xba\xb1") != nullptr); // numeric entity ằ
    assert(xreader::epub::load_document(epub_path, &book, 1, &document) == ESP_OK);
    assert(strstr(document.text, "Chapter two") != nullptr);
    assert(strstr(document.text, "SECRET") == nullptr);
    assert(strstr(document.text, "A  B") != nullptr);

    const xreader::input::action_event_t pointer_right = {xreader::input::action_pointer, 721, 0};
    const xreader::input::action_event_t pointer_left = {xreader::input::action_pointer, 240, 0};
    const xreader::input::action_event_t next_action = {xreader::input::action_right, 0, 0};
    const xreader::input::action_event_t prev_action = {xreader::input::action_left, 0, 0};
    assert(xreader::ui::page_delta(&pointer_right, 960, 0, 3) == 1);
    assert(xreader::ui::page_delta(&pointer_left, 960, 1, 3) == -1);
    assert(xreader::ui::page_delta(&next_action, 960, 0, 3) == 1);
    assert(xreader::ui::page_delta(&prev_action, 960, 2, 3) == -1);
    assert(xreader::ui::page_delta(&pointer_right, 960, 2, 3) == 0);
    assert(xreader::ui::page_delta(&pointer_left, 960, 0, 3) == 0);
    assert(xreader::ui::navigation_result(&pointer_right, 960, 0, 2, 0, 3) ==
           xreader::ui::navigation_page_forward);
    assert(xreader::ui::navigation_result(&pointer_right, 960, 1, 2, 0, 3) ==
           xreader::ui::navigation_chapter_forward);
    assert(xreader::ui::navigation_result(&pointer_left, 960, 0, 2, 1, 3) ==
           xreader::ui::navigation_chapter_backward);
    assert(xreader::ui::navigation_result(&pointer_left, 960, 0, 2, 0, 3) ==
           xreader::ui::navigation_none);

    // Responsive metrics must distinguish XTeink-class 800x480 and M5Paper-class 960x540.
    const auto compact_metrics = xreader::ui::layout::metrics({800, 480});
    const auto medium_metrics = xreader::ui::layout::metrics({960, 540});
    assert(compact_metrics.display_class == xreader::ui::layout::display_compact);
    assert(medium_metrics.display_class == xreader::ui::layout::display_medium);
    assert(compact_metrics.margin < medium_metrics.margin);
    // Bottom navigation is a direct touch target, so keep it at least finger-sized.
    assert(compact_metrics.footer_height >= 48);
    assert(medium_metrics.footer_height >= 48);
    const auto compact_content = xreader::ui::layout::content({800, 480});
    const auto medium_content = xreader::ui::layout::content({960, 540});
    assert(compact_content.width == 800 && medium_content.width == 960);
    assert(compact_content.height < medium_content.height);
    assert(xreader::ui::layout::footer_hit({960, 540}, 100, 530) == xreader::ui::layout::footer_left);
    assert(xreader::ui::layout::footer_hit({960, 540}, 480, 530) == xreader::ui::layout::footer_center);
    assert(xreader::ui::layout::footer_hit({960, 540}, 900, 530) == xreader::ui::layout::footer_right);

    // Pagination responds to viewport size instead of assuming 960x540.
    const uint8_t compact_pages =
        xreader::ui::page_count(&paged_document, &reader_settings, 800, 480);
    const uint8_t medium_pages =
        xreader::ui::page_count(&paged_document, &reader_settings, 960, 540);
    assert(compact_pages >= medium_pages);

    xreader::ui::screen_state_t screen = {};
    const xreader::input::action_event_t down = {xreader::input::action_down, 0, 0};
    const xreader::input::action_event_t select = {xreader::input::action_select, 0, 0};
    const xreader::input::action_event_t back = {xreader::input::action_back, 0, 0};
    const xreader::ui::screen_context_t screen_context = {
        .viewport = {960, 540},
        .library_count = 3,
        .page = 0,
        .page_count = 1,
        .spine_index = 0,
        .spine_count = 2,
        .toc_count = 2,
        .bookmark_count = 0,
        .wifi_network_count = 0,
        .file_browser_count = 0,
        .file_browser_at_root = true,
    };
    xreader::ui::initialize(&screen);
    assert(screen.screen == xreader::ui::screen_home);
    assert(xreader::ui::dispatch(&screen, &down, &screen_context) ==
           xreader::ui::screen_command_redraw);
    assert(screen.home_focus == xreader::ui::home_library);
    assert(xreader::ui::dispatch(&screen, &select, &screen_context) ==
           xreader::ui::screen_command_show_library);
    assert(screen.screen == xreader::ui::screen_library);
    assert(xreader::ui::dispatch(&screen, &down, &screen_context) ==
           xreader::ui::screen_command_redraw);
    assert(screen.library_focus == 1);
    assert(xreader::ui::dispatch(&screen, &back, &screen_context) ==
           xreader::ui::screen_command_show_home);
    assert(screen.screen == xreader::ui::screen_home);

    // Recent books enters a library-style recent view rather than immediately reopening one book.
    screen.home_focus = xreader::ui::home_recent_books;
    assert(xreader::ui::dispatch(&screen, &select, &screen_context) ==
           xreader::ui::screen_command_show_recent_library);
    assert(screen.screen == xreader::ui::screen_library);
    const xreader::input::action_event_t left_action = {xreader::input::action_left, 0, 0};
    const xreader::input::action_event_t right_action = {xreader::input::action_right, 0, 0};
    assert(xreader::ui::dispatch(&screen, &left_action, &screen_context) ==
           xreader::ui::screen_command_library_search);
    assert(xreader::ui::dispatch(&screen, &right_action, &screen_context) ==
           xreader::ui::screen_command_show_library_details);
    assert(screen.screen == xreader::ui::screen_library_details);
    assert(xreader::ui::dispatch(&screen, &back, &screen_context) ==
           xreader::ui::screen_command_show_library);
    assert(screen.screen == xreader::ui::screen_library);


    // Reader menu exposes the extended screens and remains button navigable.
    screen.screen = xreader::ui::screen_quick_settings;
    screen.quick_focus = xreader::ui::quick_setting_contents;
    assert(xreader::ui::dispatch(&screen, &select, &screen_context) ==
           xreader::ui::screen_command_show_contents);
    assert(screen.screen == xreader::ui::screen_contents);
    assert(xreader::ui::dispatch(&screen, &back, &screen_context) ==
           xreader::ui::screen_command_close_quick_settings);
    assert(screen.screen == xreader::ui::screen_reader);


    // Management screens are reachable with the same button navigation model.
    // Book Manager and Book Sync now hang off the Settings hub, not Home.
    screen.screen = xreader::ui::screen_settings;
    screen.settings_focus = xreader::ui::settings_book_manager;
    assert(xreader::ui::dispatch(&screen, &select, &screen_context) ==
           xreader::ui::screen_command_show_book_manager);
    assert(screen.screen == xreader::ui::screen_book_manager);
    assert(xreader::ui::dispatch(&screen, &back, &screen_context) ==
           xreader::ui::screen_command_show_settings);

    screen.screen = xreader::ui::screen_settings;
    screen.settings_focus = xreader::ui::settings_connectivity;
    assert(xreader::ui::dispatch(&screen, &select, &screen_context) ==
           xreader::ui::screen_command_show_connectivity);
    assert(screen.screen == xreader::ui::screen_connectivity);
    assert(xreader::ui::dispatch(&screen, &back, &screen_context) ==
           xreader::ui::screen_command_show_settings);
    screen.settings_focus = xreader::ui::settings_ota;
    assert(xreader::ui::dispatch(&screen, &select, &screen_context) ==
           xreader::ui::screen_command_show_ota);
    assert(screen.screen == xreader::ui::screen_ota);

    // Footer cells are real touch targets, not decoration.  Settings is a hub, so
    // its centre footer cell opens the focused row's screen (Display Settings
    // here); that screen's own Back footer cell then returns to Settings, not
    // all the way to Home.
    screen.screen = xreader::ui::screen_settings;
    screen.settings_focus = xreader::ui::settings_display;
    const xreader::input::action_event_t footer_change = {xreader::input::action_pointer, 480, 530};
    assert(xreader::ui::dispatch(&screen, &footer_change, &screen_context) ==
           xreader::ui::screen_command_show_display_settings);
    assert(screen.screen == xreader::ui::screen_display_settings);
    const xreader::input::action_event_t footer_back = {xreader::input::action_pointer, 900, 530};
    assert(xreader::ui::dispatch(&screen, &footer_back, &screen_context) ==
           xreader::ui::screen_command_show_settings);
    assert(screen.screen == xreader::ui::screen_settings);

    // Wi-Fi network selection and text input are fully button navigable.
    screen.screen = xreader::ui::screen_connectivity;
    screen.connectivity_focus = xreader::ui::connectivity_network;
    assert(xreader::ui::dispatch(&screen, &select, &screen_context) ==
           xreader::ui::screen_command_show_wifi_networks);
    assert(screen.screen == xreader::ui::screen_wifi_networks);

    xreader::ui::screen_context_t wifi_context = screen_context;
    wifi_context.wifi_network_count = 3;
    assert(xreader::ui::dispatch(&screen, &down, &wifi_context) ==
           xreader::ui::screen_command_redraw);
    assert(screen.wifi_network_focus == 1);
    assert(xreader::ui::dispatch(&screen, &back, &wifi_context) ==
           xreader::ui::screen_command_show_connectivity);

    xreader::ui::keyboard_begin(&screen.keyboard, xreader::ui::keyboard_purpose_wifi_password,
                                "WI-FI PASSWORD", "", true, xreader::ui::keyboard_qwerty);
    screen.screen = xreader::ui::screen_keyboard;
    screen.keyboard.focus_row = 1;
    screen.keyboard.focus_col = 0; // q
    assert(xreader::ui::dispatch(&screen, &select, &wifi_context) ==
           xreader::ui::screen_command_redraw);
    assert(strcmp(screen.keyboard.text, "q") == 0);
    screen.keyboard.focus_row = 4;
    screen.keyboard.focus_col = 4; // switch to T9
    assert(xreader::ui::dispatch(&screen, &select, &wifi_context) ==
           xreader::ui::screen_command_redraw);
    assert(screen.keyboard.mode == xreader::ui::keyboard_t9);


    // Persistent library catalog supports metadata sorting and filtering without rescanning.
    xreader::services::library_index::catalog_t catalog = {};
    catalog.count = 3;
    snprintf(catalog.entries[0].title, sizeof(catalog.entries[0].title), "Zulu");
    snprintf(catalog.entries[0].author, sizeof(catalog.entries[0].author), "Beta");
    catalog.entries[0].modified_time = 10;
    snprintf(catalog.entries[1].title, sizeof(catalog.entries[1].title), "Alpha");
    snprintf(catalog.entries[1].author, sizeof(catalog.entries[1].author), "Gamma");
    catalog.entries[1].modified_time = 30;
    snprintf(catalog.entries[2].title, sizeof(catalog.entries[2].title), "Middle");
    snprintf(catalog.entries[2].author, sizeof(catalog.entries[2].author), "Alpha");
    catalog.entries[2].modified_time = 20;
    xreader::services::library_index::sort(&catalog, xreader::services::library_index::sort_title);
    assert(strcmp(catalog.entries[0].title, "Alpha") == 0);
    xreader::services::library_index::sort(&catalog, xreader::services::library_index::sort_author);
    assert(strcmp(catalog.entries[0].author, "Alpha") == 0);
    xreader::services::library_index::sort(&catalog, xreader::services::library_index::sort_recent_added);
    assert(strcmp(catalog.entries[0].title, "Alpha") == 0);
    catalog.entries[0].last_read_order = 2;
    catalog.entries[1].last_read_order = 9;
    catalog.entries[2].last_read_order = 4;
    xreader::services::library_index::sort(&catalog, xreader::services::library_index::sort_recent_read);
    assert(catalog.entries[0].last_read_order == 9);
    uint16_t matches[3] = {};
    assert(xreader::services::library_index::filter(&catalog, "mid", matches, 3) == 1);


    // Keyboard supports symbols, cursor editing, and password reveal.
    xreader::ui::keyboard_begin(&screen.keyboard, xreader::ui::keyboard_purpose_wifi_password,
                                "PASSWORD", "ab", true, xreader::ui::keyboard_qwerty);
    screen.keyboard.focus_row = 4;
    screen.keyboard.focus_col = 3; // symbols
    assert(xreader::ui::keyboard_handle(&screen.keyboard, &select, {800, 480}) ==
           xreader::ui::keyboard_result_redraw);
    assert(screen.keyboard.mode == xreader::ui::keyboard_symbols);
    screen.keyboard.focus_row = 4;
    screen.keyboard.focus_col = 1; // cursor left
    assert(xreader::ui::keyboard_handle(&screen.keyboard, &select, {800, 480}) ==
           xreader::ui::keyboard_result_redraw);
    assert(screen.keyboard.cursor == 1);
    screen.keyboard.focus_row = 0;
    screen.keyboard.focus_col = 0; // !
    assert(xreader::ui::keyboard_handle(&screen.keyboard, &select, {800, 480}) ==
           xreader::ui::keyboard_result_redraw);
    assert(strcmp(screen.keyboard.text, "a!b") == 0);
    const xreader::input::action_event_t menu = {xreader::input::action_menu, 0, 0};
    assert(xreader::ui::keyboard_handle(&screen.keyboard, &menu, {800, 480}) ==
           xreader::ui::keyboard_result_redraw);
    assert(screen.keyboard.reveal);

    // Generic dialogs remain fully button navigable.
    xreader::ui::dialog_begin(&screen.dialog, xreader::ui::dialog_confirm, "DELETE",
                              "Delete selected book?", "DELETE", "CANCEL");
    assert(xreader::ui::dialog_handle(&screen.dialog, &next_action, {800, 480}) ==
           xreader::ui::dialog_result_redraw);
    assert(!screen.dialog.focus_accept);
    assert(xreader::ui::dialog_handle(&screen.dialog, &select, {800, 480}) ==
           xreader::ui::dialog_result_cancel);

    // Storage/About are reachable from Settings.
    screen.screen = xreader::ui::screen_settings;
    screen.settings_focus = xreader::ui::settings_storage;
    assert(xreader::ui::dispatch(&screen, &select, &screen_context) ==
           xreader::ui::screen_command_show_storage);
    assert(screen.screen == xreader::ui::screen_storage);
    assert(xreader::ui::dispatch(&screen, &back, &screen_context) ==
           xreader::ui::screen_command_show_settings);
    screen.settings_focus = xreader::ui::settings_about;
    assert(xreader::ui::dispatch(&screen, &select, &screen_context) ==
           xreader::ui::screen_command_show_about);
    assert(screen.screen == xreader::ui::screen_about);


    // File browser and book-management services operate without fixed UI paths.
    char temp_root[128] = {};
    snprintf(temp_root, sizeof(temp_root), "/tmp/xreader-files-%ld", static_cast<long>(getpid()));
    mkdir(temp_root, 0755);
    char subdir[160] = {};
    snprintf(subdir, sizeof(subdir), "%s/Folder", temp_root);
    mkdir(subdir, 0755);
    char first_path[192] = {};
    char second_path[192] = {};
    snprintf(first_path, sizeof(first_path), "%s/one.epub", temp_root);
    snprintf(second_path, sizeof(second_path), "%s/two.epub", temp_root);
    FILE* fixture = fopen(first_path, "wb");
    assert(fixture != nullptr);
    assert(fwrite("same", 1, 4, fixture) == 4);
    fclose(fixture);
    fixture = fopen(second_path, "wb");
    assert(fixture != nullptr);
    assert(fwrite("same", 1, 4, fixture) == 4);
    fclose(fixture);

    xreader::services::file_browser::listing_t listing = {};
    assert(xreader::services::file_browser::open(temp_root, temp_root, &listing) == ESP_OK);
    assert(listing.count == 3);
    assert(listing.entries[0].directory);
    assert(xreader::services::file_browser::at_root(&listing));
    assert(xreader::services::file_browser::open(temp_root, subdir, &listing) == ESP_OK);
    assert(!xreader::services::file_browser::at_root(&listing));
    assert(xreader::services::file_browser::parent(&listing) == ESP_OK);
    assert(xreader::services::file_browser::at_root(&listing));

    xreader::services::library_index::catalog_t duplicate_catalog = {};
    duplicate_catalog.count = 2;
    snprintf(duplicate_catalog.entries[0].path, sizeof(duplicate_catalog.entries[0].path), "%s", first_path);
    snprintf(duplicate_catalog.entries[1].path, sizeof(duplicate_catalog.entries[1].path), "%s", second_path);
    duplicate_catalog.entries[0].file_size = 4;
    duplicate_catalog.entries[1].file_size = 4;
    assert(xreader::services::book_manager::duplicate_count(&duplicate_catalog) == 1);

    char renamed_path[192] = {};
    assert(xreader::services::book_manager::rename_book(first_path, "renamed", renamed_path,
                                                         sizeof(renamed_path)) == ESP_OK);
    assert(strstr(renamed_path, "renamed.epub") != nullptr);
    assert(xreader::services::book_manager::delete_book(renamed_path) == ESP_OK);
    unlink(second_path);
    rmdir(subdir);
    rmdir(temp_root);

    // Reader supports physical directional/select/back actions.
    screen.screen = xreader::ui::screen_reader;
    assert(xreader::ui::dispatch(&screen, &select, &screen_context) ==
           xreader::ui::screen_command_open_quick_settings);
    assert(screen.screen == xreader::ui::screen_quick_settings);
    assert(xreader::ui::dispatch(&screen, &back, &screen_context) ==
           xreader::ui::screen_command_close_quick_settings);
    assert(screen.screen == xreader::ui::screen_reader);
    return 0;
}
