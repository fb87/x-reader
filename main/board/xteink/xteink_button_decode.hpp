#pragma once

#include "input/input.hpp"

namespace xreader
{
namespace board
{
namespace xteink
{

inline input::key_t decode_ladder1_value(int raw)
{
    if (raw >= 3200 && raw <= 3820)
        return input::key_back;
    if (raw >= 2300 && raw <= 3000)
        return input::key_select;
    if (raw >= 1050 && raw <= 1850)
        return input::key_left;
    if (raw >= 0 && raw <= 350)
        return input::key_right;
    return input::key_none;
}

inline input::key_t decode_ladder2_value(int raw)
{
    if (raw >= 1750 && raw <= 2650)
        return input::key_up;
    if (raw >= 0 && raw <= 350)
        return input::key_down;
    return input::key_none;
}

} // namespace xteink
} // namespace board
} // namespace xreader
