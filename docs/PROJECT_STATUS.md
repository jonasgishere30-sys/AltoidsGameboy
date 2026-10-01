# Altoids Gameboy - Project Status

Altoids-tin handheld game console running Arcade OS on an ESP32-S3.

## Firmware: Arcade OS v1.6 (Sept 30, 2026)

- Compiles for ESP32-S3. Not yet tested on the physical device.
- No WiFi or AI features on main (AI Chat is a separate unmerged PR).
- Scrolling menu.
- 15 games: Invaders, Asteroids, Dino, Racer, Tron, Stack, Jumper, Mines, Connect 4, Simon, and others.
- Notes app: 6 notes saved to flash, up to 1000 characters each.
- 2048 was removed in v1.6.

## Hardware

- ESP32-S3 N16R8
- 2.4 inch ST7789 display (GMT024-10 V2.1, 7-pin), backlight tied to VCC
- Unit Card KB2 keyboard
- 700mAh LiPo, TP4056 charger, MT3608 boost converter set to 5V

## Display wiring

| Display pin | ESP32-S3 pin |
|---|---|
| SCK | GPIO12 |
| SDA | GPIO11 |
| RST | GPIO10 |
| DC | GPIO9 |
| CS | GPIO8 |
| VCC | 3V3 |

Display settings: Adafruit ST7789 library, init(240,320), 40 MHz SPI, invertDisplay(false), setRotation(3) for landscape with the display pins on the left.

## Planned for version 2

- 1000mAh battery
- USB-C charging module with 5V output
- Larger screen
- Brightness knob
