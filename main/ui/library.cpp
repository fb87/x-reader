#include "library.hpp"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/layout/layout.hpp"
#include "ui/navigation/focus.hpp"

namespace xreader
{
namespace ui
{

namespace
{

static constexpr uint8_t max_books = 8;
static constexpr size_t max_name_length = 48;

struct book_list_t
{
    uint8_t count;
    char names[max_books][max_name_length];
    char titles[max_books][max_name_length];
};

// Mockup 2 puts a "Library / SD Card" header under the status bar and fills the
// rest with rows.  Drawing and hit-testing both go through these helpers.
static layout::rect_t catalog_area(layout::viewport_t vp)
{
    const layout::metrics_t m = layout::metrics(vp);
    layout::rect_t area = layout::content(vp);
    const uint16_t header = m.display_class == layout::display_compact ? 40U : 46U;
    if (area.height > header)
    {
        area.y = static_cast<uint16_t>(area.y + header);
        area.height = static_cast<uint16_t>(area.height - header);
    }
    const uint16_t inset = m.display_class == layout::display_compact ? 14U : 20U;
    if (area.width > inset * 2U)
    {
        area.x = static_cast<uint16_t>(area.x + inset);
        area.width = static_cast<uint16_t>(area.width - inset * 2U);
    }
    return area;
}

static uint16_t catalog_row_height(layout::viewport_t vp)
{
    // +36 (was +16) to fit three stacked 1x text lines (title/author/size) at
    // the larger glyph size: each line needs a full 24px box plus a couple of
    // pixels of gap, and cramming that into the old 60px total is what used to
    // make the text look "too small" -- fewer, taller rows is the trade-off.
    return static_cast<uint16_t>(layout::metrics(vp).row_height + 36U);
}

static uint8_t catalog_visible_rows(layout::viewport_t vp, size_t count)
{
    const layout::metrics_t m = layout::metrics(vp);
    const layout::rect_t area = catalog_area(vp);
    const uint16_t step = static_cast<uint16_t>(catalog_row_height(vp) + m.gap);
    uint8_t fits = step == 0U ? 1U : static_cast<uint8_t>((area.height + m.gap) / step);
    if (fits == 0U)
        fits = 1U;
    return static_cast<uint8_t>(count < fits ? count : fits);
}

static uint8_t catalog_first_row(uint8_t focus, uint8_t visible, size_t count)
{
    if (count == 0U || visible == 0U)
        return 0U;
    const uint8_t selected = static_cast<uint8_t>(focus % count);
    return selected >= visible ? static_cast<uint8_t>(selected - visible + 1U) : 0U;
}

static layout::rect_t catalog_row(layout::viewport_t vp, uint8_t index, uint8_t visible)
{
    const layout::metrics_t m = layout::metrics(vp);
    return layout::stacked_row(catalog_area(vp), index, visible, catalog_row_height(vp), m.gap);
}

static void format_size(uint32_t bytes, char* output, size_t capacity)
{
    if (bytes >= 1024U * 1024U)
        snprintf(output, capacity, "EPUB  %lu.%lu MB",
                 static_cast<unsigned long>(bytes / (1024U * 1024U)),
                 static_cast<unsigned long>((bytes % (1024U * 1024U)) / (105U * 1024U)));
    else
        snprintf(output, capacity, "EPUB  %lu KB", static_cast<unsigned long>(bytes / 1024U));
}

static bool is_epub(const char* name)
{
    const char* extension = nullptr;
    for (const char* cursor = name; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '.')
        {
            extension = cursor + 1;
        }
    }
    if (extension == nullptr)
    {
        return false;
    }
    const size_t extension_length = strlen(extension);
    if (extension_length != 3 && extension_length != 4)
    {
        return false;
    }
    return (extension[0] == 'e' || extension[0] == 'E') &&
           (extension[1] == 'p' || extension[1] == 'P') &&
           (extension[2] == 'u' || extension[2] == 'U') &&
           (extension_length == 3 ||
            ((extension[3] == 'b' || extension[3] == 'B') && extension[4] == '\0'));
}

static void copy_name(char* destination, const char* source)
{
    size_t index = 0;
    while (index + 1 < max_name_length && source[index] != '\0')
    {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

static int compare_names(const char* left, const char* right)
{
    while (*left != '\0' && *right != '\0')
    {
        const int left_value = tolower(static_cast<unsigned char>(*left));
        const int right_value = tolower(static_cast<unsigned char>(*right));
        if (left_value != right_value)
            return left_value < right_value ? -1 : 1;
        ++left;
        ++right;
    }
    return *left == *right ? 0 : (*left == '\0' ? -1 : 1);
}

static void sort_books(book_list_t* books)
{
    for (uint8_t index = 1; index < books->count; ++index)
    {
        char name[max_name_length] = {};
        char title[max_name_length] = {};
        copy_name(name, books->names[index]);
        copy_name(title, books->titles[index]);
        uint8_t position = index;
        while (position > 0 && compare_names(name, books->names[position - 1]) < 0)
        {
            copy_name(books->names[position], books->names[position - 1]);
            copy_name(books->titles[position], books->titles[position - 1]);
            --position;
        }
        copy_name(books->names[position], name);
        copy_name(books->titles[position], title);
    }
}

static void scan_directory(const char* path, book_list_t* books, uint8_t depth)
{
    if (depth > 3 || books->count >= max_books)
        return;
    DIR* directory = opendir(path);
    if (directory == nullptr)
    {
        return;
    }

    struct dirent* entry = nullptr;
    while (books->count < max_books && (entry = readdir(directory)) != nullptr)
    {
        if (entry->d_name[0] == '.')
            continue;
        char entry_path[512] = {};
        const size_t path_length = strlen(path);
        if (path_length + 1 + strlen(entry->d_name) >= sizeof(entry_path))
            continue;
        memcpy(entry_path, path, path_length);
        entry_path[path_length] = '/';
        strcpy(entry_path + path_length + 1, entry->d_name);
        struct stat entry_stat = {};
        if (stat(entry_path, &entry_stat) != 0)
            continue;
        if (S_ISDIR(entry_stat.st_mode))
        {
            scan_directory(entry_path, books, static_cast<uint8_t>(depth + 1));
            continue;
        }
        if (!is_epub(entry->d_name))
            continue;
        copy_name(books->names[books->count], entry->d_name);
        copy_name(books->titles[books->count], entry->d_name);
        ++books->count;
    }
    closedir(directory);
}

static void collect_books(const char* directory, char paths[][book_path_length], size_t capacity,
                          size_t* count, uint8_t depth)
{
    if (depth > 3 || *count >= capacity)
        return;
    DIR* handle = opendir(directory);
    if (handle == nullptr)
        return;
    struct dirent* entry = nullptr;
    while (*count < capacity && (entry = readdir(handle)) != nullptr)
    {
        if (entry->d_name[0] == '.')
            continue;
        char candidate[book_path_length] = {};
        const size_t directory_length = strlen(directory);
        const size_t entry_length = strlen(entry->d_name);
        if (directory_length + 1 + entry_length >= sizeof(candidate))
            continue;
        memcpy(candidate, directory, directory_length);
        candidate[directory_length] = '/';
        strcpy(candidate + directory_length + 1, entry->d_name);
        struct stat entry_stat = {};
        if (stat(candidate, &entry_stat) != 0)
            continue;
        if (S_ISDIR(entry_stat.st_mode))
        {
            collect_books(candidate, paths, capacity, count, static_cast<uint8_t>(depth + 1));
        }
        else if (is_epub(entry->d_name))
        {
            strcpy(paths[*count], candidate);
            ++*count;
        }
    }
    closedir(handle);
}

static void sort_paths(char paths[][book_path_length], size_t count)
{
    for (size_t index = 1; index < count; ++index)
    {
        char path[book_path_length] = {};
        strcpy(path, paths[index]);
        size_t position = index;
        while (position > 0 && compare_names(path, paths[position - 1]) < 0)
        {
            strcpy(paths[position], paths[position - 1]);
            --position;
        }
        strcpy(paths[position], path);
    }
}

} // namespace

namespace
{
static const char* basename_from_path(const char* path)
{
    const char* result = path;
    if (path == nullptr)
        return "";
    for (const char* cursor = path; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '/')
            result = cursor + 1;
    }
    return result;
}

static void draw_book_icon(gfx::framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint16_t w,
                           uint16_t h, uint8_t fg = 0x00)
{
    gfx::draw_rect(framebuffer, x, y, w, h, fg);
    if (w > 12 && h > 12)
    {
        gfx::fill_rect(framebuffer, static_cast<uint16_t>(x + 7U), y, 1, h, fg);
        gfx::fill_rect(framebuffer, static_cast<uint16_t>(x + 13U), static_cast<uint16_t>(y + 9U),
                       static_cast<uint16_t>(w > 20 ? w - 18U : 2U), 1, fg);
        gfx::fill_rect(framebuffer, static_cast<uint16_t>(x + 13U), static_cast<uint16_t>(y + 15U),
                       static_cast<uint16_t>(w > 24 ? w - 22U : 2U), 1, fg);
    }
}

static layout::rect_t library_area(layout::viewport_t vp)
{
    const layout::metrics_t m = layout::metrics(vp);
    return layout::inset(layout::content(vp), m.margin);
}

static void draw_library_rows(gfx::framebuffer_t* framebuffer, const char* const* titles,
                              uint8_t count, uint8_t focus_index, int8_t footer_focus)
{
    const layout::viewport_t vp = {framebuffer->width, framebuffer->height};
    const layout::metrics_t m = layout::metrics(vp);
    const layout::rect_t area = library_area(vp);
    const uint8_t visible = count > 6 ? 6 : count;
    for (uint8_t index = 0; index < visible; ++index)
    {
        const layout::rect_t item =
            layout::row(area, index, visible, static_cast<uint16_t>(m.row_height + 8U), m.gap);
        const bool selected = footer_focus < 0 && index == static_cast<uint8_t>(focus_index % count);
        // Focus-move redraws use the panel's fast 1-bit-only refresh mode,
        // which thresholds every pixel to pure black/white -- a subtle gray
        // wash is invisible under it. Inverting to a solid black row with
        // white content survives that threshold instead.
        gfx::fill_rect(framebuffer, item.x, item.y, item.width, item.height,
                       selected ? 0x00 : 0x0f);
        if (!selected)
            gfx::draw_rect(framebuffer, item.x, item.y, item.width, item.height, 0x0d);

        const uint16_t icon_h =
            item.height > 16 ? static_cast<uint16_t>(item.height - 16U) : item.height;
        const uint16_t icon_w = static_cast<uint16_t>(icon_h * 3U / 4U);
        draw_book_icon(framebuffer, static_cast<uint16_t>(item.x + 12U),
                       static_cast<uint16_t>(item.y + (item.height - icon_h) / 2U), icon_w, icon_h,
                       selected ? 0x0f : 0x00);

        const uint16_t text_x = static_cast<uint16_t>(item.x + 24U + icon_w);
        gfx::draw_text(framebuffer, text_x, static_cast<uint16_t>(item.y + 11U), titles[index], 1,
                       selected ? 0x0f : 0x00);
        if (item.height >= 60)
            gfx::draw_text(framebuffer, text_x, static_cast<uint16_t>(item.y + 35U), "EPUB", 1,
                           selected ? 0x0f : 0x00);
    }
}
} // namespace

void draw_library(gfx::framebuffer_t* framebuffer, bool storage_mounted, const char* mount_path,
                  uint8_t focus_index, int8_t footer_focus)
{
    if (framebuffer == nullptr)
        return;

    const layout::viewport_t vp = {framebuffer->width, framebuffer->height};
    const layout::metrics_t m = layout::metrics(vp);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "Library");
    chrome::draw_indication_bar(framebuffer, {"Move", gfx::icon_list}, {"Open", gfx::icon_book},
                                {"Back", gfx::icon_arrow_back}, footer_focus);

    if (!storage_mounted)
    {
        gfx::draw_text(framebuffer, m.margin, static_cast<uint16_t>(m.status_height + m.margin),
                       "NO SD CARD", 2, 0x00);
        return;
    }

    book_list_t books = {};
    if (mount_path == nullptr)
    {
        gfx::draw_text(framebuffer, m.margin, static_cast<uint16_t>(m.status_height + m.margin),
                       "SD ERROR", 2, 0x00);
        return;
    }
    scan_directory(mount_path, &books, 0);
    sort_books(&books);
    if (books.count == 0)
    {
        gfx::draw_text(framebuffer, m.margin, static_cast<uint16_t>(m.status_height + m.margin),
                       "NO EPUB BOOKS", 2, 0x00);
        return;
    }

    const char* titles[max_books] = {};
    for (uint8_t index = 0; index < books.count; ++index)
        titles[index] = books.titles[index];
    draw_library_rows(framebuffer, titles, books.count, focus_index, footer_focus);
}

void draw_library_list(gfx::framebuffer_t* framebuffer, bool storage_mounted,
                       const char paths[][book_path_length], size_t count, uint8_t focus_index,
                       int8_t footer_focus)
{
    if (framebuffer == nullptr)
        return;
    const layout::viewport_t vp = {framebuffer->width, framebuffer->height};
    const layout::metrics_t m = layout::metrics(vp);
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, "Library");
    chrome::draw_indication_bar(framebuffer, {"Move", gfx::icon_list}, {"Open", gfx::icon_book},
                                {"Back", gfx::icon_arrow_back}, footer_focus);
    if (!storage_mounted)
    {
        gfx::draw_text(framebuffer, m.margin, static_cast<uint16_t>(m.status_height + m.margin),
                       "NO SD CARD", 2, 0x00);
        return;
    }
    if (paths == nullptr || count == 0)
    {
        gfx::draw_text(framebuffer, m.margin, static_cast<uint16_t>(m.status_height + m.margin),
                       "NO EPUB BOOKS", 2, 0x00);
        return;
    }
    const uint8_t visible_count = static_cast<uint8_t>(count > max_books ? max_books : count);
    const char* titles[max_books] = {};
    for (uint8_t index = 0; index < visible_count; ++index)
        titles[index] = basename_from_path(paths[index]);
    draw_library_rows(framebuffer, titles, visible_count, focus_index, footer_focus);
}

void draw_library_catalog_view(gfx::framebuffer_t* framebuffer, bool storage_mounted,
                               const services::library_index::catalog_t* catalog,
                               const uint16_t* indices, size_t count, uint8_t focus_index,
                               const char* title, int8_t footer_focus)
{
    if (framebuffer == nullptr)
        return;
    const layout::viewport_t vp = {framebuffer->width, framebuffer->height};
    const layout::metrics_t m = layout::metrics(vp);
    gfx::clear(framebuffer, 0x0f);
    const char* heading = title == nullptr || title[0] == '\0' ? "Library" : title;
    const char* sort_name = "Title";
    if (catalog != nullptr)
    {
        if (catalog->sort_mode == services::library_index::sort_author)
            sort_name = "Author";
        else if (catalog->sort_mode == services::library_index::sort_recent_added)
            sort_name = "Added";
        else if (catalog->sort_mode == services::library_index::sort_recent_read)
            sort_name = "Recent";
    }
    char trailing[32] = {};
    snprintf(trailing, sizeof(trailing), "%s  %s", storage_mounted ? "SD Card" : "No SD card",
             sort_name);
    chrome::draw_status_bar(framebuffer, nullptr);
    chrome::draw_title_bar(framebuffer, heading, trailing);
    chrome::draw_indication_bar(framebuffer, {"Search", gfx::icon_search}, {"Open", gfx::icon_book},
                                {"Details", gfx::icon_info}, footer_focus);
    if (!storage_mounted)
    {
        gfx::draw_text(framebuffer, m.margin, static_cast<uint16_t>(m.status_height + m.margin),
                       "No SD card", 2, 0x00);
        return;
    }
    if (catalog == nullptr || count == 0U)
    {
        gfx::draw_text(framebuffer, m.margin, static_cast<uint16_t>(m.status_height + m.margin),
                       "No matching books", 2, 0x00);
        return;
    }

    const uint8_t visible = catalog_visible_rows(vp, count);
    const uint8_t selected_index = static_cast<uint8_t>(focus_index % count);
    const uint8_t start_row = catalog_first_row(focus_index, visible, count);
    for (uint8_t row_index = 0; row_index < visible; ++row_index)
    {
        const uint8_t view_index = static_cast<uint8_t>(start_row + row_index);
        if (view_index >= count)
            break;
        const uint16_t catalog_index = indices == nullptr ? view_index : indices[view_index];
        if (catalog_index >= catalog->count)
            continue;
        const layout::rect_t item = catalog_row(vp, row_index, visible);
        if (item.height == 0U)
            break;
        const bool selected = footer_focus < 0 && view_index == selected_index;
        // Focus-move redraws use the panel's fast 1-bit-only refresh mode,
        // which thresholds every pixel to pure black/white -- a subtle gray
        // wash is invisible under it. Inverting to a solid black row with
        // white content survives that threshold instead.
        gfx::fill_rect(framebuffer, item.x, item.y, item.width, item.height,
                       selected ? 0x00 : 0x0f);
        if (!selected)
            gfx::fill_rect(framebuffer, item.x, static_cast<uint16_t>(item.y + item.height - 1U),
                           item.width, 1, 0x0c);

        // The cached cover is a still-encoded JPEG/PNG; decoding one per row on
        // every redraw would cost more than the e-paper refresh itself, so the
        // list keeps the line-art placeholder and Book Info shows the real cover.
        const uint16_t icon_h =
            item.height > 18U ? static_cast<uint16_t>(item.height - 18U) : item.height;
        const uint16_t icon_w = static_cast<uint16_t>(icon_h * 3U / 4U);
        draw_book_icon(framebuffer, static_cast<uint16_t>(item.x + 10U),
                       static_cast<uint16_t>(item.y + (item.height - icon_h) / 2U), icon_w, icon_h,
                       selected ? 0x0f : 0x00);

        const uint16_t text_x = static_cast<uint16_t>(item.x + 20U + icon_w);
        const auto& entry = catalog->entries[catalog_index];
        gfx::draw_text(framebuffer, text_x, static_cast<uint16_t>(item.y + 6U), entry.title, 1,
                       selected ? 0x0f : 0x00);
        if (item.height >= 56U)
            gfx::draw_text(framebuffer, text_x, static_cast<uint16_t>(item.y + 32U), entry.author,
                           1, selected ? 0x0f : 0x00);
        if (item.height >= 75U)
        {
            char size_text[32] = {};
            format_size(entry.file_size, size_text, sizeof(size_text));
            gfx::draw_text(framebuffer, text_x, static_cast<uint16_t>(item.y + 58U), size_text, 1,
                           selected ? 0x0f : 0x00);
        }
    }
}

void draw_library_catalog(gfx::framebuffer_t* framebuffer, bool storage_mounted,
                          const services::library_index::catalog_t* catalog, size_t count,
                          uint8_t focus_index, const char* title, int8_t footer_focus)
{
    if (catalog != nullptr && count > catalog->count)
        count = catalog->count;
    draw_library_catalog_view(framebuffer, storage_mounted, catalog, nullptr, count, focus_index,
                              title, footer_focus);
}

bool library_touch_index(uint16_t display_width, uint16_t display_height, uint16_t x, uint16_t y,
                         uint8_t count, uint8_t current_focus, uint8_t* index)
{
    if (count == 0 || index == nullptr)
        return false;
    const layout::viewport_t vp = {display_width, display_height};
    const uint8_t visible = catalog_visible_rows(vp, count);
    const uint8_t start = catalog_first_row(current_focus, visible, count);
    for (uint8_t row = 0; row < visible; ++row)
    {
        const layout::rect_t bounds = catalog_row(vp, row, visible);
        if (bounds.height == 0U || !layout::contains(bounds, x, y))
            continue;
        const uint8_t resolved = static_cast<uint8_t>(start + row);
        if (resolved >= count)
            return false;
        *index = resolved;
        return true;
    }
    return false;
}

bool find_first_book(const char* mount_path, char* path, size_t capacity)
{
    if (mount_path == nullptr || path == nullptr || capacity == 0)
        return false;
    char paths[max_books][book_path_length] = {};
    const size_t count = find_books(mount_path, paths, max_books);
    if (count == 0 || strlen(paths[0]) + 1 > capacity)
        return false;
    strcpy(path, paths[0]);
    return true;
}

size_t find_books(const char* mount_path, char paths[][book_path_length], size_t capacity)
{
    if (mount_path == nullptr || paths == nullptr || capacity == 0)
        return 0;
    size_t count = 0;
    collect_books(mount_path, paths, capacity, &count, 0);
    sort_paths(paths, count);
    return count;
}

} // namespace ui
} // namespace xreader
