#!/bin/sh
set -eu

# Wi-Fi application UI/service must live in the plugin, not the base app.
test -f plugins/wifi/cpp/init.hpp
test -f plugins/wifi/cpp/routes.hpp
test -f plugins/wifi/cpp/pages/wifi.hpp
test -f plugins/wifi/cpp/service.hpp
test ! -e plugins/wifi/cpp/ui.hpp
test -f plugins/wifi/lua/init.lua
test -f plugins/wifi/lua/pages/wifi.lua
test ! -e app/cpp/pages/connectivity.hpp
test ! -e app/lua/pages/connectivity.lua
! grep -Rqs 'shell\.wifi' app runtime plugins --include='*.lua'

grep -q '#if defined(CONFIG_PLUGIN_WIFI)' plugins/wifi/cpp/init.hpp
grep -q '#if defined(CONFIG_PLUGIN_WIFI)' plugins/wifi/cpp/routes.hpp
grep -q '#if defined(CONFIG_PLUGIN_WIFI)' plugins/wifi/cpp/pages/wifi.hpp
grep -q 'menu_section = "settings"' plugins/wifi/cpp/routes.hpp
grep -q 'section = "settings"' plugins/wifi/lua/init.lua

# Project headers use stable root-relative includes, never ../ traversal.
! grep -Rqs '#include "\.\./' core reader app runtime boards plugins tests \
    --include='*.hpp' --include='*.cpp'

echo 'wifi plugin structure: PASS'

# Disabled plugin headers remain safe to include directly: undefined guard => empty implementation.
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/disabled.cpp" <<'CPP'
#include "plugins/wifi/cpp/init.hpp"
#include "plugins/wifi/cpp/routes.hpp"
#include "plugins/wifi/cpp/service.hpp"
#include "plugins/wifi/cpp/pages/wifi.hpp"
int main() { return 0; }
CPP
${CXX:-c++} -std=c++20 -I"$(pwd)" "$tmp/disabled.cpp" -o "$tmp/disabled"
"$tmp/disabled"
echo 'optional plugin header guards: PASS'
