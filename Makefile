CC      ?= cc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Wshadow
CFLAGS  += -Iinclude -Ifonts -Iapp -Iport/sim

CORE  := $(wildcard src/*.c)
FONTS := $(wildcard fonts/*.c)
APP   := $(wildcard app/*.c)
SIM   := $(wildcard port/sim/*.c)
SDL_SIM := port/sdl/m5_gui.c port/sim/sim_port.c

all: build/xr_sim

build/xr_sim: $(CORE) $(FONTS) $(APP) $(SIM) $(wildcard include/xr/*.h app/*.h fonts/*.h port/sim/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) $(CORE) $(FONTS) $(APP) $(SIM) -o $@

build/xr_m5paper_gui: $(CORE) $(FONTS) $(APP) $(SDL_SIM) $(wildcard include/xr/*.h app/*.h fonts/*.h port/sim/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) $$(pkg-config --cflags sdl2) $(CORE) $(FONTS) $(APP) $(SDL_SIM) -o $@ $$(pkg-config --libs sdl2)

gui: build/xr_m5paper_gui

fonts:
	python3 tools/gen_font.py fonts

run: build/xr_sim
	rm -rf out && ./build/xr_sim out && python3 tools/sim_report.py out

clean:
	rm -rf build out

.PHONY: all fonts gui run clean
