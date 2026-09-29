#include "keyboard.hpp"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

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
static constexpr uint8_t qwerty_counts[] = {10, 10, 9, 10, 6};
static const char* const action_labels[] = {"SHIFT", "SPACE", "BKSP", "T9", "OK", "CANCEL"};

static const key_t t9_rows[][3] = {
    {{"1", "1.,!?@"}, {"2 ABC", "abc2"}, {"3 DEF", "def3"}},
    {{"4 GHI", "ghi4"}, {"5 JKL", "jkl5"}, {"6 MNO", "mno6"}},
    {{"7 PQRS", "pqrs7"}, {"8 TUV", "tuv8"}, {"9 WXYZ", "wxyz9"}},
    {{"*", "*-_"}, {"0 SPACE", " 0"}, {"#", "#/:"}},
};
static constexpr uint8_t t9_counts[] = {3, 3, 3, 3, 4};
static const char* const t9_action_labels[] = {"QWERTY", "BKSP", "OK", "CANCEL"};

static uint8_t row_count(const keyboard_state_t* state)
{
    return state->mode == keyboard_qwerty ? 5U : 5U;
}

static uint8_t column_count(const keyboard_state_t* state, uint8_t row)
{
    if (state->mode == keyboard_qwerty)
        return row < 5U ? qwerty_counts[row] : 0U;
    return row < 5U ? t9_counts[row] : 0U;
}

static layout::rect_t keyboard_area(layout::viewport_t viewport)
{
    const layout::metrics_t metrics = layout::metrics(viewport);
    layout::rect_t area = layout::content(viewport);
    area = layout::inset(area, static_cast<uint16_t>(metrics.margin / 2U));
    // Leave a text field at the top of the content area.
    const uint16_t field = static_cast<uint16_t>(metrics.row_height + metrics.gap);
    if (area.height > field)
    {
        area.y = static_cast<uint16_t>(area.y + field);
        area.height = static_cast<uint16_t>(area.height - field);
    }
    return area;
}

static layout::rect_t text_area(layout::viewport_t viewport)
{
    const layout::metrics_t metrics = layout::metrics(viewport);
    layout::rect_t area =
        layout::inset(layout::content(viewport), static_cast<uint16_t>(metrics.margin / 2U));
    area.height = metrics.row_height;
    return area;
}

static layout::rect_t key_rect(layout::viewport_t viewport, const keyboard_state_t* state,
                               uint8_t row, uint8_t col)
{
    const layout::metrics_t metrics = layout::metrics(viewport);
    const layout::rect_t area = keyboard_area(viewport);
    const uint8_t rows = row_count(state);
    const uint16_t row_gap = metrics.gap;
    const uint32_t gaps_y = static_cast<uint32_t>(rows - 1U) * row_gap;
    const uint16_t key_h =
        static_cast<uint16_t>((area.height > gaps_y ? area.height - gaps_y : 0U) / rows);
    const uint8_t cols = column_count(state, row);
    if (cols == 0 || col >= cols)
        return {};
    const uint16_t col_gap = metrics.gap;
    const uint32_t gaps_x = static_cast<uint32_t>(cols - 1U) * col_gap;
    const uint16_t key_w =
        static_cast<uint16_t>((area.width > gaps_x ? area.width - gaps_x : 0U) / cols);
    return {static_cast<uint16_t>(area.x + col * (key_w + col_gap)),
            static_cast<uint16_t>(area.y + row * (key_h + row_gap)), key_w, key_h};
}

static void append_char(keyboard_state_t* state, char c)
{
    const size_t length = strlen(state->text);
    if (length + 1U >= sizeof(state->text))
        return;
    state->text[length] = c;
    state->text[length + 1U] = '\0';
}

static void backspace(keyboard_state_t* state)
{
    const size_t length = strlen(state->text);
    if (length == 0)
        return;
    state->text[length - 1U] = '\0';
    state->t9_pending = false;
}

static void commit_pending(keyboard_state_t* state)
{
    state->t9_pending = false;
}

static keyboard_result_t activate_qwerty(keyboard_state_t* state)
{
    if (state->focus_row < 4U)
    {
        const key_t& key = qwerty_rows[state->focus_row][state->focus_col];
        if (key.chars == nullptr || key.chars[0] == '\0')
            return keyboard_result_none;
        char value = key.chars[0];
        if (state->shifted && isalpha(static_cast<unsigned char>(value)) != 0)
            value = static_cast<char>(toupper(static_cast<unsigned char>(value)));
        append_char(state, value);
        return keyboard_result_redraw;
    }

    switch (state->focus_col)
    {
    case 0:
        state->shifted = !state->shifted;
        return keyboard_result_redraw;
    case 1:
        append_char(state, ' ');
        return keyboard_result_redraw;
    case 2:
        backspace(state);
        return keyboard_result_redraw;
    case 3:
        state->mode = keyboard_t9;
        state->focus_row = 0;
        state->focus_col = 0;
        state->t9_pending = false;
        return keyboard_result_redraw;
    case 4:
        return keyboard_result_submit;
    case 5:
        return keyboard_result_cancel;
    default:
        return keyboard_result_none;
    }
}

static keyboard_result_t activate_t9(keyboard_state_t* state)
{
    if (state->focus_row < 4U)
    {
        const uint8_t group = static_cast<uint8_t>(state->focus_row * 3U + state->focus_col);
        const char* chars = t9_rows[state->focus_row][state->focus_col].chars;
        const size_t count = strlen(chars);
        if (count == 0)
            return keyboard_result_none;
        if (state->t9_pending && state->t9_group == group && state->text[0] != '\0')
        {
            state->t9_index = static_cast<uint8_t>((state->t9_index + 1U) % count);
            const size_t length = strlen(state->text);
            char value = chars[state->t9_index];
            if (state->shifted && isalpha(static_cast<unsigned char>(value)) != 0)
                value = static_cast<char>(toupper(static_cast<unsigned char>(value)));
            state->text[length - 1U] = value;
        }
        else
        {
            commit_pending(state);
            state->t9_group = group;
            state->t9_index = 0;
            char value = chars[0];
            if (state->shifted && isalpha(static_cast<unsigned char>(value)) != 0)
                value = static_cast<char>(toupper(static_cast<unsigned char>(value)));
            append_char(state, value);
            state->t9_pending = true;
        }
        return keyboard_result_redraw;
    }

    commit_pending(state);
    switch (state->focus_col)
    {
    case 0:
        state->mode = keyboard_qwerty;
        state->focus_row = 0;
        state->focus_col = 0;
        return keyboard_result_redraw;
    case 1:
        backspace(state);
        return keyboard_result_redraw;
    case 2:
        return keyboard_result_submit;
    case 3:
        return keyboard_result_cancel;
    default:
        return keyboard_result_none;
    }
}

static void move_focus(keyboard_state_t* state, input::action_t action)
{
    if (state->mode == keyboard_t9)
        commit_pending(state);
    const uint8_t rows = row_count(state);
    if (action == input::action_left)
    {
        const uint8_t cols = column_count(state, state->focus_row);
        state->focus_col = state->focus_col == 0 ? static_cast<uint8_t>(cols - 1U)
                                                 : static_cast<uint8_t>(state->focus_col - 1U);
    }
    else if (action == input::action_right)
    {
        const uint8_t cols = column_count(state, state->focus_row);
        state->focus_col = static_cast<uint8_t>((state->focus_col + 1U) % cols);
    }
    else if (action == input::action_up || action == input::action_down)
    {
        if (action == input::action_up)
            state->focus_row = state->focus_row == 0 ? static_cast<uint8_t>(rows - 1U)
                                                     : static_cast<uint8_t>(state->focus_row - 1U);
        else
            state->focus_row = static_cast<uint8_t>((state->focus_row + 1U) % rows);
        const uint8_t cols = column_count(state, state->focus_row);
        if (state->focus_col >= cols)
            state->focus_col = static_cast<uint8_t>(cols - 1U);
    }
}

static const char* label_for(const keyboard_state_t* state, uint8_t row, uint8_t col, char* scratch,
                             size_t scratch_size)
{
    if (state->mode == keyboard_qwerty)
    {
        if (row == 4U)
            return action_labels[col];
        const char* label = qwerty_rows[row][col].label;
        if (state->shifted && label != nullptr && label[1] == '\0' &&
            isalpha(static_cast<unsigned char>(label[0])) != 0)
        {
            snprintf(scratch, scratch_size, "%c", toupper(static_cast<unsigned char>(label[0])));
            return scratch;
        }
        return label;
    }
    return row == 4U ? t9_action_labels[col] : t9_rows[row][col].label;
}
} // namespace

void keyboard_begin(keyboard_state_t* state, keyboard_purpose_t purpose, const char* title,
                    const char* initial, bool masked, keyboard_mode_t mode)
{
    if (state == nullptr)
        return;
    *state = {};
    state->mode = mode;
    state->purpose = purpose;
    state->masked = masked;
    snprintf(state->title, sizeof(state->title), "%s", title == nullptr ? "INPUT" : title);
    snprintf(state->text, sizeof(state->text), "%s", initial == nullptr ? "" : initial);
}

void draw_keyboard(gfx::framebuffer_t* framebuffer, const keyboard_state_t* state)
{
    if (framebuffer == nullptr || state == nullptr)
        return;
    const layout::viewport_t viewport = {framebuffer->width, framebuffer->height};
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, state->title,
                            state->mode == keyboard_qwerty ? "QWERTY" : "T9");

    const layout::rect_t field = text_area(viewport);
    gfx::draw_rect(framebuffer, field.x, field.y, field.width, field.height, 0x04);
    char display[100] = {};
    if (state->masked)
    {
        const size_t length = strlen(state->text);
        const size_t count = length < sizeof(display) - 1U ? length : sizeof(display) - 1U;
        memset(display, '*', count);
        display[count] = '\0';
    }
    else
        snprintf(display, sizeof(display), "%s", state->text);
    gfx::draw_text(
        framebuffer, static_cast<uint16_t>(field.x + 10U),
        static_cast<uint16_t>(field.y + (field.height > 16U ? (field.height - 16U) / 2U : 0U)),
        display, 1, 0x00);

    const uint8_t rows = row_count(state);
    for (uint8_t row = 0; row < rows; ++row)
    {
        const uint8_t cols = column_count(state, row);
        for (uint8_t col = 0; col < cols; ++col)
        {
            const layout::rect_t rect = key_rect(viewport, state, row, col);
            const bool selected = row == state->focus_row && col == state->focus_col;
            gfx::fill_rect(framebuffer, rect.x, rect.y, rect.width, rect.height,
                           selected ? 0x00 : 0x0f);
            gfx::draw_rect(framebuffer, rect.x, rect.y, rect.width, rect.height,
                           selected ? 0x00 : 0x08);
            char scratch[4] = {};
            const char* label = label_for(state, row, col, scratch, sizeof(scratch));
            if (label == nullptr)
                continue;
            const uint16_t width = gfx::measure_text(label, 1);
            const uint16_t x = rect.width > width
                                   ? static_cast<uint16_t>(rect.x + (rect.width - width) / 2U)
                                   : rect.x;
            const uint16_t y =
                static_cast<uint16_t>(rect.y + (rect.height > 16U ? (rect.height - 16U) / 2U : 0U));
            gfx::draw_text(framebuffer, x, y, label, 1, selected ? 0x0f : 0x00);
        }
    }
    chrome::draw_indication_bar(framebuffer, "MOVE", "TYPE", "BACK");
}

keyboard_result_t keyboard_handle(keyboard_state_t* state, const input::action_event_t* event,
                                  layout::viewport_t viewport)
{
    if (state == nullptr || event == nullptr)
        return keyboard_result_none;
    if (event->action == input::action_back)
        return keyboard_result_cancel;
    if (event->action == input::action_up || event->action == input::action_down ||
        event->action == input::action_left || event->action == input::action_right)
    {
        move_focus(state, event->action);
        return keyboard_result_redraw;
    }
    if (event->action == input::action_pointer)
    {
        const uint8_t rows = row_count(state);
        for (uint8_t row = 0; row < rows; ++row)
        {
            const uint8_t cols = column_count(state, row);
            for (uint8_t col = 0; col < cols; ++col)
            {
                if (layout::contains(key_rect(viewport, state, row, col), event->x, event->y))
                {
                    state->focus_row = row;
                    state->focus_col = col;
                    return state->mode == keyboard_qwerty ? activate_qwerty(state)
                                                          : activate_t9(state);
                }
            }
        }
        return keyboard_result_none;
    }
    if (event->action != input::action_select)
        return keyboard_result_none;
    return state->mode == keyboard_qwerty ? activate_qwerty(state) : activate_t9(state);
}

} // namespace ui
} // namespace xreader
