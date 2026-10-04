/* File Manager: browse registered storage without platform-specific logic. */
#include "app_internal.h"
#include "book_title.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#define FILE_MANAGER_MAX_ENTRIES 64
#define FILE_MANAGER_PATH_SIZE 256
#define FILE_MANAGER_NAME_SIZE 128

enum { ACT_UP_BACK = 1 };

typedef struct file_entry {
    char name[FILE_MANAGER_NAME_SIZE];
    char display[FILE_MANAGER_NAME_SIZE];
    bool directory;
} file_entry_t;

typedef struct file_manager_page {
    xr_page_t base;
    xr_list_t list;
    file_entry_t entries[FILE_MANAGER_MAX_ENTRIES];
    char path[FILE_MANAGER_PATH_SIZE];
    char title[FILE_MANAGER_NAME_SIZE];
    int count;
} file_manager_page_t;

static file_manager_page_t s_file_manager;

static void up_icon(xr_canvas_t *canvas, xr_rect_t rect, uint8_t gray)
{
    app_draw_icon(canvas, rect, XR_ICON_ARROW_BACK, gray);
}

static const xr_action_t k_file_actions[] = {
    { ACT_UP_BACK, "Up / Back", XR_KEY_NONE, up_icon },
};

static bool is_epub(const char *name)
{
    size_t length = strlen(name);
    return (length > 5 && strcasecmp(name + length - 5, ".epub") == 0) ||
           (length > 4 && strcasecmp(name + length - 4, ".epu") == 0);
}

static bool collect_entry(const char *name, bool directory, void *user)
{
    file_manager_page_t *page = user;
    if (!name || !name[0] || (!directory && !is_epub(name))) return true;
    if (page->count >= FILE_MANAGER_MAX_ENTRIES) return false;
    file_entry_t *entry = &page->entries[page->count++];
    snprintf(entry->name, sizeof entry->name, "%s", name);
    entry->directory = directory;
    app_book_display_title(entry->display, sizeof entry->display, name);
    return true;
}

static const char *path_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash && slash[1] ? slash + 1 : path;
}

static void load_directory(file_manager_page_t *page, const char *path)
{
    snprintf(page->path, sizeof page->path, "%s", path);
    page->count = 0;
    app_storage_list(page->path, collect_entry, page);
    xr_list_set_count(&page->list, page->count);
    snprintf(page->title, sizeof page->title, "Files: %.120s", path_name(page->path));
    xr_page_set_title(&page->base, page->title);
}

static void file_row(xr_list_t *list, int index, const char **primary, const char **secondary)
{
    file_manager_page_t *page = list->user;
    file_entry_t *entry = &page->entries[index];
    *primary = entry->display;
    *secondary = entry->directory ? "Folder" : "EPUB";
}

static void file_render(xr_page_t *base, xr_canvas_t *canvas)
{
    file_manager_page_t *page = XR_CONTAINER_OF(base, file_manager_page_t, base);
    xr_list_t *list = &page->list;
    int visible = xr_list_visible_rows(list);
    bool focused = xr_widget_has_focus(&list->base);
    int row_width = list->base.rect.w - (list->count > visible ? 10 : 0);

    for (int i = list->top; i < list->count && i < list->top + visible; ++i) {
        xr_rect_t row = xr_rect(list->base.rect.x,
                                list->base.rect.y + (i - list->top) * list->row_h,
                                row_width, list->row_h);
        if (!xr_rect_intersects(row, canvas->clip)) continue;
        bool inverted = i == list->selected && focused;
        uint8_t foreground = inverted ? XR_WHITE : XR_BLACK;
        uint8_t secondary = inverted ? XR_WHITE : XR_DARK;
        file_entry_t *entry = &page->entries[i];
        int icon = entry->directory ? XR_ICON_FOLDER : XR_ICON_DESCRIPTION;
        int icon_y = row.y + (row.h - 24) / 2;
        int text_x = row.x + 42;
        int primary_height = base->shell->theme->font_normal->line_height;
        int secondary_height = base->shell->theme->font_small->line_height;
        int text_y = row.y + (row.h - primary_height - secondary_height) / 2;

        xr_canvas_fill_rect(canvas, row, inverted ? XR_BLACK : XR_WHITE);
        if (i == list->selected && !focused)
            xr_canvas_fill_rect(canvas, xr_rect(row.x, row.y + 6, 5, row.h - 12), XR_BLACK);
        if (!inverted) xr_canvas_hline(canvas, row.x, row.y + row.h - 1, row.w, XR_LIGHT);
        app_draw_icon(canvas, xr_rect(row.x + 10, icon_y, 24, 24), icon, foreground);
        xr_canvas_draw_text_in(canvas, base->shell->theme->font_normal,
                               xr_rect(text_x, text_y, row.x + row.w - text_x - 8, primary_height),
                               entry->display, XR_ALIGN_LEFT, foreground);
        xr_canvas_draw_text_in(canvas, base->shell->theme->font_small,
                               xr_rect(text_x, text_y + primary_height,
                                       row.x + row.w - text_x - 8, secondary_height),
                               entry->directory ? "Folder" : "EPUB", XR_ALIGN_LEFT, secondary);
    }
}

static bool make_child_path(char *output, size_t capacity, const char *parent, const char *name)
{
    int written = snprintf(output, capacity, "%s%s%s", parent,
                           parent[0] && parent[strlen(parent) - 1] == '/' ? "" : "/", name);
    return written >= 0 && (size_t)written < capacity;
}

static void file_selected(xr_list_t *list, int index)
{
    file_manager_page_t *page = list->user;
    if (index < 0 || index >= page->count) return;
    file_entry_t *entry = &page->entries[index];
    char path[FILE_MANAGER_PATH_SIZE];
    if (!make_child_path(path, sizeof path, page->path, entry->name)) return;
    if (entry->directory) {
        load_directory(page, path);
        return;
    }

    const char *root = app_storage_root();
    size_t root_length = strlen(root);
    const char *open_path = path;
    if (root_length && strncmp(path, root, root_length) == 0 &&
        (path[root_length] == '/' || path[root_length] == '\0')) {
        open_path = path + root_length;
        if (*open_path == '/') ++open_path;
    }
    app_open_storage_epub(open_path, entry->display);
}

static void file_create(xr_page_t *base)
{
    file_manager_page_t *page = XR_CONTAINER_OF(base, file_manager_page_t, base);
    const xr_theme_t *theme = base->shell->theme;
    xr_rect_t area = base->area;
    xr_list_init(&page->list, xr_rect(area.x + 4, area.y + 4, area.w - 12, area.h - 8),
                 XR_LIST_TWO_LINE, theme->row_h, theme->font_normal, theme->font_small,
                 file_row, page);
    page->list.on_select = file_selected;
    xr_page_add(base, &page->list.base);
    load_directory(page, app_storage_root());
}

static void file_action(xr_page_t *base, uint16_t action)
{
    if (action != ACT_UP_BACK) return;
    file_manager_page_t *page = XR_CONTAINER_OF(base, file_manager_page_t, base);
    const char *root = app_storage_root();
    if (!strcmp(page->path, root)) {
        xr_shell_pop(base->shell);
        return;
    }
    char parent[FILE_MANAGER_PATH_SIZE];
    snprintf(parent, sizeof parent, "%s", page->path);
    char *slash = strrchr(parent, '/');
    if (!slash || slash < parent + strlen(root)) snprintf(parent, sizeof parent, "%s", root);
    else if (slash == parent) slash[1] = '\0';
    else *slash = '\0';
    load_directory(page, parent);
}

static const xr_page_vtbl_t k_file_vtbl = {
    .on_create = file_create,
    .render = file_render,
    .on_action = file_action,
};

xr_page_t *app_page_file_manager(void)
{
    xr_page_init(&s_file_manager.base, &k_file_vtbl, "File Manager", XR_CHROME_ALL);
    xr_page_set_actions(&s_file_manager.base, k_file_actions, XR_ARRAY_LEN(k_file_actions));
    return &s_file_manager.base;
}
