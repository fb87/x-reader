#include "core/router.hpp"
#include <cstdio>
#include <cstring>

struct target { int hits = 0; char id[32]{}; char filter[32]{}; };

static bool open_book(const router::request& req, void* user) {
  auto& t = *static_cast<target*>(user);
  ++t.hits;
  if (const char* id = req.param("id")) std::snprintf(t.id, sizeof(t.id), "%s", id);
  if (const char* f = req.query_value("filter")) std::snprintf(t.filter, sizeof(t.filter), "%s", f);
  return true;
}

int main() {
  router::context r{};
  target t{};
  int fail = 0;
  auto check = [&](bool ok, const char* what) { if (!ok) { ++fail; std::printf("FAIL %s\n", what); } };
  check(router::register_route(r, {.path="/", .title_key="home", .open=open_book, .user=&t}), "root register");
  check(router::register_route(r, {.path="/book/:id", .title_key="book", .open=open_book, .user=&t}), "param register");
  check(router::open(r, "/"), "root open");
  check(router::open(r, "/book/42?filter=favorite"), "param open");
  check(std::strcmp(t.id, "42") == 0, "path param");
  check(std::strcmp(t.filter, "favorite") == 0, "query param");
  check(std::strcmp(router::current(r), "/book/42?filter=favorite") == 0, "current");
  check(router::back(r), "back");
  check(std::strcmp(router::current(r), "/") == 0, "history current");
  check(!router::register_route(r, {.path="/", .open=open_book, .user=&t}), "duplicate rejected");
  std::printf("router tests: %d failures\n", fail);
  return fail == 0 ? 0 : 1;
}
