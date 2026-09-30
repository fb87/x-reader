#include "keyboard.hpp"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#else
#include <chrono>
#endif
#include "gfx/font.hpp"
#include "ui/chrome.hpp"
namespace xreader
{
namespace ui
{
namespace
{
struct key_t
{
    const char* label;
    const char* chars;
};
static uint32_t now_ms()
{
#ifdef ESP_PLATFORM
    return static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
#else
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
#endif
}
static constexpr uint32_t t9_commit_ms = 900U;
static const key_t qwerty_rows[][10] = {
    {{"1", "1"},
     {"2", "2"},
     {"3", "3"},
     {"4", "4"},
     {"5", "5"},
     {"6", "6"},
     {"7", "7"},
     {"8", "8"},
     {"9", "9"},
     {"0", "0"}},
    {{"q", "q"},
     {"w", "w"},
     {"e", "e"},
     {"r", "r"},
     {"t", "t"},
     {"y", "y"},
     {"u", "u"},
     {"i", "i"},
     {"o", "o"},
     {"p", "p"}},
    {{"a", "a"},
     {"s", "s"},
     {"d", "d"},
     {"f", "f"},
     {"g", "g"},
     {"h", "h"},
     {"j", "j"},
     {"k", "k"},
     {"l", "l"},
     {nullptr, nullptr}},
    {{"z", "z"},
     {"x", "x"},
     {"c", "c"},
     {"v", "v"},
     {"b", "b"},
     {"n", "n"},
     {"m", "m"},
     {".", "."},
     {"-", "-"},
     {"_", "_"}},
};
static constexpr uint8_t q_counts[] = {10, 10, 9, 10, 7};
static const char* const q_actions[] = {"SHIFT", "SPACE", "BKSP", "SYM", "T9", "OK", "CANCEL"};
static const key_t sym_rows[][10] = {
    {{"!", "!"},
     {"@", "@"},
     {"#", "#"},
     {"$", "$"},
     {"%", "%"},
     {"^", "^"},
     {"&", "&"},
     {"*", "*"},
     {"(", "("},
     {")", ")"}},
    {{"[", "["},
     {"]", "]"},
     {"{", "{"},
     {"}", "}"},
     {"<", "<"},
     {">", ">"},
     {"=", "="},
     {"+", "+"},
     {"/", "/"},
     {"\\", "\\"}},
    {{":", ":"},
     {";", ";"},
     {"\"", "\""},
     {"'", "'"},
     {"?", "?"},
     {"~", "~"},
     {"`", "`"},
     {"|", "|"},
     {",", ","},
     {".", "."}},
    {{"-", "-"},
     {"_", "_"},
     {" ", " "},
     {nullptr, nullptr},
     {nullptr, nullptr},
     {nullptr, nullptr},
     {nullptr, nullptr},
     {nullptr, nullptr},
     {nullptr, nullptr},
     {nullptr, nullptr}},
};
static constexpr uint8_t s_counts[] = {10, 10, 10, 3, 7};
static const char* const s_actions[] = {"ABC", "LEFT", "RIGHT", "BKSP", "SHOW", "OK", "CANCEL"};
static const key_t t9_rows[][3] = {
    {{"1", "1.,!?@"}, {"2 ABC", "abc2"}, {"3 DEF", "def3"}},
    {{"4 GHI", "ghi4"}, {"5 JKL", "jkl5"}, {"6 MNO", "mno6"}},
    {{"7 PQRS", "pqrs7"}, {"8 TUV", "tuv8"}, {"9 WXYZ", "wxyz9"}},
    {{"*", "*-_"}, {"0 SPACE", " 0"}, {"#", "#/:"}},
};
static constexpr uint8_t t_counts[] = {3, 3, 3, 3, 5};
static const char* const t_actions[] = {"QWERTY", "BKSP", "SHOW", "OK", "CANCEL"};
uint8_t rows(const keyboard_state_t*)
{
    return 5U;
}
uint8_t cols(const keyboard_state_t* s, uint8_t r)
{
    if (r >= 5U)
        return 0;
    return s->mode == keyboard_qwerty    ? q_counts[r]
           : s->mode == keyboard_symbols ? s_counts[r]
                                         : t_counts[r];
}
layout::rect_t kb_area(layout::viewport_t v)
{
    const auto m = layout::metrics(v);
    auto a = layout::inset(layout::content(v), static_cast<uint16_t>(m.margin / 2U));
    const uint16_t f = static_cast<uint16_t>(m.row_height + m.gap);
    if (a.height > f)
    {
        a.y = static_cast<uint16_t>(a.y + f);
        a.height = static_cast<uint16_t>(a.height - f);
    }
    return a;
}
layout::rect_t text_area(layout::viewport_t v)
{
    const auto m = layout::metrics(v);
    auto a = layout::inset(layout::content(v), static_cast<uint16_t>(m.margin / 2U));
    a.height = m.row_height;
    return a;
}
layout::rect_t key_rect(layout::viewport_t v, const keyboard_state_t* s, uint8_t r, uint8_t c)
{
    const auto m = layout::metrics(v);
    const auto a = kb_area(v);
    const uint8_t rs = rows(s);
    const uint32_t gy = static_cast<uint32_t>(rs - 1U) * m.gap;
    const uint16_t kh = static_cast<uint16_t>((a.height > gy ? a.height - gy : 0U) / rs);
    const uint8_t cs = cols(s, r);
    if (cs == 0 || c >= cs)
        return {};
    const uint32_t gx = static_cast<uint32_t>(cs - 1U) * m.gap;
    const uint16_t kw = static_cast<uint16_t>((a.width > gx ? a.width - gx : 0U) / cs);
    return {static_cast<uint16_t>(a.x + c * (kw + m.gap)),
            static_cast<uint16_t>(a.y + r * (kh + m.gap)), kw, kh};
}
void commit(keyboard_state_t* s)
{
    s->t9_pending = false;
    s->t9_last_ms = 0U;
}
void insert_char(keyboard_state_t* s, char c)
{
    const size_t len = strlen(s->text);
    if (len + 1U >= sizeof(s->text))
        return;
    size_t cur = s->cursor;
    if (cur > len)
        cur = len;
    memmove(s->text + cur + 1U, s->text + cur, len - cur + 1U);
    s->text[cur] = c;
    s->cursor = static_cast<uint8_t>(cur + 1U);
}
void backspace(keyboard_state_t* s)
{
    const size_t len = strlen(s->text);
    size_t cur = s->cursor;
    if (cur == 0 || len == 0)
        return;
    if (cur > len)
        cur = len;
    memmove(s->text + cur - 1U, s->text + cur, len - cur + 1U);
    s->cursor = static_cast<uint8_t>(cur - 1U);
    commit(s);
}
void cursor_left(keyboard_state_t* s)
{
    if (s->cursor > 0)
        s->cursor--;
    commit(s);
}
void cursor_right(keyboard_state_t* s)
{
    if (s->cursor < strlen(s->text))
        s->cursor++;
    commit(s);
}
keyboard_result_t activate_q(keyboard_state_t* s)
{
    if (s->focus_row < 4U)
    {
        const auto& k = qwerty_rows[s->focus_row][s->focus_col];
        if (!k.chars || !k.chars[0])
            return keyboard_result_none;
        char v = k.chars[0];
        if (s->shifted && isalpha(static_cast<unsigned char>(v)))
            v = static_cast<char>(toupper(static_cast<unsigned char>(v)));
        insert_char(s, v);
        return keyboard_result_redraw;
    }
    switch (s->focus_col)
    {
    case 0:
        s->shifted = !s->shifted;
        return keyboard_result_redraw;
    case 1:
        insert_char(s, ' ');
        return keyboard_result_redraw;
    case 2:
        backspace(s);
        return keyboard_result_redraw;
    case 3:
        s->mode = keyboard_symbols;
        s->focus_row = s->focus_col = 0;
        return keyboard_result_redraw;
    case 4:
        s->mode = keyboard_t9;
        s->focus_row = s->focus_col = 0;
        commit(s);
        return keyboard_result_redraw;
    case 5:
        return keyboard_result_submit;
    case 6:
        return keyboard_result_cancel;
    default:
        return keyboard_result_none;
    }
}
keyboard_result_t activate_sym(keyboard_state_t* s)
{
    if (s->focus_row < 4U)
    {
        const auto& k = sym_rows[s->focus_row][s->focus_col];
        if (k.chars && k.chars[0])
            insert_char(s, k.chars[0]);
        return keyboard_result_redraw;
    }
    switch (s->focus_col)
    {
    case 0:
        s->mode = keyboard_qwerty;
        s->focus_row = s->focus_col = 0;
        return keyboard_result_redraw;
    case 1:
        cursor_left(s);
        return keyboard_result_redraw;
    case 2:
        cursor_right(s);
        return keyboard_result_redraw;
    case 3:
        backspace(s);
        return keyboard_result_redraw;
    case 4:
        s->reveal = !s->reveal;
        return keyboard_result_redraw;
    case 5:
        return keyboard_result_submit;
    case 6:
        return keyboard_result_cancel;
    default:
        return keyboard_result_none;
    }
}
keyboard_result_t activate_t(keyboard_state_t* s)
{
    if (s->focus_row < 4U)
    {
        const uint8_t g = static_cast<uint8_t>(s->focus_row * 3U + s->focus_col);
        const char* ch = t9_rows[s->focus_row][s->focus_col].chars;
        const size_t n = strlen(ch);
        if (!n)
            return keyboard_result_none;
        const uint32_t now = now_ms();
        const bool cycle = s->t9_pending && s->t9_group == g && s->cursor > 0 &&
                           static_cast<uint32_t>(now - s->t9_last_ms) <= t9_commit_ms;
        if (cycle)
        {
            s->t9_index = static_cast<uint8_t>((s->t9_index + 1U) % n);
            char v = ch[s->t9_index];
            if (s->shifted && isalpha(static_cast<unsigned char>(v)))
                v = static_cast<char>(toupper(static_cast<unsigned char>(v)));
            s->text[s->cursor - 1U] = v;
        }
        else
        {
            commit(s);
            s->t9_group = g;
            s->t9_index = 0;
            char v = ch[0];
            if (s->shifted && isalpha(static_cast<unsigned char>(v)))
                v = static_cast<char>(toupper(static_cast<unsigned char>(v)));
            insert_char(s, v);
            s->t9_pending = true;
        }
        s->t9_last_ms = now;
        return keyboard_result_redraw;
    }
    commit(s);
    switch (s->focus_col)
    {
    case 0:
        s->mode = keyboard_qwerty;
        s->focus_row = s->focus_col = 0;
        return keyboard_result_redraw;
    case 1:
        backspace(s);
        return keyboard_result_redraw;
    case 2:
        s->reveal = !s->reveal;
        return keyboard_result_redraw;
    case 3:
        return keyboard_result_submit;
    case 4:
        return keyboard_result_cancel;
    default:
        return keyboard_result_none;
    }
}
void move_focus(keyboard_state_t* s, input::action_t a)
{
    if (s->mode == keyboard_t9)
        commit(s);
    if (a == input::action_left)
    {
        const uint8_t c = cols(s, s->focus_row);
        s->focus_col = s->focus_col == 0 ? static_cast<uint8_t>(c - 1U)
                                         : static_cast<uint8_t>(s->focus_col - 1U);
    }
    else if (a == input::action_right)
    {
        const uint8_t c = cols(s, s->focus_row);
        s->focus_col = static_cast<uint8_t>((s->focus_col + 1U) % c);
    }
    else if (a == input::action_up || a == input::action_down)
    {
        if (a == input::action_up)
            s->focus_row = s->focus_row == 0 ? 4U : static_cast<uint8_t>(s->focus_row - 1U);
        else
            s->focus_row = static_cast<uint8_t>((s->focus_row + 1U) % 5U);
        const uint8_t c = cols(s, s->focus_row);
        if (s->focus_col >= c)
            s->focus_col = static_cast<uint8_t>(c - 1U);
    }
}
const char* label(const keyboard_state_t* s, uint8_t r, uint8_t c, char* scratch, size_t cap)
{
    if (s->mode == keyboard_qwerty)
    {
        if (r == 4U)
            return q_actions[c];
        const char* l = qwerty_rows[r][c].label;
        if (s->shifted && l && l[1] == '\0' && isalpha(static_cast<unsigned char>(l[0])))
        {
            snprintf(scratch, cap, "%c", toupper(static_cast<unsigned char>(l[0])));
            return scratch;
        }
        return l;
    }
    if (s->mode == keyboard_symbols)
        return r == 4U ? s_actions[c] : sym_rows[r][c].label;
    return r == 4U ? t_actions[c] : t9_rows[r][c].label;
}
keyboard_result_t activate(keyboard_state_t* s)
{
    return s->mode == keyboard_qwerty    ? activate_q(s)
           : s->mode == keyboard_symbols ? activate_sym(s)
                                         : activate_t(s);
}
} // namespace
void keyboard_begin(keyboard_state_t* s, keyboard_purpose_t p, const char* t, const char* i,
                    bool masked, keyboard_mode_t m)
{
    if (!s)
        return;
    *s = {};
    s->mode = m;
    s->purpose = p;
    s->masked = masked;
    snprintf(s->title, sizeof(s->title), "%s", t ? t : "INPUT");
    snprintf(s->text, sizeof(s->text), "%s", i ? i : "");
    s->cursor = static_cast<uint8_t>(strlen(s->text));
}
void draw_keyboard(gfx::framebuffer_t* fb, const keyboard_state_t* s)
{
    if (!fb || !s)
        return;
    const layout::viewport_t vp = {fb->width, fb->height};
    gfx::clear(fb, 0x0f);
    chrome::draw_status_bar(fb, s->title);
    const auto f = text_area(vp);
    gfx::draw_rect(fb, f.x, f.y, f.width, f.height, 0x04);
    char d[100] = {};
    if (s->masked && !s->reveal)
    {
        const size_t n = strlen(s->text);
        memset(d, '*', n < sizeof(d) - 1U ? n : sizeof(d) - 1U);
    }
    else
        snprintf(d, sizeof(d), "%s", s->text);
    gfx::draw_text(fb, static_cast<uint16_t>(f.x + 10U),
                   static_cast<uint16_t>(f.y + (f.height > 16U ? (f.height - 16U) / 2U : 0U)), d, 1,
                   0x00);
    for (uint8_t r = 0; r < rows(s); ++r)
        for (uint8_t c = 0; c < cols(s, r); ++c)
        {
            const auto k = key_rect(vp, s, r, c);
            const bool sel = r == s->focus_row && c == s->focus_col;
            gfx::fill_rect(fb, k.x, k.y, k.width, k.height, sel ? 0x00 : 0x0f);
            gfx::draw_rect(fb, k.x, k.y, k.width, k.height, sel ? 0x00 : 0x08);
            char sc[4] = {};
            const char* l = label(s, r, c, sc, sizeof(sc));
            if (!l)
                continue;
            const uint16_t w = gfx::measure_text(l, 1);
            gfx::draw_text(
                fb, k.width > w ? static_cast<uint16_t>(k.x + (k.width - w) / 2U) : k.x,
                static_cast<uint16_t>(k.y + (k.height > 16U ? (k.height - 16U) / 2U : 0U)), l, 1,
                sel ? 0x0f : 0x00);
        }
    chrome::draw_indication_bar(fb, {"Move", gfx::icon_list}, {"Type", gfx::icon_check},
                                {"Back", gfx::icon_arrow_back});
}
keyboard_result_t keyboard_handle(keyboard_state_t* s, const input::action_event_t* e,
                                  layout::viewport_t vp)
{
    if (!s || !e)
        return keyboard_result_none;
    if (e->action == input::action_back)
        return keyboard_result_cancel;
    if (e->action == input::action_menu && s->masked)
    {
        s->reveal = !s->reveal;
        return keyboard_result_redraw;
    }
    if (e->action == input::action_up || e->action == input::action_down ||
        e->action == input::action_left || e->action == input::action_right)
    {
        move_focus(s, e->action);
        return keyboard_result_redraw;
    }
    if (e->action == input::action_pointer)
    {
        for (uint8_t r = 0; r < rows(s); ++r)
            for (uint8_t c = 0; c < cols(s, r); ++c)
                if (layout::contains(key_rect(vp, s, r, c), e->x, e->y))
                {
                    s->focus_row = r;
                    s->focus_col = c;
                    return activate(s);
                }
        return keyboard_result_none;
    }
    return e->action == input::action_select ? activate(s) : keyboard_result_none;
}
} // namespace ui
} // namespace xreader
