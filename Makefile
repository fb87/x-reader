CC       ?= cc
CXX      ?= clang++
BOARD    ?= sim
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -fno-exceptions -fno-rtti \
            -fno-threadsafe-statics

CPP_HEADERS := $(shell find core reader app boards -type f -name '*.hpp' 2>/dev/null)
SELECTED    := build/selected_board.hpp

# app/assets.hpp bridges the C-compiled bitmap font/icon tables (byte-identical layout
# to text::font/xr_icon_glyph_t, verified by differential tests during the migration)
# into this tree via extern "C", so pages render with real fonts -- including full
# Vietnamese coverage -- without a separate C++ font-data format.
FONT_BRIDGE_SRC := fonts/xr_font_alegreya_14.c fonts/xr_font_alegreya_17.c \
                   fonts/xr_font_alegreya_18.c fonts/xr_font_alegreya_20.c \
                   fonts/xr_font_alegreya_24.c fonts/xr_font_alegreya_bold_18.c \
                   fonts/xr_font_alegreya_bold_26.c fonts/xr_icons.c
FONT_BRIDGE_OBJ := $(patsubst fonts/%.c,build/fontbridge/%.o,$(FONT_BRIDGE_SRC))

build/fontbridge/%.o: fonts/%.c
	@mkdir -p build/fontbridge
	$(CC) -std=c99 -O2 -Ifonts -Iinclude -c $< -o $@

$(SELECTED):
	@mkdir -p build
	printf '#pragma once\n#include "boards/$(BOARD)/runtime.hpp"\nnamespace selected_board = board::$(BOARD);\n' > $@

build/reader: app_main.cpp $(CPP_HEADERS) $(SELECTED) $(FONT_BRIDGE_OBJ)
	$(CXX) $(CXXFLAGS) -I. -Ibuild app_main.cpp $(FONT_BRIDGE_OBJ) -o $@ -lz

build/simulator_gui: boards/sim/gui.cpp $(CPP_HEADERS) $(SELECTED) $(FONT_BRIDGE_OBJ)
	$(CXX) $(CXXFLAGS) $$(pkg-config --cflags sdl2) -I. -Ibuild boards/sim/gui.cpp $(FONT_BRIDGE_OBJ) -o $@ $$(pkg-config --libs sdl2) -lz

build/simulator_test: tests/simulator_test.cpp $(CPP_HEADERS) $(FONT_BRIDGE_OBJ)
	$(CXX) $(CXXFLAGS) -I. tests/simulator_test.cpp $(FONT_BRIDGE_OBJ) -o $@ -lz

build/epub_test: tests/epub_test.cpp $(CPP_HEADERS)
	$(CXX) $(CXXFLAGS) -I. tests/epub_test.cpp -o $@ -lz

build/font_coverage_test: tests/font_coverage_test.cpp $(CPP_HEADERS) $(FONT_BRIDGE_OBJ)
	$(CXX) $(CXXFLAGS) -I. tests/font_coverage_test.cpp $(FONT_BRIDGE_OBJ) -o $@ -lz

build/xteink_test: tests/xteink_test.cpp $(CPP_HEADERS) $(FONT_BRIDGE_OBJ)
	$(CXX) $(CXXFLAGS) -I. tests/xteink_test.cpp $(FONT_BRIDGE_OBJ) -o $@ -lz

build/book_title_test: tests/book_title_test.cpp $(CPP_HEADERS)
	$(CXX) $(CXXFLAGS) -I. tests/book_title_test.cpp -o $@

all: build/reader

gui: build/simulator_gui
	./build/simulator_gui

test: build/reader build/simulator_test build/epub_test build/font_coverage_test build/xteink_test build/book_title_test
	./build/simulator_test
	./build/epub_test
	./build/font_coverage_test
	./build/xteink_test
	./build/book_title_test

fonts:
	python3 tools/gen_font.py fonts

format:
	clang-format -i $$(find core reader app boards tests -type f \( -name '*.hpp' -o -name '*.cpp' \) 2>/dev/null) app_main.cpp 2>/dev/null || true

format-check:
	@if command -v clang-format >/dev/null 2>&1; then \
		find core reader app boards tests -type f \( -name '*.hpp' -o -name '*.cpp' \) -print0 2>/dev/null | \
		xargs -0 -r clang-format --dry-run --Werror; \
	else \
		echo 'clang-format not installed; format check skipped'; \
	fi

clean:
	rm -rf build out

.PHONY: all gui test fonts format format-check clean
