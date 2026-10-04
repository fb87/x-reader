CC      ?= cc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Wshadow
CFLAGS  += -Iinclude -Ifonts -Iapp -Iport/sim

CORE  := $(wildcard src/*.c)
FONTS := $(wildcard fonts/*.c)
APP   := $(wildcard app/*.c)
SIM   := $(wildcard port/sim/*.c)
SDL_SIM := port/sdl/m5_gui.c port/sim/sim_port.c port/sim/sim_fatfs.c
EPUB_TEST := tests/epub_import_test.c src/xr_epub.c port/sim/sim_fatfs.c

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

fonts:
	python3 tools/gen_font.py fonts

run: build/xr_sim
	rm -rf out && ./build/xr_sim out && python3 tools/sim_report.py out

clean:
	rm -rf build out

.PHONY: all fonts gui test-epub run clean
