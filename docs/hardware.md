# Hardware

## M5Paper K049

The initial board is the M5Paper K049. The values below are design inputs and
must be checked against the device schematic during driver bring-up.

| Function | Controller or signal | ESP32 connection |
| --- | --- | --- |
| E-ink and SD MISO | SPI | GPIO 13 |
| E-ink and SD MOSI | SPI | GPIO 12 |
| E-ink and SD SCK | SPI | GPIO 14 |
| E-ink CS | SPI | GPIO 15 |
| E-ink busy | IT8951E status | GPIO 27 |
| Main power enable | Power rail | GPIO 2 |
| External power enable | Power rail | GPIO 5 |
| E-ink power enable | Power rail | GPIO 23 |
| SD CS | SPI | GPIO 4 |
| Internal I2C SDA | I2C | GPIO 21 |
| Internal I2C SCL | I2C | GPIO 22 |
| GT911 interrupt | GPIO | GPIO 36 |
| Rotary right | GPIO | GPIO 37 |
| Rotary press/power | GPIO | GPIO 38 |
| Rotary left | GPIO | GPIO 39 |
| USB serial RX/TX | UART | GPIO 3 / GPIO 1 |

The IT8951E panel is 540 x 960 pixels with 16 grayscale levels. The display
driver must define its native orientation and expose rotation as a logical
configuration rather than scattering coordinate transforms through the UI.

## Bring-up Order

1. Confirm pin mapping against the board schematic.
2. Reset and identify the IT8951E.
3. Render a full-screen black/white diagnostic pattern.
4. Render grayscale ramps and verify orientation.
5. Mount the SD card and read a known test file.
6. Read GT911 coordinates and verify rotation.
7. Verify rotary direction, press, and debounce.
8. Read the BM8563 clock.
9. Verify sleep, wake, and display power behavior.

## Hardware Safety

- Do not assume display refresh is interrupt-safe.
- Serialize all display transactions.
- Do not refresh during unsafe power transitions.
- Bound all SPI, I2C, and SD timeouts.
- Treat removable storage removal or corruption as recoverable user errors.
- Verify battery and power-control behavior on real hardware before enabling
  automatic deep sleep.

## Future Xteink 4

The Xteink 4 backend is blocked until the exact revision, SoC, display
controller, panel dimensions, touch hardware, storage bus, and pin map are
confirmed. No M5Paper pin or controller assumption may be reused implicitly.
