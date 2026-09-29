#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace xreader
{
namespace services
{
namespace debug_console
{

// Test-harness console on the ESP-IDF UART.  It injects synthetic input events
// into the same queue the GT911 touch and rotary drivers feed, so a host can
// drive the UI without touching the panel.  Compiled only when
// XREADER_DEBUG_CONSOLE is defined; it is not part of a release image.

// The active UI loop publishes its event queue here.  main.cpp owns more than
// one loop, and only the running one may receive injected events.  Passing
// nullptr detaches the console.
void set_event_queue(QueueHandle_t events);

// Publishes the live UI state so the `state` command can report which screen
// and which focused row the firmware believes it is on.  The pointer refers to
// run()'s stack, which outlives the console.
void set_ui_state(const void* screen_state, uint16_t width, uint16_t height);

esp_err_t start();

} // namespace debug_console
} // namespace services
} // namespace xreader
