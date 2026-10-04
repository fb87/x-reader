/* app_internal.h - state and helpers shared by the app's pages. */
#ifndef APP_INTERNAL_H
#define APP_INTERNAL_H

#include "app.h"
#include "xr_fonts.h"
#include "xr_icons.h"

#define APP_LIBRARY_MAX_BOOKS 16
#define APP_MAX_BOOKS (APP_LIBRARY_MAX_BOOKS + 1)
void app_draw_icon(xr_canvas_t *c, xr_rect_t r, int icon, uint8_t gray);

typedef struct app_book {
    const char *title, *author, *format;
    char path[256];
    uint16_t pages;
    uint8_t progress; /* percent */
    uint16_t size_kb;
    bool favorite;
    bool epub_source;
    bool transient;
} app_book_t;


typedef struct app {
    xr_shell_t *shell;
    app_book_t books[APP_MAX_BOOKS];
    char book_titles[APP_MAX_BOOKS][128];
    int book_count;
    int current; /* book on the Home card, -1 = none */
    /* settings */
    int font_idx;
    int full_refresh_every;
    bool show_progress;
    bool wifi_connected;
    bool bluetooth_connected;
    int sleep_timeout_minutes;
    bool epub_open;
    uint16_t epub_spine;
} app_t;

void app_library_clear(void);
bool app_library_add(const char *title);
bool app_library_add_path(const char *path, const char *title);
void app_set_epub_loader(app_epub_loader_fn loader);
const char *app_storage_root(void);
bool app_storage_list(const char *path,
                      bool (*entry)(const char *name, bool directory, void *user),
                      void *user);
bool app_open_storage_epub(const char *path, const char *title);

extern app_t g_app;
extern const xr_font_t *const app_body_fonts[3];
extern const char *const app_font_names[3];

xr_page_t *app_page_splash(void);
xr_page_t *app_page_home(void);
xr_page_t *app_page_library(void);
xr_page_t *app_page_favorites(void);
xr_page_t *app_page_file_manager(void);
xr_page_t *app_page_sleep(void);
xr_page_t *app_page_reader(void);
xr_page_t *app_page_settings(void);

void app_show_book_info(int index);
void app_open_book(int index);
const char *app_current_text(void);
const char *app_current_chapter_title(void);
void app_set_reading_progress(int page, int total_pages);
bool app_load_epub_chapter(uint16_t chapter);
uint16_t app_epub_chapter_index(void);
uint16_t app_epub_chapter_count(void);
bool app_current_is_cover_placeholder(void);
bool app_turn_epub_chapter(int direction);
void app_delete_book(int index);
void app_confirm(const char *title, const char *message, const char *yes, xr_button_icon_fn icon,
                 xr_dialog_result_fn cb, void *user);
void app_draw_progress(xr_canvas_t *c, xr_rect_t r, int percent);
const char *app_sample_text(void);

#endif /* APP_INTERNAL_H */
