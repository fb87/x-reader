#!/bin/sh
set -eu
root="$(mktemp -d)"
trap 'rm -rf "$root"' EXIT
mkdir -p "$root/plugins/probe/cpp" "$root/plugins/probe/lua" "$root/out"
cat > "$root/plugins/probe/cpp/init.hpp" <<'HPP'
#pragma once
#if defined(CONFIG_PLUGIN_PROBE)
namespace plugins::probe {
template <typename Context>
inline bool init(Context& ctx) { ++ctx; return true; }
}
#endif
HPP
cat > "$root/plugins/probe/lua/init.lua" <<'LUA'
return { init = function() _G.plugin_probe_loaded = true; return true end }
LUA
cat > "$root/plugins/probe/Kconfig" <<'KC'
config PLUGIN_PROBE
    bool "Probe plugin"
KC
printf 'CONFIG_PLUGIN_PROBE=y\n' > "$root/.config"
python3 scripts/gen_plugins.py --plugins "$root/plugins" --config "$root/.config" --out "$root/out" >/dev/null

grep -q '#define CONFIG_PLUGIN_PROBE 1' "$root/out/plugin_config.hpp"
grep -q 'plugins/probe/cpp/init.hpp' "$root/out/enabled_plugins.hpp"
grep -q 'plugins::probe::init' "$root/out/enabled_plugins.hpp"
grep -q 'plugins.probe.lua.init' "$root/out/enabled_plugins.lua"
grep -q 'plugins/probe/Kconfig' "$root/out/plugins.Kconfig"
cat > "$root/test.cpp" <<'CPP'
#include "enabled_plugins.hpp"
int main() { int value = 0; return generated::plugins::init(value) && value == 1 ? 0 : 1; }
CPP
${CXX:-c++} -std=c++20 -I"$root" -I"$root/out" "$root/test.cpp" -o "$root/test"
"$root/test"
printf 'plugin discovery: PASS\n'
