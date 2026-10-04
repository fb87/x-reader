#include "app_internal.h"

#include <string.h>

#define APP_EPUB_MANIFEST_MAX 640
#define APP_EPUB_SPINE_MAX 640
#define APP_EPUB_SCRATCH_SIZE (80 * 1024)
#define APP_EPUB_TEXT_SIZE (64 * 1024)

app_t g_app;
static xr_epub_t s_epub;
static xr_epub_manifest_item_t s_epub_manifest[APP_EPUB_MANIFEST_MAX];
static xr_epub_spine_item_t s_epub_spine[APP_EPUB_SPINE_MAX];
static uint8_t s_epub_scratch[APP_EPUB_SCRATCH_SIZE];
static char s_epub_text[APP_EPUB_TEXT_SIZE];
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

/* Demo catalogue (public-domain titles). A real build scans the SD card. */
static const app_book_t k_books[] = {
    { "Pride and Prejudice", "Jane Austen", "EPUB", 432, 37, 712, false, false },
    { "Moby-Dick; or, The Whale", "Herman Melville", "EPUB", 720, 5, 1290, false, false },
    { "The Adventures of Sherlock Holmes", "Arthur Conan Doyle", "EPUB", 307, 0, 540, false, false },
    { "Frankenstein", "Mary Shelley", "EPUB", 280, 100, 460, false, false },
    { "Alice's Adventures in Wonderland", "Lewis Carroll", "EPUB", 120, 64, 210, false, false },
    { "Dracula", "Bram Stoker", "EPUB", 488, 12, 880, false, false },
    { "The Time Machine", "H. G. Wells", "TXT", 104, 0, 180, false, false },
    { "Little Women", "Louisa May Alcott", "EPUB", 560, 0, 950, false, false },
    { "War and Peace", "Leo Tolstoy", "EPUB", 1392, 2, 3410, false, false },
    { "The Odyssey", "Homer", "PDF", 416, 0, 2200, false, false },
    { "Walden", "Henry David Thoreau", "EPUB", 352, 48, 600, false, false },
    { "Jane Eyre", "Charlotte Bronte", "EPUB", 532, 0, 890, false, false },
};

void app_start(xr_shell_t *shell)
{
    memset(&g_app, 0, sizeof g_app);
    g_app.shell = shell;
    g_app.book_count = (int)XR_ARRAY_LEN(k_books);
    memcpy(g_app.books, k_books, sizeof k_books);
    g_app.current = 0;
    g_app.font_idx = 1;
    g_app.full_refresh_every = 6;
    g_app.show_progress = true;
    g_app.wifi_connected = false;
    g_app.bluetooth_connected = false;
    xr_shell_set_full_refresh_every(shell, (uint16_t)g_app.full_refresh_every);
    xr_shell_push(shell, app_page_splash());
}

void app_open_book(int index)
{
    if (index < 0 || index >= g_app.book_count) return;
    g_app.current = index;
    xr_shell_push(g_app.shell, app_page_reader());
}

bool app_load_epub(const xr_storage_t *storage, const char *title)
{
    if (!storage || !title || g_app.book_count >= APP_MAX_BOOKS) return false;
    if (xr_epub_open(&s_epub, storage, s_epub_scratch, sizeof(s_epub_scratch),
                     s_epub_manifest, XR_ARRAY_LEN(s_epub_manifest),
                     s_epub_spine, XR_ARRAY_LEN(s_epub_spine)) != XR_EPUB_OK)
        return false;
    if (xr_epub_spine_text(&s_epub, 0, s_epub_scratch, sizeof(s_epub_scratch),
                           s_epub_text, sizeof(s_epub_text), NULL) != XR_EPUB_OK)
        return false;
    s_epub_cover_placeholder = !s_epub_text[0];
    if (s_epub_cover_placeholder) strcpy(s_epub_text, "Cover");
    app_book_t *book = &g_app.books[g_app.book_count++];
    *book = (app_book_t) { s_epub.title[0] ? s_epub.title : title, "Imported EPUB", "EPUB", 0, 0, 0, false, true };
    g_app.current = g_app.book_count - 1;
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
    memmove(&g_app.books[index], &g_app.books[index + 1],
            (size_t)(g_app.book_count - index - 1) * sizeof(app_book_t));
    g_app.book_count--;
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
