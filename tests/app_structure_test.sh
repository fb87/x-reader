#!/bin/sh
set -eu

root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

for stem in init main model routes; do
    test -f "$root/app/cpp/$stem.hpp"
    test -f "$root/app/lua/$stem.lua"
done

cpp_pages="$(find "$root/app/cpp/pages" -maxdepth 1 -type f -name '*.hpp' ! -name 'all.hpp' ! -name 'common.hpp' -printf '%f\n' | sed 's/\.hpp$//' | sort)"
lua_pages="$(find "$root/app/lua/pages" -maxdepth 1 -type f -name '*.lua' -printf '%f\n' | sed 's/\.lua$//' | sort)"

if [ "$cpp_pages" != "$lua_pages" ]; then
    echo "C++ and Lua page sets differ" >&2
    echo "C++:" >&2
    echo "$cpp_pages" >&2
    echo "Lua:" >&2
    echo "$lua_pages" >&2
    exit 1
fi

if grep -R "router\.register" "$root/runtime/lua" >/dev/null 2>&1; then
    echo "product route registration leaked into runtime/lua" >&2
    exit 1
fi

if grep -R "register_route" "$root/app/cpp/pages" >/dev/null 2>&1; then
    echo "route wiring leaked into C++ page modules" >&2
    exit 1
fi

printf 'app structure tests: passed\n'
