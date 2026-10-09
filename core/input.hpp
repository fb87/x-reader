#pragma once

#include "core/event.hpp"

namespace input {

/** @brief Generic input source supplied by a board implementation. */
struct device {
  void* context;
  bool (*poll)(device& self, event::value& out);
};

/** @brief Polls one event from the input device. */
inline bool poll(device& self, event::value& out) {
  return self.poll != nullptr && self.poll(self, out);
}

}  // namespace input
