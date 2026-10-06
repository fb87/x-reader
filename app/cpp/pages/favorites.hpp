#pragma once

#include "app/cpp/pages/library.hpp"

namespace app::pages::favorites {

inline void render(context& self) { library::render(self); }
inline bool event(context& self, const event::value& value) { return library::event(self, value); }

}  // namespace app::pages::favorites
