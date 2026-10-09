#pragma once

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "core/state.hpp"
#include "core/storage.hpp"
#include "reader/book.hpp"

namespace library {

inline constexpr std::size_t max_books = 16;
inline constexpr std::size_t scan_queue_size = 32;

/** @brief Fixed-capacity ebook library cache reconstructed from storage. */
struct index {
  std::array<book::item, max_books + 1> books{};
  std::size_t count = 0;
};

/** @brief Returns true when a filename is an EPUB-like reader document. */
inline bool supported(const char* name) {
  if (name == nullptr) {
    return false;
  }
  const auto length = std::strlen(name);
  if (length < 5) {
    return false;
  }
  const char* suffix = name + length - 5;
  if (suffix[0] == '.' && (suffix[1] == 'e' || suffix[1] == 'E') &&
      (suffix[2] == 'p' || suffix[2] == 'P') && (suffix[3] == 'u' || suffix[3] == 'U') &&
      (suffix[4] == 'b' || suffix[4] == 'B'))
    return true;
  return length >= 4 && name[length - 4] == '.' &&
         (name[length - 3] == 'e' || name[length - 3] == 'E') &&
         (name[length - 2] == 'p' || name[length - 2] == 'P') &&
         (name[length - 1] == 'u' || name[length - 1] == 'U');
}

/** @brief Adds a document to the in-memory library cache. */
inline bool add(index& self, const char* path) {
  if (path == nullptr || self.count >= max_books) {
    return false;
  }
  auto& out = self.books[self.count++];
  std::snprintf(out.path.data(), out.path.size(), "%s", path);
  book::make_title(out.title.data(), out.title.size(), path);
  std::snprintf(out.author.data(), out.author.size(), "%s", "EPUB");
  return true;
}

/** @brief Adds a File Manager EPUB in the reserved transient slot. */
inline bool add_transient(index& self, const char* path) {
  if (path == nullptr || self.count >= self.books.size()) return false;
  auto& out = self.books[self.count++];
  std::snprintf(out.path.data(), out.path.size(), "%s", path);
  book::make_title(out.title.data(), out.title.size(), path);
  std::snprintf(out.author.data(), out.author.size(), "%s", "EPUB");
  return true;
}

/** @brief Clears the cached library without touching storage. */
inline void clear(index& self) { self = {}; }

struct scan_context {
  index* output = nullptr;
  const char* directory = nullptr;
  std::array<std::array<char, storage::path_max>, scan_queue_size>* queue = nullptr;
  std::size_t* tail = nullptr;
};

/** @brief Collects supported files and queues discovered directories. */
inline bool collect_entry(const storage::entry& value, void* user) {
  auto& context = *static_cast<scan_context*>(user);
  char path[storage::path_max]{};
  std::snprintf(
      path, sizeof(path), "%s%s%s", context.directory,
      context.directory[0] != '\0' && context.directory[std::strlen(context.directory) - 1] == '/'
          ? ""
          : "/",
      value.name);
  if (value.directory) {
    if (*context.tail < context.queue->size() && std::strlen(path) < storage::path_max) {
      std::snprintf((*context.queue)[*context.tail].data(), storage::path_max, "%s", path);
      ++*context.tail;
    }
    return true;
  }
  if (supported(value.name)) add(*context.output, path);
  return true;
}

/** @brief Recursively scans storage with a bounded queue and updates shared library state. */
inline bool scan(index& self, storage::device& storage, state::store& shared) {
  clear(self);
  std::array<std::array<char, storage::path_max>, scan_queue_size> queue{};
  std::size_t head = 0;
  std::size_t tail = 1;
  std::snprintf(queue[0].data(), queue[0].size(), "%s",
                storage.root == nullptr ? "" : storage.root);
  bool ok = true;
  while (head < tail) {
    scan_context context{
        .output = &self, .directory = queue[head].data(), .queue = &queue, .tail = &tail};
    if (!storage::list(storage, context.directory, collect_entry, &context)) {
      ok = false;
      break;
    }
    ++head;
  }
  state::set(shared, "reader.library.count", static_cast<std::int64_t>(self.count));
  if (state::get(shared, "reader.library.selected", std::int64_t{0}) >=
      static_cast<std::int64_t>(self.count)) {
    state::set(shared, "reader.library.selected", std::int64_t{0});
  }
  return ok;
}

/** @brief Removes one book from the in-memory library cache. */
inline bool remove(index& self, std::size_t item) {
  if (item >= self.count) return false;
  for (std::size_t i = item; i + 1 < self.count; ++i) self.books[i] = self.books[i + 1];
  --self.count;
  return true;
}

/** @brief Returns the selected book or nullptr when the library is empty. */
inline book::item* selected(index& self, const state::store& shared) {
  const auto selected = state::get(shared, "reader.library.selected", std::int64_t{0});
  if (selected < 0 || selected >= static_cast<std::int64_t>(self.count)) {
    return nullptr;
  }
  return &self.books[static_cast<std::size_t>(selected)];
}

}  // namespace library
