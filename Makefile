CXX ?= clang++
BOARD ?= sim
APP ?= cpp
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -fno-exceptions -fno-rtti \
            -fno-threadsafe-statics

PROJECT_ROOT := $(CURDIR)
BUILD := build
GENERATED := $(BUILD)/generated
SELECTED := $(BUILD)/selected_board.hpp
PLUGIN_CONFIG ?= .config
PLUGINS ?=
PLUGIN_KEY := $(shell printf '%s' '$(PLUGINS)' | cksum | awk '{print $$1}')
PLUGIN_STAMP := $(GENERATED)/.plugins.$(PLUGIN_KEY).stamp
HEADERS := $(shell find core reader app runtime boards plugins -type f -name '*.hpp' 2>/dev/null)
LUA_SOURCES := $(shell find app/lua runtime/lua plugins -type f -name '*.lua' 2>/dev/null)
LANG_SOURCES := $(shell find lang -type f -name '*.txt')
LUA_LIBS := -ldl
WAYLAND_LIBS ?= $(shell pkg-config --libs wayland-client 2>/dev/null || echo -Wl,-l:libwayland-client.so.0)
CPPFLAGS += -I$(PROJECT_ROOT) -I$(PROJECT_ROOT)/$(BUILD) -I$(PROJECT_ROOT)/$(GENERATED) \
            -include $(PROJECT_ROOT)/$(GENERATED)/plugin_config.hpp

.PHONY: all gui cpp-gui lua-gui test test-cpp test-lua test-structure test-plugins test-wifi-plugin clean format format-check bundle plugins plugin-kconfig

ifeq ($(APP),lua)
APP_MAIN := lua_main.cpp
APP_BIN := $(BUILD)/reader-lua
else
APP_MAIN := app_main.cpp
APP_BIN := $(BUILD)/reader
endif

all: $(APP_BIN)

$(BUILD):
	mkdir -p $(BUILD)

$(GENERATED): | $(BUILD)
	mkdir -p $(GENERATED)

$(PLUGIN_STAMP): scripts/gen_plugins.py $(shell find plugins -maxdepth 4 -type f 2>/dev/null) $(wildcard $(PLUGIN_CONFIG)) | $(GENERATED)
	python3 scripts/gen_plugins.py --plugins plugins --config $(PLUGIN_CONFIG) --out $(GENERATED) --enable "$(PLUGINS)"
	touch $@

plugins: $(PLUGIN_STAMP)

plugin-kconfig: $(PLUGIN_STAMP)
	@cat $(GENERATED)/plugins.Kconfig

$(SELECTED): | $(BUILD)
	printf '#pragma once\n#include "boards/$(BOARD)/runtime.hpp"\nnamespace selected_board = board::$(BOARD);\n' > $@

$(BUILD)/reader: app_main.cpp $(HEADERS) $(SELECTED) $(PLUGIN_STAMP) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) app_main.cpp -o $@ -lz

$(BUILD)/reader-lua: lua_main.cpp $(LUA_SOURCES) $(LANG_SOURCES) $(HEADERS) $(SELECTED) $(PLUGIN_STAMP) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) lua_main.cpp -o $@ -lz $(LUA_LIBS)

$(BUILD)/simulator_gui: boards/sim/gui.cpp $(HEADERS) $(PLUGIN_STAMP) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) boards/sim/gui.cpp -o $@ -lz $(WAYLAND_LIBS)

$(BUILD)/simulator_lua_gui: boards/sim/lua_gui.cpp $(LUA_SOURCES) $(LANG_SOURCES) $(HEADERS) $(PLUGIN_STAMP) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) boards/sim/lua_gui.cpp -o $@ -lz $(WAYLAND_LIBS) $(LUA_LIBS)

cpp-gui: $(BUILD)/simulator_gui
	./$(BUILD)/simulator_gui

lua-gui: $(BUILD)/simulator_lua_gui
	./$(BUILD)/simulator_lua_gui

gui: $(if $(filter lua,$(APP)),lua-gui,cpp-gui)

$(BUILD)/simulator_test: tests/simulator_test.cpp $(HEADERS) $(PLUGIN_STAMP) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/simulator_test.cpp -o $@ -lz

$(BUILD)/epub_test: tests/epub_test.cpp $(HEADERS) $(PLUGIN_STAMP) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/epub_test.cpp -o $@ -lz

$(BUILD)/architecture_test: tests/architecture_test.cpp $(HEADERS) $(PLUGIN_STAMP) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/architecture_test.cpp -o $@

$(BUILD)/router_test: tests/router_test.cpp $(HEADERS) $(PLUGIN_STAMP) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/router_test.cpp -o $@

$(BUILD)/lua_parity_test: tests/lua_parity_test.cpp $(LUA_SOURCES) $(LANG_SOURCES) $(HEADERS) $(PLUGIN_STAMP) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/lua_parity_test.cpp -o $@ -lz $(LUA_LIBS)

$(BUILD)/lua_app_test: tests/lua_app_test.cpp $(LUA_SOURCES) $(LANG_SOURCES) $(HEADERS) $(PLUGIN_STAMP) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/lua_app_test.cpp -o $@ -lz $(LUA_LIBS)

$(BUILD)/lua_hot_reload_test: tests/lua_hot_reload_test.cpp $(LUA_SOURCES) $(LANG_SOURCES) $(HEADERS) $(PLUGIN_STAMP) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/lua_hot_reload_test.cpp -o $@ -lz $(LUA_LIBS)

test-cpp: $(BUILD)/reader $(BUILD)/simulator_gui $(BUILD)/simulator_test $(BUILD)/epub_test $(BUILD)/architecture_test $(BUILD)/router_test
	./$(BUILD)/reader
	./$(BUILD)/simulator_test
	./$(BUILD)/epub_test
	./$(BUILD)/architecture_test
	./$(BUILD)/router_test

test-lua: $(BUILD)/reader-lua $(BUILD)/simulator_lua_gui $(BUILD)/lua_app_test $(BUILD)/lua_parity_test $(BUILD)/lua_hot_reload_test
	./$(BUILD)/reader-lua
	./$(BUILD)/lua_app_test
	./$(BUILD)/lua_parity_test
	./$(BUILD)/lua_hot_reload_test

test-structure:
	./tests/app_structure_test.sh

test-plugins:
	CXX=$(CXX) ./tests/plugin_discovery_test.sh

test-wifi-plugin:
	./tests/wifi_plugin_test.sh

test: test-structure test-plugins test-wifi-plugin test-cpp test-lua

format:
	clang-format -i $$(find core reader app runtime boards plugins tests -type f \( -name '*.hpp' -o -name '*.cpp' \)) app_main.cpp lua_main.cpp

format-check:
	@if command -v clang-format >/dev/null 2>&1; then \
		find core reader app runtime boards plugins tests -type f \( -name '*.hpp' -o -name '*.cpp' \) -print0 | \
		xargs -0 clang-format --dry-run --Werror; \
	else \
		echo 'clang-format not installed; format check skipped'; \
	fi

clean:
	rm -rf $(BUILD)

bundle: test
	zip -qr ../reader-lua.zip . -x 'build/*'
