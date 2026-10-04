/* app_internal.h - state and helpers shared by the app's pages. */
#ifndef APP_INTERNAL_H
#define APP_INTERNAL_H

#include "app.h"
#include "xr_fonts.h"

#define APP_MAX_BOOKS 16

typedef struct app_book {
    const char *title, *author, *format;
    uint16_t pages;
    uint8_t progress; /* percent */
    uint16_t size_kb;
    bool favorite;
    bool epub_source;
} app_book_t;

typedef struct app {
    xr_shell_t *shell;
    app_book_t books[APP_MAX_BOOKS];
    int book_count;
    int current; /* book on the Home card, -1 = none */
    /* settings */
    int font_idx;
    int full_refresh_every;
    bool show_progress;
    bool wifi_connected;
    bool bluetooth_connected;
    bool epub_open;
    uint16_t epub_spine;
} app_t;

extern app_t g_app;
extern const xr_font_t *const app_body_fonts[3];
extern const char *const app_font_names[3];

xr_page_t *app_page_splash(void);
xr_page_t *app_page_home(void);
xr_page_t *app_page_library(void);
xr_page_t *app_page_favorites(void);
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
