#pragma once

#include "event.hpp"

/**
 * @brief Generic input source capability. A board's main loop (or, for the
 * simulator, an injection queue) produces `event::value`s; nothing above
 * this header knows how an event was physically produced.
 */
namespace input {

/** @brief Generic input device supplied by a board implementation. */
struct device {
  void* context = nullptr;
  bool (*poll)(device& self, event::value& out) = nullptr;
};

/** @brief Polls one pending event; returns false when none is available. */
inline bool poll(device& self, event::value& out) {
  return self.poll != nullptr && self.poll(self, out);
}

}  // namespace input
