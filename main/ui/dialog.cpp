#include "dialog.hpp"

#include <stdio.h>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"

namespace xreader
{
namespace ui
{
namespace
{
layout::rect_t panel(layout::viewport_t viewport)
{
    const auto metrics = layout::metrics(viewport);
    const uint16_t width = static_cast<uint16_t>(viewport.width > metrics.margin * 4U
                                                     ? viewport.width - metrics.margin * 4U
                                                     : viewport.width);
    const uint16_t height = static_cast<uint16_t>(viewport.height / 2U);
    return {static_cast<uint16_t>((viewport.width - width) / 2U),
            static_cast<uint16_t>((viewport.height - height) / 2U), width, height};
}

layout::rect_t button_rect(layout::viewport_t viewport, bool accept)
{
    const auto bounds = panel(viewport);
    const uint16_t gap = 12;
    const uint16_t width = static_cast<uint16_t>((bounds.width - gap - 24U) / 2U);
    return {static_cast<uint16_t>(bounds.x + 12U + (accept ? 0U : width + gap)),
            static_cast<uint16_t>(bounds.y + bounds.height - 56U), width, 42};
}
} // namespace

void dialog_begin(dialog_state_t* state, dialog_kind_t kind, const char* title, const char* message,
                  const char* accept_label, const char* cancel_label)
{
    if (state == nullptr)
        return;
    *state = {};
    state->kind = kind;
    state->focus_accept = true;
    snprintf(state->title, sizeof(state->title), "%s", title == nullptr ? "MESSAGE" : title);
    snprintf(state->message, sizeof(state->message), "%s", message == nullptr ? "" : message);
    snprintf(state->accept_label, sizeof(state->accept_label), "%s",
             accept_label == nullptr ? "OK" : accept_label);
    snprintf(state->cancel_label, sizeof(state->cancel_label), "%s",
             cancel_label == nullptr ? "CANCEL" : cancel_label);
}

void draw_dialog(gfx::framebuffer_t* framebuffer, const dialog_state_t* state)
{
    if (framebuffer == nullptr || state == nullptr)
        return;
    const layout::viewport_t viewport = {framebuffer->width, framebuffer->height};
    gfx::clear(framebuffer, 0x0f);
    chrome::draw_status_bar(framebuffer, state->title);
    const auto bounds = panel(viewport);
    gfx::draw_rect(framebuffer, bounds.x, bounds.y, bounds.width, bounds.height, 0x04);
    gfx::draw_text(framebuffer, static_cast<uint16_t>(bounds.x + 16U),
                   static_cast<uint16_t>(bounds.y + 28U), state->message, 1, 0x00);

    const auto accept = button_rect(viewport, true);
    const auto cancel = button_rect(viewport, false);
    const bool show_cancel = state->kind == dialog_confirm;
    gfx::fill_rect(framebuffer, accept.x, accept.y, accept.width, accept.height,
                   state->focus_accept ? 0x00 : 0x0f);
    gfx::draw_rect(framebuffer, accept.x, accept.y, accept.width, accept.height, 0x00);
    gfx::draw_text(framebuffer, static_cast<uint16_t>(accept.x + 10U),
                   static_cast<uint16_t>(accept.y + 13U), state->accept_label, 1,
                   state->focus_accept ? 0x0f : 0x00);
    if (show_cancel)
    {
        gfx::fill_rect(framebuffer, cancel.x, cancel.y, cancel.width, cancel.height,
                       state->focus_accept ? 0x0f : 0x00);
        gfx::draw_rect(framebuffer, cancel.x, cancel.y, cancel.width, cancel.height, 0x00);
        gfx::draw_text(framebuffer, static_cast<uint16_t>(cancel.x + 10U),
                       static_cast<uint16_t>(cancel.y + 13U), state->cancel_label, 1,
                       state->focus_accept ? 0x00 : 0x0f);
    }
    chrome::draw_indication_bar(framebuffer, {"Move", gfx::icon_swap_horiz},
                                {"Select", gfx::icon_check}, {"Back", gfx::icon_arrow_back});
}

dialog_result_t dialog_handle(dialog_state_t* state, const input::action_event_t* event,
                              layout::viewport_t viewport)
{
    if (state == nullptr || event == nullptr)
        return dialog_result_none;
    if (event->action == input::action_back)
        return dialog_result_cancel;
    if (state->kind == dialog_confirm &&
        (event->action == input::action_left || event->action == input::action_right))
    {
        state->focus_accept = !state->focus_accept;
        return dialog_result_redraw;
    }
    if (event->action == input::action_pointer)
    {
        if (layout::contains(button_rect(viewport, true), event->x, event->y))
            return dialog_result_accept;
        if (state->kind == dialog_confirm &&
            layout::contains(button_rect(viewport, false), event->x, event->y))
            return dialog_result_cancel;
        return dialog_result_none;
    }
    if (event->action == input::action_select)
        return state->focus_accept ? dialog_result_accept : dialog_result_cancel;
    return dialog_result_none;
}

} // namespace ui
} // namespace xreader
