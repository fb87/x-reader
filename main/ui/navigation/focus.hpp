#pragma once

#include <stdint.h>

#include "ui/layout/layout.hpp"

namespace xreader
{
namespace ui
{
namespace focus
{

uint8_t next(uint8_t current, uint8_t count);
uint8_t previous(uint8_t current, uint8_t count);
bool hit_rows(layout::rect_t area, uint8_t count, uint16_t row_height, uint16_t gap, uint16_t x,
              uint16_t y, uint8_t* index);

} // namespace focus
} // namespace ui
} // namespace xreader
