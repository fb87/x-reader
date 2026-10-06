#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace state {

inline constexpr std::size_t max_entries = 96;
inline constexpr std::size_t max_key = 64;
inline constexpr std::size_t max_string = 128;
inline constexpr std::size_t max_observers = 16;
inline constexpr std::uint16_t invalid_observer = UINT16_MAX;

/** @brief Type of value stored in a state entry. */
enum class value_type : std::uint8_t {
    empty,
    boolean,
    integer,
    string,
};

/** @brief Small scalar value stored in the shared state tree. */
struct value {
    value_type type = value_type::empty;
    bool boolean = false;
    std::int64_t integer = 0;
    std::array<char, max_string> string{};
};

/** @brief Callback invoked when a key below an observed prefix changes. */
using observer_fn = void (*)(const char* key, void* user);

/** @brief Stable handle used to unregister an observer. */
struct observer_handle {
    std::uint16_t slot = invalid_observer;
};

/** @brief Observer registered on an exact key or hierarchical prefix. */
struct observer {
    std::array<char, max_key> prefix{};
    observer_fn callback = nullptr;
    void* user = nullptr;
};

/** @brief Fixed-capacity hierarchical key-value state store. */
struct store {
    struct entry {
        std::array<char, max_key> key{};
        value data{};
        bool used = false;
    };

    std::array<entry, max_entries> entries{};
    std::array<observer, max_observers> observers{};
};

/** @brief Returns true when a key belongs to an observed hierarchical prefix. */
inline bool matches_prefix(const char* key, const char* prefix)
{
    const std::size_t length = std::strlen(prefix);
    if (std::strncmp(key, prefix, length) != 0) {
        return false;
    }
    return key[length] == '\0' || key[length] == '.' || (length > 0 && prefix[length - 1] == '.');
}

/** @brief Finds an existing state entry. */
inline store::entry* find(store& self, const char* key)
{
    for (auto& entry : self.entries) {
        if (entry.used && std::strcmp(entry.key.data(), key) == 0) {
            return &entry;
        }
    }
    return nullptr;
}

/** @brief Finds or allocates a state entry from the fixed-capacity store. */
inline store::entry* ensure(store& self, const char* key)
{
    if (auto* existing = find(self, key)) {
        return existing;
    }
    if (std::strlen(key) >= max_key) {
        return nullptr;
    }
    for (auto& entry : self.entries) {
        if (!entry.used) {
            entry.used = true;
            std::snprintf(entry.key.data(), entry.key.size(), "%s", key);
            return &entry;
        }
    }
    return nullptr;
}

/** @brief Notifies observers interested in a changed state key. */
inline void notify(store& self, const char* key)
{
    for (const auto& item : self.observers) {
        if (item.callback != nullptr && matches_prefix(key, item.prefix.data())) {
            item.callback(key, item.user);
        }
    }
}

/** @brief Registers an observer and returns a handle for later removal. */
inline observer_handle subscribe(store& self, const char* prefix, observer_fn callback, void* user)
{
    if (prefix == nullptr || callback == nullptr || std::strlen(prefix) >= max_key) {
        return {};
    }
    for (std::size_t i = 0; i < self.observers.size(); ++i) {
        auto& item = self.observers[i];
        if (item.callback == nullptr) {
            std::snprintf(item.prefix.data(), item.prefix.size(), "%s", prefix);
            item.callback = callback;
            item.user = user;
            return {static_cast<std::uint16_t>(i)};
        }
    }
    return {};
}

/** @brief Compatibility wrapper for callers that only need success/failure. */
inline bool observe(store& self, const char* prefix, observer_fn callback, void* user)
{
    return subscribe(self, prefix, callback, user).slot != invalid_observer;
}

/** @brief Removes a previously registered observer. */
inline bool unobserve(store& self, observer_handle handle)
{
    if (handle.slot >= self.observers.size()) {
        return false;
    }
    auto& item = self.observers[handle.slot];
    if (item.callback == nullptr) {
        return false;
    }
    item = {};
    return true;
}

/** @brief Removes one key and notifies matching observers. */
inline bool remove(store& self, const char* key)
{
    auto* entry = find(self, key);
    if (entry == nullptr) {
        return false;
    }
    *entry = {};
    notify(self, key);
    return true;
}

/** @brief Stores a boolean value. */
inline bool set(store& self, const char* key, bool value)
{
    auto* entry = ensure(self, key);
    if (entry == nullptr) {
        return false;
    }
    entry->data = {.type = value_type::boolean, .boolean = value};
    notify(self, key);
    return true;
}

/** @brief Stores an integer value. */
inline bool set(store& self, const char* key, std::int64_t value)
{
    auto* entry = ensure(self, key);
    if (entry == nullptr) {
        return false;
    }
    entry->data = {.type = value_type::integer, .integer = value};
    notify(self, key);
    return true;
}

/** @brief Stores a string value. */
inline bool set(store& self, const char* key, const char* value)
{
    if (value == nullptr || std::strlen(value) >= max_string) {
        return false;
    }
    auto* entry = ensure(self, key);
    if (entry == nullptr) {
        return false;
    }
    entry->data = {.type = value_type::string};
    std::snprintf(entry->data.string.data(), entry->data.string.size(), "%s", value);
    notify(self, key);
    return true;
}

/** @brief Reads an integer value with a caller-provided fallback. */
inline std::int64_t get(const store& self, const char* key, std::int64_t fallback)
{
    for (const auto& entry : self.entries) {
        if (entry.used && entry.data.type == value_type::integer &&
            std::strcmp(entry.key.data(), key) == 0) {
            return entry.data.integer;
        }
    }
    return fallback;
}

/** @brief Reads a boolean value with a caller-provided fallback. */
inline bool get(const store& self, const char* key, bool fallback)
{
    for (const auto& entry : self.entries) {
        if (entry.used && entry.data.type == value_type::boolean &&
            std::strcmp(entry.key.data(), key) == 0) {
            return entry.data.boolean;
        }
    }
    return fallback;
}

/** @brief Reads a string value with a caller-provided fallback. */
inline const char* get(const store& self, const char* key, const char* fallback)
{
    for (const auto& entry : self.entries) {
        if (entry.used && entry.data.type == value_type::string &&
            std::strcmp(entry.key.data(), key) == 0) {
            return entry.data.string.data();
        }
    }
    return fallback;
}

/** @brief Serializes scalar state to a simple text file. */
inline bool save(const store& self, const char* path)
{
    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        return false;
    }
    for (const auto& entry : self.entries) {
        if (!entry.used) {
            continue;
        }
        switch (entry.data.type) {
        case value_type::boolean:
            std::fprintf(file, "b\t%s\t%d\n", entry.key.data(), entry.data.boolean ? 1 : 0);
            break;
        case value_type::integer:
            std::fprintf(file, "i\t%s\t%lld\n", entry.key.data(),
                         static_cast<long long>(entry.data.integer));
            break;
        case value_type::string:
            std::fprintf(file, "s\t%s\t%s\n", entry.key.data(), entry.data.string.data());
            break;
        case value_type::empty:
            break;
        }
    }
    return std::fclose(file) == 0;
}

/** @brief Loads scalar state from a file previously produced by state::save(). */
inline bool load(store& self, const char* path)
{
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return false;
    }
    char line[256]{};
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        char type = 0;
        char key[max_key]{};
        char data[max_string]{};
        if (std::sscanf(line, "%c\t%63[^\t]\t%127[^\n]", &type, key, data) != 3) {
            continue;
        }
        if (type == 'b') {
            set(self, key, std::strcmp(data, "0") != 0);
        } else if (type == 'i') {
            set(self, key, static_cast<std::int64_t>(std::strtoll(data, nullptr, 10)));
        } else if (type == 's') {
            set(self, key, data);
        }
    }
    std::fclose(file);
    return true;
}

} // namespace state
