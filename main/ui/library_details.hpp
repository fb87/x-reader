#pragma once

#include "gfx/framebuffer.hpp"
#include "services/library_index.hpp"

namespace xreader::ui
{

void draw_library_details(gfx::framebuffer_t* framebuffer,
                          const services::library_index::entry_t* entry);

} // namespace xreader::ui
