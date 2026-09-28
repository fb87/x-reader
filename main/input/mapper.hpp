#pragma once

#include "input/action.hpp"
#include "input/input.hpp"

namespace xreader
{
namespace input
{

bool map_event(const event_t* event, action_event_t* action);

} // namespace input
} // namespace xreader
