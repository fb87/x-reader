# Hardware Target

## First Target: Original M5Paper

The first supported device is the original M5Paper, not a later similarly
named board.

Relevant characteristics:

| Area | Target characteristic |
| --- | --- |
| MCU | ESP32-D0WDQ6-V3, dual-core Xtensa LX6 |
| External RAM | 8 MB PSRAM |
| Flash | 16 MB SPI flash |
| Display | 4.7 inch, 540 x 960, 16-level grayscale |
| Display controller | IT8951E with ED047TC1 panel |
| Touch | GT911 capacitive touch |
| Storage | microSD/TF slot |
| Other input | Three-way dial switch and power/center action |

Exact pin definitions and power sequencing must be verified against the board
schematic before driver implementation. They belong in `platform/esp`, not in
generic display, touch, or reader code.

## Driver Policy

The project does not use M5Stack's Arduino libraries, M5Unified, M5EPD, or an
external BSP. Required board support is implemented with ESP-IDF GPIO, SPI,
I2C, SD, FatFs, and power-management APIs.

The `display_it8951.c` and `input_gt911.c` modules remain independent of reader
logic so they can be tested with hardware diagnostics and replaced by a future
platform implementation.

## Display Considerations

- Native resolution is 540 x 960.
- The panel supports 16 grayscale levels, but UI elements may use lower-bit
  intermediate buffers when memory or update speed benefits.
- A 4-bit framebuffer is 259,200 bytes before alignment.
- A 1-bit framebuffer is 64,800 bytes before alignment.
- PSRAM is suitable for large persistent framebuffers.
- SPI DMA staging memory must satisfy ESP-IDF's DMA-capability requirements.
- Busy waits must have timeouts.
- Full refresh is the correctness baseline.
- Partial refresh and waveform selection require hardware measurements for
  latency and ghosting.
- Sleep and wake sequences must leave the panel and controller in a defined
  state.

## Touch Considerations

- The GT911 is accessed over I2C with interrupt and reset control as required
  by the board wiring.
- Raw coordinates must be transformed to the reader's portrait coordinate
  system.
- Touch initialization and polling must not block indefinitely if the device
  is absent or held in reset.
- Calibration or orientation settings should be represented by board data,
  not scattered constants.

## Storage Considerations

- Mount the microSD card through ESP-IDF FatFs/VFS.
- Enable long filenames because EPUB filenames are user-visible and often
  exceed 8.3 limits.
- Treat card removal and I/O errors as recoverable reader errors.
- ZIP member paths are handled by the core ZIP module; they are not FatFs paths.
- Persist settings atomically so power loss does not corrupt reader state.

## Power Considerations

The architecture must allow the reader to:

- Put the display controller into a defined sleep state
- Unmount or synchronize storage before power-down
- Save the current logical reading position
- Configure touch and GPIO wake behavior deliberately
- Enter ESP32 light sleep or deep sleep after later measurement

Power safety, timeout handling, and recoverable shutdown are required from the
first implementation. Final sleep thresholds and power policy remain deferred
until display, touch, and storage behavior has been measured on hardware.

## Future XTEINK Support

XTEINK X4 devices are not one compatible hardware target. Known generations
use different MCUs, memory configurations, display controllers, inputs, and
pin mappings. The original X4 is based on ESP32-C3 without PSRAM, while later
Classic/V2 and Pro variants use ESP32-S3 configurations.

No XTEINK board component should be started until the exact model and hardware
revision are available for testing. A future implementation should add another
platform directory and preserve the core and Lua interfaces.

## Bring-Up Checklist

1. Confirm the board revision and schematic pin mapping.
2. Verify ESP32 target, flash size, and PSRAM initialization.
3. Initialize buses without the display or touch drivers.
4. Read and validate IT8951 device information.
5. Clear the display and render a fixed diagnostic pattern.
6. Validate grayscale ordering and display orientation.
7. Read GT911 contacts and validate transformed coordinates.
8. Mount a microSD card and read a known file.
9. Exercise display sleep/wake and timeout recovery.
10. Record internal RAM, PSRAM, stack use, and refresh timings.
