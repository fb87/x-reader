#pragma once
#include <stdint.h>
#include "gfx/framebuffer.hpp"
#include "input/action.hpp"
#include "ui/layout/layout.hpp"
namespace xreader { namespace ui {
enum dialog_kind_t : uint8_t { dialog_info, dialog_warning, dialog_confirm };
enum dialog_result_t : uint8_t { dialog_result_none, dialog_result_redraw, dialog_result_accept, dialog_result_cancel };
struct dialog_state_t {
    dialog_kind_t kind;
    bool focus_accept;
    char title[32];
    char message[128];
    char accept_label[16];
    char cancel_label[16];
};
void dialog_begin(dialog_state_t* state, dialog_kind_t kind, const char* title, const char* message,
                  const char* accept_label = "OK", const char* cancel_label = "CANCEL");
void draw_dialog(gfx::framebuffer_t* framebuffer, const dialog_state_t* state);
dialog_result_t dialog_handle(dialog_state_t* state, const input::action_event_t* event,
                              layout::viewport_t viewport);
} }
