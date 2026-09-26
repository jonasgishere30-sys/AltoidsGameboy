# Altoids Gameboy

A pocket game console in an Altoids tin: **ESP32-S3 + 2.4" ST7789 screen + M5Stack CardKB2 keyboard (Bluetooth)**,
running on a 700mAh LiPo.

![startup](docs/images/startup.gif)

![preview](docs/images/preview.png)

## Repo layout
| Folder | What's in it |
|---|---|
| `firmware/AltoidsOS_OneFile/` | **The main program** (Arcade OS). One file: boot animation, menu, 6 games, settings. |
| `docs/Wiring-and-Power.md` | Full wiring + power plan (battery, TP4056, switch, MT3608, display). |
| `docs/images/` | Wiring diagram, screenshots, startup animation. |
| `tests/ScreenTest/` | Simple screen test (use first if the screen shows nothing). |
| `tests/KeyboardScreenTest/` | Screen + CardKB2 Bluetooth test (shows raw key codes). |
| `archive/` | First BLE experiment, kept for reference only. |

## Upload the firmware
1. Arduino IDE → open `firmware/AltoidsOS_OneFile/AltoidsOS_OneFile.ino`
   (or paste its contents into any sketch — it is a single file).
2. Libraries: **Adafruit ST7735 and ST7789 Library** (+ Adafruit GFX), **NimBLE-Arduino** 2.x.
3. Tools: **ESP32S3 Dev Module**, Flash **16MB**, PSRAM **OPI PSRAM**,
   Partition **16M Flash (3MB APP/9.9MB FATFS)**, USB CDC On Boot **Disabled**.
4. Upload, then put the CardKB2 in BLE mode: **Fn + Sym + 4**. The **KB** dot turns green when connected.

## Controls
| Key | Action |
|---|---|
| **D** up · **X** down · **Z** left · **C** right | Move / navigate (arrow keys also work) |
| SPACE or ENTER | Select / action |
| ESC or BACKSPACE | Back / pause |
| P | Pause |

## Games
Snake · Blocks · Pong · Breakout · Flappy · 2048 — best scores are saved in flash.

## Hardware summary
| Part | Connection |
|---|---|
| ST7789 display | GND→GND, VCC→3V3, SCK→GPIO12, SDA→GPIO11, RST→GPIO10, DC→GPIO9, CS→GPIO8 |
| Power | LiPo → TP4056 → switch → MT3608 (5.00V) → ESP32 5V/GND, 220–470µF cap on boost output |
| Keyboard | M5Stack Unit CardKB2 over Bluetooth LE (no wires) |

Screen orientation: landscape with the display pins on the **left** (`SCREEN_ROTATION 3`).
If it's ever upside down: **Settings → Flip screen**.
