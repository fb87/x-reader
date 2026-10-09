#pragma once

#include <cstddef>
#include <cstring>

#include "app/cpp/model.hpp"
#include "core/router.hpp"

namespace app::routes {

inline const char* path_for_page(page value) {
  switch (value) {
    case page::splash:
      return "/splash";
    case page::home:
      return "/";
    case page::library:
      return "/library";
    case page::favorites:
      return "/library/favorites";
    case page::files:
      return "/files";
    case page::reader:
      return "/book/0/reader";
    case page::settings:
      return "/settings";
    case page::connectivity:
      return "/settings/network";
    case page::sleep:
      return "/sleep";
  }
  return "/";
}

inline page page_for_path(const char* path) {
  if (std::strcmp(path, "/splash") == 0) return page::splash;
  if (std::strcmp(path, "/") == 0) return page::home;
  if (std::strcmp(path, "/library") == 0) return page::library;
  if (std::strcmp(path, "/library/favorites") == 0) return page::favorites;
  if (std::strcmp(path, "/files") == 0) return page::files;
  if (std::strncmp(path, "/book/", 6) == 0) return page::reader;
  if (std::strcmp(path, "/settings") == 0) return page::settings;
  if (std::strcmp(path, "/settings/network") == 0) return page::connectivity;
  if (std::strcmp(path, "/sleep") == 0) return page::sleep;
  return page::home;
}

inline bool open_page(const router::request& request, void* user) {
  auto& self = *static_cast<context*>(user);
  const auto next = page_for_path(request.path);
  if (next == page::sleep && current_page(self) != page::sleep) {
    state::set(*self.memory, "app.sleep.return_route",
               state::get(*self.memory, "app.route.current", "/"));
  }
  apply_page(self, next);
  state::set(*self.memory, "app.route.current", request.uri);
  return true;
}

/** @brief Registers the product's URI-to-page wiring into the framework router. */
inline bool register_all(context& self) {
  struct route_seed {
    const char* path;
    const char* title;
    const char* menu;
    int order;
  };

  constexpr route_seed entries[] = {
      {"/splash", "reader_name", nullptr, 0}, {"/", "home", nullptr, 0},
      {"/library", "library", "home", 10},    {"/library/favorites", "favorites", "home", 20},
      {"/files", "file_manager", "home", 30}, {"/book/:id/reader", "reading", nullptr, 0},
      {"/settings", "settings", "home", 40},  {"/sleep", "sleep", "home", 50},
  };

  for (const auto& entry : entries) {
    if (!router::register_route(self.router, {.path = entry.path,
                                              .title_key = entry.title,
                                              .menu_section = entry.menu,
                                              .menu_order = entry.order,
                                              .open = open_page,
                                              .user = &self})) {
      return false;
    }
  }
  return true;
}

inline bool push(context& self, const char* uri) { return router::open(self.router, uri); }
inline bool replace(context& self, const char* uri) { return router::replace(self.router, uri); }
inline bool back(context& self) { return router::back(self.router); }

inline const char* menu_value(const router::descriptor& route) {
  return route.menu_value != nullptr ? route.menu_value(route.user) : "";
}

inline const char* menu_label(const router::descriptor& route) {
  if (std::strcmp(route.title_key, "library") == 0) return "Library";
  if (std::strcmp(route.title_key, "favorites") == 0) return "Favorites";
  if (std::strcmp(route.title_key, "file_manager") == 0) return "File Manager";
  if (std::strcmp(route.title_key, "wifi") == 0) return "Wi-Fi";
  if (std::strcmp(route.title_key, "settings") == 0) return "Settings";
  if (std::strcmp(route.title_key, "sleep") == 0) return "Sleep";
  return route.title_key;
}

inline int menu_count(const context& self, const char* section) {
  int count = 0;
  for (std::size_t i = 0; i < self.router.route_count; ++i) {
    if (self.router.routes[i].menu_section != nullptr &&
        std::strcmp(self.router.routes[i].menu_section, section) == 0) {
      ++count;
    }
  }
  return count;
}

inline const router::descriptor* menu_route(const context& self, const char* section, int index) {
  const router::descriptor* best = nullptr;
  int previous_order = -2147483647;
  for (int n = 0; n <= index; ++n) {
    best = nullptr;
    int best_order = 2147483647;
    for (std::size_t i = 0; i < self.router.route_count; ++i) {
      const auto& route = self.router.routes[i];
      if (route.menu_section == nullptr || std::strcmp(route.menu_section, section) != 0) continue;
      if (route.menu_order > previous_order && route.menu_order < best_order) {
        best = &route;
        best_order = route.menu_order;
      }
    }
    if (best == nullptr) return nullptr;
    previous_order = best->menu_order;
  }
  return best;
}

/** @brief Compatibility page navigation implemented through app-owned URI wiring. */
inline void set_page(context& self, page value) {
  if (self.router.route_count == 0 || !push(self, path_for_page(value))) apply_page(self, value);
}

}  // namespace app::routes
