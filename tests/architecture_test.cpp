#include <cstdio>

#include "core/capability.hpp"
#include "core/display.hpp"
#include "core/event.hpp"
#include "core/fixed_queue.hpp"
#include "core/state.hpp"
#include "core/text.hpp"
#include "core/unicode_glyphs.hpp"

namespace {
int failures = 0;
void expect(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
  }
}

void changed(const char*, void* user) { ++*static_cast<int*>(user); }
}  // namespace

int main() {
  fixed_queue::queue<int, 2> q{};
  expect(fixed_queue::push(q, 1), "queue accepts first value");
  expect(fixed_queue::push(q, 2), "queue accepts second value");
  expect(!fixed_queue::push(q, 3), "queue rejects overflow");
  int value = 0;
  expect(fixed_queue::pop(q, value) && value == 1, "queue preserves FIFO order");

  capability::registry caps{};
  display::device display{};
  capability::set(caps, capability::id::display, &display);
  expect(capability::get<display::device>(caps) == &display, "typed capability lookup works");

  state::store store{};
  int notifications = 0;
  const auto observer = state::subscribe(store, "reader.book", changed, &notifications);
  expect(observer.slot != state::invalid_observer, "observer subscription succeeds");
  expect(state::set(store, "reader.book.current", std::int64_t{2}), "state set succeeds");
  expect(notifications == 1, "observer receives matching update");
  expect(state::remove(store, "reader.book.current"), "state remove succeeds");
  expect(notifications == 2, "observer receives removal update");
  expect(state::unobserve(store, observer), "observer removal succeeds");
  state::set(store, "reader.book.current", std::int64_t{3});
  expect(notifications == 2, "removed observer is not called");

  expect(text::unicode::find(U'中') != nullptr, "shipped Chinese glyph coverage exists");
  expect(text::unicode::find(U'Ế') != nullptr, "shipped Vietnamese glyph coverage exists");
  expect(text::width("TIẾNG VIỆT", 1) == text::width("TIENG VIET", 1),
         "UTF-8 UI glyphs share one cell width across scripts");
  expect(text::height("TIẾNG VIỆT", 2) == text::height("ASCII", 2),
         "UTF-8 UI glyphs share one baseline height across scripts");
  const auto long_key = event::key(event::key_code::menu, 900, true);
  expect(long_key.long_press && long_key.duration_ms == 900,
         "key ABI carries long-press classification and duration");

  std::printf("architecture tests: %d failures\n", failures);
  return failures == 0 ? 0 : 1;
}
