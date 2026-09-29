# XTeink X4 bring-up

The UI is already board-independent: physical keys are translated to `input::key_t`, then to
semantic `input::action_t` events. The XTeink X4 backend now provides the ADC/button layer.

## Hardware profile

- ESP32-C3, 16 MB flash, no PSRAM
- 800x480 GDEQ0426T82 e-paper, SSD1677 controller
- EPD SPI: SCK GPIO8, MOSI GPIO10, CS GPIO21, DC GPIO4, RST GPIO5, BUSY GPIO6
- microSD shares SCK/MOSI; MISO GPIO7, CS GPIO12
- ADC button ladder 1 on GPIO1: Back, Confirm, Left, Right
- ADC button ladder 2 on GPIO2: Up, Down
- Power button on GPIO3, active-low

## Input backend

`board/xteink/xteink_buttons.cpp` polls the two ADC ladders, debounces transitions, and emits
normal `input::key_t` events. No screen code contains XTeink-specific button logic.

Nominal ADC values are treated as broad windows rather than exact values. If a board revision
shows different readings, adjust only the threshold helper in `xteink_button_decode.hpp`.

The power key emits `key_power` only after a long press (800 ms by default) to avoid accidental
sleep while navigating.

## Remaining bring-up

The next hardware task is the SSD1677 display backend plus a shared SPI-bus owner for EPD and
microSD. Until that is merged, `main.cpp` still boots the M5Paper/IT8951E path by default.

For an X4 build use an ESP32-C3 target once the SSD1677 backend is selected:

```sh
idf.py set-target esp32c3
```
