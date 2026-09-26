// Altoids Gameboy - Screen Test
// Board: ESP32-S3 N16R8   Display: GMT024-10 V2.1 2.4" ST7789 240x320
// Library: "Adafruit ST7735 and ST7789 Library" (installs Adafruit GFX + BusIO too)

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// ---- Pins (match the wiring diagram) ----
#define TFT_SCK   12
#define TFT_MOSI  11
#define TFT_RST   10
#define TFT_DC     9
#define TFT_CS     8

// If RED shows as CYAN (colors look like a photo negative), change to true
#define INVERT_COLORS false

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);

void showColor(uint16_t color, const char* name, uint16_t textColor) {
  tft.fillScreen(color);
  tft.setTextColor(textColor);
  tft.setTextSize(4);
  tft.setCursor(20, 140);
  tft.print(name);
  Serial.print("Showing: ");
  Serial.println(name);
  delay(1000);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Screen test starting...");

  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);   // SCK, MISO (none), MOSI, CS
  tft.init(240, 320);                         // width, height
  tft.setSPISpeed(40000000);                  // 40 MHz
  tft.invertDisplay(INVERT_COLORS);
  tft.setRotation(0);                         // portrait
  Serial.println("Display init done");

  showColor(ST77XX_RED,   "RED",   ST77XX_WHITE);
  showColor(ST77XX_GREEN, "GREEN", ST77XX_BLACK);
  showColor(ST77XX_BLUE,  "BLUE",  ST77XX_WHITE);
  showColor(ST77XX_WHITE, "WHITE", ST77XX_BLACK);
  showColor(ST77XX_BLACK, "BLACK", ST77XX_WHITE);

  // Corner markers: checks the full 240x320 area is used
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRect(0, 0, 240, 320, ST77XX_WHITE);
  tft.fillRect(0,   0,   20, 20, ST77XX_RED);     // top-left
  tft.fillRect(220, 0,   20, 20, ST77XX_GREEN);   // top-right
  tft.fillRect(0,   300, 20, 20, ST77XX_BLUE);    // bottom-left
  tft.fillRect(220, 300, 20, 20, ST77XX_YELLOW);  // bottom-right
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  tft.setCursor(60, 40);
  tft.print("SCREEN OK");
  tft.setTextSize(2);
  tft.setCursor(30, 90);
  tft.print("ESP32-S3 + ST7789");
  Serial.println("Test pattern drawn. Bouncing square running.");
}

// Bouncing square: shows the screen updates smoothly
int x = 110, y = 200, dx = 3, dy = 2;
const int S = 20;

void loop() {
  tft.fillRect(x, y, S, S, ST77XX_BLACK);        // erase old
  x += dx; y += dy;
  if (x <= 2 || x >= 240 - S - 2) dx = -dx;
  if (y <= 130 || y >= 320 - S - 2) dy = -dy;
  tft.fillRect(x, y, S, S, ST77XX_CYAN);         // draw new
  delay(16);                                     // ~60 fps
}
