#pragma once

#include <cstddef>

namespace secret {

/** @brief Protected small-value storage used for credentials and other secrets. */
struct store {
  void* context = nullptr;
  bool (*get)(store& self, const char* key, char* destination, std::size_t capacity) = nullptr;
  bool (*set)(store& self, const char* key, const char* value) = nullptr;
  bool (*remove)(store& self, const char* key) = nullptr;
};

}  // namespace secret
