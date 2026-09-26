#pragma once

#include "drivers/gt911/gt911.hpp"
#include "drivers/it8951e/it8951e.hpp"
#include "freertos/queue.h"
#include "gfx/framebuffer.hpp"

namespace xreader
{
namespace ui
{

void run_hardware_test(drivers::it8951e::device_t* display, gfx::framebuffer_t* framebuffer,
                       QueueHandle_t events);

} // namespace ui
} // namespace xreader
