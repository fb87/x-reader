# Simulator GUI

The simulator GUI is a native Wayland client. It does not use X11/Xlib, XWayland, SDL, or Qt.

## Backend

`boards/sim/wayland_backend.hpp` owns the reusable simulator window/input backend. It uses:

- `xdg_wm_base` / `xdg_surface` / `xdg_toplevel` for the desktop window,
- `wl_shm` with two XRGB8888 buffers for presentation,
- `wl_keyboard` for press/release timing and long-press classification,
- `wl_pointer` for touch-style clicks and wheel/rotary input.

Both `boards/sim/gui.cpp` and `boards/sim/lua_gui.cpp` use the same backend.

## Input

Keyboard input is normalized into the shared semantic event ABI. A key action is emitted on release,
with `duration_ms` and `long_press`. The shell receives the event first and stops propagation when it
consumes it.

F5 is reserved by the Lua simulator for manual hot reload.

## Build

```sh
make APP=cpp gui
make APP=lua gui
```

The build prefers `pkg-config --libs wayland-client`. When development metadata is unavailable but
the stable runtime library is installed, it can link directly to `libwayland-client.so.0`; the small
stable ABI/protocol declarations used by the simulator live in `boards/sim/wayland_protocol.hpp`.

A running Wayland compositor and valid `WAYLAND_DISPLAY` are required to launch the interactive GUI.
The autonomous simulator tests do not require a compositor.
