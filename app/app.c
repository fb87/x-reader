#include "app_internal.h"
#include "book_title.h"
#include "xr_icons.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define APP_LARGE_BSS EXT_RAM_BSS_ATTR
#else
#define APP_LARGE_BSS
#endif

#define APP_EPUB_MANIFEST_MAX 640
#define APP_EPUB_SPINE_MAX 640
#define APP_EPUB_SCRATCH_SIZE (80 * 1024)
#define APP_EPUB_TEXT_SIZE (64 * 1024)

app_t g_app;
static app_epub_loader_fn s_epub_loader;
static app_dir_entry_fn s_storage_list_dir;
static void *s_storage_context;
static char s_storage_root[256];

void app_draw_icon(xr_canvas_t *c, xr_rect_t r, int icon, uint8_t gray)
{
    if (icon < 0 || icon >= XR_ICON_COUNT) return;
    const xr_icon_glyph_t *g = &xr_icons[icon];
    for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24; ++x)
            if (g->bitmap[y * 3 + x / 8] & (uint8_t)(0x80u >> (x % 8)))
                xr_canvas_fill_rect(c, xr_rect(r.x + x, r.y + y, 1, 1), gray);
}
static xr_epub_t s_epub;
static xr_epub_manifest_item_t s_epub_manifest[APP_EPUB_MANIFEST_MAX] APP_LARGE_BSS;
static xr_epub_spine_item_t s_epub_spine[APP_EPUB_SPINE_MAX] APP_LARGE_BSS;
static uint8_t s_epub_scratch[APP_EPUB_SCRATCH_SIZE] APP_LARGE_BSS;
static char s_epub_text[APP_EPUB_TEXT_SIZE] APP_LARGE_BSS;
static bool s_epub_cover_placeholder;
static char s_chapter_title[96];

const xr_font_t *const app_body_fonts[3] = { &xr_font_alegreya_17, &xr_font_alegreya_20, &xr_font_alegreya_24 };
const char *const app_font_names[3] = { "Small", "Medium", "Large" };

static const xr_theme_t k_theme = {
    .font_small = &xr_font_alegreya_14,
    .font_normal = &xr_font_alegreya_18,
    .font_bold = &xr_font_alegreya_bold_18,
    .font_title = &xr_font_alegreya_bold_26,
    .font_body = &xr_font_alegreya_20,
    .status_h = 44,
    .dock_h = 64,
    .pad = 16,
    .row_h = 72,
};

const xr_theme_t *app_theme(void) { return &k_theme; }

void app_start(xr_shell_t *shell)
{
    memset(&g_app, 0, sizeof g_app);
    g_app.shell = shell;
    g_app.current = -1;
    g_app.font_idx = 1;
    g_app.full_refresh_every = 6;
    g_app.show_progress = true;
    g_app.wifi_connected = false;
    g_app.bluetooth_connected = false;
    g_app.sleep_timeout_minutes = 10;
    xr_shell_set_full_refresh_every(shell, (uint16_t)g_app.full_refresh_every);
    xr_shell_push(shell, app_page_splash());
}

void app_library_clear(void) { g_app.book_count = 0; g_app.current = -1; }

bool app_library_add(const char *title)
{
    return app_library_add_path(title, title);
}

bool app_library_add_path(const char *path, const char *title)
{
    if (!path || !title || g_app.book_count >= APP_LIBRARY_MAX_BOOKS) return false;
    int index = g_app.book_count++;
    app_book_display_title(g_app.book_titles[index], sizeof g_app.book_titles[index], title);
    g_app.books[index] = (app_book_t) {
        .title = g_app.book_titles[index], .author = "FatFS EPUB", .format = "EPUB",
        .epub_source = true,
    };
    snprintf(g_app.books[index].path, sizeof g_app.books[index].path, "%s", path);
    if (g_app.current < 0) g_app.current = 0;
    return true;
}

void app_set_epub_loader(app_epub_loader_fn loader) { s_epub_loader = loader; }

void app_register_storage(const char *root, app_dir_entry_fn list_dir, void *context)
{
    s_storage_list_dir = list_dir;
    s_storage_context = context;
    if (!root) {
        s_storage_root[0] = '\0';
        return;
    }
    snprintf(s_storage_root, sizeof s_storage_root, "%s", root);
    size_t length = strlen(s_storage_root);
    while (length > 1 && s_storage_root[length - 1] == '/') s_storage_root[--length] = '\0';
}

const char *app_storage_root(void) { return s_storage_root; }

bool app_storage_list(const char *path,
                      bool (*entry)(const char *name, bool directory, void *user),
                      void *user)
{
    return s_storage_list_dir && path && entry &&
           s_storage_list_dir(s_storage_context, path, entry, user);
}

bool app_open_storage_epub(const char *path, const char *title)
{
    if (!path || !title || !s_epub_loader) return false;
    int index = -1;
    for (int i = 0; i < g_app.book_count; ++i) {
        if (g_app.books[i].epub_source && strcmp(g_app.books[i].path, path) == 0) {
            index = i;
            break;
        }
    }
    if (index < 0 && g_app.book_count < APP_LIBRARY_MAX_BOOKS) {
        if (!app_library_add_path(path, title)) return false;
        index = g_app.book_count - 1;
    } else if (index < 0) {
        index = APP_LIBRARY_MAX_BOOKS;
        if (g_app.book_count <= index) g_app.book_count = index + 1;
        app_book_display_title(g_app.book_titles[index], sizeof g_app.book_titles[index], title);
        g_app.books[index] = (app_book_t) {
            g_app.book_titles[index], "Storage EPUB", "EPUB", "", 0, 0, 0,
            false, true, true
        };
        snprintf(g_app.books[index].path, sizeof g_app.books[index].path, "%s", path);
    }
    g_app.current = index;
    if (!s_epub_loader(path, g_app.books[index].title)) return false;
    xr_shell_push(g_app.shell, app_page_reader());
    return true;
}

typedef struct app_scan_context { const char *root; } app_scan_context_t;
static char s_scan_mount[256];
static char s_scan_queue[16][256];
static unsigned s_scan_head, s_scan_tail;
static bool scan_entry(const char *name, bool directory, void *user)
{
    app_scan_context_t *scan = user;
    char path[512];
    int written = snprintf(path, sizeof path, "%s/%s", scan->root, name);
    if (written < 0 || (size_t)written >= sizeof path) return true;
    if (directory) {
        size_t length = strlen(path);
        if (s_scan_tail < sizeof(s_scan_queue) / sizeof(s_scan_queue[0]) &&
            length < sizeof s_scan_queue[0]) {
            memcpy(s_scan_queue[s_scan_tail], path, length + 1);
            ++s_scan_tail;
        }
        return true;
    }
    size_t n = strlen(name);
    if ((n > 5 && strcasecmp(name + n - 5, ".epub") == 0) || (n > 4 && strcasecmp(name + n - 4, ".epu") == 0))
        app_library_add_path(path + strlen(s_scan_mount) +
                             (path[strlen(s_scan_mount)] == '/' ? 1 : 0), name);
    return true;
}
void app_scan_library(const char *root, app_dir_entry_fn list_dir, void *context)
{
    if (!root || !list_dir) return;
    app_library_clear();
    s_scan_head = s_scan_tail = 0;
    snprintf(s_scan_mount, sizeof s_scan_mount, "%s", root);
    snprintf(s_scan_queue[s_scan_tail++], sizeof s_scan_queue[0], "%s", root);
    while (s_scan_head < s_scan_tail) {
        app_scan_context_t scan = { s_scan_queue[s_scan_head++] };
        if (!list_dir(context, scan.root, scan_entry, &scan)) break;
    }
}

void app_open_book(int index)
{
    if (index < 0 || index >= g_app.book_count) return;
    g_app.current = index;
    if (g_app.books[index].epub_source) {
        if (!s_epub_loader || !s_epub_loader(g_app.books[index].path, g_app.books[index].title)) return;
    }
    xr_shell_push(g_app.shell, app_page_reader());
}

bool app_load_epub(const xr_storage_t *storage, const char *title)
{
    if (!storage || !title) return false;
    if (xr_epub_open(&s_epub, storage, s_epub_scratch, sizeof(s_epub_scratch),
                     s_epub_manifest, XR_ARRAY_LEN(s_epub_manifest),
                     s_epub_spine, XR_ARRAY_LEN(s_epub_spine)) != XR_EPUB_OK)
        return false;
    if (xr_epub_spine_text(&s_epub, 0, s_epub_scratch, sizeof(s_epub_scratch),
                           s_epub_text, sizeof(s_epub_text), NULL) != XR_EPUB_OK)
        return false;
    s_epub_cover_placeholder = !s_epub_text[0];
    if (s_epub_cover_placeholder) strcpy(s_epub_text, "Cover");
    int existing = (g_app.current >= 0 && g_app.current < g_app.book_count &&
                    g_app.books[g_app.current].epub_source) ? g_app.current : -1;
    if (existing < 0)
        for (int i = 0; i < g_app.book_count; ++i)
            if (strcmp(g_app.books[i].title, title) == 0) { existing = i; break; }
    if (existing < 0 && g_app.book_count >= APP_MAX_BOOKS) return false;
    int index = existing >= 0 ? existing : g_app.book_count++;
    app_book_t *book = &g_app.books[index];
    char path[sizeof book->path];
    bool transient = existing >= 0 && book->transient;
    if (existing >= 0) snprintf(path, sizeof path, "%s", book->path);
    const char *loaded_title = s_epub.title[0] ? s_epub.title : title;
    if (loaded_title != g_app.book_titles[index])
        snprintf(g_app.book_titles[index], sizeof g_app.book_titles[index], "%s", loaded_title);
    *book = (app_book_t) {
        .title = g_app.book_titles[index], .author = "Imported EPUB", .format = "EPUB",
        .epub_source = true,
    };
    book->transient = transient;
    snprintf(book->path, sizeof book->path, "%s", existing >= 0 ? path : title);
    g_app.current = index;
    g_app.epub_open = true;
    g_app.epub_spine = 0;
    return true;
}

const char *app_current_text(void)
{
    return g_app.epub_open && g_app.current >= 0 && g_app.books[g_app.current].epub_source
        ? s_epub_text : app_sample_text();
}

bool app_current_is_cover_placeholder(void)
{
    return g_app.epub_open && s_epub_cover_placeholder;
}

const char *app_current_chapter_title(void)
{
    const char *text = app_current_text();
    size_t n = 0;
    while (text[n] && text[n] != '\n' && n + 1 < sizeof(s_chapter_title)) ++n;
    memcpy(s_chapter_title, text, n);
    s_chapter_title[n] = '\0';
    return s_chapter_title[0] ? s_chapter_title : "Cover";
}

void app_set_reading_progress(int page, int total_pages)
{
    if (!g_app.epub_open || g_app.current < 0 || !g_app.books[g_app.current].epub_source || total_pages <= 0)
        return;
    uint32_t chapters = app_epub_chapter_count();
    uint32_t chapter_progress = (uint32_t)(page + 1) * 100u / (uint32_t)total_pages;
    uint32_t progress = ((uint32_t)g_app.epub_spine * 100u + chapter_progress) / chapters;
    if (progress == 0 && (page > 0 || g_app.epub_spine > 0)) progress = 1;
    g_app.books[g_app.current].progress = (uint8_t)XR_MIN(progress, 100u);
}

bool app_turn_epub_chapter(int direction)
{
    if (!g_app.epub_open || g_app.current < 0 || !g_app.books[g_app.current].epub_source) return false;
    int next = (int)g_app.epub_spine + direction;
    if (next < 0 || next >= s_epub.spine_count) return false;
    return app_load_epub_chapter((uint16_t)next);
}

bool app_load_epub_chapter(uint16_t chapter)
{
    if (!g_app.epub_open || chapter >= s_epub.spine_count) return false;
    if (xr_epub_spine_text(&s_epub, chapter, s_epub_scratch, sizeof(s_epub_scratch),
                           s_epub_text, sizeof(s_epub_text), NULL) != XR_EPUB_OK) return false;
    s_epub_cover_placeholder = !s_epub_text[0];
    if (s_epub_cover_placeholder) strcpy(s_epub_text, "Cover");
    g_app.epub_spine = chapter;
    return true;
}

uint16_t app_epub_chapter_index(void) { return g_app.epub_spine; }
uint16_t app_epub_chapter_count(void) { return g_app.epub_open ? s_epub.spine_count : 0; }

void app_delete_book(int index)
{
    if (index < 0 || index >= g_app.book_count) return;
    if (!g_app.books[index].transient && g_app.book_count == APP_MAX_BOOKS &&
        g_app.books[APP_LIBRARY_MAX_BOOKS].transient) {
        if (g_app.current == APP_LIBRARY_MAX_BOOKS) g_app.current = -1;
        g_app.book_count--;
    }
    memmove(&g_app.books[index], &g_app.books[index + 1],
            (size_t)(g_app.book_count - index - 1) * sizeof(app_book_t));
    g_app.book_count--;
    for (int i = index; i < g_app.book_count; ++i) {
        snprintf(g_app.book_titles[i], sizeof g_app.book_titles[i], "%s", g_app.books[i].title);
        g_app.books[i].title = g_app.book_titles[i];
    }
    if (g_app.current == index) g_app.current = g_app.book_count ? 0 : -1;
    else if (g_app.current > index) g_app.current--;
}

static xr_confirm_t s_confirm;

void app_confirm(const char *title, const char *message, const char *yes, xr_button_icon_fn icon,
                 xr_dialog_result_fn cb, void *user)
{
    xr_confirm_show(g_app.shell, &s_confirm, title, message, yes, "Cancel", cb, user);
    xr_dialog_set_icon(&s_confirm.base, icon);
}

void app_draw_progress(xr_canvas_t *c, xr_rect_t r, int percent)
{
    xr_canvas_draw_rect(c, r, 2, XR_BLACK);
    xr_rect_t in = xr_rect_inset(r, 3);
    xr_canvas_fill_rect(c, xr_rect(in.x, in.y, in.w * percent / 100, in.h), XR_DARK);
}

const char *app_sample_text(void)
{
    /* Pride and Prejudice, Chapter 1 (public domain). */
    return
        "Chapter 1\n"
        "\n"
        "It is a truth universally acknowledged, that a single man in possession of a good "
        "fortune, must be in want of a wife.\n"
        "    However little known the feelings or views of such a man may be on his first "
        "entering a neighbourhood, this truth is so well fixed in the minds of the surrounding "
        "families, that he is considered the rightful property of some one or other of their "
        "daughters.\n"
        "    \"My dear Mr. Bennet,\" said his lady to him one day, \"have you heard that "
        "Netherfield Park is let at last?\"\n"
        "    Mr. Bennet replied that he had not.\n"
        "    \"But it is,\" returned she; \"for Mrs. Long has just been here, and she told me "
        "all about it.\"\n"
        "    Mr. Bennet made no answer.\n"
        "    \"Do you not want to know who has taken it?\" cried his wife impatiently.\n"
        "    \"You want to tell me, and I have no objection to hearing it.\"\n"
        "    This was invitation enough.\n"
        "    \"Why, my dear, you must know, Mrs. Long says that Netherfield is taken by a young "
        "man of large fortune from the north of England; that he came down on Monday in a "
        "chaise and four to see the place, and was so much delighted with it, that he agreed "
        "with Mr. Morris immediately; that he is to take possession before Michaelmas, and "
        "some of his servants are to be in the house by the end of next week.\"\n"
        "    \"What is his name?\"\n"
        "    \"Bingley.\"\n"
        "    \"Is he married or single?\"\n"
        "    \"Oh! Single, my dear, to be sure! A single man of large fortune; four or five "
        "thousand a year. What a fine thing for our girls!\"\n"
        "    \"How so? How can it affect them?\"\n"
        "    \"My dear Mr. Bennet,\" replied his wife, \"how can you be so tiresome! You must "
        "know that I am thinking of his marrying one of them.\"\n"
        "    \"Is that his design in settling here?\"\n"
        "    \"Design! Nonsense, how can you talk so! But it is very likely that he may fall in "
        "love with one of them, and therefore you must visit him as soon as he comes.\"\n"
        "    \"I see no occasion for that. You and the girls may go, or you may send them by "
        "themselves, which perhaps will be still better, for as you are as handsome as any of "
        "them, Mr. Bingley may like you the best of the party.\"\n"
        "    \"My dear, you flatter me. I certainly have had my share of beauty, but I do not "
        "pretend to be anything extraordinary now. When a woman has five grown-up daughters, "
        "she ought to give over thinking of her own beauty.\"\n"
        "    \"In such cases, a woman has not often much beauty to think of.\"\n"
        "    \"But, my dear, you must indeed go and see Mr. Bingley when he comes into the "
        "neighbourhood.\"\n"
        "    \"It is more than I engage for, I assure you.\"\n"
        "    \"But consider your daughters. Only think what an establishment it would be for one "
        "of them. Sir William and Lady Lucas are determined to go, merely on that account, for "
        "in general, you know, they visit no newcomers. Indeed you must go, for it will be "
        "impossible for us to visit him if you do not.\"\n"
        "    \"You are over-scrupulous, surely. I dare say Mr. Bingley will be very glad to see "
        "you; and I will send a few lines by you to assure him of my hearty consent to his "
        "marrying whichever he chooses of the girls; though I must throw in a good word for "
        "my little Lizzy.\"\n"
        "    \"I desire you will do no such thing. Lizzy is not a bit better than the others; and "
        "I am sure she is not half so handsome as Jane, nor half so good-humoured as Lydia. But "
        "you are always giving her the preference.\"\n"
        "    \"They have none of them much to recommend them,\" replied he; \"they are all silly "
        "and ignorant like other girls; but Lizzy has something more of quickness than her "
        "sisters.\"\n"
        "    \"Mr. Bennet, how can you abuse your own children in such a way? You take delight in "
        "vexing me. You have no compassion for my poor nerves.\"\n"
        "    \"You mistake me, my dear. I have a high respect for your nerves. They are my old "
        "friends. I have heard you mention them with consideration these last twenty years at "
        "least.\"\n";
}
