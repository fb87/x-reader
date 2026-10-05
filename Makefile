CC      ?= cc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Wshadow
CFLAGS  += -Iinclude -Ifonts -Iapp -Iport/sim

CORE  := $(wildcard src/*.c)
FONTS := $(wildcard fonts/*.c)
APP   := $(wildcard app/*.c)
SIM   := $(wildcard port/sim/*.c)
SDL_SIM := port/sdl/m5_gui.c port/sim/sim_port.c port/sim/sim_fatfs.c
EPUB_TEST := tests/epub_import_test.c src/xr_epub.c port/sim/sim_fatfs.c
TITLE_TEST := tests/book_title_test.c app/book_title.c

all: build/xr_sim

build/xr_sim: $(CORE) $(FONTS) $(APP) $(SIM) $(wildcard include/xr/*.h app/*.h fonts/*.h port/sim/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) $(CORE) $(FONTS) $(APP) $(SIM) -o $@

build/xr_m5paper_gui: $(CORE) $(FONTS) $(APP) $(SDL_SIM) $(wildcard include/xr/*.h app/*.h fonts/*.h port/sim/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) $$(pkg-config --cflags sdl2) $(CORE) $(FONTS) $(APP) $(SDL_SIM) -o $@ $$(pkg-config --libs sdl2) -lz

gui: build/xr_m5paper_gui

build/xr_epub_import_test: $(EPUB_TEST) include/xr/xr_epub.h include/xr/xr_storage.h port/sim/sim_fatfs.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(EPUB_TEST) -o $@ -lz

test-epub: build/xr_epub_import_test
	./build/xr_epub_import_test /home/dao/data/sample.epub

build/xr_book_title_test: $(TITLE_TEST) app/book_title.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(TITLE_TEST) -o $@

test-title: build/xr_book_title_test
	./build/xr_book_title_test

fonts:
	python3 tools/gen_font.py fonts

run: build/xr_sim
	rm -rf out && ./build/xr_sim out && python3 tools/sim_report.py out

clean:
	rm -rf build out

.PHONY: all fonts gui test-epub test-title run clean

# --- refactor/cpp-design: restricted-C++20 tree (core/ reader/ app/ boards/) ---
# Scaffolding only until the corresponding phases land; see docs/MIGRATION.md.
CXX      ?= clang++
BOARD    ?= sim
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -fno-exceptions -fno-rtti \
            -fno-threadsafe-statics

CPP_HEADERS := $(shell find core reader app boards -type f -name '*.hpp' 2>/dev/null)
SELECTED    := build/selected_board.hpp

# app/assets.hpp bridges the existing C-compiled bitmap font/icon tables (byte-identical
# layout to text::font/xr_icon_glyph_t, verified by Phase 4/5 differential tests) into the
# new tree, so real pages render with real fonts before the font-generator-tooling phase
# regenerates them against this same shape.
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

cpp-all: build/reader

cpp-gui: build/simulator_gui
	./build/simulator_gui

test: build/reader build/simulator_test build/epub_test build/font_coverage_test
	./build/simulator_test
	./build/epub_test
	./build/font_coverage_test

format:
	clang-format -i $$(find core reader app boards tests -type f \( -name '*.hpp' -o -name '*.cpp' \) 2>/dev/null) app_main.cpp 2>/dev/null || true

format-check:
	@if command -v clang-format >/dev/null 2>&1; then \
		find core reader app boards tests -type f \( -name '*.hpp' -o -name '*.cpp' \) -print0 2>/dev/null | \
		xargs -0 -r clang-format --dry-run --Werror; \
	else \
		echo 'clang-format not installed; format check skipped'; \
	fi

.PHONY: cpp-all cpp-gui test format format-check
