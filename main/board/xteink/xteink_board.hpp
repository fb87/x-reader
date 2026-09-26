#pragma once

#include "board/board.hpp"

#ifndef XREADER_XTEINK_CONFIGURED
#define XREADER_XTEINK_CONFIGURED 0
#endif

namespace xreader
{
namespace board
{
namespace xteink
{

esp_err_t get_capabilities(capabilities_t* capabilities);

} // namespace xteink
} // namespace board
} // namespace xreader
