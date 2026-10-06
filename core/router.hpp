#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <string_view>

namespace router {

inline constexpr std::size_t max_routes = 32;
inline constexpr std::size_t max_params = 8;
inline constexpr std::size_t max_query = 8;
inline constexpr std::size_t max_path = 128;
inline constexpr std::size_t max_name = 32;
inline constexpr std::size_t max_value = 64;
inline constexpr std::size_t max_history = 16;

struct pair {
  char name[max_name]{};
  char value[max_value]{};
};

struct request {
  char uri[max_path]{};
  char path[max_path]{};
  std::array<pair, max_params> params{};
  std::size_t param_count = 0;
  std::array<pair, max_query> query{};
  std::size_t query_count = 0;

  const char* param(const char* key) const {
    for (std::size_t i = 0; i < param_count; ++i)
      if (std::strcmp(params[i].name, key) == 0) return params[i].value;
    return nullptr;
  }

  const char* query_value(const char* key) const {
    for (std::size_t i = 0; i < query_count; ++i)
      if (std::strcmp(query[i].name, key) == 0) return query[i].value;
    return nullptr;
  }
};

using handler = bool (*)(const request&, void*);
using menu_value_handler = const char* (*)(void*);

struct descriptor {
  const char* path = nullptr;
  const char* title_key = nullptr;
  const char* menu_section = nullptr;
  int menu_order = 0;
  handler open = nullptr;
  void* user = nullptr;
  menu_value_handler menu_value = nullptr;
};

struct match {
  const descriptor* route = nullptr;
  request req{};
  explicit operator bool() const { return route != nullptr; }
};

struct context {
  std::array<descriptor, max_routes> routes{};
  std::size_t route_count = 0;
  std::array<std::array<char, max_path>, max_history> history{};
  std::size_t history_count = 0;
  std::array<char, max_path> current{};
};

inline bool copy(char* dst, std::size_t capacity, std::string_view value) {
  if (capacity == 0 || value.size() >= capacity) return false;
  std::memcpy(dst, value.data(), value.size());
  dst[value.size()] = '\0';
  return true;
}

inline bool register_route(context& self, descriptor value) {
  if (self.route_count >= self.routes.size() || value.path == nullptr || value.path[0] != '/')
    return false;
  for (std::size_t i = 0; i < self.route_count; ++i)
    if (std::strcmp(self.routes[i].path, value.path) == 0) return false;
  self.routes[self.route_count++] = value;
  return true;
}

inline bool add_pair(std::array<pair, max_params>& out, std::size_t& count,
                     std::string_view name, std::string_view value) {
  if (count >= out.size()) return false;
  return copy(out[count].name, sizeof(out[count].name), name) &&
         copy(out[count++].value, sizeof(out[0].value), value);
}

inline bool add_query(std::array<pair, max_query>& out, std::size_t& count,
                      std::string_view name, std::string_view value) {
  if (count >= out.size()) return false;
  return copy(out[count].name, sizeof(out[count].name), name) &&
         copy(out[count++].value, sizeof(out[0].value), value);
}

inline void parse_query(std::string_view query, request& out) {
  while (!query.empty() && out.query_count < out.query.size()) {
    const auto amp = query.find('&');
    const auto item = query.substr(0, amp);
    const auto eq = item.find('=');
    const auto key = item.substr(0, eq);
    const auto value = eq == std::string_view::npos ? std::string_view{} : item.substr(eq + 1);
    (void)add_query(out.query, out.query_count, key, value);
    if (amp == std::string_view::npos) break;
    query.remove_prefix(amp + 1);
  }
}

inline bool match_path(std::string_view pattern, std::string_view path, request& out) {
  if (pattern.empty() || path.empty() || pattern.front() != '/' || path.front() != '/') return false;
  if (pattern == "/") return path == "/";
  pattern.remove_prefix(1);
  path.remove_prefix(1);
  while (true) {
    const auto pslash = pattern.find('/');
    const auto uslash = path.find('/');
    const auto pseg = pattern.substr(0, pslash);
    const auto useg = path.substr(0, uslash);
    if (pseg.empty() || useg.empty()) return false;
    if (pseg.front() == ':') {
      if (!add_pair(out.params, out.param_count, pseg.substr(1), useg)) return false;
    } else if (pseg != useg) {
      return false;
    }
    const bool pend = pslash == std::string_view::npos;
    const bool uend = uslash == std::string_view::npos;
    if (pend || uend) return pend && uend;
    pattern.remove_prefix(pslash + 1);
    path.remove_prefix(uslash + 1);
  }
}

inline match resolve(const context& self, std::string_view uri) {
  match result{};
  const auto q = uri.find('?');
  const auto path = uri.substr(0, q);
  for (std::size_t i = 0; i < self.route_count; ++i) {
    request candidate{};
    if (!copy(candidate.uri, sizeof(candidate.uri), uri) ||
        !copy(candidate.path, sizeof(candidate.path), path)) return {};
    if (!match_path(self.routes[i].path, path, candidate)) continue;
    if (q != std::string_view::npos) parse_query(uri.substr(q + 1), candidate);
    result.route = &self.routes[i];
    result.req = candidate;
    return result;
  }
  return {};
}

inline bool open(context& self, const char* uri, bool remember = true) {
  if (uri == nullptr) return false;
  auto found = resolve(self, uri);
  if (!found || found.route->open == nullptr) return false;
  if (!found.route->open(found.req, found.route->user)) return false;
  if (remember && self.current[0] != '\0') {
    if (self.history_count == self.history.size()) {
      for (std::size_t i = 1; i < self.history.size(); ++i) self.history[i - 1] = self.history[i];
      --self.history_count;
    }
    (void)copy(self.history[self.history_count++].data(), max_path, self.current.data());
  }
  return copy(self.current.data(), self.current.size(), uri);
}

inline bool replace(context& self, const char* uri) { return open(self, uri, false); }

inline bool back(context& self) {
  if (self.history_count == 0) return false;
  auto previous = self.history[--self.history_count];
  return open(self, previous.data(), false);
}

inline const char* current(const context& self) { return self.current.data(); }

}  // namespace router
