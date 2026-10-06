#pragma once

#include "core/display.hpp"
#include "core/event.hpp"
#include "core/input.hpp"
#include "core/platform.hpp"

namespace shell {

/** @brief Callback used by the shell to deliver one input event to the application. */
using event_fn = bool (*)(const event::value& event, void* user);

/** @brief Minimal shell context shared by native and scripted applications. */
struct context {
    display::device* display = nullptr;
    input::device* input = nullptr;
    platform::device* platform = nullptr;
    event_fn on_shell_event = nullptr;
    void* shell_user = nullptr;
    event_fn on_event = nullptr;
    void* user = nullptr;
};

/** @brief Initializes a shell context with board-independent interfaces. */
inline void init(context& self, display::device& display, input::device& input,
                 platform::device& platform, event_fn on_event, void* user,
                 event_fn on_shell_event = nullptr, void* shell_user = nullptr)
{
    self.display = &display;
    self.input = &input;
    self.platform = &platform;
    self.on_shell_event = on_shell_event;
    self.shell_user = shell_user;
    self.on_event = on_event;
    self.user = user;
}

/** @brief Dispatches one event to the active application. */
inline bool dispatch(context& self, const event::value& value)
{
    if (self.on_shell_event != nullptr && self.on_shell_event(value, self.shell_user)) return true;
    return self.on_event != nullptr && self.on_event(value, self.user);
}

/** @brief Polls and dispatches all currently queued input events. */
inline void pump(context& self)
{
    event::value value{};
    while (self.input != nullptr && input::poll(*self.input, value)) {
        dispatch(self, value);
    }
}

} // namespace shell
