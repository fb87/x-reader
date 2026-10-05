#pragma once

#include <cstddef>

/**
 * @brief Protected small-value storage for credentials, kept structurally
 * separate from `state::store`. Contract defined here in Phase 3; a real
 * implementation lands on the simulator board in a later phase so the
 * Wi-Fi password path has somewhere real to go rather than being declared
 * and left unimplemented.
 *
 * A board without a credential flow may legitimately leave this
 * capability unregistered (`capability::has(..., capability::id::secret)
 * == false`); callers must degrade gracefully, not fall back to writing
 * the secret into ordinary state.
 */
namespace secret {

/** @brief Protected key/value store for credentials and other secrets. */
struct store {
  void* context = nullptr;
  bool (*get)(store& self, const char* key, char* destination, std::size_t capacity) = nullptr;
  bool (*set)(store& self, const char* key, const char* value) = nullptr;
  bool (*remove)(store& self, const char* key) = nullptr;
};

/** @brief Reads a secret value into `destination`; returns false when absent/unsupported. */
inline bool get(store& self, const char* key, char* destination, std::size_t capacity) {
  return self.get != nullptr && self.get(self, key, destination, capacity);
}

/** @brief Stores a secret value. */
inline bool set(store& self, const char* key, const char* value) {
  return self.set != nullptr && self.set(self, key, value);
}

/** @brief Removes a secret value, if present. */
inline bool remove(store& self, const char* key) {
  return self.remove != nullptr && self.remove(self, key);
}

}  // namespace secret
