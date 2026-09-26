// Altoids Gameboy - Arcade OS v1.5 (landscape) - SINGLE FILE: just open this .ino and upload
// Boot animation, menu and 6 built-in games, controlled with the CardKB2 over BLE.
//
// Board:    ESP32-S3 N16R8 -> Tools: "ESP32S3 Dev Module", Flash Size 16MB,
//           PSRAM "OPI PSRAM", Partition "16M Flash (3MB APP/9.9MB FATFS)"
// Display:  GMT024-10 ST7789 240x320 used sideways as 320x240 (SCK 12, SDA 11, RST 10, DC 9, CS 8)
//           Pins on the LEFT. If the picture is upside down: Settings > Flip screen
//           (or change SCREEN_ROTATION below from 3 to 1).
// Keyboard: M5Stack Unit CardKB2 in BLE HID mode (Fn + Sym + 4)
// Libraries: "Adafruit ST7735 and ST7789 Library" (+ Adafruit GFX), "NimBLE-Arduino" 2.x
//
// Controls: D = up, X = down, Z = left, C = right (arrow keys also work),  SPACE / ENTER = select / action,
//           ESC / BACKSPACE = back / pause,  P = pause
//           In Ask AI / text boxes letters type; arrows scroll, TAB = scroll mode, ENTER send, ESC stop / back
// Ask AI:   Settings > WiFi (scan, pick, type password) and Settings > API keys (add a Perplexity key)

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <WiFi.h>
#include <NetworkClientSecure.h>
// ================= UI + games (all in one file) =================
// Boot animation -> playful transition -> menu -> AI chat / games / settings
// Colors, easing, text and the mascot. Everything draws into a 320x240 (landscape) canvas.
#include <Adafruit_GFX.h>
#include <math.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

typedef GFXcanvas16 Canvas;
static const int SW = 320, SH = 240;

// RGB888 -> RGB565 (a macro so it works before any function is defined)
#define rgb(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
// Palette: black & white with one teal accent + a colour per game
static const uint16_t C_BG     = rgb(0, 0, 0);
static const uint16_t C_CARD   = rgb(20, 20, 22);
static const uint16_t C_LINE   = rgb(44, 44, 48);
static const uint16_t C_DIM    = rgb(110, 110, 116);
static const uint16_t C_SOFT   = rgb(170, 170, 176);
static const uint16_t C_WHITE  = rgb(240, 240, 240);
static const uint16_t C_TEAL   = rgb(32, 184, 205);
static const uint16_t C_GREEN  = rgb(74, 222, 128);
static const uint16_t C_PURPLE = rgb(167, 139, 250);
static const uint16_t C_ORANGE = rgb(251, 146, 60);
static const uint16_t C_YELLOW = rgb(250, 204, 21);
static const uint16_t C_PINK   = rgb(244, 114, 182);
static const uint16_t C_RED    = rgb(248, 113, 113);
static const uint16_t C_BLUE   = rgb(96, 165, 250);

// ---- Types used by the drawing functions (kept above every function so the
//      sketch also compiles as one single .ino file) ----
enum Font { F_SMALL, F_REG, F_BOLD, F_BIG, F_HUGE };
enum Icon { IC_SNAKE, IC_BLOCKS, IC_PONG, IC_BREAKOUT, IC_FLAPPY, IC_RACE, IC_SETTINGS, IC_ABOUT, IC_AI };
struct Mascot {
  float cx = 120, cy = 130, s = 0.5f;  // centre of the screen box at rest
  float look = 0;       // -1 left .. 1 right
  float lookY = 0;      // -1 up .. 1 down
  float eyeOpen = 1;    // 1 open, 0 closed (blink)
  float eyeScale = 1;   // 0 = no eyes (pop-in)
  float happy = 0;      // 1 = eyes squint into little arcs
  float squash = 0;     // + wide/short, - tall/thin
  float lift = 0;       // pixels the box floats above its resting spot
  float dropY = 0;      // extra offset for drop-in
  float barW = 1;       // 0..1 width of the bar
  bool  showBar = true;
  uint16_t body = C_WHITE, eye = C_TEAL, bg = C_BG;
};

static inline float clamp01(float t) { return t < 0 ? 0 : (t > 1 ? 1 : t); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float seg(uint32_t t, uint32_t a, uint32_t b) { return t <= a ? 0 : (t >= b ? 1 : (float)(t - a) / (float)(b - a)); }
static inline float easeOutCubic(float t) { t = 1 - t; return 1 - t * t * t; }
static inline float easeInCubic(float t) { return t * t * t; }
static inline float easeInOutCubic(float t) { return t < 0.5f ? 4 * t * t * t : 1 - powf(-2 * t + 2, 3) / 2; }
static inline float easeOutBack(float t) { const float c1 = 1.70158f, c3 = c1 + 1; return 1 + c3 * powf(t - 1, 3) + c1 * powf(t - 1, 2); }
static inline float bump(float t) { return sinf(clamp01(t) * 3.14159265f); }   // 0 -> 1 -> 0
static inline int   ir(float v) { return (int)lroundf(v); }
static inline int   imin(int a, int b) { return a < b ? a : b; }
static inline int   imax(int a, int b) { return a > b ? a : b; }

// Mix two RGB565 colours: t=0 -> a, t=1 -> b
static uint16_t blend(uint16_t a, uint16_t b, float t) {
  t = clamp01(t);
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  int r = ir(ar + (br - ar) * t), g = ir(ag + (bg - ag) * t), bl = ir(ab + (bb - ab) * t);
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

static void rrect(Canvas& g, float x, float y, float w, float h, float r, uint16_t c) {
  int X = ir(x), Y = ir(y), W = ir(w), H = ir(h);
  if (W < 1 || H < 1) return;
  int R = imin(ir(r), imin(W, H) / 2);
  if (R < 1) g.fillRect(X, Y, W, H, c); else g.fillRoundRect(X, Y, W, H, R, c);
}

// Halve the brightness of the whole screen (for pause / game over overlays)
static void dimScreen(Canvas& g) {
  uint16_t* p = g.getBuffer();
  for (int i = 0; i < SW * SH; i++) p[i] = (p[i] >> 1) & 0x7BEF;
}

// ---- Text ----
static void setFont(Canvas& g, Font f) {
  switch (f) {
    case F_SMALL: g.setFont(nullptr); g.setTextSize(1); break;
    case F_REG:   g.setFont(&FreeSans9pt7b); g.setTextSize(1); break;
    case F_BOLD:  g.setFont(&FreeSansBold9pt7b); g.setTextSize(1); break;
    case F_BIG:   g.setFont(&FreeSansBold12pt7b); g.setTextSize(1); break;
    case F_HUGE:  g.setFont(&FreeSansBold18pt7b); g.setTextSize(1); break;
  }
}
static int textW(Canvas& g, Font f, const char* s) {
  setFont(g, f);
  int16_t x1, y1; uint16_t w, h;
  g.getTextBounds(s, 0, 60, &x1, &y1, &w, &h);
  return w;
}
// y = baseline for GFX fonts; for F_SMALL y = top of text
static void text(Canvas& g, Font f, int x, int y, const char* s, uint16_t c) {
  setFont(g, f); g.setTextColor(c); g.setTextWrap(false); g.setCursor(x, y); g.print(s);
}
static void textC(Canvas& g, Font f, int cx, int y, const char* s, uint16_t c) {
  setFont(g, f);
  int16_t x1, y1; uint16_t w, h;
  g.getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  g.setTextColor(c); g.setTextWrap(false); g.setCursor(cx - (int)w / 2 - x1, y); g.print(s);
}
static void textR(Canvas& g, Font f, int rx, int y, const char* s, uint16_t c) {
  setFont(g, f);
  int16_t x1, y1; uint16_t w, h;
  g.getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  g.setTextColor(c); g.setTextWrap(false); g.setCursor(rx - (int)w - x1, y); g.print(s);
}

// ---- Mascot: rounded screen with two pill eyes, sitting on a bar ----
// Proportions measured from the reference image (units at s = 1).

static void drawMascot(Canvas& g, const Mascot& m) {
  const float s = m.s;
  float W = 248 * s * (1 + 0.16f * m.squash);
  float H = 192 * s * (1 - 0.16f * m.squash);
  float st = fmaxf(28 * s, 2), R = 46 * s;
  float bottom = m.cy + 96 * s - m.lift + m.dropY;
  float top = bottom - H, left = m.cx - W / 2;
  if (m.showBar && m.barW > 0.01f) {
    float bw = 302 * s * m.barW, bh = fmaxf(26 * s, 2);
    rrect(g, m.cx - bw / 2, m.cy + 96 * s + 29 * s, bw, bh, bh / 2, m.body);
  }
  rrect(g, left, top, W, H, R, m.body);
  rrect(g, left + st, top + st, W - 2 * st, H - 2 * st, fmaxf(R - st, 1), m.bg);
  if (m.eyeScale > 0.02f) {
    float ew = 24 * s * m.eyeScale;
    float ehFull = 56 * s * m.eyeScale * (1 - 0.1f * m.squash);
    float eh = fmaxf(ehFull * m.eyeOpen, fmaxf(3 * s, 2));
    float ecy = top + H / 2 - 4 * s + m.lookY * 18 * s;
    float pairCx = m.cx + m.look * 34 * s;
    for (int i = -1; i <= 1; i += 2) {
      float ex = pairCx + i * 26 * s;
      if (m.happy > 0.5f) {
        // happy "^" eyes: two short thick strokes
        float hw = ew * 0.9f, t = fmaxf(ew * 0.45f, 2);
        float x0 = ex - hw, y0 = ecy + hw * 0.4f;
        for (int k = 0; k < ir(t); k++) {
          g.drawLine(ir(x0), ir(y0 + k), ir(ex), ir(ecy - hw * 0.5f + k), m.eye);
          g.drawLine(ir(ex), ir(ecy - hw * 0.5f + k), ir(ex + hw), ir(y0 + k), m.eye);
        }
      } else {
        rrect(g, ex - ew / 2, ecy - eh / 2, ew, eh, ew / 2, m.eye);
      }
    }
  }
}

// Four-point twinkle star
static void drawSparkle(Canvas& g, float x, float y, float r, uint16_t c) {
  if (r < 1) return;
  float w = fmaxf(r * 0.28f, 1);
  g.fillTriangle(ir(x), ir(y - r), ir(x - w), ir(y), ir(x + w), ir(y), c);
  g.fillTriangle(ir(x), ir(y + r), ir(x - w), ir(y), ir(x + w), ir(y), c);
  g.fillTriangle(ir(x - r), ir(y), ir(x), ir(y - w), ir(x), ir(y + w), c);
  g.fillTriangle(ir(x + r), ir(y), ir(x), ir(y - w), ir(x), ir(y + w), c);
}
// Turns CardKB2 HID key reports into game buttons and typed characters.
// Game / menu mode: Arrows or D/X/C/Z = direction (D up, X down, Z left, C right),
//                   SPACE/ENTER = A, ESC/BACKSPACE = B, P = pause, TAB = tab
// Text mode (chat, passwords, API keys): every letter types. Arrows still move,
//                   ENTER = A (send/save), ESC = B (back), TAB = tab, BACKSPACE deletes.
#include <stdint.h>
#include <string.h>

enum Btn : uint8_t { B_UP, B_DOWN, B_LEFT, B_RIGHT, B_A, B_B, B_PAUSE, B_TAB, B_COUNT };

struct Input {
  bool     held[B_COUNT]    = {};
  bool     pressed[B_COUNT] = {};   // true for exactly one frame when pressed
  bool     rep[B_COUNT]     = {};   // pressed + auto-repeat while held (menus)
  bool     anyPressed = false;
  uint32_t holdStart[B_COUNT] = {};
  uint32_t lastRep[B_COUNT]   = {};
  bool     pending[B_COUNT]   = {};
  bool     rawHeld[B_COUNT]   = {};
  uint8_t  prevKeys[6] = {};

  // ---- text typing ----
  bool     textMode = false;          // set by the UI each frame
  char     chq[64];                   // typed characters ('\b' = backspace)
  uint8_t  chHead = 0, chTail = 0;
  bool     bkspHeld = false, bkspNew = false;
  uint32_t bkspStart = 0, bkspLast = 0;

  static int map(uint8_t kc) {
    switch (kc) {
      case 0x52: case 0x07: return B_UP;      // Up arrow, D
      case 0x51: case 0x1B: return B_DOWN;    // Down arrow, X
      case 0x50: case 0x1D: return B_LEFT;    // Left arrow, Z
      case 0x4F: case 0x06: return B_RIGHT;   // Right arrow, C
      case 0x2C: case 0x28: return B_A;       // Space, Enter
      case 0x29: case 0x2A: return B_B;       // Esc, Backspace
      case 0x13:            return B_PAUSE;   // P
      case 0x2B:            return B_TAB;     // Tab
    }
    return -1;
  }
  // While typing only these keys act as buttons; everything else types
  static int mapText(uint8_t kc) {
    switch (kc) {
      case 0x52: return B_UP;
      case 0x51: return B_DOWN;
      case 0x50: return B_LEFT;
      case 0x4F: return B_RIGHT;
      case 0x28: case 0x58: return B_A;       // Enter, keypad Enter
      case 0x29: return B_B;                  // Esc
      case 0x2B: return B_TAB;                // Tab
    }
    return -1;
  }
  // US keyboard layout
  static char toChar(uint8_t kc, bool shift) {
    if (kc >= 0x04 && kc <= 0x1D) { char c = (char)('a' + (kc - 0x04)); return shift ? (char)(c - 32) : c; }
    if (kc >= 0x1E && kc <= 0x27) {
      static const char num[] = "1234567890", sym[] = "!@#$%^&*()";
      return shift ? sym[kc - 0x1E] : num[kc - 0x1E];
    }
    switch (kc) {
      case 0x2C: return ' ';
      case 0x2D: return shift ? '_' : '-';
      case 0x2E: return shift ? '+' : '=';
      case 0x2F: return shift ? '{' : '[';
      case 0x30: return shift ? '}' : ']';
      case 0x31: return shift ? '|' : '\\';
      case 0x33: return shift ? ':' : ';';
      case 0x34: return shift ? '"' : '\'';
      case 0x35: return shift ? '~' : '`';
      case 0x36: return shift ? '<' : ',';
      case 0x37: return shift ? '>' : '.';
      case 0x38: return shift ? '?' : '/';
    }
    return 0;
  }
  void pushChar(char c) {
    uint8_t n = (uint8_t)((chHead + 1) % sizeof(chq));
    if (n == chTail) return;              // full: drop
    chq[chHead] = c; chHead = n;
  }
  // Next typed character, 0 if none
  char getChar() {
    if (chTail == chHead) return 0;
    char c = chq[chTail]; chTail = (uint8_t)((chTail + 1) % sizeof(chq));
    return c;
  }
  void clearChars() { chHead = chTail = 0; }

  // Called with each 8-byte keyboard report: mod = modifier byte, keys = the 6 keycode bytes
  void onHid(uint8_t mod, const uint8_t keys[6]) {
    bool now[B_COUNT] = {};
    bool shift = (mod & 0x22) != 0;
    bool bk = false;
    for (int i = 0; i < 6; i++) {
      uint8_t kc = keys[i];
      if (kc < 0x04) continue;              // 0 = none, 1-3 = errors
      bool was = false;
      for (int j = 0; j < 6; j++) if (prevKeys[j] == kc) was = true;
      if (textMode) {
        if (kc == 0x2A) { bk = true; if (!was) bkspNew = true; continue; }
        char c = toChar(kc, shift);
        if (c) { if (!was) pushChar(c); continue; }
      }
      int b = textMode ? mapText(kc) : map(kc);
      if (b < 0) continue;
      now[b] = true;
      if (!was) pending[b] = true;
    }
    bkspHeld = bk;
    memcpy(rawHeld, now, sizeof(now));
    memcpy(prevKeys, keys, 6);
  }

  void releaseAll() { memset(rawHeld, 0, sizeof(rawHeld)); memset(prevKeys, 0, 6); bkspHeld = false; }

  // Call once per frame before the UI/game update
  void frame(uint32_t now) {
    anyPressed = false;
    for (int b = 0; b < B_COUNT; b++) {
      pressed[b] = pending[b];
      pending[b] = false;
      if (pressed[b]) { holdStart[b] = now; lastRep[b] = now; anyPressed = true; }
      held[b] = rawHeld[b] || pressed[b];
      rep[b] = pressed[b];
      if (!pressed[b] && rawHeld[b] && now - holdStart[b] > 320 && now - lastRep[b] > 110) {
        rep[b] = true; lastRep[b] = now;
      }
    }
    // backspace: one delete on press, then repeats while held
    if (bkspNew) { pushChar('\b'); bkspNew = false; bkspStart = bkspLast = now; }
    else if (bkspHeld && now - bkspStart > 400 && now - bkspLast > 60) { pushChar('\b'); bkspLast = now; }
  }
  uint32_t heldMs(Btn b, uint32_t now) const { return held[b] ? now - holdStart[b] : 0; }
};
// Functions the UI/games need from the hardware. Defined in AltoidsOS.ino (device)
// so the UI code itself stays hardware-independent.
#include <stdint.h>
uint32_t plat_millis();
long     plat_random(long n);                       // 0 .. n-1
int      plat_loadInt(const char* key, int def);
void     plat_saveInt(const char* key, int value);
int      plat_kbState();                            // 0 scanning, 1 connecting, 2 paired, 3 ready
void     plat_setFlip(bool flip);                    // rotate the screen 180 degrees

// ---- storage (strings) and big buffers ----
void     plat_loadStr(const char* key, char* out, int max);   // "" if missing
void     plat_saveStr(const char* key, const char* value);
void*    plat_bigAlloc(int bytes);                  // PSRAM on the device

// ---- WiFi ----
void     plat_wifiScanStart();
int      plat_wifiScanCount();                      // -1 still scanning, -2 failed, else count
void     plat_wifiScanGet(int i, char* ssid, int max, int* rssi, int* open);
void     plat_wifiConnect(const char* ssid, const char* pass);
void     plat_wifiDisconnect();
int      plat_wifiState();                          // 0 off, 1 connecting, 2 connected, 3 failed
int      plat_wifiReason();                         // last disconnect reason code
void     plat_wifiInfo(char* ssid, int max, char* ip, int ipmax, int* rssi);

// ---- AI request (runs in the background) ----
bool     plat_aiStart(const char* apiKey, const char* body, bool verifyTls);  // false if busy
int      plat_aiState();                            // 0 idle, 1 connecting, 2 waiting, 3 streaming, 4 done, 5 error
int      plat_aiRead(char* out, int max);           // new answer text since last call
void     plat_aiInfo(char* status, int smax, char* sources, int srcmax, char* err, int emax);
void     plat_aiCancel();
// "Ask" - AI chat in a Perplexity-style layout:
//   question as a bold title -> source chips -> little computer + "Answer" -> answer text
// Uses the Perplexity Agent API (POST https://api.perplexity.ai/v1/agent, streamed).
// Hardware-independent helpers for the AI chat:
//  - JSON writing / reading (just enough for the Perplexity Agent API)
//  - HTTP response + chunked transfer + server-sent-events line splitter
//  - Agent API stream event handling (text deltas, search queries, sources, errors)
//  - Unicode -> ASCII (the screen fonts only have ASCII)
// Everything is inside structs so the Arduino IDE's auto-prototypes never break it.
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// ---------------- fixed-size string builder ----------------
struct SBuf {
  char* p; int cap; int n; bool overflow;
  SBuf(char* buf, int c) : p(buf), cap(c), n(0), overflow(false) { if (cap > 0) p[0] = 0; }
  void addc(char c) { if (n < cap - 1) { p[n++] = c; p[n] = 0; } else overflow = true; }
  void add(const char* s) { while (*s) addc(*s++); }
  // JSON string with quotes and escaping
  void addJson(const char* s) {
    addc('"');
    for (; *s; s++) {
      unsigned char c = (unsigned char)*s;
      if (c == '"' || c == '\\') { addc('\\'); addc((char)c); }
      else if (c == '\n') add("\\n");
      else if (c == '\r') add("\\r");
      else if (c == '\t') add("\\t");
      else if (c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); add(b); }
      else addc((char)c);
    }
    addc('"');
  }
};

// ---------------- Unicode -> ASCII ----------------
struct Ascii {
  // Writes up to 3 chars for code point cp into o, returns count
  static int fromCp(uint32_t cp, char* o) {
    if (cp == '\n') { o[0] = '\n'; return 1; }
    if (cp == '\t') { o[0] = ' '; return 1; }
    if (cp < 0x20 || cp == 0x7F) return 0;
    if (cp < 0x80) { o[0] = (char)cp; return 1; }
    if (cp >= 0xC0 && cp <= 0xFF) {
      static const char lat[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
      o[0] = lat[cp - 0xC0]; return 1;
    }
    switch (cp) {
      case 0x2018: case 0x2019: case 0x201A: case 0x2032: case 0x00B4: o[0] = '\''; return 1;
      case 0x201C: case 0x201D: case 0x201E: case 0x2033: case 0x00AB: case 0x00BB: o[0] = '"'; return 1;
      case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: case 0x2212: o[0] = '-'; return 1;
      case 0x00A0: case 0x2002: case 0x2003: case 0x2004: case 0x2005: case 0x2006:
      case 0x2007: case 0x2008: case 0x2009: case 0x200A: case 0x202F: o[0] = ' '; return 1;
      case 0x2022: case 0x00B7: case 0x25CF: case 0x2023: case 0x25E6: o[0] = '*'; return 1;
      case 0x2026: o[0] = '.'; o[1] = '.'; o[2] = '.'; return 3;
      case 0x00D7: o[0] = 'x'; return 1;
      case 0x2192: o[0] = '-'; o[1] = '>'; return 2;
      case 0x2190: o[0] = '<'; o[1] = '-'; return 2;
      case 0x2264: o[0] = '<'; o[1] = '='; return 2;
      case 0x2265: o[0] = '>'; o[1] = '='; return 2;
      case 0x2248: o[0] = '~'; return 1;
      case 0x00B0: o[0] = 'd'; o[1] = 'e'; o[2] = 'g'; return 3;
      case 0x00B1: o[0] = '+'; o[1] = '/'; o[2] = '-'; return 3;
      case 0x00BD: o[0] = '1'; o[1] = '/'; o[2] = '2'; return 3;
      case 0x00BC: o[0] = '1'; o[1] = '/'; o[2] = '4'; return 3;
      case 0x00A9: o[0] = '('; o[1] = 'c'; o[2] = ')'; return 3;
      case 0x00AE: o[0] = '('; o[1] = 'R'; o[2] = ')'; return 3;
      case 0x2122: o[0] = 'T'; o[1] = 'M'; return 2;
      case 0x20AC: o[0] = 'E'; o[1] = 'U'; o[2] = 'R'; return 3;
      case 0x00A3: o[0] = 'G'; o[1] = 'B'; o[2] = 'P'; return 3;
      case 0x00A5: o[0] = 'Y'; o[1] = 'E'; o[2] = 'N'; return 3;
      case 0x200B: case 0x200C: case 0x200D: case 0xFEFF: case 0xFE0F: return 0;
    }
    if (cp >= 0x2600 && cp <= 0x27BF) return 0;     // symbols / dingbats
    if (cp >= 0x1F000) return 0;                    // emoji
    o[0] = '?'; return 1;
  }
};

// ---------------- minimal JSON reader ----------------
struct Json {
  static bool ws(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
  // Pointer to the value of "key". depth 1 = keys of the outer object, -1 = any depth.
  static const char* find(const char* s, const char* key, int wantDepth) {
    if (!s) return nullptr;
    int depth = 0; bool inStr = false; size_t kl = strlen(key);
    for (const char* p = s; *p; p++) {
      char c = *p;
      if (inStr) {
        if (c == '\\') { if (p[1]) p++; }
        else if (c == '"') inStr = false;
        continue;
      }
      if (c == '"') {
        if ((wantDepth < 0 || depth == wantDepth) && strncmp(p + 1, key, kl) == 0 && p[1 + kl] == '"') {
          const char* q = p + 2 + kl;
          while (ws(*q)) q++;
          if (*q == ':') { q++; while (ws(*q)) q++; return q; }
        }
        inStr = true; continue;
      }
      if (c == '{' || c == '[') depth++;
      else if (c == '}' || c == ']') depth--;
    }
    return nullptr;
  }
  static int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }
  static long hex4(const char* p) {
    long v = 0;
    for (int i = 0; i < 4; i++) { int h = hexv(p[i]); if (h < 0) return -1; v = v * 16 + h; }
    return v;
  }
  // Decode the JSON string at v (pointing at the opening quote) into printable ASCII.
  // Returns the number of chars written (0 if v is not a string). *end = char after the string.
  static int str(const char* v, char* out, int max, const char** end) {
    int n = 0;
    if (max > 0) out[0] = 0;
    if (end) *end = v;
    if (!v || *v != '"' || max <= 0) return 0;
    const unsigned char* p = (const unsigned char*)v + 1;
    char tmp[3];
    while (*p && *p != '"') {
      uint32_t cp;
      if (*p == '\\') {
        p++;
        if (!*p) break;
        switch (*p) {
          case 'n': cp = '\n'; p++; break;
          case 't': case 'b': case 'f': cp = ' '; p++; break;
          case 'r': cp = 0; p++; break;
          case 'u': {
            long h = hex4((const char*)p + 1);
            if (h < 0) { p++; cp = 0; break; }
            p += 5; cp = (uint32_t)h;
            if (cp >= 0xD800 && cp < 0xDC00 && p[0] == '\\' && p[1] == 'u') {
              long lo = hex4((const char*)p + 2);
              if (lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + ((uint32_t)lo - 0xDC00); p += 6; }
            }
            break;
          }
          default: cp = *p; p++; break;       // \" \\ \/
        }
      } else if (*p < 0x80) { cp = *p; p++; }
      else {                                   // raw UTF-8
        int extra = (*p >= 0xF0) ? 3 : (*p >= 0xE0) ? 2 : (*p >= 0xC0) ? 1 : 0;
        cp = *p & (0x3F >> extra); p++;
        for (int i = 0; i < extra && (*p & 0xC0) == 0x80; i++) { cp = (cp << 6) | (*p & 0x3F); p++; }
      }
      int k = cp ? Ascii::fromCp(cp, tmp) : 0;
      for (int i = 0; i < k && n < max - 1; i++) out[n++] = tmp[i];
    }
    out[n] = 0;
    if (end) *end = (const char*)(*p == '"' ? p + 1 : p);
    return n;
  }
  static int getStr(const char* json, const char* key, int depth, char* out, int max) {
    return str(find(json, key, depth), out, max, nullptr);
  }
};

// ---------------- HTTP response -> body lines ----------------
// Feed raw socket bytes; calls onLine for every body line (status line/headers handled here).
typedef void (*LineFn)(void* ctx, const char* line, int len, bool truncated);
struct HttpLines {
  int status; bool chunked; bool inBody;
  int ck;              // 0 size line, 1 data, 2 CRLF after data, 3 finished
  long ckLeft; char ckLine[24]; int ckLen;
  char hline[320]; int hlen;
  char* line; int cap; int len; bool trunc;
  LineFn fn; void* ctx;
  void begin(char* buf, int c, LineFn f, void* x) {
    status = 0; chunked = false; inBody = false; ck = 0; ckLeft = 0; ckLen = 0; hlen = 0;
    line = buf; cap = c; len = 0; trunc = false; fn = f; ctx = x;
  }
  void headerLine() {
    hline[hlen] = 0;
    if (status == 0) {                       // "HTTP/1.1 200 OK"
      const char* sp = strchr(hline, ' ');
      status = sp ? atoi(sp + 1) : -1;
      if (status == 0) status = -1;
    } else if (hlen == 0) {
      inBody = true;
    } else {
      for (int i = 0; i < hlen; i++) hline[i] = (char)tolower((unsigned char)hline[i]);
      if (strncmp(hline, "transfer-encoding:", 18) == 0 && strstr(hline, "chunked")) chunked = true;
    }
    hlen = 0;
  }
  void bodyByte(char c) {
    if (c == '\n') { line[len] = 0; fn(ctx, line, len, trunc); len = 0; trunc = false; }
    else if (c == '\r') {}
    else if (len < cap - 1) line[len++] = c;
    else trunc = true;
  }
  void feed(const uint8_t* d, int n) {
    for (int i = 0; i < n; i++) {
      char c = (char)d[i];
      if (!inBody) {
        if (c == '\n') headerLine();
        else if (c != '\r' && hlen < (int)sizeof(hline) - 1) hline[hlen++] = c;
        continue;
      }
      if (!chunked) { bodyByte(c); continue; }
      switch (ck) {
        case 0:
          if (c == '\n') {
            ckLine[ckLen] = 0; ckLeft = strtol(ckLine, nullptr, 16); ckLen = 0;
            ck = ckLeft > 0 ? 1 : 3;
          } else if (c != '\r' && ckLen < (int)sizeof(ckLine) - 1) ckLine[ckLen++] = c;
          break;
        case 1: bodyByte(c); if (--ckLeft <= 0) ck = 2; break;
        case 2: if (c == '\n') ck = 0; break;
        default: break;
      }
    }
  }
  void finish() { if (len > 0) { line[len] = 0; fn(ctx, line, len, trunc); len = 0; } }
};

// ---------------- Perplexity Agent API stream ----------------
// POST https://api.perplexity.ai/v1/agent  {"preset":..., "stream":true, "input":[...]}
// Events arrive as "data: {json}" lines; text comes in "response.output_text.delta" events,
// sources in "search_results" output items, and the stream ends with "data: [DONE]".
struct AiStream {
  HttpLines http;
  char* out; int outCap; int outLen;        // decoded answer text (ASCII)
  char status[72];                          // e.g. "Searching: best shrimp food"
  char sources[200]; int nSrc;              // domains separated by '\n'
  char err[200];
  char errBody[700]; int errLen;
  bool gotDone, failed, incomplete, gotText;

  static void lineCb(void* ctx, const char* l, int n, bool t) { ((AiStream*)ctx)->onLine(l, n, t); }
  void begin(char* lineBuf, int lineCap, char* outBuf, int outCapacity) {
    http.begin(lineBuf, lineCap, lineCb, this);
    out = outBuf; outCap = outCapacity; outLen = 0; if (outCap > 0) out[0] = 0;
    status[0] = sources[0] = err[0] = errBody[0] = 0; nSrc = 0; errLen = 0;
    gotDone = failed = incomplete = gotText = false;
  }
  void feed(const uint8_t* d, int n) { http.feed(d, n); }
  void finish() { http.finish(); if (http.status != 200) buildHttpError(); }

  void append(const char* s, int n) {
    for (int i = 0; i < n && outLen < outCap - 1; i++) out[outLen++] = s[i];
    out[outLen] = 0;
    if (n > 0) gotText = true;
  }
  void addSource(const char* url) {
    const char* p = strstr(url, "://"); p = p ? p + 3 : url;
    if (strncmp(p, "www.", 4) == 0) p += 4;
    char d[40]; int k = 0;
    while (*p && *p != '/' && *p != ':' && *p != '?' && *p != '#' && k < 39) d[k++] = *p++;
    d[k] = 0;
    if (!k || nSrc >= 6) return;
    // no duplicates
    const char* s = sources;
    while (*s) {
      const char* e = strchr(s, '\n'); int L = e ? (int)(e - s) : (int)strlen(s);
      if (L == k && strncmp(s, d, k) == 0) return;
      s += L; if (*s) s++;
    }
    size_t used = strlen(sources);
    if (used + k + 2 >= sizeof(sources)) return;
    if (used) strcat(sources, "\n");
    strcat(sources, d); nSrc++;
  }
  void onSearchItem(const char* j) {
    // first query -> status line
    const char* q = Json::find(j, "queries", -1);
    if (q && *q == '[') {
      q++; while (Json::ws(*q)) q++;
      char qs[56];
      if (Json::str(q, qs, sizeof(qs), nullptr) > 0) snprintf(status, sizeof(status), "Searching: %s", qs);
    }
    // every "url" -> source domain
    const char* p = j;
    while ((p = Json::find(p, "url", -1)) != nullptr) {
      char u[160]; const char* e;
      if (Json::str(p, u, sizeof(u), &e) > 0) addSource(u);
      p = e > p ? e : p + 1;
    }
  }
  void onLine(const char* l, int n, bool truncated) {
    (void)n;
    if (http.status != 200) {                  // error body: keep the start of it
      for (const char* s = l; *s && errLen < (int)sizeof(errBody) - 2; s++) errBody[errLen++] = *s;
      errBody[errLen++] = ' '; errBody[errLen] = 0;
      return;
    }
    if (strncmp(l, "data:", 5) != 0) return;    // ignore "event:", "id:", ": comments"
    const char* j = l + 5;
    while (*j == ' ') j++;
    if (strncmp(j, "[DONE]", 6) == 0) { gotDone = true; return; }
    char type[64];
    Json::getStr(j, "type", 1, type, sizeof(type));
    if (strcmp(type, "response.output_text.delta") == 0) {
      char buf[1024];
      int k = Json::getStr(j, "delta", 1, buf, sizeof(buf));
      append(buf, k);
      if (!status[0] || strncmp(status, "Searching", 9) == 0) strcpy(status, "Answering");
    } else if (strncmp(type, "response.output_item.", 21) == 0 || (!type[0] && truncated)) {
      char it[40];
      Json::getStr(j, "type", 2, it, sizeof(it));
      if (strcmp(it, "search_results") == 0 || (!it[0] && strstr(j, "\"search_results\""))) onSearchItem(j);
      else if (!gotText && strcmp(it, "fetch_url_results") == 0) strcpy(status, "Reading pages");
    } else if (strcmp(type, "response.created") == 0 || strcmp(type, "response.in_progress") == 0) {
      if (!status[0]) strcpy(status, "Thinking");
    } else if (strcmp(type, "error") == 0) {
      char m[160];
      if (Json::getStr(j, "message", -1, m, sizeof(m)) > 0) snprintf(err, sizeof(err), "%s", m);
      else strcpy(err, "The API sent an error");
      failed = true;
    } else if (strcmp(type, "response.failed") == 0) {
      if (!err[0]) {
        char m[160];
        if (Json::getStr(j, "message", -1, m, sizeof(m)) > 0) snprintf(err, sizeof(err), "%s", m);
        else strcpy(err, "The request failed");
      }
      failed = true;
    } else if (strcmp(type, "response.incomplete") == 0) {
      incomplete = true;
    }
  }
  void buildHttpError() {
    char m[150] = "";
    Json::getStr(errBody, "message", -1, m, sizeof(m));
    int s = http.status;
    const char* hint = "";
    if (s == 401) hint = "API key not accepted. Check the key in Settings > API keys.";
    else if (s == 402) hint = "No API credits left on this account.";
    else if (s == 403) hint = "This key is not allowed to use the Agent API.";
    else if (s == 429) hint = "Too many requests or out of credits. Wait a bit.";
    else if (s >= 500) hint = "Perplexity server problem. Try again.";
    SBuf e(err, sizeof(err));
    if (s <= 0) { e.add("No valid reply from the server."); }
    else {
      char num[12]; snprintf(num, sizeof(num), "%d", s);
      e.add("Error "); e.add(num);
      if (m[0]) { e.add(": "); e.add(m); }
      if (hint[0]) { e.add(m[0] ? ". " : ". "); e.add(hint); }
    }
    failed = true;
  }
};

// ---- turns raw model markdown into clean screen text ----
// Output lines: "\x01" prefix = heading, "\x02" prefix = bullet, empty line = paragraph gap.
struct AiText {
  static void put(char* out, int& o, int max, char c) { if (o < max - 1) out[o++] = c; }
  // "[1]", "[web:1]", "[1, 2]" citation markers. Returns length, 0 = not one, -1 = cut off at the end
  static int citeLen(const char* p, const char* e) {
    const char* q = p + 1;
    const char* w = q;
    while (q < e && isalpha((unsigned char)*q)) q++;
    if (q < e && *q == ':' && q > w) q++;
    else if (q >= e) return -1;
    else q = w;
    if (q >= e) return -1;
    if (!isdigit((unsigned char)*q)) return 0;
    while (q < e && (isdigit((unsigned char)*q) || *q == ',' || *q == ' ' || *q == '-')) q++;
    if (q >= e) return -1;
    return *q == ']' ? (int)(q - p + 1) : 0;
  }
  static bool sepLine(const char* s, const char* e) {      // "|---|:--|" or "---"
    bool dash = false;
    for (const char* q = s; q < e; q++) {
      if (*q == '-') dash = true;
      else if (*q != '|' && *q != ':' && *q != ' ') return false;
    }
    return dash;
  }
  static int clean(const char* in, int n, char* out, int max, bool streaming) {
    int o = 0;
    const char* p = in; const char* e = in + n;
    bool lastBlank = true;
    while (p < e) {
      const char* le = p; while (le < e && *le != '\n') le++;
      const char* s = p; while (s < le && *s == ' ') s++;
      const char* lineEnd = le;
      while (lineEnd > s && lineEnd[-1] == ' ') lineEnd--;
      if (s == lineEnd) {                                // blank line
        if (!lastBlank) { put(out, o, max, '\n'); lastBlank = true; }
        p = le + 1; continue;
      }
      if (sepLine(s, lineEnd)) { p = le + 1; continue; }
      if (*s == '#') {
        while (s < lineEnd && *s == '#') s++;
        while (s < lineEnd && *s == ' ') s++;
        put(out, o, max, '\x01');
      } else if ((*s == '-' || *s == '*' || *s == '+') && s + 1 < lineEnd && s[1] == ' ') {
        s += 2; put(out, o, max, '\x02');
      } else if (*s == '|') {
        s++; while (s < lineEnd && *s == ' ') s++;
        while (lineEnd > s && (lineEnd[-1] == '|' || lineEnd[-1] == ' ')) lineEnd--;
      }
      bool atEnd = (le == e);
      const char* q = s;
      while (q < lineEnd) {
        char c = *q;
        if (c == '*') {
          if (q + 1 < lineEnd && q[1] == '*') { q += 2; continue; }
          bool spaced = q > s && q[-1] == ' ' && q + 1 < lineEnd && q[1] == ' ';
          if (!spaced) { q++; continue; }
        }
        if (c == '_' && q + 1 < lineEnd && q[1] == '_') { q += 2; continue; }
        if (c == '`') { q++; continue; }
        if (c == '\\' && q + 1 < lineEnd && strchr("()[]", q[1])) { q += 2; continue; }
        if (c == '[') {
          int k = citeLen(q, lineEnd);
          if (k < 0) {
            if (streaming && atEnd) break;               // still arriving: hide for now
            k = 0;
          }
          if (k > 0) {
            char nx = q + k < lineEnd ? q[k] : ' ';
            if (o > 0 && out[o - 1] == ' ' && strchr(" .,;:!?)", nx)) o--;
            q += k; continue;
          }
          // [text](url) -> text
          const char* r = q + 1; while (r < lineEnd && *r != ']') r++;
          if (r + 1 < lineEnd && r[1] == '(') {
            const char* t = r + 2; while (t < lineEnd && *t != ')') t++;
            if (t < lineEnd) {
              for (const char* z = q + 1; z < r; z++) if (*z != '*') put(out, o, max, *z);
              q = t + 1; continue;
            }
          }
        }
        put(out, o, max, c);
        q++;
      }
      while (o > 0 && out[o - 1] == ' ') o--;
      put(out, o, max, '\n');
      lastBlank = false;
      p = le + 1;
    }
    while (o > 0 && out[o - 1] == '\n') o--;
    out[o] = 0;
    return o;
  }
};

// line kinds for the chat layout (outside the struct so the Arduino IDE parses it)
enum ChatKind { L_Q, L_QS, L_SRC, L_LABEL, L_SKEL, L_BODY, L_HEAD, L_BUL1, L_BUL, L_NOTE, L_GAP, L_RULE };

struct Chat {
  static const int MAXM = 16, RAW_CAP = 6000, DISP_CAP = 6200, MAXL = 1400, BODY_CAP = 16384, IN_MAX = 300;
  static const int TOP = 34, BOT = 200, LX = 12, LW = 296;
  struct Msg {
    bool user, pending, noteErr, bad;   // noteErr = red note, bad = leave out of the history
    char* raw; int rawLen;
    char* disp; int dispLen;
    char src[200];
    char note[200];
  };
  struct Line { int16_t msg; uint16_t start, len; uint8_t kind, h; };

  Msg msgs[MAXM]; int nMsg = 0;
  Line* lines = nullptr; int nLines = 0, contentH = 0;
  char* body = nullptr;
  bool ready = false;

  char input[IN_MAX + 1] = ""; int inLen = 0;
  bool scrollMode = false, follow = true, busy = false;
  float scroll = 0, scrollDraw = 0;
  char status[72] = "";
  int aiSt = 0;
  char toast[96] = ""; uint32_t toastAt = 0;
  uint32_t sentAt = 0;

  // set by the App before update()
  const char* apiKey = "";
  bool shortAnswers = true, pro = false, verifyTls = true;

  void init() {
    if (ready) return;
    char* arena = (char*)plat_bigAlloc(MAXM * (RAW_CAP + DISP_CAP));
    lines = (Line*)plat_bigAlloc(MAXL * sizeof(Line));
    body = (char*)plat_bigAlloc(BODY_CAP);
    if (!arena || !lines || !body) return;
    for (int i = 0; i < MAXM; i++) {
      msgs[i].raw = arena + i * (RAW_CAP + DISP_CAP);
      msgs[i].disp = msgs[i].raw + RAW_CAP;
    }
    ready = true;
    clear();
  }
  void clear() { nMsg = 0; nLines = 0; contentH = 0; scroll = scrollDraw = 0; follow = true; }
  void enter() { init(); scrollMode = false; }

  // ---------------- layout ----------------
  static int adv(const GFXfont* f, char c) {
    uint8_t u = (uint8_t)c;
    if (u < f->first || u > f->last) return 0;
    return f->glyph[u - f->first].xAdvance;
  }
  void addLine(int m, int start, int len, ChatKind k, int h) {
    if (nLines >= MAXL) return;
    Line& L = lines[nLines++];
    L.msg = (int16_t)m; L.start = (uint16_t)start; L.len = (uint16_t)len; L.kind = k; L.h = (uint8_t)h;
    contentH += h;
  }
  // word-wrap text[from, to) at pixel width w
  void wrap(int m, const char* t, int from, int to, const GFXfont* f, int w, ChatKind first, ChatKind rest, int h) {
    int i = from; bool firstLine = true;
    while (i < to) {
      int ls = i, px = 0, lastSp = -1, end = to, next = to;
      while (i < to) {
        int cw = adv(f, t[i]);
        if (px + cw > w && i > ls) {
          if (lastSp > ls) { end = lastSp; next = lastSp + 1; }
          else { end = i; next = i; }
          break;
        }
        if (t[i] == ' ') lastSp = i;
        px += cw; i++;
      }
      if (i >= to) { end = to; next = to; }
      addLine(m, ls, end - ls, firstLine ? first : rest, h);
      firstLine = false;
      i = next;
      while (i < to && t[i] == ' ') i++;          // no leading spaces on wrapped lines
    }
  }
  void layout() {
    nLines = 0; contentH = 0;
    addLine(-1, 0, 0, L_GAP, 6);
    for (int m = 0; m < nMsg; m++) {
      Msg& M = msgs[m];
      if (M.user) {
        bool big = M.rawLen <= 70;
        wrap(m, M.raw, 0, M.rawLen, big ? &FreeSansBold12pt7b : &FreeSansBold9pt7b, LW, big ? L_Q : L_QS, big ? L_Q : L_QS, big ? 23 : 18);
        addLine(m, 0, 0, L_GAP, 6);
        continue;
      }
      if (M.src[0]) addLine(m, 0, 0, L_SRC, 24);
      addLine(m, 0, 0, L_LABEL, 22);
      if (M.pending && M.dispLen == 0) { for (int k = 0; k < 3; k++) addLine(m, k, 0, L_SKEL, 13); }
      const char* d = M.disp; int i = 0;
      while (i < M.dispLen) {
        int e = i; while (e < M.dispLen && d[e] != '\n') e++;
        if (e == i) addLine(m, 0, 0, L_GAP, 7);
        else if (d[i] == '\x01') wrap(m, d, i + 1, e, &FreeSansBold9pt7b, LW, L_HEAD, L_HEAD, 19);
        else if (d[i] == '\x02') wrap(m, d, i + 1, e, &FreeSans9pt7b, LW - 14, L_BUL1, L_BUL, 17);
        else wrap(m, d, i, e, &FreeSans9pt7b, LW, L_BODY, L_BODY, 17);
        i = e + 1;
      }
      if (M.note[0]) { addLine(m, 0, 0, L_GAP, 4); wrap(m, M.note, 0, (int)strlen(M.note), &FreeSans9pt7b, LW, L_NOTE, L_NOTE, 17); }
      if (m < nMsg - 1) { addLine(m, 0, 0, L_GAP, 10); addLine(m, 0, 0, L_RULE, 1); addLine(m, 0, 0, L_GAP, 12); }
      else addLine(m, 0, 0, L_GAP, 10);
    }
  }
  int maxScroll() const { int v = contentH - (BOT - TOP); return v > 0 ? v : 0; }

  // ---------------- messages ----------------
  Msg& push(bool user) {
    if (nMsg >= MAXM) {                    // drop the oldest question + answer
      Msg a = msgs[0], b = msgs[1];
      for (int i = 2; i < MAXM; i++) msgs[i - 2] = msgs[i];
      msgs[MAXM - 2] = a; msgs[MAXM - 1] = b;
      nMsg -= 2;
    }
    Msg& M = msgs[nMsg++];
    M.user = user; M.pending = false; M.noteErr = false; M.bad = false;
    M.rawLen = 0; M.raw[0] = 0; M.dispLen = 0; M.disp[0] = 0; M.src[0] = 0; M.note[0] = 0;
    return M;
  }
  void reclean(Msg& M) { M.dispLen = AiText::clean(M.raw, M.rawLen, M.disp, DISP_CAP, M.pending); }
  void showToast(const char* s, uint32_t now) { snprintf(toast, sizeof(toast), "%s", s); toastAt = now; }

  bool buildBody() {
    SBuf b(body, BODY_CAP);
    b.add("{\"preset\":"); b.addJson(pro ? "low" : "fast");
    b.add(",\"stream\":true,\"input\":[");
    // newest history that fits (the new question is msgs[nMsg-2])
    int start = nMsg - 2, budget = 9000;
    for (int i = nMsg - 3; i >= 1; i -= 2) {
      Msg& q = msgs[i - 1]; Msg& a = msgs[i];
      if (!q.user || a.user) break;
      if (a.bad || a.rawLen == 0) continue;
      budget -= q.rawLen + a.rawLen + 120;
      if (budget < 0) break;
      start = i - 1;
    }
    bool firstItem = true;
    for (int i = start; i < nMsg - 1; i++) {
      Msg& M = msgs[i];
      if (!M.user && (M.rawLen == 0 || M.bad)) continue;
      if (M.user && i + 1 < nMsg - 1 && (msgs[i + 1].rawLen == 0 || msgs[i + 1].bad)) { i++; continue; }  // skip failed turns
      if (!firstItem) b.addc(',');
      firstItem = false;
      b.add("{\"type\":\"message\",\"role\":"); b.addJson(M.user ? "user" : "assistant");
      b.add(",\"content\":");
      if (M.user && i == nMsg - 2 && shortAnswers) {
        static char tmp[IN_MAX + 120];
        snprintf(tmp, sizeof(tmp), "%s\n\n(Keep the answer short: under 120 words, plain text, no tables.)", M.raw);
        b.addJson(tmp);
      } else b.addJson(M.raw);
      b.addc('}');
    }
    b.add("]}");
    return !b.overflow;
  }
  void send(uint32_t now) {
    if (!inLen) return;
    if (!strcmp(input, "/new") || !strcmp(input, "/clear")) { clear(); inLen = 0; input[0] = 0; return; }
    if (busy) return;
    if (!apiKey || !apiKey[0]) { showToast("Add an API key first: Settings > API keys", now); return; }
    if (plat_wifiState() != 2) { showToast("No WiFi. Connect in Settings > WiFi", now); return; }
    Msg& q = push(true);
    memcpy(q.raw, input, inLen + 1); q.rawLen = inLen;
    Msg& a = push(false);
    a.pending = true;
    inLen = 0; input[0] = 0;
    if (!buildBody() || !plat_aiStart(apiKey, body, verifyTls)) {
      a.pending = false; a.noteErr = true; a.bad = true;
      snprintf(a.note, sizeof(a.note), "Could not start the request (busy or too long).");
    } else { busy = true; sentAt = now; status[0] = 0; aiSt = 1; }
    follow = true;
    layout();
  }
  void poll() {
    if (!busy || nMsg == 0) return;
    Msg& a = msgs[nMsg - 1];
    bool changed = false;
    char tmp[512]; int n;
    while ((n = plat_aiRead(tmp, sizeof(tmp))) > 0) {
      int room = RAW_CAP - 1 - a.rawLen;
      if (n > room) n = room;
      memcpy(a.raw + a.rawLen, tmp, n); a.rawLen += n; a.raw[a.rawLen] = 0;
      changed = true;
      if (room <= 0) break;
    }
    char src[200], err[200];
    plat_aiInfo(status, sizeof(status), src, sizeof(src), err, sizeof(err));
    if (strcmp(src, a.src)) { strcpy(a.src, src); changed = true; }
    int st = plat_aiState();
    aiSt = st;
    if (st == 4 || st == 5 || st == 0) {
      a.pending = false; busy = false;
      if (st == 5) {
        bool stopped = strcmp(err, "Stopped.") == 0;
        a.bad = true; a.noteErr = !stopped;
        snprintf(a.note, sizeof(a.note), "%s", err[0] ? err : "Something went wrong.");
      } else if (a.rawLen == 0) {
        a.bad = true; a.noteErr = true;
        snprintf(a.note, sizeof(a.note), "%s", err[0] ? err : "No answer received.");
      } else if (err[0]) snprintf(a.note, sizeof(a.note), "%s", err);   // e.g. answer cut off
      changed = true;
    }
    if (changed) { reclean(a); layout(); }
  }

  // ---------------- update ----------------
  // returns false when the user leaves the chat
  bool update(Input& in, uint32_t now, uint32_t dt) {
    in.textMode = !scrollMode;
    if (!scrollMode) {
      char c;
      while ((c = in.getChar()) != 0) {
        if (c == '\b') { if (inLen) input[--inLen] = 0; }
        else if (inLen < IN_MAX) { input[inLen++] = c; input[inLen] = 0; }
      }
    }
    if (in.pressed[B_TAB]) { scrollMode = !scrollMode; in.clearChars(); }
    float step = 0;
    if (in.rep[B_UP]) step -= 34;
    if (in.rep[B_DOWN]) step += 34;
    if (step != 0) {
      scroll += step; follow = false;
      if (scroll < 0) scroll = 0;
      if (scroll >= maxScroll()) { scroll = (float)maxScroll(); follow = true; }
    }
    if (in.pressed[B_A]) { if (scrollMode) { scrollMode = false; in.clearChars(); } else send(now); }
    if (in.pressed[B_B]) {
      if (busy) { plat_aiCancel(); }
      else if (scrollMode) { scrollMode = false; in.clearChars(); }
      else { in.textMode = false; return false; }
    }
    poll();
    if (follow) scroll = (float)maxScroll();
    if (scroll > maxScroll()) scroll = (float)maxScroll();
    float k = 1 - powf(0.0005f, dt / 1000.0f * 2.2f);
    scrollDraw = lerpf(scrollDraw, scroll, k);
    if (fabsf(scrollDraw - scroll) < 0.5f) scrollDraw = scroll;
    return true;
  }

  // ---------------- drawing ----------------
  void drawSub(Canvas& g, Font f, int x, int y, const char* s, int len, uint16_t c) {
    char buf[200]; if (len > 199) len = 199;
    memcpy(buf, s, len); buf[len] = 0;
    text(g, f, x, y, buf, c);
  }
  void drawLabel(Canvas& g, int y, const Msg& M, uint32_t now) {
    Mascot m; m.cx = 20; m.cy = y + 10; m.s = 0.062f;
    if (M.pending) { m.look = sinf(now * 0.006f); m.lift = 1.5f * (0.5f + 0.5f * sinf(now * 0.012f)); }
    else m.eyeOpen = 1 - bump(seg(now % 5000, 4700, 4880));
    drawMascot(g, m);
    if (M.pending && M.dispLen == 0) {
      const char* s = status[0] ? status : (aiSt <= 1 ? "Connecting" : "Searching the web");
      char b[80]; int dots = (now / 350) % 4;
      int keep = (int)strlen(s); if (keep > 64) keep = 64;
      snprintf(b, sizeof(b), "%.*s...", keep, s);
      while (keep > 4 && textW(g, F_REG, b) > LW - 30) { keep--; snprintf(b, sizeof(b), "%.*s...", keep, s); }
      if (keep == (int)strlen(s)) snprintf(b, sizeof(b), "%.*s%.*s", keep, s, dots, "...");   // animate only when it fits
      text(g, F_REG, 36, y + 15, b, C_SOFT);
    } else {
      text(g, F_BOLD, 36, y + 15, "Answer", C_WHITE);
    }
  }
  void drawSources(Canvas& g, int y, const Msg& M) {
    int x = LX; const char* s = M.src; int shown = 0, total = 0;
    for (const char* p = s; *p; p++) if (*p == '\n') total++;
    if (*s) total++;
    while (*s) {
      const char* e = strchr(s, '\n'); int L = e ? (int)(e - s) : (int)strlen(s);
      char d[40]; int k = L > 22 ? 22 : L; memcpy(d, s, k); d[k] = 0;
      int w = textW(g, F_SMALL, d) + 22;
      if (x + w > LX + LW - (total - shown > 1 ? 28 : 0)) break;
      rrect(g, x, y + 3, w, 17, 8, C_CARD);
      g.fillCircle(x + 8, y + 11, 2, C_TEAL);
      text(g, F_SMALL, x + 14, y + 8, d, C_SOFT);
      x += w + 5; shown++;
      s += L; if (*s) s++;
    }
    if (total > shown) {
      char b[16]; snprintf(b, sizeof(b), "+%d", total - shown);
      int w = textW(g, F_SMALL, b) + 12;
      rrect(g, x, y + 3, w, 17, 8, C_CARD);
      text(g, F_SMALL, x + 6, y + 8, b, C_DIM);
    }
  }
  void drawEmpty(Canvas& g, uint32_t now, bool wifiOk) {
    Mascot m; m.cx = 160; m.cy = 82; m.s = 0.22f;
    m.lift = 3 * (0.5f + 0.5f * sinf(now * 0.004f));
    m.look = sinf(now * 0.0013f) * 0.8f;
    m.eyeOpen = 1 - bump(seg(now % 3600, 3300, 3480));
    drawMascot(g, m);
    textC(g, F_BIG, 160, 150, "Ask anything", C_WHITE);
    if (!apiKey || !apiKey[0]) textC(g, F_SMALL, 160, 166, "ADD AN API KEY: SETTINGS > API KEYS", C_ORANGE);
    else if (!wifiOk) textC(g, F_SMALL, 160, 166, "CONNECT WIFI: SETTINGS > WIFI", C_ORANGE);
    else textC(g, F_SMALL, 160, 166, pro ? "PRO SEARCH  -  WEB ANSWERS WITH SOURCES" : "FAST SEARCH  -  WEB ANSWERS WITH SOURCES", C_DIM);
    textC(g, F_SMALL, 160, 182, "ENTER send   TAB scroll   /new clears", C_DIM);
  }
  void drawInputBar(Canvas& g, uint32_t now) {
    g.fillRect(0, BOT, SW, SH - BOT, C_BG);
    rrect(g, 8, 205, 304, 30, 15, C_CARD);
    g.drawRoundRect(8, 205, 304, 30, 15, scrollMode ? C_LINE : blend(C_TEAL, C_CARD, 0.45f));
    if (scrollMode) {
      text(g, F_SMALL, 22, 216, "SCROLL: D/X OR ARROWS   TAB TYPE", C_DIM);
    } else if (!inLen) {
      text(g, F_REG, 20, 225, busy ? "Answering..." : "Ask anything...", C_DIM);
      if (!busy && (now / 530) % 2) g.fillRect(19, 211, 2, 18, C_TEAL);
    } else {
      // show the end of the text if it is too long
      const int maxW = 246;
      const char* s = input; int w = textW(g, F_REG, s);
      while (w > maxW && *s) { s++; w = textW(g, F_REG, s); }
      text(g, F_REG, 20, 225, s, C_WHITE);
      if ((now / 530) % 2) g.fillRect(21 + w, 211, 2, 18, C_TEAL);
    }
    // send / stop button
    int bx = 293, by = 220;
    if (busy) {
      g.fillCircle(bx, by, 11, C_LINE);
      g.fillRect(bx - 4, by - 4, 8, 8, C_WHITE);
    } else {
      g.fillCircle(bx, by, 11, inLen ? C_TEAL : C_LINE);
      uint16_t ac = inLen ? C_BG : C_DIM;
      g.fillTriangle(bx, by - 6, bx - 5, by - 1, bx + 5, by - 1, ac);
      g.fillRect(bx - 1, by - 2, 3, 8, ac);
    }
  }
  void draw(Canvas& g, uint32_t now, bool wifiOk) {
    g.fillScreen(C_BG);
    if (!ready) { textC(g, F_BOLD, 160, 120, "Not enough memory (PSRAM off?)", C_RED); return; }
    if (nMsg == 0) drawEmpty(g, now, wifiOk);
    else {
      int y = TOP - ir(scrollDraw);
      for (int i = 0; i < nLines; i++) {
        const Line& L = lines[i];
        int h = L.h;
        if (y + h >= TOP - 4 && y <= BOT) {
          const Msg* M = L.msg >= 0 ? &msgs[L.msg] : nullptr;
          switch (L.kind) {
            case L_Q:    drawSub(g, F_BIG, LX, y + 17, M->raw + L.start, L.len, C_WHITE); break;
            case L_QS:   drawSub(g, F_BOLD, LX, y + 13, M->raw + L.start, L.len, C_WHITE); break;
            case L_SRC:  drawSources(g, y, *M); break;
            case L_LABEL: drawLabel(g, y, *M, now); break;
            case L_SKEL: {
              static const int ws[3] = {280, 240, 160};
              float pulse = 0.5f + 0.5f * sinf(now * 0.006f - L.start * 0.9f);
              rrect(g, LX, y + 3, ws[L.start % 3], 7, 3, blend(C_CARD, C_LINE, pulse));
              break;
            }
            case L_BODY: drawSub(g, F_REG, LX, y + 13, M->disp + L.start, L.len, rgb(218, 218, 222)); break;
            case L_HEAD: drawSub(g, F_BOLD, LX, y + 14, M->disp + L.start, L.len, C_WHITE); break;
            case L_BUL1: g.fillCircle(LX + 4, y + 8, 2, C_TEAL); // fall through
            case L_BUL:  drawSub(g, F_REG, LX + 14, y + 13, M->disp + L.start, L.len, rgb(218, 218, 222)); break;
            case L_NOTE: drawSub(g, F_REG, LX, y + 13, M->note + L.start, L.len, M->noteErr ? C_RED : C_ORANGE); break;
            case L_RULE: g.drawFastHLine(LX, y, LW, C_LINE); break;
            default: break;
          }
        }
        y += h;
        if (y > BOT + 30) break;
      }
      // scrollbar
      if (contentH > BOT - TOP) {
        int vh = BOT - TOP, th = imax(14, vh * vh / contentH);
        int ty = TOP + (int)((vh - th) * (scrollDraw / (float)imax(1, maxScroll())));
        rrect(g, 314, ty, 3, th, 1, scrollMode ? C_TEAL : C_LINE);
      }
    }
    drawInputBar(g, now);
    if (toast[0] && now - toastAt < 3200) {
      int w = textW(g, F_SMALL, toast) + 20;
      rrect(g, 160 - w / 2, 180, w, 18, 9, rgb(60, 36, 16));
      textC(g, F_SMALL, 160, 185, toast, C_ORANGE);
    }
  }
};
#include <stdio.h>

// Every game follows this shape. update() returns false when the player quits to the menu.
struct Game {
  virtual const char* saveKey() = 0;
  virtual void begin() = 0;
  virtual bool update(Input& in, uint32_t now, uint32_t dt) = 0;
  virtual void draw(Canvas& g, uint32_t now) = 0;
  virtual ~Game() {}

  // Shared pause / game-over handling
  enum Phase { PLAY, PAUSED, OVER } phase = PLAY;
  int score = 0, best = 0;
  uint32_t overAt = 0;

  void loadBest() { best = plat_loadInt(saveKey(), 0); }
  void gameOver(uint32_t now) {
    phase = OVER; overAt = now;
    if (score > best) { best = score; plat_saveInt(saveKey(), best); }
  }
  // Returns: 0 keep playing, 1 restart requested, 2 quit
  int handleMeta(Input& in, uint32_t now) {
    if (phase == PLAY && (in.pressed[B_PAUSE] || in.pressed[B_B])) { phase = PAUSED; return 0; }
    if (phase == PAUSED) {
      if (in.pressed[B_A] || in.pressed[B_PAUSE]) phase = PLAY;
      else if (in.pressed[B_B]) return 2;
      return 0;
    }
    if (phase == OVER && now - overAt > 600) {
      if (in.pressed[B_A]) return 1;
      if (in.pressed[B_B]) return 2;
    }
    return 0;
  }
  void drawHud(Canvas& g, const char* title, uint16_t accent) {
    g.fillRect(0, 0, SW, 26, C_BG);
    g.fillCircle(10, 13, 4, accent);
    text(g, F_BOLD, 20, 19, title, C_WHITE);
    char sc[16], bs[20];
    snprintf(sc, sizeof(sc), "%d", score);
    snprintf(bs, sizeof(bs), "BEST %d", best);
    int scW = textW(g, F_BOLD, sc);
    textR(g, F_BOLD, SW - 6, 19, sc, C_WHITE);
    int bsW = (int)strlen(bs) * 6;
    text(g, F_SMALL, SW - 6 - scW - 8 - bsW, 10, bs, C_DIM);
    g.drawFastHLine(0, 26, SW, C_LINE);
  }
  void drawOverlays(Canvas& g, uint32_t now, uint16_t accent) {
    const int cx = SW / 2;
    if (phase == PAUSED) {
      dimScreen(g);
      rrect(g, cx - 90, 70, 180, 100, 14, C_CARD);
      g.drawRoundRect(cx - 90, 70, 180, 100, 14, C_LINE);
      textC(g, F_BIG, cx, 108, "Paused", C_WHITE);
      textC(g, F_SMALL, cx, 130, "ENTER  resume", C_SOFT);
      textC(g, F_SMALL, cx, 146, "ESC    menu", C_SOFT);
    } else if (phase == OVER) {
      dimScreen(g);
      float t = easeOutBack(seg(now, overAt, overAt + 350));
      int y = ir(lerpf(SH + 20, 56, t));
      rrect(g, cx - 96, y, 192, 128, 16, C_CARD);
      g.drawRoundRect(cx - 96, y, 192, 128, 16, accent);
      textC(g, F_BIG, cx, y + 34, "Game Over", C_WHITE);
      char buf[32];
      snprintf(buf, sizeof(buf), "%d", score);
      textC(g, F_HUGE, cx, y + 76, buf, accent);
      bool rec = score > 0 && score >= best;
      if (rec) textC(g, F_SMALL, cx, y + 88, "NEW BEST!", C_YELLOW);
      textC(g, F_SMALL, cx, y + 108, "ENTER again   ESC menu", C_SOFT);
    }
  }
};

struct SnakeGame : Game {
  static const int CS = 12, COLS = 26, ROWS = 17, OX = 4, OY = 32;
  int8_t bx[COLS * ROWS], by[COLS * ROWS];
  int len, dx, dy, fx, fy;
  int8_t qdx[2], qdy[2]; int qn;
  uint32_t acc;
  const char* saveKey() override { return "snake"; }

  void placeFood() {
    for (int tries = 0; tries < 1000; tries++) {
      int x = plat_random(COLS), y = plat_random(ROWS);
      bool hit = false;
      for (int i = 0; i < len; i++) if (bx[i] == x && by[i] == y) { hit = true; break; }
      if (!hit) { fx = x; fy = y; return; }
    }
  }
  void begin() override {
    loadBest(); score = 0; phase = PLAY;
    len = 4; dx = 1; dy = 0; qn = 0; acc = 0;
    for (int i = 0; i < len; i++) { bx[i] = 8 - i; by[i] = 8; }
    placeFood();
  }
  void queueDir(int x, int y) {
    int lx = qn ? qdx[qn - 1] : dx, ly = qn ? qdy[qn - 1] : dy;
    if ((x == -lx && y == -ly) || (x == lx && y == ly) || qn >= 2) return;
    qdx[qn] = x; qdy[qn] = y; qn++;
  }
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    if (in.pressed[B_UP]) queueDir(0, -1);
    if (in.pressed[B_DOWN]) queueDir(0, 1);
    if (in.pressed[B_LEFT]) queueDir(-1, 0);
    if (in.pressed[B_RIGHT]) queueDir(1, 0);
    uint32_t interval = (uint32_t)imax(65, 150 - len * 2);
    acc += dt;
    while (acc >= interval && phase == PLAY) {
      acc -= interval;
      if (qn) { dx = qdx[0]; dy = qdy[0]; qdx[0] = qdx[1]; qdy[0] = qdy[1]; qn--; }
      int nx = bx[0] + dx, ny = by[0] + dy;
      bool eat = (nx == fx && ny == fy);
      bool dead = nx < 0 || ny < 0 || nx >= COLS || ny >= ROWS;
      int check = eat ? len : len - 1;
      for (int i = 0; i < check && !dead; i++) if (bx[i] == nx && by[i] == ny) dead = true;
      if (dead) { gameOver(now); break; }
      if (eat && len < COLS * ROWS) len++;
      for (int i = len - 1; i > 0; i--) { bx[i] = bx[i - 1]; by[i] = by[i - 1]; }
      bx[0] = nx; by[0] = ny;
      if (eat) { score += 10; placeFood(); }
    }
    return true;
  }
  void draw(Canvas& g, uint32_t now) override {
    g.fillScreen(C_BG);
    drawHud(g, "Snake", C_GREEN);
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++)
      g.drawPixel(OX + x * CS + CS / 2, OY + y * CS + CS / 2, C_LINE);
    float pulse = 0.5f + 0.5f * sinf(now * 0.008f);
    g.fillCircle(OX + fx * CS + 6, OY + fy * CS + 6, 4 + ir(pulse), C_RED);
    for (int i = len - 1; i >= 0; i--) {
      uint16_t c = blend(C_GREEN, rgb(20, 90, 50), (float)i / imax(len, 8));
      rrect(g, OX + bx[i] * CS + 1, OY + by[i] * CS + 1, CS - 2, CS - 2, 3, c);
    }
    // eyes on the head, facing the direction of travel
    int hx = OX + bx[0] * CS + 6, hy = OY + by[0] * CS + 6;
    int ex = dy != 0 ? 3 : 0, ey = dx != 0 ? 3 : 0;
    g.fillRect(hx - ex + dx * 2 - 1, hy - ey + dy * 2 - 1, 2, 2, C_BG);
    g.fillRect(hx + ex + dx * 2 - 1, hy + ey + dy * 2 - 1, 2, 2, C_BG);
    drawOverlays(g, now, C_GREEN);
  }
};

// Falling-blocks puzzle (Tetris-style)
struct BlocksGame : Game {
  static const int BW = 10, BH = 20, CS = 10, OX = 110, OY = 33;
  uint8_t board[BH][BW];
  int px, py, pr, pt, nextT;
  uint8_t bag[7]; int bagN;
  int lines, level;
  uint32_t fallAcc, groundMs, lrAcc; int lockResets;
  int clearRows[4], clearN; uint32_t clearAt;
  const char* saveKey() override { return "blocks"; }

  static uint16_t shape(int t, int r) {
    static const uint16_t S[7][4] = {
      {0x0F00, 0x2222, 0x00F0, 0x4444},  // I
      {0x8E00, 0x6440, 0x0E20, 0x44C0},  // J
      {0x2E00, 0x4460, 0x0E80, 0xC440},  // L
      {0x6600, 0x6600, 0x6600, 0x6600},  // O
      {0x6C00, 0x4620, 0x06C0, 0x8C40},  // S
      {0x4E00, 0x4640, 0x0E40, 0x4C40},  // T
      {0xC600, 0x2640, 0x0C60, 0x4C80}}; // Z
    return S[t][r & 3];
  }
  static bool cell(uint16_t m, int r, int c) { return (m >> (15 - (r * 4 + c))) & 1; }
  static uint16_t color(int t) {
    static const uint16_t C[7] = {C_TEAL, C_BLUE, C_ORANGE, C_YELLOW, C_GREEN, C_PURPLE, C_RED};
    return C[t];
  }
  int draw7() {
    if (bagN == 0) {
      for (int i = 0; i < 7; i++) bag[i] = i;
      for (int i = 6; i > 0; i--) { int j = plat_random(i + 1); uint8_t tmp = bag[i]; bag[i] = bag[j]; bag[j] = tmp; }
      bagN = 7;
    }
    return bag[--bagN];
  }
  bool fits(int t, int r, int x, int y) {
    uint16_t m = shape(t, r);
    for (int rr = 0; rr < 4; rr++) for (int cc = 0; cc < 4; cc++) if (cell(m, rr, cc)) {
      int bx = x + cc, by = y + rr;
      if (bx < 0 || bx >= BW || by >= BH) return false;
      if (by >= 0 && board[by][bx]) return false;
    }
    return true;
  }
  void spawn(uint32_t now) {
    pt = nextT; nextT = draw7(); pr = 0; px = 3; py = (pt == 0) ? -1 : 0;
    groundMs = 0; lockResets = 0;
    if (!fits(pt, pr, px, py)) gameOver(now);
  }
  void begin() override {
    loadBest(); score = 0; phase = PLAY;
    memset(board, 0, sizeof(board));
    bagN = 0; lines = 0; level = 1; fallAcc = 0; lrAcc = 0; clearN = 0;
    nextT = draw7(); spawn(0);
  }
  void lockPiece(uint32_t now) {
    uint16_t m = shape(pt, pr);
    bool above = false;
    for (int rr = 0; rr < 4; rr++) for (int cc = 0; cc < 4; cc++) if (cell(m, rr, cc)) {
      int by = py + rr;
      if (by < 0) above = true; else board[by][px + cc] = pt + 1;
    }
    if (above) { gameOver(now); return; }
    clearN = 0;
    for (int y = 0; y < BH; y++) {
      bool full = true;
      for (int x = 0; x < BW; x++) if (!board[y][x]) { full = false; break; }
      if (full) clearRows[clearN++] = y;
    }
    if (clearN) { clearAt = now; }
    else spawn(now);
  }
  void finishClear(uint32_t now) {
    for (int i = 0; i < clearN; i++) {
      for (int y = clearRows[i]; y > 0; y--) memcpy(board[y], board[y - 1], BW);
      memset(board[0], 0, BW);
    }
    static const int pts[5] = {0, 100, 300, 500, 800};
    score += pts[clearN] * level;
    lines += clearN; level = 1 + lines / 10;
    clearN = 0;
    spawn(now);
  }
  bool tryMove(int dx, int dy) {
    if (fits(pt, pr, px + dx, py + dy)) { px += dx; py += dy; return true; }
    return false;
  }
  void tryRotate() {
    static const int kicks[6][2] = {{0,0},{-1,0},{1,0},{-2,0},{2,0},{0,-1}};
    int nr = (pr + 1) & 3;
    for (auto& k : kicks) if (fits(pt, nr, px + k[0], py + k[1])) { pr = nr; px += k[0]; py += k[1]; return; }
  }
  void touched() { if (groundMs && lockResets < 15) { groundMs = 1; lockResets++; } }

  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    if (clearN) { if (now - clearAt > 180) finishClear(now); return true; }

    if (in.pressed[B_UP]) { tryRotate(); touched(); }
    // left/right with auto-repeat (170 ms delay, then every 50 ms)
    int dir = in.held[B_LEFT] ? -1 : in.held[B_RIGHT] ? 1 : 0;
    if (in.pressed[B_LEFT] || in.pressed[B_RIGHT]) { if (tryMove(dir, 0)) touched(); lrAcc = 0; }
    else if (dir && in.heldMs(dir < 0 ? B_LEFT : B_RIGHT, now) > 170) {
      lrAcc += dt; while (lrAcc >= 50) { lrAcc -= 50; if (tryMove(dir, 0)) touched(); }
    }
    if (in.pressed[B_A]) {   // hard drop
      int d = 0; while (tryMove(0, 1)) d++;
      score += d * 2; lockPiece(now); return true;
    }
    uint32_t interval = (uint32_t)imax(70, 800 - (level - 1) * 70);
    if (in.held[B_DOWN]) interval = imin(interval, 40);
    fallAcc += dt;
    while (fallAcc >= interval) {
      fallAcc -= interval;
      if (tryMove(0, 1)) { groundMs = 0; if (in.held[B_DOWN]) score += 1; }
      else if (!groundMs) groundMs = 1;
    }
    if (groundMs) {
      if (fits(pt, pr, px, py + 1)) groundMs = 0;
      else { groundMs += dt; if (groundMs > 450) lockPiece(now); }
    }
    return true;
  }
  void drawCell(Canvas& g, int x, int y, uint16_t c) {
    rrect(g, x + 1, y + 1, CS - 2, CS - 2, 2, c);
    g.drawFastHLine(x + 2, y + 2, CS - 5, blend(c, C_WHITE, 0.5f));
  }
  void draw(Canvas& g, uint32_t now) override {
    g.fillScreen(C_BG);
    drawHud(g, "Blocks", C_PURPLE);
    g.drawRoundRect(OX - 3, OY - 3, BW * CS + 6, BH * CS + 6, 4, C_LINE);
    for (int y = 0; y < BH; y++) for (int x = 0; x < BW; x++) {
      int sx = OX + x * CS, sy = OY + y * CS;
      bool flashing = false;
      for (int i = 0; i < clearN; i++) if (clearRows[i] == y) flashing = true;
      if (flashing) { g.fillRect(sx, sy, CS, CS, blend(C_WHITE, C_BG, seg(now, clearAt, clearAt + 180))); continue; }
      if (board[y][x]) drawCell(g, sx, sy, color(board[y][x] - 1));
      else g.drawPixel(sx + CS / 2, sy + CS / 2, C_LINE);
    }
    if (phase != OVER && !clearN) {
      int gy = py; while (fits(pt, pr, px, gy + 1)) gy++;
      uint16_t m = shape(pt, pr);
      for (int rr = 0; rr < 4; rr++) for (int cc = 0; cc < 4; cc++) if (cell(m, rr, cc)) {
        if (gy + rr >= 0) g.drawRoundRect(OX + (px + cc) * CS + 1, OY + (gy + rr) * CS + 1, CS - 2, CS - 2, 2, blend(color(pt), C_BG, 0.55f));
        if (py + rr >= 0) drawCell(g, OX + (px + cc) * CS, OY + (py + rr) * CS, color(pt));
      }
    }
    // left panel: next piece
    text(g, F_SMALL, 16, 40, "NEXT", C_DIM);
    rrect(g, 14, 52, 78, 54, 8, C_CARD);
    uint16_t nm = shape(nextT, 0);
    int offX = (nextT == 0 || nextT == 3) ? 15 : 21, offY = (nextT == 0) ? 12 : 18;
    for (int rr = 0; rr < 4; rr++) for (int cc = 0; cc < 4; cc++) if (cell(nm, rr, cc))
      rrect(g, 14 + offX + cc * 12 + 1, 52 + offY - 6 + rr * 12 + 1, 10, 10, 2, color(nextT));
    text(g, F_SMALL, 16, 150, "UP  rotate", C_DIM);
    text(g, F_SMALL, 16, 164, "DN  soft drop", C_DIM);
    text(g, F_SMALL, 16, 178, "SPC hard drop", C_DIM);
    text(g, F_SMALL, 16, 192, "ESC pause", C_DIM);
    // right panel: lines & level
    char buf[16];
    int sx = 232;
    text(g, F_SMALL, sx, 40, "LINES", C_DIM);
    snprintf(buf, sizeof(buf), "%d", lines); text(g, F_BIG, sx, 72, buf, C_WHITE);
    text(g, F_SMALL, sx, 92, "LEVEL", C_DIM);
    snprintf(buf, sizeof(buf), "%d", level); text(g, F_BIG, sx, 124, buf, C_PURPLE);
    drawOverlays(g, now, C_PURPLE);
  }
};

// Classic Pong: you are the left paddle (UP/DOWN), the CPU is on the right. First to 7 wins.
struct PongGame : Game {
  static const int TOP = 28, BOT = 240, PW = 6, PH = 42, BS = 6, PX = 10, CXP = 304;
  float pY, cY, bx, by, vx, vy, speed;
  int you, cpu; uint32_t serveAt; bool serving; int serveDir; bool won;
  const char* saveKey() override { return "pong"; }

  void serve(uint32_t now, int dir) {
    serving = true; serveAt = now; serveDir = dir;
    bx = SW / 2 - BS / 2; by = (TOP + BOT) / 2 - BS / 2; vx = vy = 0; speed = 190;
  }
  void begin() override {
    loadBest(); score = 0; phase = PLAY; you = cpu = 0; won = false;
    pY = cY = (TOP + BOT) / 2 - PH / 2; serve(0, 1);
  }
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    float t = dt / 1000.0f;
    if (in.held[B_UP])   pY -= 240 * t;
    if (in.held[B_DOWN]) pY += 240 * t;
    pY = fmaxf(TOP, fminf(BOT - PH, pY));
    // CPU: follows the ball while it is coming, drifts to centre otherwise
    float target = (vx > 0 ? by + BS / 2 : (TOP + BOT) / 2) - PH / 2;
    float cpuSpeed = 120 + 8 * (you + cpu);
    if (fabsf(target - cY) > 4) cY += (target > cY ? 1 : -1) * fminf(cpuSpeed * t, fabsf(target - cY));
    cY = fmaxf(TOP, fminf(BOT - PH, cY));

    if (serving) {
      if (now - serveAt > 800) {
        serving = false;
        float ang = (plat_random(60) - 30) * 3.14159f / 180;
        vx = cosf(ang) * speed * serveDir; vy = sinf(ang) * speed;
      }
      return true;
    }
    for (int step = 0; step < 4; step++) {
      bx += vx * t / 4; by += vy * t / 4;
      if (by < TOP) { by = TOP; vy = fabsf(vy); }
      if (by > BOT - BS) { by = BOT - BS; vy = -fabsf(vy); }
      if (vx < 0 && bx <= PX + PW && bx >= PX - 4 && by + BS >= pY && by <= pY + PH) bounce(pY, 1, true);
      if (vx > 0 && bx + BS >= CXP && bx + BS <= CXP + PW + 4 && by + BS >= cY && by <= cY + PH) bounce(cY, -1, false);
    }
    if (bx < -BS) { cpu++; if (cpu >= 7) { won = false; gameOver(now); } else serve(now, 1); }
    if (bx > SW) { you++; score += 100; if (you >= 7) { won = true; score += 500; gameOver(now); } else serve(now, -1); }
    return true;
  }
  void bounce(float padY, int dir, bool player) {
    float hit = ((by + BS / 2) - (padY + PH / 2)) / (PH / 2);   // -1 .. 1
    hit = fmaxf(-1, fminf(1, hit));
    speed = fminf(speed * 1.06f, 420);
    float ang = hit * 55 * 3.14159f / 180;
    vx = cosf(ang) * speed * dir; vy = sinf(ang) * speed;
    if (player) score += 10;
  }
  void draw(Canvas& g, uint32_t now) override {
    g.fillScreen(C_BG);
    drawHud(g, "Pong", C_WHITE);
    int mid = SW / 2;
    for (int y = TOP + 4; y < BOT; y += 14) g.fillRect(mid - 1, y, 2, 7, C_LINE);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", you); textC(g, F_HUGE, mid - 40, 70, buf, C_LINE);
    snprintf(buf, sizeof(buf), "%d", cpu); textC(g, F_HUGE, mid + 40, 70, buf, C_LINE);
    rrect(g, PX, pY, PW, PH, 3, C_TEAL);
    rrect(g, CXP, cY, PW, PH, 3, C_SOFT);
    if (!serving || (now / 150) % 2) rrect(g, bx, by, BS, BS, 2, C_WHITE);
    if (serving) textC(g, F_SMALL, mid, 200, "UP/DOWN to move", C_DIM);
    drawOverlays(g, now, C_TEAL);
    if (phase == OVER) textC(g, F_BOLD, mid, 48, won ? "You win!" : "CPU wins", won ? C_GREEN : C_RED);
  }
};

struct BreakoutGame : Game {
  static const int COLS = 10, ROWS = 6, BRW = 30, BRH = 10, BX0 = 1, BY0 = 46;
  static const int PADY = 222, PADH = 6, BALL_R = 3;
  uint8_t bricks[ROWS][COLS]; int left;
  float padX, padW, bx, by, vx, vy, speed;
  int lives, level; bool stuck;
  const char* saveKey() override { return "breakout"; }

  void resetBall() { stuck = true; bx = padX + padW / 2; by = PADY - BALL_R - 1; vx = vy = 0; }
  void buildLevel() {
    left = 0;
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) { bricks[r][c] = 1; left++; }
    speed = 190 + (level - 1) * 25;
  }
  void begin() override {
    loadBest(); score = 0; phase = PLAY; lives = 3; level = 1;
    padW = 52; padX = SW / 2 - padW / 2; buildLevel(); resetBall();
  }
  static uint16_t rowColor(int r) {
    static const uint16_t C[ROWS] = {C_RED, C_ORANGE, C_YELLOW, C_GREEN, C_TEAL, C_PURPLE};
    return C[r];
  }
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    float t = dt / 1000.0f;
    if (in.held[B_LEFT])  padX -= 300 * t;
    if (in.held[B_RIGHT]) padX += 300 * t;
    padX = fmaxf(0, fminf(SW - padW, padX));
    if (stuck) {
      bx = padX + padW / 2; by = PADY - BALL_R - 1;
      if (in.pressed[B_A] || in.pressed[B_UP]) {
        stuck = false; float a = (plat_random(40) - 20) * 3.14159f / 180;
        vx = sinf(a) * speed; vy = -cosf(a) * speed;
      }
      return true;
    }
    const int N = 6;
    for (int s = 0; s < N; s++) {
      bx += vx * t / N; by += vy * t / N;
      if (bx < BALL_R) { bx = BALL_R; vx = fabsf(vx); }
      if (bx > SW - BALL_R) { bx = SW - BALL_R; vx = -fabsf(vx); }
      if (by < 28 + BALL_R) { by = 28 + BALL_R; vy = fabsf(vy); }
      if (vy > 0 && by + BALL_R >= PADY && by + BALL_R <= PADY + PADH + 4 && bx >= padX - BALL_R && bx <= padX + padW + BALL_R) {
        float hit = fmaxf(-1, fminf(1, (bx - (padX + padW / 2)) / (padW / 2)));
        float a = hit * 62 * 3.14159f / 180;
        vx = sinf(a) * speed; vy = -cosf(a) * speed;
      }
      // bricks
      int c = (int)((bx - BX0) / (BRW + 2)), r = (int)((by - BY0) / (BRH + 2));
      for (int rr = r - 1; rr <= r + 1; rr++) for (int cc = c - 1; cc <= c + 1; cc++) {
        if (rr < 0 || rr >= ROWS || cc < 0 || cc >= COLS || !bricks[rr][cc]) continue;
        float x0 = BX0 + cc * (BRW + 2), y0 = BY0 + rr * (BRH + 2);
        if (bx + BALL_R < x0 || bx - BALL_R > x0 + BRW || by + BALL_R < y0 || by - BALL_R > y0 + BRH) continue;
        bricks[rr][cc] = 0; left--; score += 10 * (ROWS - rr);
        float ox = fminf(bx + BALL_R - x0, x0 + BRW - (bx - BALL_R));
        float oy = fminf(by + BALL_R - y0, y0 + BRH - (by - BALL_R));
        if (ox < oy) vx = -vx; else vy = -vy;
        rr = ROWS; break;
      }
    }
    if (by > SH + 6) {
      lives--;
      if (lives <= 0) gameOver(now); else resetBall();
    }
    if (left == 0) { level++; score += 200; buildLevel(); resetBall(); }
    return true;
  }
  void draw(Canvas& g, uint32_t now) override {
    g.fillScreen(C_BG);
    drawHud(g, "Breakout", C_ORANGE);
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) if (bricks[r][c])
      rrect(g, BX0 + c * (BRW + 2), BY0 + r * (BRH + 2), BRW, BRH, 2, rowColor(r));
    rrect(g, padX, PADY, padW, PADH, 3, C_WHITE);
    g.fillCircle(ir(bx), ir(by), BALL_R, C_WHITE);
    for (int i = 0; i < lives; i++) g.fillCircle(10 + i * 12, 36, 3, C_ORANGE);
    char buf[16]; snprintf(buf, sizeof(buf), "LEVEL %d", level);
    textR(g, F_SMALL, SW - 6, 33, buf, C_DIM);
    if (stuck && phase == PLAY) textC(g, F_SMALL, SW / 2, 170, "SPACE to launch", C_SOFT);
    drawOverlays(g, now, C_ORANGE);
  }
};

struct FlappyGame : Game {
  static const int GROUND = 218, PIPEW = 36, GAP = 84, NP = 4, SPACING = 140;
  float birdY, vel, scroll, pipeX[NP]; int gapY[NP]; bool passed[NP];
  bool started;
  const char* saveKey() override { return "flappy"; }

  void newPipe(int i, float x) { pipeX[i] = x; gapY[i] = 50 + plat_random(GROUND - 50 - GAP - 20); passed[i] = false; }
  void begin() override {
    loadBest(); score = 0; phase = PLAY; started = false;
    birdY = 110; vel = 0; scroll = 0;
    for (int i = 0; i < NP; i++) newPipe(i, SW + 40 + i * SPACING);
  }
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    float t = dt / 1000.0f;
    bool flap = in.pressed[B_A] || in.pressed[B_UP];
    if (!started) {
      birdY = 110 + sinf(now * 0.006f) * 6;
      scroll += 70 * t;
      if (flap) { started = true; vel = -270; }
      return true;
    }
    if (flap) vel = -270;
    vel = fminf(vel + 900 * t, 420);
    birdY += vel * t;
    float sp = 95 + imin(score, 40) * 1.5f;
    scroll += sp * t;
    for (int i = 0; i < NP; i++) {
      pipeX[i] -= sp * t;
      if (pipeX[i] < -PIPEW) {
        float far = pipeX[0]; for (int j = 1; j < NP; j++) far = fmaxf(far, pipeX[j]);
        newPipe(i, far + SPACING);
      }
      if (!passed[i] && pipeX[i] + PIPEW < 60) { passed[i] = true; score++; }
      // collision (bird radius 7 at x=60)
      if (60 + 6 > pipeX[i] && 60 - 6 < pipeX[i] + PIPEW && (birdY - 6 < gapY[i] || birdY + 6 > gapY[i] + GAP)) gameOver(now);
    }
    if (birdY + 7 >= GROUND) { birdY = GROUND - 7; gameOver(now); }
    if (birdY < 34) { birdY = 34; vel = 0; }
    return true;
  }
  void draw(Canvas& g, uint32_t now) override {
    g.fillScreen(C_BG);
    // twinkling stars
    for (int i = 0; i < 18; i++) {
      int sx = (i * 53 + 17) % SW, sy = 36 + (i * 97) % 170;
      sx = ((sx - (int)(scroll * 0.2f)) % SW + SW) % SW;
      g.drawPixel(sx, sy, ((now / 400 + i) % 5) ? C_LINE : C_SOFT);
    }
    for (int i = 0; i < NP; i++) {
      int x = ir(pipeX[i]);
      uint16_t pc = rgb(34, 197, 94), dk = rgb(21, 128, 61);
      g.fillRect(x, 27, PIPEW, gapY[i] - 27, pc);
      g.fillRect(x + PIPEW - 8, 27, 6, gapY[i] - 27, dk);
      rrect(g, x - 3, gapY[i] - 12, PIPEW + 6, 12, 3, pc);
      int by2 = gapY[i] + GAP;
      g.fillRect(x, by2, PIPEW, GROUND - by2, pc);
      g.fillRect(x + PIPEW - 8, by2, 6, GROUND - by2, dk);
      rrect(g, x - 3, by2, PIPEW + 6, 12, 3, pc);
    }
    g.fillRect(0, GROUND, SW, SH - GROUND, C_CARD);
    g.drawFastHLine(0, GROUND, SW, C_SOFT);
    for (int x = -((int)scroll % 16); x < SW; x += 16) g.drawLine(x, GROUND + 4, x + 8, GROUND + 12, C_LINE);
    // bird
    int bY = ir(birdY);
    g.fillCircle(60, bY, 7, C_YELLOW);
    int wing = (started && (now / 90) % 2) ? -3 : 1;
    rrect(g, 51, bY + wing, 8, 5, 2, blend(C_YELLOW, C_WHITE, 0.5f));
    g.fillCircle(63, bY - 2, 2, C_WHITE); g.drawPixel(64, bY - 2, C_BG);
    g.fillTriangle(66, bY, 72, bY + 2, 66, bY + 4, C_ORANGE);
    drawHud(g, "Flappy", C_YELLOW);
    if (!started && phase == PLAY) { textC(g, F_BOLD, SW / 2, 80, "Get ready", C_WHITE); textC(g, F_SMALL, SW / 2, 150, "SPACE or UP to flap", C_SOFT); }
    drawOverlays(g, now, C_YELLOW);
  }
};

// Turbo - pseudo-3D arcade racer (OutRun style).
// Gas is automatic. Z/C (or arrows) steer, X brakes, D / SPACE = nitro.
// Reach each checkpoint before the timer runs out. Overtaking cars gives bonus points.
struct RacerGame : Game {
  // world units
  static const int SEG_LEN = 200, ROAD_W = 2000, CAM_H = 1000, DRAW = 150, MAXSEG = 1700;
  static const int RUMBLE = 3, N_CARS = 12, CP_EVERY = 300, N_PROPS = 2;
  static constexpr float DEPTH = 0.84f;                 // 1 / tan(fov / 2), fov ~100 deg
  static constexpr float PLAYER_Z = CAM_H * DEPTH;      // distance from camera to the player's car
  static constexpr float MAX_SPEED = SEG_LEN * 60.0f;   // 60 segments per second
  static constexpr float CAR_W = 560;                   // car width in world units
  static constexpr float CENTRIFUGAL = 0.3f;

  struct Seg { float curve, y; };                       // y = height at the far edge
  struct Car { float z, x, speed; float prevRel; uint16_t col; };
  struct Proj { float x1, y1, w1, s1, x2, y2, w2, s2, clip; };

  Seg* segs = nullptr; int nSeg = 0; float trackLen = 0;
  Proj* proj = nullptr;
  Car cars[N_CARS];
  uint16_t sky[SH];

  float position, speed, playerX, dist, timeLeft, skyOff, nextCp, steerVis;
  int nitros, cpCount, overtakes, cpBonusShown;
  uint32_t startAt, boostUntil, crashAt, cpAt, overAt2;
  bool started;

  const char* saveKey() override { return "turbo"; }

  // ---------------- track ----------------
  float lastY() { return nSeg ? segs[nSeg - 1].y : 0; }
  void addSeg(float curve, float y) { if (nSeg < MAXSEG) { segs[nSeg].curve = curve; segs[nSeg].y = y; nSeg++; } }
  static float easeIn(float a, float b, float p) { return a + (b - a) * p * p; }
  static float easeInOut(float a, float b, float p) { return a + (b - a) * (-cosf(p * 3.14159265f) / 2 + 0.5f); }
  void addRoad(int enter, int hold, int leave, float curve, float hill) {
    float y0 = lastY(), y1 = y0 + hill * SEG_LEN; int total = enter + hold + leave;
    for (int n = 0; n < enter; n++) addSeg(easeIn(0, curve, (float)n / enter), easeInOut(y0, y1, (float)n / total));
    for (int n = 0; n < hold; n++)  addSeg(curve, easeInOut(y0, y1, (float)(enter + n) / total));
    for (int n = 0; n < leave; n++) addSeg(easeInOut(curve, 0, (float)n / leave), easeInOut(y0, y1, (float)(enter + hold + n) / total));
  }
  void buildTrack() {
    nSeg = 0;
    const int S = 25, M = 50, L = 100;
    addRoad(M, M, M, 0, 0);                 // start straight
    addRoad(S, S, S, 0, 20); addRoad(S, S, S, 0, -20);   // little bumps
    addRoad(M, M, M, 3, 30);                // right, uphill
    addRoad(M, M, M, 0, -30);
    addRoad(S, M, S, -4, 0);                // S-bends
    addRoad(S, M, S, 4, 0);
    addRoad(S, M, S, -4, 20);
    addRoad(L, M, L, 5, 40);                // long right, big hill
    addRoad(M, S, M, 0, -60);               // drop
    for (int i = 0; i < 4; i++) addRoad(S, S, S, 0, (i & 1) ? -15 : 15);   // rolling hills
    addRoad(M, L, M, -6, 0);                // hard left
    addRoad(M, M, M, 2, 25);
    addRoad(S, M, S, -3, -25);
    addRoad(M, M, M, 5, 0);
    addRoad(S, S, S, -5, 0);
    addRoad(L, M, L, 0, 0);
    // back down to height 0 so the loop joins smoothly
    float h = lastY() / SEG_LEN;
    addRoad(M, M, M, -2, -h);
    trackLen = (float)nSeg * SEG_LEN;
  }
  Seg& segAt(float z) { int i = (int)floorf(z / SEG_LEN) % nSeg; if (i < 0) i += nSeg; return segs[i]; }
  float segY1(int i) { return segs[(i + nSeg - 1) % nSeg].y; }   // height at the near edge
  float wrapZ(float z) { while (z >= trackLen) z -= trackLen; while (z < 0) z += trackLen; return z; }

  // ---------------- setup ----------------
  void begin() override {
    if (!segs) segs = (Seg*)plat_bigAlloc(MAXSEG * sizeof(Seg));
    if (!proj) proj = (Proj*)plat_bigAlloc(DRAW * sizeof(Proj));
    if (!segs || !proj) return;
    if (nSeg == 0) buildTrack();
    for (int y = 0; y < SH; y++) {           // sunset sky
      float t = clamp01(y / 128.0f);
      uint16_t top = rgb(28, 16, 64), mid = rgb(150, 44, 110), low = rgb(252, 140, 70);
      sky[y] = t < 0.6f ? blend(top, mid, t / 0.6f) : blend(mid, low, (t - 0.6f) / 0.4f);
    }
    loadBest(); score = 0; phase = PLAY; started = false;
    position = 0; speed = 0; playerX = 0; dist = 0; timeLeft = 25; skyOff = 0; steerVis = 0;
    nitros = 2; cpCount = 0; overtakes = 0; nextCp = CP_EVERY * SEG_LEN; cpBonusShown = 0;
    startAt = plat_millis(); boostUntil = crashAt = cpAt = overAt2 = 0;
    static const uint16_t cols[] = {rgb(96, 165, 250), rgb(250, 204, 21), rgb(74, 222, 128), rgb(167, 139, 250),
                                    rgb(255, 255, 255), rgb(244, 114, 182), rgb(251, 146, 60)};
    for (int i = 0; i < N_CARS; i++) {
      Car& c = cars[i];
      c.z = wrapZ(PLAYER_Z + 3000 + i * 2400.0f);
      c.x = (plat_random(3) - 1) * 0.62f;
      c.speed = MAX_SPEED * (0.28f + plat_random(30) / 100.0f);
      c.col = cols[i % 7];
      c.prevRel = 1;
    }
  }

  // ---------------- update ----------------
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY || !segs) return true;
    float t = dt / 1000.0f;
    if (!started) { if (now - startAt >= 2400) started = true; else return true; }   // 3-2-1-GO

    bool boost = now < boostUntil;
    if ((in.pressed[B_UP]) && nitros > 0 && !boost) { nitros--; boostUntil = now + 2200; boost = true; }
    float maxS = boost ? MAX_SPEED * 1.35f : MAX_SPEED;
    float pct = speed / MAX_SPEED;

    // steering + the curve pushing the car outwards
    Seg& ps = segAt(position + PLAYER_Z);
    float dx = t * 2.0f * fminf(pct, 1.0f);
    float steer = 0;
    if (in.held[B_LEFT]) steer -= 1;
    if (in.held[B_RIGHT]) steer += 1;
    playerX += steer * dx;
    playerX -= dx * pct * ps.curve * CENTRIFUGAL;
    steerVis = lerpf(steerVis, steer, fminf(1, t * 10));

    // speed
    if (in.held[B_DOWN]) speed -= MAX_SPEED * 1.1f * t;            // brake
    else speed += (boost ? MAX_SPEED * 0.9f : MAX_SPEED / 4.5f) * t; // automatic gas
    speed -= speed * 0.04f * t;                                       // drag
    bool offRoad = playerX < -1 || playerX > 1;
    if (offRoad && speed > MAX_SPEED / 4) speed -= MAX_SPEED * 0.9f * t;
    if (speed > maxS) speed = fmaxf(maxS, speed - MAX_SPEED * 0.6f * t);
    if (speed < 0) speed = 0;
    if (playerX < -2.3f) playerX = -2.3f;
    if (playerX > 2.3f) playerX = 2.3f;

    position = wrapZ(position + speed * t);
    dist += speed * t;
    skyOff += ps.curve * pct * t * 30;

    // traffic
    float pz = position + PLAYER_Z;
    for (int i = 0; i < N_CARS; i++) {
      Car& c = cars[i];
      c.z = wrapZ(c.z + c.speed * t);
      float rel = c.z - wrapZ(pz);
      if (rel > trackLen / 2) rel -= trackLen;
      if (rel < -trackLen / 2) rel += trackLen;
      if (fabsf(rel) < 160 && fabsf(c.x - playerX) < 0.5f && speed > c.speed) {   // crash
        speed = c.speed * 0.45f;
        position = wrapZ(c.z - PLAYER_Z - 170);
        crashAt = now; rel = 170;
        boostUntil = 0;
      }
      if (c.prevRel > 0 && rel <= 0 && rel > -2000) { overtakes++; }
      c.prevRel = rel;
      // respawn cars that fall far behind, ahead of the player
      if (rel < -3000) { c.z = wrapZ(pz + DRAW * SEG_LEN * 0.9f + plat_random(4000)); c.x = (plat_random(3) - 1) * 0.62f; c.prevRel = 1; }
    }

    // checkpoints + timer
    if (dist >= nextCp) {
      float bonus = fmaxf(6, 11 - cpCount / 2);
      timeLeft += bonus; cpBonusShown = (int)bonus;
      cpCount++; nextCp += CP_EVERY * SEG_LEN; cpAt = now;
      if (nitros < 3) nitros++;
    }
    timeLeft -= t;
    score = (int)(dist / SEG_LEN / 4) + overtakes * 25;
    if (timeLeft <= 0) { timeLeft = 0; gameOver(now); }
    return true;
  }

  // ---------------- drawing ----------------
  static void hline(Canvas& g, int x0, int x1, int y, uint16_t c) {
    if (x0 < 0) x0 = 0;
    if (x1 > SW) x1 = SW;
    if (x1 > x0) g.drawFastHLine(x0, y, x1 - x0, c);
  }
  // clipped filled box, bottom-clipped at "clip" (hills in front hide sprites)
  static void box(Canvas& g, float x, float y, float w, float h, float clip, uint16_t c) {
    int x0 = ir(x), y0 = ir(y), x1 = ir(x + w), y1 = ir(y + h);
    if (y1 > clip) y1 = (int)clip;
    if (x0 < 0) x0 = 0;
    if (x1 > SW) x1 = SW;
    if (y0 < 0) y0 = 0;
    if (x1 > x0 && y1 > y0) g.fillRect(x0, y0, x1 - x0, y1 - y0, c);
  }
  void drawSegment(Canvas& g, const Proj& p, int idx, float maxy) {
    bool light = (idx / RUMBLE) % 2;
    bool start = idx >= 4 && idx < 6;
    uint16_t grass = light ? rgb(22, 92, 70) : rgb(16, 76, 60);
    uint16_t rumble = light ? rgb(240, 240, 240) : rgb(220, 50, 60);
    uint16_t road = light ? rgb(70, 70, 82) : rgb(64, 64, 76);
    int top = (int)ceilf(p.y2), bot = (int)fminf(ceilf(p.y1), maxy);
    if (top < 0) top = 0;
    for (int y = top; y < bot; y++) {
      float k = (p.y1 - y) / (p.y1 - p.y2);
      float cx = p.x1 + (p.x2 - p.x1) * k, w = p.w1 + (p.w2 - p.w1) * k;
      float r = w / 7;
      hline(g, 0, SW, y, grass);
      hline(g, ir(cx - w - r), ir(cx - w), y, rumble);
      hline(g, ir(cx + w), ir(cx + w + r), y, rumble);
      if (start) {                                   // chequered start line
        int cells = 8;
        for (int c = 0; c < cells; c++)
          hline(g, ir(cx - w + 2 * w * c / cells), ir(cx - w + 2 * w * (c + 1) / cells), y,
                ((c + (idx & 1)) & 1) ? C_WHITE : rgb(20, 20, 24));
      } else {
        hline(g, ir(cx - w), ir(cx + w), y, road);
        if (light) {
          float lw = fmaxf(1, w / 36);
          for (int l = 1; l < 3; l++) { float lx = cx - w + 2 * w * l / 3; hline(g, ir(lx - lw / 2), ir(lx + lw / 2) + 1, y, rgb(230, 230, 230)); }
        }
      }
    }
  }
  // rear view of a car. x = centre, y = bottom, w = width in pixels
  void drawCar(Canvas& g, float x, float y, float w, uint16_t col, float clip, float lean) {
    float h = w * 0.52f, l = x - w / 2;
    uint16_t dark = blend(col, C_BG, 0.45f);
    box(g, l + w * 0.04f, y - h * 0.14f, w * 0.92f, h * 0.16f, clip, rgb(10, 10, 12));      // shadow / tyres
    box(g, l + w * 0.06f, y - h * 0.22f, w * 0.16f, h * 0.22f, clip, rgb(18, 18, 20));      // wheels
    box(g, l + w * 0.78f, y - h * 0.22f, w * 0.16f, h * 0.22f, clip, rgb(18, 18, 20));
    box(g, l, y - h * 0.62f, w, h * 0.44f, clip, col);                                      // body
    box(g, l + w * 0.18f + lean, y - h, w * 0.64f, h * 0.40f, clip, dark);                   // cabin
    box(g, l + w * 0.24f + lean, y - h * 0.92f, w * 0.52f, h * 0.26f, clip, rgb(40, 60, 90)); // rear window
    box(g, l + w * 0.05f, y - h * 0.50f, w * 0.20f, h * 0.10f, clip, rgb(255, 60, 60));      // tail lights
    box(g, l + w * 0.75f, y - h * 0.50f, w * 0.20f, h * 0.10f, clip, rgb(255, 60, 60));
    box(g, l + w * 0.36f, y - h * 0.36f, w * 0.28f, h * 0.10f, clip, rgb(230, 230, 230));    // plate
  }
  void drawPalm(Canvas& g, float x, float y, float s, float clip) {
    float h = s * 2.6f, tw = fmaxf(1, s * 0.12f);
    box(g, x - tw / 2, y - h, tw, h, clip, rgb(90, 60, 40));
    uint16_t leaf = rgb(20, 120, 80);
    for (int i = -2; i <= 2; i++) box(g, x + i * s * 0.28f - s * 0.25f, y - h - s * 0.12f + abs(i) * s * 0.1f, s * 0.5f, fmaxf(1, s * 0.14f), clip, leaf);
  }
  void drawSky(Canvas& g, uint32_t now) {
    for (int y = 0; y < 132; y++) g.drawFastHLine(0, y, SW, sky[y]);
    // retro striped sun
    int sx = 160 - ((int)(skyOff * 0.4f) % 640 + 640) % 640 + 320; if (sx > 480) sx -= 640;
    for (int dy = -30; dy <= 30; dy++) {
      int yy = 98 + dy;
      if (dy > 4 && ((dy / 4) % 2)) continue;
      int hw = (int)sqrtf(900 - dy * dy);
      uint16_t c = blend(rgb(255, 230, 90), rgb(255, 90, 120), (dy + 30) / 60.0f);
      hline(g, sx - hw, sx + hw + 1, yy, c);
    }
    // two layers of mountains (parallax)
    for (int x = 0; x < SW; x += 2) {
      float fx = x + skyOff * 0.8f;
      int h1 = 22 + (int)(12 * sinf(fx * 0.011f) + 8 * sinf(fx * 0.027f + 1) + 4 * sinf(fx * 0.061f + 2));
      g.fillRect(x, 124 - h1, 2, h1, rgb(70, 30, 90));
      float fx2 = x + skyOff * 1.6f;
      int h2 = 10 + (int)(7 * sinf(fx2 * 0.019f + 3) + 5 * sinf(fx2 * 0.043f));
      g.fillRect(x, 124 - h2, 2, h2, rgb(40, 20, 60));
    }
    g.fillRect(0, 124, SW, SH - 124, rgb(16, 76, 60));
  }
  void draw(Canvas& g, uint32_t now) override {
    if (!segs || !proj) { g.fillScreen(C_BG); textC(g, F_BOLD, 160, 120, "Not enough memory", C_RED); return; }
    int shake = (crashAt && now - crashAt < 300) ? (int)(plat_random(7)) - 3 : 0;
    drawSky(g, now);

    // ---- road, front to back ----
    int base = (int)floorf(position / SEG_LEN) % nSeg;
    float basePct = fmodf(position, SEG_LEN) / SEG_LEN;
    float pz = position + PLAYER_Z;
    int pSeg = (int)floorf(pz / SEG_LEN) % nSeg;
    float pPct = fmodf(pz, SEG_LEN) / SEG_LEN;
    float playerY = lerpf(segY1(pSeg), segs[pSeg].y, pPct);
    float camY = playerY + CAM_H, camX = playerX * ROAD_W;
    float x = 0, dx = -(segs[base].curve * basePct);
    float maxy = SH;
    for (int n = 0; n < DRAW; n++) {
      int i = (base + n) % nSeg;
      bool looped = i < base;
      float camZ = position - (looped ? trackLen : 0);
      float z1 = (float)i * SEG_LEN - camZ, z2 = z1 + SEG_LEN;
      Proj& p = proj[n];
      p.clip = maxy;
      p.s1 = DEPTH / fmaxf(z1, 1); p.s2 = DEPTH / fmaxf(z2, 1);
      p.x1 = SW / 2 + p.s1 * (-(camX - x)) * SW / 2 + shake;
      p.x2 = SW / 2 + p.s2 * (-(camX - x - dx)) * SW / 2 + shake;
      p.y1 = SH / 2 - p.s1 * (segY1(i) - camY) * SH / 2;
      p.y2 = SH / 2 - p.s2 * (segs[i].y - camY) * SH / 2;
      p.w1 = p.s1 * ROAD_W * SW / 2; p.w2 = p.s2 * ROAD_W * SW / 2;
      x += dx; dx += segs[i].curve;
      if (z1 <= DEPTH || p.y2 >= p.y1 || p.y2 >= maxy) { p.s1 = 0; continue; }
      drawSegment(g, p, i, maxy);
      maxy = p.y2;
    }

    // ---- sprites, back to front ----
    int cpSeg = (int)floorf(wrapZ(nextCp - dist + pz) / SEG_LEN) % nSeg;   // next checkpoint
    for (int n = DRAW - 1; n > 0; n--) {
      Proj& p = proj[n];
      if (p.s1 <= 0) continue;
      int i = (base + n) % nSeg;
      float sc = p.w1 / ROAD_W;                                   // pixels per world unit
      if (i % 8 == 0) {                                          // palms along both sides
        float s = sc * 900;
        drawPalm(g, p.x1 - p.w1 * 1.45f, p.y1, s, p.clip);
        drawPalm(g, p.x1 + p.w1 * 1.45f, p.y1, s, p.clip);
      }
      if (i == cpSeg) {                                          // checkpoint arch
        float ph = sc * 2300, pw = fmaxf(2, sc * 180);
        uint16_t ac = rgb(250, 204, 21);
        box(g, p.x1 - p.w1 * 1.2f - pw / 2, p.y1 - ph, pw, ph, p.clip, ac);
        box(g, p.x1 + p.w1 * 1.2f - pw / 2, p.y1 - ph, pw, ph, p.clip, ac);
        float bh = sc * 420;
        box(g, p.x1 - p.w1 * 1.2f, p.y1 - ph, p.w1 * 2.4f, bh, p.clip, rgb(30, 30, 36));
        if (bh > 12 && p.y1 - ph + bh < p.clip) textC(g, F_SMALL, ir(p.x1), ir(p.y1 - ph + bh / 2 - 3), "CHECKPOINT", ac);
      }
      for (int c = 0; c < N_CARS; c++) {
        int ci = (int)floorf(cars[c].z / SEG_LEN) % nSeg;
        if (ci != i) continue;
        float pc = fmodf(cars[c].z, SEG_LEN) / SEG_LEN;
        float cx = lerpf(p.x1, p.x2, pc), cy = lerpf(p.y1, p.y2, pc), w = lerpf(p.w1, p.w2, pc);
        float ww = w / ROAD_W * CAR_W;
        cx += cars[c].x * w;
        if (ww > 2) drawCar(g, cx, cy, ww, cars[c].col, p.clip, 0);
      }
    }

    // ---- player car ----
    bool boost = now < boostUntil;
    bool crashFlash = crashAt && now - crashAt < 600 && (now / 80) % 2;
    float bounce = (playerX < -1 || playerX > 1) && speed > 500 ? (float)plat_random(3) : 0;
    float py = 226 - bounce;
    if (boost) {                                                // exhaust flames
      int fl = 6 + plat_random(6);
      g.fillTriangle(136, ir(py - 10), 146, ir(py - 10), 141, ir(py - 10 + fl), rgb(255, 170, 40));
      g.fillTriangle(174, ir(py - 10), 184, ir(py - 10), 179, ir(py - 10 + fl), rgb(255, 170, 40));
    }
    if (!crashFlash) drawCar(g, 160 + shake, py, 92, rgb(230, 50, 60), SH, steerVis * 3);

    drawRaceHud(g, now);
    drawOverlays(g, now, C_ORANGE);
  }
  void drawRaceHud(Canvas& g, uint32_t now) {
    char b[24];
    // time (top centre)
    bool low = timeLeft < 5 && started;
    rrect(g, 128, 4, 64, 34, 10, blend(C_BG, sky[10], 0.3f));
    textC(g, F_SMALL, 160, 8, "TIME", low ? C_RED : C_SOFT);
    snprintf(b, sizeof(b), "%d", (int)ceilf(timeLeft));
    textC(g, F_BIG, 160, 34, b, low && (now / 250) % 2 ? C_RED : C_WHITE);
    // score (top left), best under it
    snprintf(b, sizeof(b), "%d", score);
    text(g, F_BOLD, 8, 20, b, C_WHITE);
    snprintf(b, sizeof(b), "BEST %d", imax(best, score));
    text(g, F_SMALL, 8, 26, b, C_SOFT);
    // speed (top right)
    int kmh = (int)(speed / MAX_SPEED * 240);
    snprintf(b, sizeof(b), "%d", kmh);
    textR(g, F_BOLD, SW - 34, 20, b, C_WHITE);
    text(g, F_SMALL, SW - 30, 12, "KM/H", C_SOFT);
    // nitro pips (under speed)
    text(g, F_SMALL, SW - 78, 26, "NITRO", C_SOFT);
    for (int i = 0; i < 3; i++) rrect(g, SW - 44 + i * 12, 26, 9, 7, 2, i < nitros ? C_TEAL : blend(C_BG, C_LINE, 0.8f));
    // countdown / messages
    uint32_t since = now - startAt;
    if (!started) {
      int k = 3 - (int)(since / 800);
      snprintf(b, sizeof(b), "%d", k < 1 ? 1 : k);
      float pop = 1 - seg(since % 800, 0, 250);
      textC(g, F_HUGE, 160, 104 - ir(pop * 6), b, C_YELLOW);
      rrect(g, 64, 116, 192, 20, 10, rgb(20, 12, 36));
      textC(g, F_SMALL, 160, 123, "Z C STEER   X BRAKE   D NITRO", C_WHITE);
    } else if (since < 3000) textC(g, F_HUGE, 160, 104, "GO!", C_GREEN);
    if (cpAt && now - cpAt < 1500) {
      snprintf(b, sizeof(b), "CHECKPOINT  +%ds", cpBonusShown);
      textC(g, F_BOLD, 160, 72, b, C_YELLOW);
    }
    if (crashAt && now - crashAt < 800) textC(g, F_BOLD, 160, 94, "CRASH!", C_RED);
  }
};

#define OS_VERSION "v1.5"

enum MenuKind : uint8_t { MK_AI, MK_GAME, MK_SETTINGS };
struct MenuItem { const char* name; const char* key; uint16_t color; Icon icon; MenuKind kind; };
static const MenuItem MENU[] = {
  {"Ask AI",   nullptr,    C_TEAL,   IC_AI,       MK_AI},
  {"Snake",    "snake",    C_GREEN,  IC_SNAKE,    MK_GAME},
  {"Blocks",   "blocks",   C_PURPLE, IC_BLOCKS,   MK_GAME},
  {"Pong",     "pong",     C_BLUE,   IC_PONG,     MK_GAME},
  {"Breakout", "breakout", C_ORANGE, IC_BREAKOUT, MK_GAME},
  {"Flappy",   "flappy",   C_YELLOW, IC_FLAPPY,   MK_GAME},
  {"Turbo",    "turbo",    C_PINK,   IC_RACE,     MK_GAME},
  {"Settings", nullptr,    C_SOFT,   IC_SETTINGS, MK_SETTINGS},
};
static const int N_MENU = sizeof(MENU) / sizeof(MENU[0]);
static const int FIRST_GAME = 1, N_GAMES = 6;

// Menu grid geometry (4 x 2 tiles, landscape)
static const int TILE_W = 72, TILE_H = 78, TILE_X0 = 7, TILE_GAP = 6, TILE_Y0 = 48;
static inline int tileX(int i) { return TILE_X0 + (i % 4) * (TILE_W + TILE_GAP); }
static inline int tileY(int i) { return TILE_Y0 + (i / 4) * (TILE_H + TILE_GAP); }

// Boot mascot geometry (s = 0.62, centred at 160,100)
static const float BM_S = 0.62f, BM_CX = 160, BM_CY = 100;
static const float BM_W = 248 * BM_S, BM_H = 192 * BM_S;
static const float BM_TOP = BM_CY + 96 * BM_S - BM_H, BM_LEFT = BM_CX - BM_W / 2;
static const float BM_BAR_Y = BM_CY + 125 * BM_S, BM_BAR_H = 26 * BM_S, BM_BAR_W = 302 * BM_S;

// WiFi signal bars (bottom-left at x, y)
static int rssiLevel(int rssi) { return rssi > -55 ? 4 : rssi > -67 ? 3 : rssi > -78 ? 2 : 1; }
static void drawBars(Canvas& g, int x, int y, int level, uint16_t on, uint16_t off) {
  for (int i = 0; i < 4; i++) { int h = 3 + i * 3; g.fillRect(x + i * 4, y - h, 3, h, i < level ? on : off); }
}
static void drawLock(Canvas& g, int x, int y, uint16_t c, uint16_t bg) {
  g.fillRoundRect(x, y + 4, 9, 7, 1, c);
  g.drawRoundRect(x + 2, y, 5, 7, 2, c);
  g.fillRect(x + 4, y + 6, 1, 3, bg);
}

static void drawIcon(Canvas& g, Icon ic, int x, int y, uint16_t col) {
  rrect(g, x, y, 34, 34, 9, col);
  uint16_t k = C_BG; int cx = x + 17, cy = y + 17;
  switch (ic) {
    case IC_SNAKE:
      g.fillRect(x + 7, y + 20, 6, 6, k); g.fillRect(x + 14, y + 20, 6, 6, k);
      g.fillRect(x + 14, y + 13, 6, 6, k); g.fillRect(x + 21, y + 13, 6, 6, k);
      g.fillCircle(x + 24, y + 7, 2, k); break;
    case IC_BLOCKS:
      g.fillRect(x + 6, y + 18, 7, 7, k); g.fillRect(x + 14, y + 18, 7, 7, k);
      g.fillRect(x + 22, y + 18, 7, 7, k); g.fillRect(x + 14, y + 10, 7, 7, k); break;
    case IC_PONG:
      g.fillRect(x + 8, y + 6, 12, 3, k); g.fillRect(x + 14, y + 25, 12, 3, k);
      g.fillRect(x + 18, y + 15, 4, 4, k); break;
    case IC_BREAKOUT:
      for (int i = 0; i < 3; i++) { g.fillRect(x + 5 + i * 8, y + 7, 7, 4, k); g.fillRect(x + 5 + i * 8, y + 12, 7, 4, k); }
      g.fillRect(x + 20, y + 12, 7, 4, col);
      g.fillCircle(x + 15, y + 21, 2, k); g.fillRect(x + 9, y + 26, 14, 3, k); break;
    case IC_FLAPPY:
      g.fillCircle(cx - 1, cy, 7, k); g.fillCircle(cx + 1, cy - 2, 2, col);
      g.fillTriangle(cx + 6, cy, cx + 11, cy + 2, cx + 6, cy + 4, k); break;
    case IC_RACE:                        // road in perspective + car from behind
      g.fillTriangle(cx - 3, y + 5, cx + 3, y + 5, x + 30, y + 30, k);
      g.fillTriangle(cx - 3, y + 5, x + 4, y + 30, x + 30, y + 30, k);
      g.fillRect(cx - 1, y + 9, 2, 3, col); g.fillRect(cx - 1, y + 15, 2, 3, col);
      g.fillRect(cx - 8, y + 21, 16, 6, col); g.fillRect(cx - 5, y + 18, 10, 4, col);
      g.fillRect(cx - 7, y + 23, 3, 2, k); g.fillRect(cx + 4, y + 23, 3, 2, k); break;
    case IC_SETTINGS:
      for (int i = 0; i < 8; i++) {
        float a = i * 3.14159f / 4;
        g.fillCircle(ir(cx + cosf(a) * 9), ir(cy + sinf(a) * 9), 3, k);
      }
      g.fillCircle(cx, cy, 8, k); g.fillCircle(cx, cy, 3, col); break;
    case IC_AI: {                       // the little computer, with a sparkle
      Mascot m; m.cx = cx; m.cy = cy - 1; m.s = 0.1f;
      m.body = k; m.eye = k; m.bg = col;
      drawMascot(g, m);
      drawSparkle(g, x + 28, y + 6, 4, C_WHITE);
      break;
    }
    case IC_ABOUT:
      g.fillCircle(cx, y + 9, 3, k); g.fillRoundRect(cx - 2, y + 14, 5, 13, 2, k); break;
  }
}

struct App {
  enum State { BOOT, INTRO, MENU_S, LAUNCH, GAME, CHAT, SETTINGS, ABOUT, WIFI, KEYS, TEXT } st = BOOT;
  uint32_t t0 = 0;
  int sel = 0;
  float hx = TILE_X0, hy = TILE_Y0;   // gliding highlight position
  uint32_t menuAt = 0;
  int bests[N_GAMES] = {};
  Game* games[N_GAMES];
  Game* cur = nullptr;
  bool showFps = false, flip = false;
  bool aiPro = false, shortAns = true, tlsVerify = true;
  int setSel = 0; float setTop = 0; uint32_t resetArmAt = 0, resetDoneAt = 0;
  uint32_t lastT = 0;
  static const uint32_t BOOT_LEN = 4150;
  char toast[80] = ""; uint32_t toastAt = 0;

  SnakeGame snake; BlocksGame blocks; PongGame pong; BreakoutGame breakout; FlappyGame flappy; RacerGame racer;
  Chat chat;

  // ---- saved WiFi networks ----
  static const int MAX_NETS = 5;
  struct SavedNet { char ssid[33]; char pass[65]; };
  SavedNet nets[MAX_NETS]; int nNets = 0;
  // ---- saved API keys ----
  static const int MAX_KEYS = 5;
  struct SavedKey { char label[24]; char val[161]; };
  SavedKey keys[MAX_KEYS]; int nKeys = 0, activeKey = -1;

  // ---- WiFi screen ----
  struct FoundNet { char ssid[33]; int rssi; bool open; };
  static const int MAX_FOUND = 20;
  FoundNet found[MAX_FOUND]; int nFound = 0;
  bool wifiScanView = false, scanning = false;
  int wSel = 0; float wTop = 0;
  int connPhase = 0;                 // 0 none, 1 connecting, 2 connected, 3 failed
  uint32_t connAt = 0;
  char connSsid[33] = "", connPass[65] = ""; bool connSave = false, connOpen = false;
  int autoPhase = 0; uint32_t autoAt = 0;   // boot / background auto-connect

  // ---- API keys screen ----
  int kSel = 0; float kTop = 0;
  char pendingLabel[24] = "";

  // ---- action sheet (small pop-up menu) ----
  bool sheetOn = false; int sheetN = 0, sheetSel = 0, sheetFor = 0;
  char sheetTitle[40] = ""; const char* sheetOpt[3] = {};

  // ---- text entry ----
  enum TePurpose { TE_KEYLABEL, TE_KEYVALUE, TE_WIFIPASS };
  TePurpose tePurpose = TE_KEYLABEL; State teReturn = SETTINGS;
  char teTitle[48] = "", teHint[64] = "", teBuf[161] = ""; int teLen = 0, teMax = 160; bool teMono = true;

  void begin() {
    games[0] = &snake; games[1] = &blocks; games[2] = &pong;
    games[3] = &breakout; games[4] = &flappy; games[5] = &racer;
    showFps = plat_loadInt("fps", 0);
    flip = plat_loadInt("flip2", 0);
    aiPro = plat_loadInt("aipro", 0);
    shortAns = plat_loadInt("short", 1);
    tlsVerify = plat_loadInt("tls", 1);
    plat_setFlip(flip);
    loadBests();
    loadNets();
    loadKeys();
    chat.init();
    if (nNets > 0) { plat_wifiScanStart(); autoPhase = 1; }
    autoAt = plat_millis();
    t0 = plat_millis();
  }
  void loadBests() { for (int i = 0; i < N_GAMES; i++) bests[i] = plat_loadInt(MENU[FIRST_GAME + i].key, 0); }
  void go(State s, uint32_t now) { st = s; t0 = now; if (s == INTRO) menuAt = now + 700; }
  void openMenu(uint32_t now) { go(MENU_S, now); menuAt = now; }
  void showToast(const char* s, uint32_t now) { snprintf(toast, sizeof(toast), "%s", s); toastAt = now; }

  // ---------------- storage ----------------
  void loadNets() {
    nNets = imin(plat_loadInt("wn", 0), MAX_NETS);
    char k[16];
    for (int i = 0; i < nNets; i++) {
      snprintf(k, sizeof(k), "ws%d", i); plat_loadStr(k, nets[i].ssid, sizeof(nets[i].ssid));
      snprintf(k, sizeof(k), "wp%d", i); plat_loadStr(k, nets[i].pass, sizeof(nets[i].pass));
    }
  }
  void saveNets() {
    char k[16];
    for (int i = 0; i < nNets; i++) {
      snprintf(k, sizeof(k), "ws%d", i); plat_saveStr(k, nets[i].ssid);
      snprintf(k, sizeof(k), "wp%d", i); plat_saveStr(k, nets[i].pass);
    }
    plat_saveInt("wn", nNets);
  }
  int findNet(const char* ssid) { for (int i = 0; i < nNets; i++) if (!strcmp(nets[i].ssid, ssid)) return i; return -1; }
  void rememberNet(const char* ssid, const char* pass) {       // newest first
    int i = findNet(ssid);
    if (i < 0) { i = nNets < MAX_NETS ? nNets++ : MAX_NETS - 1; }
    for (int j = i; j > 0; j--) nets[j] = nets[j - 1];
    snprintf(nets[0].ssid, sizeof(nets[0].ssid), "%s", ssid);
    snprintf(nets[0].pass, sizeof(nets[0].pass), "%s", pass);
    saveNets();
  }
  void forgetNet(int i) {
    if (i < 0 || i >= nNets) return;
    for (int j = i; j < nNets - 1; j++) nets[j] = nets[j + 1];
    nNets--; saveNets();
  }
  void loadKeys() {
    nKeys = imin(plat_loadInt("kn", 0), MAX_KEYS);
    char k[16];
    for (int i = 0; i < nKeys; i++) {
      snprintf(k, sizeof(k), "kl%d", i); plat_loadStr(k, keys[i].label, sizeof(keys[i].label));
      snprintf(k, sizeof(k), "kv%d", i); plat_loadStr(k, keys[i].val, sizeof(keys[i].val));
    }
    activeKey = plat_loadInt("ka", nKeys ? 0 : -1);
    if (activeKey >= nKeys) activeKey = nKeys ? 0 : -1;
  }
  void saveKeys() {
    char k[16];
    for (int i = 0; i < nKeys; i++) {
      snprintf(k, sizeof(k), "kl%d", i); plat_saveStr(k, keys[i].label);
      snprintf(k, sizeof(k), "kv%d", i); plat_saveStr(k, keys[i].val);
    }
    plat_saveInt("kn", nKeys);
    plat_saveInt("ka", activeKey);
  }
  static void maskKey(const char* v, char* out, int max) {     // "pplx-AbC...wxyz"
    int n = (int)strlen(v);
    if (n <= 12) snprintf(out, max, "%.*s...", n > 4 ? 4 : n, v);
    else snprintf(out, max, "%.8s...%s", v, v + n - 4);
  }

  // ---------------- WiFi helpers ----------------
  bool wifiOk() { return plat_wifiState() == 2; }
  void connectTo(const char* ssid, const char* pass, bool save, bool open, uint32_t now) {
    char s2[33], p2[65];                               // copy first: ssid/pass may point at connSsid/connPass
    snprintf(s2, sizeof(s2), "%s", ssid);
    snprintf(p2, sizeof(p2), "%s", pass);
    memcpy(connSsid, s2, sizeof(s2));
    memcpy(connPass, p2, sizeof(p2));
    connSave = save; connOpen = open;
    autoPhase = 0;
    plat_wifiConnect(connSsid, connPass);
    connPhase = 1; connAt = now;
  }
  const char* reasonText() {
    int r = plat_wifiReason();
    if (r == 201 || r == 210 || r == 211) return "Network not found. Move closer?";
    if (r == 212) return "Signal too weak.";
    if (r == 2 || r == 15 || r == 202 || r == 204) return "Wrong password?";
    return "Could not connect.";
  }
  // runs every frame: background auto-connect to the strongest saved network
  void netTick(uint32_t now) {
    if (connPhase == 1) {
      int s = plat_wifiState();
      if (s == 2) { connPhase = 2; connAt = now; if (connSave) rememberNet(connSsid, connPass); }
      else if (s == 3) { connPhase = 3; connAt = now; }
    }
    if (autoPhase == 1 && !scanning) {
      int n = plat_wifiScanCount();
      if (n >= 0) {
        int best = -1, bestRssi = -999;
        for (int i = 0; i < n; i++) {
          char ss[33]; int rssi, open;
          plat_wifiScanGet(i, ss, sizeof(ss), &rssi, &open);
          int k = findNet(ss);
          if (k >= 0 && rssi > bestRssi) { best = k; bestRssi = rssi; }
        }
        if (best >= 0) { plat_wifiConnect(nets[best].ssid, nets[best].pass); autoPhase = 2; }
        else autoPhase = 3;
        autoAt = now;
      } else if (n == -2) { autoPhase = 3; autoAt = now; }
    } else if (autoPhase == 2) {
      int s = plat_wifiState();
      if (s == 2 || s == 3) { autoPhase = 3; autoAt = now; }
    }
    // retry every 60 s while disconnected (not while the WiFi screen is busy)
    if ((autoPhase == 3 || autoPhase == 0) && nNets > 0 && connPhase != 1 && !scanning && st != WIFI &&
        now - autoAt > 60000 && (plat_wifiState() == 0 || plat_wifiState() == 3) && !chat.busy) {
      plat_wifiScanStart(); autoPhase = 1; autoAt = now;
    }
  }

  // ---------------- update ----------------
  void update(Input& in, uint32_t now) {
    uint32_t dt = lastT ? now - lastT : 16; if (dt > 50) dt = 50; lastT = now;
    uint32_t t = now - t0;
    in.textMode = false;
    netTick(now);
    if (sheetOn) { updateSheet(in, now); }
    else switch (st) {
      case BOOT:  if (t > BOOT_LEN || (t > 400 && in.anyPressed)) go(INTRO, now); break;
      case INTRO: if (t > 1100) go(MENU_S, now); break;
      case MENU_S: updateMenu(in, now); break;
      case LAUNCH:
        if (t > 480) {
          if (MENU[sel].kind == MK_AI) { chat.enter(); go(CHAT, now); }
          else { cur = games[sel - FIRST_GAME]; cur->begin(); go(GAME, now); }
        }
        break;
      case GAME:  if (!cur->update(in, now, dt)) { loadBests(); openMenu(now); } break;
      case CHAT:
        chat.apiKey = activeKey >= 0 ? keys[activeKey].val : "";
        chat.pro = aiPro; chat.shortAnswers = shortAns; chat.verifyTls = tlsVerify;
        if (!chat.update(in, now, dt)) openMenu(now);
        break;
      case SETTINGS: updateSettings(in, now); break;
      case ABOUT: if (in.pressed[B_B] || in.pressed[B_A]) go(SETTINGS, now); break;
      case WIFI: updateWifi(in, now); break;
      case KEYS: updateKeys(in, now); break;
      case TEXT: updateText(in, now); break;
    }
    // keep text mode on while typing so the next key report types
    if (st == TEXT && !sheetOn) in.textMode = true;
    if (st == CHAT && !sheetOn) in.textMode = !chat.scrollMode;
    float k = 1 - powf(0.0005f, dt / 1000.0f * 1.6f);
    hx = lerpf(hx, tileX(sel), k);
    hy = lerpf(hy, tileY(sel), k);
    setTop = lerpf(setTop, listTopFor(setSel, 10, setTop, 4), k);
    wTop = lerpf(wTop, listTopFor(wSel, wifiRowCount(), wTop, 3), k);
    kTop = lerpf(kTop, listTopFor(kSel, keyRowCount(), kTop, 4), k);
  }
  // first visible row so that sel stays on screen (4 rows visible)
  static float listTopFor(int sel, int n, float top, int VIS) {
    int t = ir(top);
    if (sel < t) t = sel;
    if (sel > t + VIS - 1) t = sel - VIS + 1;
    if (t > n - VIS) t = n - VIS;
    if (t < 0) t = 0;
    return (float)t;
  }
  void updateMenu(Input& in, uint32_t now) {
    if (in.rep[B_LEFT])  sel = (sel + N_MENU - 1) % N_MENU;
    if (in.rep[B_RIGHT]) sel = (sel + 1) % N_MENU;
    if (in.rep[B_UP] || in.rep[B_DOWN]) sel = (sel + 4) % N_MENU;
    if (in.pressed[B_A]) {
      if (MENU[sel].kind == MK_SETTINGS) { setSel = 0; setTop = 0; resetArmAt = resetDoneAt = 0; go(SETTINGS, now); }
      else go(LAUNCH, now);
    }
  }

  // ---- action sheet ----
  void openSheet(const char* title, int forWhat, const char* a, const char* b, const char* c) {
    snprintf(sheetTitle, sizeof(sheetTitle), "%s", title);
    sheetOpt[0] = a; sheetOpt[1] = b; sheetOpt[2] = c;
    sheetN = c ? 3 : (b ? 2 : 1); sheetSel = 0; sheetFor = forWhat; sheetOn = true;
  }
  void updateSheet(Input& in, uint32_t now) {
    if (in.rep[B_UP]) sheetSel = (sheetSel + sheetN - 1) % sheetN;
    if (in.rep[B_DOWN]) sheetSel = (sheetSel + 1) % sheetN;
    if (in.pressed[B_B]) { sheetOn = false; return; }
    if (!in.pressed[B_A]) return;
    sheetOn = false;
    if (st == WIFI) {                                   // saved network: Connect / Forget / Cancel
      int i = sheetFor;
      if (sheetSel == 0 && i < nNets) connectTo(nets[i].ssid, nets[i].pass, true, !nets[i].pass[0], now);
      else if (sheetSel == 1) { forgetNet(i); wSel = 0; showToast("Network forgotten", now); }
    } else if (st == KEYS) {                            // key: Use / Delete / Cancel
      int i = sheetFor;
      if (sheetSel == 0 && i < nKeys) { activeKey = i; saveKeys(); showToast("Key in use", now); }
      else if (sheetSel == 1 && i < nKeys) {
        for (int j = i; j < nKeys - 1; j++) keys[j] = keys[j + 1];
        nKeys--;
        if (activeKey == i) activeKey = nKeys ? 0 : -1;
        else if (activeKey > i) activeKey--;
        saveKeys(); kSel = 0; showToast("Key deleted", now);
      }
    }
  }

  // ---- settings ----
  void updateSettings(Input& in, uint32_t now) {
    const int N = 10;
    if (in.rep[B_UP]) setSel = (setSel + N - 1) % N;
    if (in.rep[B_DOWN]) setSel = (setSel + 1) % N;
    if (in.pressed[B_B] || in.pressed[B_LEFT]) { openMenu(now); return; }
    if (!in.pressed[B_A] && !in.pressed[B_RIGHT]) return;
    bool enter = in.pressed[B_A];
    switch (setSel) {
      case 0: wifiScanView = false; wSel = 0; wTop = 0; go(WIFI, now); break;
      case 1: kSel = 0; kTop = 0; go(KEYS, now); break;
      case 2: aiPro = !aiPro; plat_saveInt("aipro", aiPro); break;
      case 3: if (enter) { shortAns = !shortAns; plat_saveInt("short", shortAns); } break;
      case 4: if (enter) { tlsVerify = !tlsVerify; plat_saveInt("tls", tlsVerify); } break;
      case 5: if (enter) { flip = !flip; plat_saveInt("flip2", flip); plat_setFlip(flip); } break;
      case 6: if (enter) { showFps = !showFps; plat_saveInt("fps", showFps); } break;
      case 8:
        if (!enter) break;
        if (resetArmAt && now - resetArmAt < 3000) {
          for (int i = 0; i < N_GAMES; i++) plat_saveInt(MENU[FIRST_GAME + i].key, 0);
          loadBests(); resetArmAt = 0; resetDoneAt = now;
        } else resetArmAt = now;
        break;
      case 9: go(ABOUT, now); break;
    }
  }

  // ---- WiFi screen ----
  // main view rows: 0 = "Scan for networks", 1.. = saved networks
  // scan view rows: found networks
  int wifiRowCount() { return wifiScanView ? imax(nFound, 1) : 1 + nNets; }
  void startScan() {
    plat_wifiScanStart(); scanning = true; nFound = 0; wifiScanView = true; wSel = 0; wTop = 0;
    if (autoPhase == 1) autoPhase = 3;
  }
  void collectScan() {
    int n = plat_wifiScanCount();
    if (n == -1) return;
    scanning = false;
    nFound = 0;
    for (int i = 0; i < n && nFound < MAX_FOUND; i++) {
      FoundNet f; int open;
      plat_wifiScanGet(i, f.ssid, sizeof(f.ssid), &f.rssi, &open);
      f.open = open != 0;
      if (!f.ssid[0]) continue;                          // hidden network
      bool dup = false;
      for (int j = 0; j < nFound; j++) if (!strcmp(found[j].ssid, f.ssid)) { dup = true; if (f.rssi > found[j].rssi) found[j].rssi = f.rssi; }
      if (!dup) found[nFound++] = f;
    }
    // strongest first
    for (int a = 1; a < nFound; a++) for (int b = a; b > 0 && found[b].rssi > found[b - 1].rssi; b--) { FoundNet t = found[b]; found[b] = found[b - 1]; found[b - 1] = t; }
  }
  void openText(TePurpose p, State ret, const char* title, const char* hint, const char* init, int maxLen, bool mono) {
    tePurpose = p; teReturn = ret;
    snprintf(teTitle, sizeof(teTitle), "%s", title);
    snprintf(teHint, sizeof(teHint), "%s", hint);
    snprintf(teBuf, sizeof(teBuf), "%s", init);
    teLen = (int)strlen(teBuf); teMax = maxLen; teMono = mono;
    go(TEXT, plat_millis());
  }
  void updateWifi(Input& in, uint32_t now) {
    if (scanning) collectScan();
    if (connPhase == 1) { if (in.pressed[B_B]) { plat_wifiDisconnect(); connPhase = 0; } return; }
    if (connPhase == 2) { if (now - connAt > 1300 || in.pressed[B_A] || in.pressed[B_B]) { connPhase = 0; wifiScanView = false; wSel = 0; } return; }
    if (connPhase == 3) {
      if (in.pressed[B_B]) connPhase = 0;
      else if (in.pressed[B_A]) {
        connPhase = 0;
        if (!connOpen) openText(TE_WIFIPASS, WIFI, connSsid, "WIFI PASSWORD (CASE SENSITIVE)", connPass, 64, true);
        else connectTo(connSsid, "", connSave, true, now);
      }
      return;
    }
    int n = wifiRowCount();
    if (in.rep[B_UP]) wSel = (wSel + n - 1) % n;
    if (in.rep[B_DOWN]) wSel = (wSel + 1) % n;
    if (in.pressed[B_B] || in.pressed[B_LEFT]) {
      if (wifiScanView) { wifiScanView = false; wSel = 0; wTop = 0; }
      else go(SETTINGS, now);
      return;
    }
    if (!in.pressed[B_A]) return;
    if (!wifiScanView) {
      if (wSel == 0) startScan();
      else openSheet(nets[wSel - 1].ssid, wSel - 1, "Connect", "Forget", "Cancel");
    } else if (!scanning && nFound > 0) {
      FoundNet& f = found[wSel];
      int k = findNet(f.ssid);
      if (k >= 0) connectTo(nets[k].ssid, nets[k].pass, true, f.open, now);
      else if (f.open) connectTo(f.ssid, "", true, true, now);
      else { snprintf(connSsid, sizeof(connSsid), "%s", f.ssid); connOpen = false;
             openText(TE_WIFIPASS, WIFI, f.ssid, "WIFI PASSWORD (CASE SENSITIVE)", "", 64, true); }
    } else if (!scanning) startScan();
  }

  // ---- API keys screen ----
  int keyRowCount() { return nKeys + (nKeys < MAX_KEYS ? 1 : 0); }
  void updateKeys(Input& in, uint32_t now) {
    int n = keyRowCount();
    if (in.rep[B_UP]) kSel = (kSel + n - 1) % n;
    if (in.rep[B_DOWN]) kSel = (kSel + 1) % n;
    if (in.pressed[B_B] || in.pressed[B_LEFT]) { go(SETTINGS, now); return; }
    if (!in.pressed[B_A]) return;
    if (kSel < nKeys) openSheet(keys[kSel].label, kSel, activeKey == kSel ? "In use" : "Use this key", "Delete", "Cancel");
    else {
      char def[24]; snprintf(def, sizeof(def), "Key %d", nKeys + 1);
      openText(TE_KEYLABEL, KEYS, "Name this key", "A SHORT NAME, E.G. MY KEY", def, 20, false);
    }
  }

  // ---- text entry ----
  void updateText(Input& in, uint32_t now) {
    in.textMode = true;
    char c;
    while ((c = in.getChar()) != 0) {
      if (c == '\b') { if (teLen) teBuf[--teLen] = 0; }
      else if (teLen < teMax) { teBuf[teLen++] = c; teBuf[teLen] = 0; }
    }
    if (in.pressed[B_B]) { in.textMode = false; in.clearChars(); go(teReturn, now); return; }
    if (!in.pressed[B_A]) return;
    // trim spaces at the ends (keys and names)
    if (tePurpose != TE_WIFIPASS) {
      while (teLen && teBuf[teLen - 1] == ' ') teBuf[--teLen] = 0;
      int s = 0; while (teBuf[s] == ' ') s++;
      if (s) { memmove(teBuf, teBuf + s, teLen - s + 1); teLen -= s; }
    }
    switch (tePurpose) {
      case TE_KEYLABEL:
        snprintf(pendingLabel, sizeof(pendingLabel), "%.23s", teLen ? teBuf : "My key");
        openText(TE_KEYVALUE, KEYS, "Type your API key", "STARTS WITH pplx-   CHECK EVERY CHARACTER", "", 160, true);
        break;
      case TE_KEYVALUE:
        if (teLen < 20) { showToast("That key looks too short", now); return; }
        if (nKeys >= MAX_KEYS) { showToast("5 keys max. Delete one first", now); return; }
        snprintf(keys[nKeys].label, sizeof(keys[nKeys].label), "%s", pendingLabel);
        snprintf(keys[nKeys].val, sizeof(keys[nKeys].val), "%s", teBuf);
        activeKey = nKeys; nKeys++;
        saveKeys();
        showToast(strncmp(teBuf, "pplx-", 5) ? "Saved. Note: Perplexity keys start with pplx-" : "Key saved and in use", now);
        kSel = activeKey; in.clearChars(); go(KEYS, now);
        break;
      case TE_WIFIPASS:
        in.clearChars();
        go(WIFI, now);
        connectTo(connSsid, teBuf, true, false, now);
        break;
    }
  }

  // ---------------- drawing ----------------
  void draw(Canvas& g, uint32_t now) {
    uint32_t t = now - t0;
    switch (st) {
      case BOOT:   drawBoot(g, t); break;
      case INTRO:  drawIntro(g, t, now); break;
      case MENU_S: drawMenu(g, now, true); break;
      case LAUNCH: drawLaunch(g, t, now); break;
      case GAME:   cur->draw(g, now); break;
      case CHAT:   chat.draw(g, now, wifiOk()); drawMiniHeader(g, now, "Ask"); break;
      case SETTINGS: drawSettings(g, now); break;
      case ABOUT:  drawAbout(g, now); break;
      case WIFI:   drawWifi(g, now); break;
      case KEYS:   drawKeys(g, now); break;
      case TEXT:   drawText(g, now); break;
    }
    if (sheetOn) drawSheet(g);
    if (toast[0] && now - toastAt < 2600 && st != GAME) {
      int w = textW(g, F_SMALL, toast) + 22;
      rrect(g, 160 - w / 2, 190, w, 20, 10, C_WHITE);
      textC(g, F_SMALL, 160, 196, toast, C_BG);
    }
  }

  // ======== Startup animation (no text) ========
  // 0.00s  a teal pixel powers on and stretches into the desk bar
  // 0.55s  the screen drops in, squashes, puffs dust, bounces
  // 1.45s  a scanline sweeps down (screen turning on), eyes pop open
  // 2.00s  a sparkle flies around - the eyes follow it
  // 3.00s  it lands on the head: boop, confetti, happy hop
  // 3.75s  settles, blinks at you -> transition to the menu
  static void sparklePos(float u, float& x, float& y) {
    x = 160 - 140 * cosf(2.5f * 3.14159f * u) * (1 - u);
    y = lerpf(18, BM_TOP - 7, u) + 16 * sinf(4 * 3.14159f * u) * (1 - u);
  }
  Mascot bootPose(uint32_t t) {
    Mascot m; m.cx = BM_CX; m.cy = BM_CY; m.s = BM_S; m.showBar = false;
    m.dropY = t < 550 ? -400 : lerpf(-230, 0, easeInCubic(seg(t, 550, 900)));
    m.squash = 0.9f * bump(seg(t, 900, 1060)) - 0.35f * bump(seg(t, 1060, 1330)) + 0.3f * bump(seg(t, 1330, 1450))
             + 0.45f * bump(seg(t, 3000, 3130));
    m.lift = 14 * bump(seg(t, 1060, 1330));
    m.eyeScale = easeOutBack(seg(t, 1700, 1950));
    // eyes follow the sparkle
    float w = seg(t, 2000, 2200) * (1 - easeInOutCubic(seg(t, 3450, 3700)));
    if (w > 0) {
      float sx, sy; sparklePos(seg(t, 2000, 3000), sx, sy);
      m.look = w * fmaxf(-1, fminf(1, (sx - 160) / 100));
      m.lookY = w * fmaxf(-1, fminf(1, (sy - 100) / 55));
    }
    float hop = seg(t, 3130, 3560);
    if (hop > 0 && hop < 1) { m.happy = 1; m.lift += 22 * bump(hop); m.squash -= 0.3f * bump(hop); }
    m.eyeOpen = 1 - bump(seg(t, 3780, 3920));
    return m;
  }
  void drawBoot(Canvas& g, uint32_t t) {
    g.fillScreen(C_BG);
    // power-on pixel -> desk bar
    if (t < 250) {
      float r = 3 * easeOutBack(seg(t, 0, 250));
      g.fillCircle(ir(BM_CX), ir(BM_BAR_Y + BM_BAR_H / 2), ir(r), C_TEAL);
    } else {
      float k = easeOutCubic(seg(t, 250, 600));
      float bw = lerpf(8, BM_BAR_W, k), bh = lerpf(6, BM_BAR_H, k);
      rrect(g, BM_CX - bw / 2, BM_BAR_Y + BM_BAR_H / 2 - bh / 2, bw, bh, bh / 2, blend(C_TEAL, C_WHITE, k));
    }
    // dust puffs when it lands
    if (t >= 900 && t < 1350) {
      float u = seg(t, 900, 1350);
      for (int side = -1; side <= 1; side += 2) for (int i = 0; i < 3; i++) {
        float sp = 50 + i * 22, x = BM_CX + side * (BM_W / 2 + sp * u * 0.9f), y = BM_BAR_Y - 4 - (10 + i * 7) * bump(u * 0.8f);
        g.fillCircle(ir(x), ir(y), ir(3 * (1 - u) + 0.4f), blend(C_SOFT, C_BG, u));
      }
    }
    Mascot m = bootPose(t);
    if (t >= 550) drawMascot(g, m);
    // scanline: the screen turning on
    if (t >= 1450 && t < 1780) {
      float st = 28 * BM_S, u = seg(t, 1450, 1750);
      float top = BM_TOP + st, h = BM_H - 2 * st;
      for (int k = 0; k < 4; k++) {
        float y = top + (u - k * 0.05f) * h;
        if (y < top || y > top + h - 2) continue;
        g.fillRect(ir(BM_LEFT + st + 4), ir(y), ir(BM_W - 2 * st - 8), 2, blend(C_TEAL, C_BG, k * 0.3f + seg(t, 1700, 1780)));
      }
    }
    // sparkle
    if (t >= 2000 && t < 3000) {
      float sx, sy; sparklePos(seg(t, 2000, 3000), sx, sy);
      float r = 7 * easeOutBack(seg(t, 2000, 2150)) * (0.8f + 0.2f * sinf(t * 0.03f));
      drawSparkle(g, sx, sy, r, C_YELLOW);
      // little trail
      for (int k = 1; k <= 3; k++) {
        float px, py; sparklePos(fmaxf(0, seg(t, 2000, 3000) - k * 0.03f), px, py);
        g.fillCircle(ir(px), ir(py), 1, blend(C_YELLOW, C_BG, k * 0.28f));
      }
    }
    // confetti burst from the boop
    if (t >= 3000 && t < 3850) {
      float u = (t - 3000) / 1000.0f;
      static const uint16_t cols[4] = {C_TEAL, C_PINK, C_YELLOW, C_GREEN};
      for (int i = 0; i < 12; i++) {
        float a = -3.14159f * (0.08f + 0.84f * i / 11.0f);
        float sp = 120 + (i % 3) * 30;
        float x = BM_CX + cosf(a) * sp * u, y = BM_TOP - 6 + sinf(a) * sp * u + 260 * u * u;
        float fade = seg(t, 3450, 3850);
        int sz = imax(1, ir(3 * (1 - fade)));
        g.fillRect(ir(x), ir(y), sz + (i % 2), sz, blend(cols[i % 4], C_BG, fade));
      }
    }
  }

  // ======== Transition: eyes close, screen grows to fill the display, white flash
  // shrinks into the menu's first tile ========
  void drawIntro(Canvas& g, uint32_t t, uint32_t now) {
    if (t < 700) {
      g.fillScreen(C_BG);
      float eyes = 1 - seg(t, 0, 180), eyeScale = 1 - seg(t, 180, 300);
      float grow = easeInOutCubic(seg(t, 150, 560));
      float fill = easeInCubic(seg(t, 520, 700));
      float bd = easeInCubic(seg(t, 100, 400));
      if (bd < 1) rrect(g, BM_CX - BM_BAR_W / 2, BM_BAR_Y + bd * 120, BM_BAR_W, BM_BAR_H, BM_BAR_H / 2, C_WHITE);
      float L = lerpf(BM_LEFT, -2, grow), T = lerpf(BM_TOP, -2, grow), W = lerpf(BM_W, SW + 4, grow), H = lerpf(BM_H, SH + 4, grow);
      float R = lerpf(46 * BM_S, 18, grow), st = lerpf(28 * BM_S, 8, grow);
      st = lerpf(st, 170, fill);
      rrect(g, L, T, W, H, R, C_WHITE);
      if (W - 2 * st > 1 && H - 2 * st > 1) rrect(g, L + st, T + st, W - 2 * st, H - 2 * st, fmaxf(R - st, 2), C_BG);
      if (eyeScale > 0.02f) {
        float ew = 24 * BM_S, eh = fmaxf(56 * BM_S * eyes, 2) * eyeScale;
        for (int i = -1; i <= 1; i += 2) rrect(g, L + W / 2 + i * 26 * BM_S - ew / 2, T + H / 2 - 4 * BM_S - eh / 2, ew, eh, ew / 2, C_TEAL);
      }
    } else {
      drawMenu(g, now, false);
      float k = easeInOutCubic(seg(t, 700, 1100));
      rrect(g, lerpf(0, tileX(0), k), lerpf(0, tileY(0), k), lerpf(SW, TILE_W, k), lerpf(SH, TILE_H, k), lerpf(0, 12, k), C_WHITE);
      if (k >= 1) drawTileContent(g, 0, tileX(0), tileY(0), true);
    }
  }

  // ======== Menu ========
  void drawTileContent(Canvas& g, int i, int x, int y, bool selected) {
    drawIcon(g, MENU[i].icon, x + (TILE_W - 34) / 2, y + 12, MENU[i].color);
    char name[12]; int n = 0;
    for (const char* p = MENU[i].name; *p && n < 11; p++) name[n++] = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p;
    name[n] = 0;
    textC(g, F_SMALL, x + TILE_W / 2, y + 56, name, selected ? C_BG : C_WHITE);
  }
  // WiFi + keyboard status pills (top right)
  void drawStatus(Canvas& g, uint32_t now, int y) {
    int ws = plat_wifiState();
    bool blinkOn = (now / 300) % 2;
    rrect(g, 222, y, 34, 20, 10, C_CARD);
    if (ws == 2) {
      char ss[33], ip[20]; int rssi = -90;
      plat_wifiInfo(ss, sizeof(ss), ip, sizeof(ip), &rssi);
      drawBars(g, 232, y + 15, rssiLevel(rssi), C_WHITE, C_LINE);
    } else drawBars(g, 232, y + 15, 4, ws == 1 ? (blinkOn ? C_YELLOW : C_LINE) : C_LINE, C_LINE);
    int ks = plat_kbState();
    uint16_t dc = ks == 3 ? C_GREEN : (ks == 0 ? C_DIM : (blinkOn ? C_YELLOW : C_BG));
    rrect(g, 262, y, 48, 20, 10, C_CARD);
    g.fillCircle(274, y + 10, 4, dc);
    text(g, F_SMALL, 284, y + 6, "KB", ks == 3 ? C_WHITE : C_DIM);
  }
  void drawHeader(Canvas& g, uint32_t now, const char* title) {
    g.fillRect(0, 0, SW, 42, C_BG);
    Mascot m; m.cx = 22; m.cy = 18; m.s = 0.1f;
    m.eyeOpen = 1 - bump(seg(now % 4200, 3900, 4100));
    float sw = sinf(now * 0.0011f);
    m.look = sw > 0.6f ? 1 : (sw < -0.6f ? -1 : 0);
    drawMascot(g, m);
    char t[48]; snprintf(t, sizeof(t), "%s", title);
    int n = (int)strlen(t);
    if (textW(g, F_BIG, t) > 172) {                    // too long: shorten with ".."
      while (n > 1) { t[--n] = 0; char tt[52]; snprintf(tt, sizeof(tt), "%s..", t); if (textW(g, F_BIG, tt) <= 172) { strcpy(t, tt); break; } }
    }
    text(g, F_BIG, 42, 29, t, C_WHITE);
    drawStatus(g, now, 10);
    g.drawFastHLine(10, 39, 300, C_LINE);
  }
  // compact header for the chat (more room for text)
  void drawMiniHeader(Canvas& g, uint32_t now, const char* title) {
    g.fillRect(0, 0, SW, 32, C_BG);
    Mascot m; m.cx = 18; m.cy = 14; m.s = 0.075f;
    m.eyeOpen = 1 - bump(seg(now % 4200, 3900, 4100));
    if (chat.busy) m.look = sinf(now * 0.006f);
    drawMascot(g, m);
    text(g, F_BOLD, 34, 21, title, C_WHITE);
    if (aiPro) { rrect(g, 70, 8, 30, 16, 8, blend(C_TEAL, C_BG, 0.6f)); text(g, F_SMALL, 76, 12, "PRO", C_TEAL); }
    drawStatus(g, now, 5);
    g.drawFastHLine(0, 31, SW, C_LINE);
  }
  void drawFooter(Canvas& g, const char* left, const char* right) {
    g.fillRect(0, 216, SW, SH - 216, C_BG);
    text(g, F_SMALL, 12, 224, left, C_SOFT);
    textR(g, F_SMALL, SW - 12, 224, right, C_DIM);
  }
  void drawMenu(Canvas& g, uint32_t now, bool highlight) {
    g.fillScreen(C_BG);
    for (int i = 0; i < N_MENU; i++) {
      float in = seg(now, menuAt + i * 40, menuAt + i * 40 + 320);
      if (in <= 0) continue;
      int x = tileX(i), y = tileY(i) + ir((1 - easeOutBack(in)) * 24);
      rrect(g, x, y, TILE_W, TILE_H, 12, blend(C_BG, C_CARD, in));
    }
    if (highlight) rrect(g, hx, hy, TILE_W, TILE_H, 12, C_WHITE);
    for (int i = 0; i < N_MENU; i++) {
      float in = seg(now, menuAt + i * 40, menuAt + i * 40 + 320);
      if (in <= 0) continue;
      int x = tileX(i), y = tileY(i) + ir((1 - easeOutBack(in)) * 24);
      bool isSel = highlight && i == sel && fabsf(hx - x) < TILE_W / 2 && fabsf(hy - tileY(i)) < TILE_H / 2;
      drawTileContent(g, i, x, y, isSel);
    }
    drawHeader(g, now, "Arcade");
    char left[40];
    if (MENU[sel].kind == MK_GAME) {
      int b = bests[sel - FIRST_GAME];
      if (b) snprintf(left, sizeof(left), "%s   BEST %d", MENU[sel].name, b); else snprintf(left, sizeof(left), "%s   NEW", MENU[sel].name);
    } else if (MENU[sel].kind == MK_AI) {
      snprintf(left, sizeof(left), "Ask AI   %s", activeKey < 0 ? "NO KEY" : (wifiOk() ? "ONLINE" : "NO WIFI"));
    } else snprintf(left, sizeof(left), "%s", MENU[sel].name);
    drawFooter(g, left, "DXZC move   ENTER open");
  }
  void drawLaunch(Canvas& g, uint32_t t, uint32_t now) {
    drawMenu(g, now, true);
    float k = easeInOutCubic(seg(t, 0, 320));
    uint16_t c = blend(MENU[sel].color, C_BG, seg(t, 320, 480));
    rrect(g, lerpf(hx, 0, k), lerpf(hy, 0, k), lerpf(TILE_W, SW, k), lerpf(TILE_H, SH, k), lerpf(12, 0, k), c);
    float nk = seg(t, 120, 320) * (1 - seg(t, 330, 470));
    if (nk > 0) textC(g, F_HUGE, SW / 2, ir(132 + 12 * (1 - easeOutCubic(nk))), MENU[sel].name, blend(c, C_BG, nk));
  }

  // ======== Lists (settings, WiFi, keys) ========
  void toggle(Canvas& g, int x, int y, bool on) {
    rrect(g, x, y, 36, 18, 9, on ? C_TEAL : C_LINE);
    g.fillCircle(on ? x + 27 : x + 9, y + 9, 7, C_WHITE);
  }
  void chevron(Canvas& g, int x, int y, uint16_t c) {
    g.drawLine(x, y - 4, x + 4, y, c); g.drawLine(x + 4, y, x, y + 4, c);
    g.drawLine(x + 1, y - 4, x + 5, y, c); g.drawLine(x + 5, y, x + 1, y + 4, c);
  }
  // one list row; returns the sub-text colour to use for extras
  void row(Canvas& g, int y, const char* title, const char* sub, bool s, uint16_t subCol) {
    rrect(g, 10, y, 300, 36, 10, s ? C_WHITE : C_CARD);
    text(g, F_BOLD, 22, y + 17, title, s ? C_BG : C_WHITE);
    text(g, F_SMALL, 22, y + 23, sub, subCol ? subCol : (s ? rgb(90, 90, 96) : C_DIM));
  }
  void scrollHint(Canvas& g, float top, int n, int vis, int y0, int h) {
    if (n <= vis) return;
    int th = imax(12, h * vis / n);
    int ty = y0 + ir((h - th) * top / (float)(n - vis));
    rrect(g, 314, ty, 3, th, 1, C_LINE);
  }
  void drawSettings(Canvas& g, uint32_t now) {
    g.fillScreen(C_BG);
    static const char* names[10] = {"WiFi", "API keys", "AI mode", "Short answers", "Secure connection",
                                    "Flip screen", "Show FPS", "Keyboard", "Reset scores", "About"};
    int ks = plat_kbState();
    bool armed = resetArmAt && now - resetArmAt < 3000;
    char sub[48];
    for (int i = 0; i < 10; i++) {
      int y = 46 + ir((i - setTop) * 42);
      if (y < 4 || y > 214) continue;
      bool s = i == setSel;
      uint16_t subCol = 0;
      sub[0] = 0;
      switch (i) {
        case 0: {
          int w = plat_wifiState();
          if (w == 2) { char ss[33], ip[20]; int r; plat_wifiInfo(ss, sizeof(ss), ip, sizeof(ip), &r); snprintf(sub, sizeof(sub), "CONNECTED: %.24s", ss); }
          else snprintf(sub, sizeof(sub), "%s", w == 1 ? "CONNECTING..." : (nNets ? "NOT CONNECTED" : "NO NETWORKS SAVED"));
          break;
        }
        case 1: if (activeKey >= 0) snprintf(sub, sizeof(sub), "IN USE: %.30s", keys[activeKey].label); else { snprintf(sub, sizeof(sub), "NO KEY YET - ADD ONE"); subCol = C_ORANGE; } break;
        case 2: snprintf(sub, sizeof(sub), "%s", aiPro ? "PRO: DEEPER RESEARCH, COSTS MORE" : "FAST: QUICK WEB ANSWERS"); break;
        case 3: snprintf(sub, sizeof(sub), "%s", shortAns ? "FITS THE SMALL SCREEN" : "FULL-LENGTH ANSWERS"); break;
        case 4: snprintf(sub, sizeof(sub), "%s", tlsVerify ? "VERIFIES THE API SERVER" : "NOT VERIFIED - LESS SAFE"); if (!tlsVerify) subCol = C_ORANGE; break;
        case 5: snprintf(sub, sizeof(sub), "USE IF THE PICTURE IS UPSIDE DOWN"); break;
        case 6: snprintf(sub, sizeof(sub), "%s", showFps ? "ON" : "OFF"); break;
        case 7: snprintf(sub, sizeof(sub), "%s", ks == 3 ? "CARDKB2 CONNECTED" : ks == 2 ? "PAIRED" : ks == 1 ? "CONNECTING..." : "SEARCHING..."); break;
        case 8: snprintf(sub, sizeof(sub), "%s", resetDoneAt && now - resetDoneAt < 2000 ? "SCORES CLEARED" : (armed ? "PRESS AGAIN TO CONFIRM" : "CLEARS ALL BEST SCORES")); if (armed) subCol = C_RED; break;
        case 9: snprintf(sub, sizeof(sub), "ARCADE OS %s", OS_VERSION); break;
      }
      row(g, y, names[i], sub, s, subCol);
      uint16_t acc = s ? C_BG : C_SOFT;
      switch (i) {
        case 0: case 1: case 9: chevron(g, 290, y + 18, acc); break;
        case 2: {
          const char* v = aiPro ? "PRO" : "FAST";
          int w = textW(g, F_SMALL, v) + 16;
          rrect(g, 298 - w, y + 9, w, 18, 9, s ? C_BG : C_LINE);
          text(g, F_SMALL, 306 - w, y + 14, v, s ? C_WHITE : C_WHITE);
          break;
        }
        case 3: toggle(g, 262, y + 9, shortAns); break;
        case 4: toggle(g, 262, y + 9, tlsVerify); break;
        case 5: toggle(g, 262, y + 9, flip); break;
        case 6: toggle(g, 262, y + 9, showFps); break;
        case 7: g.fillCircle(280, y + 18, 5, ks == 3 ? C_GREEN : C_DIM); break;
      }
    }
    scrollHint(g, setTop, 10, 4, 46, 162);
    drawHeader(g, now, "Settings");
    drawFooter(g, "", "ENTER select   ESC back");
  }

  // ======== WiFi ========
  void spinner(Canvas& g, int cx, int cy, uint32_t now, uint16_t c) {
    for (int i = 0; i < 8; i++) {
      float a = i * 3.14159f / 4 + now * 0.008f;
      g.fillCircle(ir(cx + cosf(a) * 9), ir(cy + sinf(a) * 9), i < 3 ? 2 : 1, blend(c, C_BG, i / 9.0f));
    }
  }
  void drawWifi(Canvas& g, uint32_t now) {
    g.fillScreen(C_BG);
    // status card
    int ws = plat_wifiState();
    rrect(g, 10, 46, 300, 38, 10, C_CARD);
    char ss[33] = "", ip[20] = ""; int rssi = -90;
    if (ws == 2) {
      plat_wifiInfo(ss, sizeof(ss), ip, sizeof(ip), &rssi);
      drawBars(g, 22, 72, rssiLevel(rssi), C_GREEN, C_LINE);
      char b[48]; snprintf(b, sizeof(b), "%.26s", ss);
      text(g, F_BOLD, 46, 63, b, C_WHITE);
      snprintf(b, sizeof(b), "CONNECTED   %s   %d dBm", ip, rssi);
      text(g, F_SMALL, 46, 70, b, C_GREEN);
    } else {
      drawBars(g, 22, 72, 4, ws == 1 ? C_YELLOW : C_LINE, C_LINE);
      text(g, F_BOLD, 46, 63, ws == 1 ? "Connecting..." : "Not connected", C_WHITE);
      text(g, F_SMALL, 46, 70, nNets ? "PICK A SAVED NETWORK OR SCAN" : "SCAN AND PICK YOUR NETWORK", C_DIM);
    }
    // list
    int n = wifiRowCount();
    for (int i = 0; i < n; i++) {
      int y = 92 + ir((i - wTop) * 40);
      if (y < 50 || y > 200) continue;
      bool s = i == wSel;
      if (!wifiScanView) {
        if (i == 0) {
          row(g, y, "Scan for networks", nNets ? "FIND A NEW NETWORK" : "START HERE", s, 0);
          chevron(g, 290, y + 18, s ? C_BG : C_SOFT);
        } else {
          SavedNet& sn = nets[i - 1];
          bool isCur = ws == 2 && !strcmp(ss, sn.ssid);
          row(g, y, sn.ssid, isCur ? "SAVED  -  CONNECTED" : "SAVED  -  ENTER FOR OPTIONS", s, isCur ? C_GREEN : 0);
        }
      } else {
        if (scanning) {
          if (i == 0) { rrect(g, 10, y, 300, 36, 10, C_CARD); spinner(g, 30, y + 18, now, C_TEAL); text(g, F_BOLD, 48, y + 23, "Scanning...", C_WHITE); }
          continue;
        }
        if (nFound == 0) { row(g, y, "No networks found", "ENTER TO SCAN AGAIN", s, 0); continue; }
        FoundNet& f = found[i];
        char sub[40];
        snprintf(sub, sizeof(sub), "%s%s", f.open ? "OPEN" : "SECURED", findNet(f.ssid) >= 0 ? "  -  SAVED" : "");
        row(g, y, f.ssid, sub, s, 0);
        drawBars(g, 284, y + 26, rssiLevel(f.rssi), s ? C_BG : C_WHITE, s ? rgb(190, 190, 196) : C_LINE);
        if (!f.open) drawLock(g, 268, y + 14, s ? C_BG : C_SOFT, s ? C_WHITE : C_CARD);
      }
    }
    scrollHint(g, wTop, n, 3, 92, 118);
    drawHeader(g, now, "WiFi");
    drawFooter(g, wifiScanView ? "PICK YOUR NETWORK" : "", "ENTER select   ESC back");
    // connecting / result pop-up
    if (connPhase) {
      dimScreen(g);
      rrect(g, 40, 70, 240, 100, 14, C_CARD);
      g.drawRoundRect(40, 70, 240, 100, 14, connPhase == 3 ? C_RED : (connPhase == 2 ? C_GREEN : C_LINE));
      char b[48]; snprintf(b, sizeof(b), "%.24s", connSsid);
      if (connPhase == 1) {
        spinner(g, 160, 98, now, C_TEAL);
        textC(g, F_BOLD, 160, 132, b, C_WHITE);
        textC(g, F_SMALL, 160, 146, "CONNECTING...   ESC CANCEL", C_DIM);
      } else if (connPhase == 2) {
        g.fillCircle(160, 98, 12, C_GREEN);
        g.drawLine(154, 98, 158, 103, C_BG); g.drawLine(158, 103, 166, 93, C_BG);
        g.drawLine(154, 99, 158, 104, C_BG); g.drawLine(158, 104, 166, 94, C_BG);
        textC(g, F_BOLD, 160, 132, "Connected", C_WHITE);
        textC(g, F_SMALL, 160, 146, b, C_SOFT);
      } else {
        g.fillCircle(160, 98, 12, C_RED);
        g.drawLine(155, 93, 165, 103, C_BG); g.drawLine(165, 93, 155, 103, C_BG);
        g.drawLine(156, 93, 166, 103, C_BG); g.drawLine(166, 93, 156, 103, C_BG);
        textC(g, F_BOLD, 160, 132, reasonText(), C_WHITE);
        textC(g, F_SMALL, 160, 146, connOpen ? "ENTER RETRY   ESC CLOSE" : "ENTER RETYPE PASSWORD   ESC CLOSE", C_DIM);
      }
    }
  }

  // ======== API keys ========
  void drawKeys(Canvas& g, uint32_t now) {
    g.fillScreen(C_BG);
    int n = keyRowCount();
    for (int i = 0; i < n; i++) {
      int y = 46 + ir((i - kTop) * 42);
      if (y < 4 || y > 214) continue;
      bool s = i == kSel;
      if (i < nKeys) {
        char m[40], sub[56]; maskKey(keys[i].val, m, sizeof(m));
        snprintf(sub, sizeof(sub), "%s%s", m, i == activeKey ? "   IN USE" : "");
        row(g, y, keys[i].label, sub, s, i == activeKey ? (s ? rgb(20, 120, 140) : C_TEAL) : 0);
        if (i == activeKey) {
          g.fillCircle(288, y + 18, 8, C_TEAL);
          g.drawLine(284, y + 18, 287, y + 21, C_BG); g.drawLine(287, y + 21, 292, y + 15, C_BG);
        }
      } else {
        row(g, y, "+  Add API key", nKeys ? "UP TO 5 KEYS" : "NEEDED FOR ASK AI", s, 0);
      }
    }
    scrollHint(g, kTop, n, 4, 46, 162);
    if (nKeys == 0) {
      textC(g, F_SMALL, 160, 100, "Make a key at console.perplexity.ai", C_SOFT);
      textC(g, F_SMALL, 160, 114, "(API Keys page), then type it in here.", C_SOFT);
      textC(g, F_SMALL, 160, 134, "Keys stay on this device only.", C_DIM);
    }
    drawHeader(g, now, "API keys");
    drawFooter(g, "", "ENTER select   ESC back");
  }

  // ======== Text entry ========
  void drawText(Canvas& g, uint32_t now) {
    g.fillScreen(C_BG);
    text(g, F_SMALL, 12, 48, teHint, C_DIM);
    int bx = 10, by = 60, bw = 300, bh = 132;
    rrect(g, bx, by, bw, bh, 12, C_CARD);
    g.drawRoundRect(bx, by, bw, bh, 12, blend(C_TEAL, C_CARD, 0.4f));
    bool cursorOn = (now / 530) % 2;
    if (teMono) {
      // big fixed-width letters so every character can be checked
      const int cw = 12, lh = 18, perLine = (bw - 24) / cw, maxLines = 7;
      int lines = (teLen + perLine) / perLine;          // +1 slot for the cursor
      int first = lines > maxLines ? lines - maxLines : 0;
      g.setFont(nullptr); g.setTextSize(2); g.setTextWrap(false);
      for (int l = first; l < lines; l++) {
        int y = by + 10 + (l - first) * lh;
        for (int c = 0; c < perLine; c++) {
          int i = l * perLine + c;
          if (i > teLen) break;
          int x = bx + 12 + c * cw;
          if (i == teLen) { if (cursorOn) g.fillRect(x, y, 10, 15, C_TEAL); break; }
          char ch = teBuf[i];
          // tell look-alike characters apart: 0 vs O, 1 vs l vs I
          uint16_t col = C_WHITE;
          if (ch >= '0' && ch <= '9') col = C_TEAL;
          else if (ch == ' ') { g.fillRect(x + 2, y + 13, 7, 1, C_DIM); continue; }
          else if (!isalpha((unsigned char)ch)) col = C_YELLOW;
          g.drawChar(x, y, ch, col, col, 2);
        }
      }
      g.setTextSize(1);
    } else {
      const char* s = teBuf; int w = textW(g, F_BIG, s);
      while (w > bw - 30 && *s) { s++; w = textW(g, F_BIG, s); }
      text(g, F_BIG, bx + 14, by + 44, s, C_WHITE);
      if (cursorOn) g.fillRect(bx + 16 + w, by + 26, 3, 22, C_TEAL);
    }
    char cnt[40];
    snprintf(cnt, sizeof(cnt), "%d CHARS%s", teLen, teMono ? "   NUMBERS TEAL, SYMBOLS YELLOW" : "");
    text(g, F_SMALL, 12, 200, cnt, C_DIM);
    drawHeader(g, now, teTitle);
    drawFooter(g, "BKSP delete", "ENTER save   ESC cancel");
  }

  // ======== Pop-up menu ========
  void drawSheet(Canvas& g) {
    dimScreen(g);
    int h = 44 + sheetN * 32, y = (SH - h) / 2;
    rrect(g, 50, y, 220, h, 14, C_CARD);
    g.drawRoundRect(50, y, 220, h, 14, C_LINE);
    char t[30]; snprintf(t, sizeof(t), "%.22s", sheetTitle);
    textC(g, F_BOLD, 160, y + 24, t, C_WHITE);
    for (int i = 0; i < sheetN; i++) {
      int ry = y + 36 + i * 32;
      bool s = i == sheetSel;
      if (s) rrect(g, 60, ry, 200, 28, 9, C_WHITE);
      const char* o = sheetOpt[i] ? sheetOpt[i] : "";
      bool danger = !strcmp(o, "Delete") || !strcmp(o, "Forget");
      textC(g, F_REG, 160, ry + 19, o, s ? (danger ? rgb(190, 40, 40) : C_BG) : (danger ? C_RED : C_WHITE));
    }
  }

  // ======== About ========
  void drawAbout(Canvas& g, uint32_t now) {
    g.fillScreen(C_BG);
    Mascot m; m.cx = 66; m.cy = 96; m.s = 0.34f;
    m.eyeOpen = 1 - bump(seg(now % 3500, 3200, 3380));
    m.lift = 4 * (0.5f + 0.5f * sinf(now * 0.004f));
    m.look = sinf(now * 0.0015f);
    drawMascot(g, m);
    textC(g, F_BOLD, 218, 64, "Altoids Gameboy", C_WHITE);
    textC(g, F_SMALL, 218, 74, "ARCADE OS  " OS_VERSION, C_TEAL);
    const char* lines[] = {"ESP32-S3 N16R8", "ST7789 320x240 display", "CardKB2 over Bluetooth LE", "WiFi + Perplexity Agent API", "700mAh LiPo + MT3608 5V"};
    for (int i = 0; i < 5; i++) textC(g, F_SMALL, 218, 96 + i * 16, lines[i], C_SOFT);
    drawFooter(g, "", "ESC back");
  }
};

// ---------------- Display ----------------
#define TFT_SCK   12
#define TFT_MOSI  11
#define TFT_RST   10
#define TFT_DC     9
#define TFT_CS     8
#define SCREEN_ROTATION 3   // landscape, pins on the LEFT (confirmed). 1 = turned 180 degrees
Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);
Canvas* canvas = nullptr;   // full-screen frame buffer (320x240x2 = 150 KB, lives in PSRAM)

App app;
Input input;
Preferences prefs;

// ---------------- Platform functions used by the UI ----------------
static volatile int g_kbState = 0;
uint32_t plat_millis() { return millis(); }
long plat_random(long n) { return n > 0 ? (long)(esp_random() % (uint32_t)n) : 0; }
int  plat_loadInt(const char* key, int def) { return prefs.getInt(key, def); }
void plat_saveInt(const char* key, int v) { prefs.putInt(key, v); }
int  plat_kbState() { return g_kbState; }
void plat_setFlip(bool flip) { tft.setRotation(flip ? (SCREEN_ROTATION + 2) % 4 : SCREEN_ROTATION); }

void plat_loadStr(const char* key, char* out, int max) {
  if (max <= 0) return;
  out[0] = 0;
  if (!prefs.isKey(key)) return;
  String v = prefs.getString(key, "");
  snprintf(out, max, "%s", v.c_str());
}
void plat_saveStr(const char* key, const char* v) { prefs.putString(key, v); }
void* plat_bigAlloc(int bytes) { void* p = ps_malloc(bytes); return p ? p : malloc(bytes); }

// ---------------- WiFi ----------------
static volatile int g_wState = 0;          // 0 off, 1 connecting, 2 connected, 3 failed
static volatile int g_wReason = 0, g_wDrops = 0;
static uint32_t g_wStart = 0;
static bool g_scanBusy = false;

static void onWifiEvent(arduino_event_id_t ev, arduino_event_info_t info) {
  if (ev == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) { g_wReason = info.wifi_sta_disconnected.reason; g_wDrops = g_wDrops + 1; }
}
void plat_wifiScanStart() {
  if (g_scanBusy && WiFi.scanComplete() == WIFI_SCAN_RUNNING) return;
  WiFi.scanDelete();
  g_scanBusy = WiFi.scanNetworks(true, false) == WIFI_SCAN_RUNNING;
}
int plat_wifiScanCount() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return -1;
  g_scanBusy = false;
  return n < 0 ? -2 : n;
}
void plat_wifiScanGet(int i, char* ssid, int max, int* rssi, int* open) {
  snprintf(ssid, max, "%s", WiFi.SSID(i).c_str());
  *rssi = WiFi.RSSI(i);
  *open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
}
void plat_wifiConnect(const char* ssid, const char* pass) {
  WiFi.disconnect(false, false);
  g_wReason = 0; g_wDrops = 0;
  WiFi.begin(ssid, (pass && pass[0]) ? pass : nullptr);
  g_wState = 1; g_wStart = millis();
}
void plat_wifiDisconnect() { WiFi.disconnect(false, false); g_wState = 0; }
int plat_wifiState() {
  bool up = WiFi.status() == WL_CONNECTED && (uint32_t)WiFi.localIP() != 0;
  if (up) { g_wState = 2; return 2; }
  if (g_wState == 2) { g_wState = 1; g_wStart = millis(); g_wDrops = 0; }   // dropped: auto-reconnect is running
  if (g_wState == 1) {
    int r = g_wReason;
    bool badPass = r == 2 || r == 15 || r == 202 || r == 204;
    if (millis() - g_wStart > 15000 || (badPass && g_wDrops >= 2)) { WiFi.disconnect(false, false); g_wState = 3; }
  }
  return g_wState;
}
int plat_wifiReason() { return g_wReason; }
void plat_wifiInfo(char* ssid, int max, char* ip, int ipmax, int* rssi) {
  snprintf(ssid, max, "%s", WiFi.SSID().c_str());
  snprintf(ip, ipmax, "%s", WiFi.localIP().toString().c_str());
  *rssi = WiFi.RSSI();
}

// ---------------- Perplexity Agent API (runs on core 0) ----------------
static const char* AI_HOST = "api.perplexity.ai";
static const int AI_OUT_CAP = 6400, AI_LINE_CAP = 16384;
static AiStream g_ai;
static char* g_aiLine = nullptr; static char* g_aiOut = nullptr; static char* g_aiBody = nullptr;
static char g_aiKey[168];
static bool g_aiTls = true;
static volatile int g_aiState = 0;         // 0 idle, 1 connecting, 2 waiting, 3 streaming, 4 done, 5 error
static volatile bool g_aiCancel = false;
static int g_aiReadPos = 0;
static char g_aiErr[200];
static SemaphoreHandle_t g_aiLock;
static TaskHandle_t g_aiTask;
static uint8_t g_aiBuf[4096];

static void aiFail(const char* msg) {
  xSemaphoreTake(g_aiLock, portMAX_DELAY);
  snprintf(g_aiErr, sizeof(g_aiErr), "%s", msg);
  xSemaphoreGive(g_aiLock);
  g_aiState = 5;
}
static void aiRun() {
  NetworkClientSecure client;
  if (g_aiTls) client.useBuiltinCACertBundle(); else client.setInsecure();
  client.setHandshakeTimeout(15);
  if (!client.connect(AI_HOST, 443, 15000)) {
    char e[120] = ""; client.lastError(e, sizeof(e));
    char m[200];
    snprintf(m, sizeof(m), "Could not reach the API%s%s", e[0] ? ": " : ".", e);
    aiFail(m); return;
  }
  if (g_aiCancel) { client.stop(); aiFail("Stopped."); return; }
  g_aiState = 2;
  int blen = (int)strlen(g_aiBody);
  char head[512];
  int hl = snprintf(head, sizeof(head),
    "POST /v1/agent HTTP/1.1\r\nHost: %s\r\nAuthorization: Bearer %s\r\n"
    "Content-Type: application/json\r\nAccept: text/event-stream\r\n"
    "Content-Length: %d\r\nConnection: close\r\n\r\n", AI_HOST, g_aiKey, blen);
  if (hl <= 0 || hl >= (int)sizeof(head)) { client.stop(); aiFail("API key is too long."); return; }
  bool ok = client.write((const uint8_t*)head, hl) == (size_t)hl;
  for (int off = 0; ok && off < blen; ) {
    int n = blen - off > 1024 ? 1024 : blen - off;
    int w = client.write((const uint8_t*)g_aiBody + off, n);
    if (w <= 0) ok = false; else off += w;
  }
  if (!ok) { client.stop(); aiFail("Sending the question failed. Check WiFi."); return; }
  uint32_t lastData = millis(), started = millis();
  for (;;) {
    if (g_aiCancel) break;
    int a = client.available();
    if (a > 0) {
      int n = client.read(g_aiBuf, a > (int)sizeof(g_aiBuf) ? (int)sizeof(g_aiBuf) : a);
      if (n > 0) {
        xSemaphoreTake(g_aiLock, portMAX_DELAY);
        g_ai.feed(g_aiBuf, n);
        xSemaphoreGive(g_aiLock);
        if (g_ai.gotText) g_aiState = 3;
        lastData = millis();
        if (g_ai.gotDone) break;
      }
      continue;
    }
    if (!client.connected()) break;
    if (millis() - lastData > 60000 || millis() - started > 240000) break;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  client.stop();
  xSemaphoreTake(g_aiLock, portMAX_DELAY);
  g_ai.finish();
  xSemaphoreGive(g_aiLock);
  if (g_aiCancel) { aiFail("Stopped."); return; }
  if (g_ai.failed) { aiFail(g_ai.err[0] ? g_ai.err : "The request failed."); return; }
  if (!g_ai.gotText) { aiFail(millis() - lastData > 60000 ? "The API stopped answering (timeout)." : "No answer came back."); return; }
  if (!g_ai.gotDone || g_ai.incomplete) {
    xSemaphoreTake(g_aiLock, portMAX_DELAY);
    snprintf(g_aiErr, sizeof(g_aiErr), "Answer was cut off.");
    xSemaphoreGive(g_aiLock);
  }
  g_aiState = 4;
}
static void aiTask(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    aiRun();
  }
}
bool plat_aiStart(const char* apiKey, const char* body, bool verifyTls) {
  if (g_aiState == 1 || g_aiState == 2 || g_aiState == 3) return false;
  if (!g_aiLine || !g_aiOut || !g_aiBody) return false;
  int bl = (int)strlen(body);
  if (bl >= Chat::BODY_CAP) return false;
  memcpy(g_aiBody, body, bl + 1);
  snprintf(g_aiKey, sizeof(g_aiKey), "%s", apiKey);
  g_aiTls = verifyTls;
  xSemaphoreTake(g_aiLock, portMAX_DELAY);
  g_ai.begin(g_aiLine, AI_LINE_CAP, g_aiOut, AI_OUT_CAP);
  g_aiReadPos = 0; g_aiErr[0] = 0;
  xSemaphoreGive(g_aiLock);
  g_aiCancel = false;
  g_aiState = 1;
  xTaskNotifyGive(g_aiTask);
  return true;
}
int plat_aiState() { return g_aiState; }
int plat_aiRead(char* out, int max) {
  xSemaphoreTake(g_aiLock, portMAX_DELAY);
  int n = g_ai.outLen - g_aiReadPos;
  if (n > max) n = max;
  if (n > 0) { memcpy(out, g_aiOut + g_aiReadPos, n); g_aiReadPos += n; }
  xSemaphoreGive(g_aiLock);
  return n > 0 ? n : 0;
}
void plat_aiInfo(char* status, int smax, char* sources, int srcmax, char* err, int emax) {
  xSemaphoreTake(g_aiLock, portMAX_DELAY);
  const char* s = g_ai.status;
  if (g_aiState == 1) s = "Connecting";
  else if (g_aiState == 2 && !s[0]) s = "Thinking";
  snprintf(status, smax, "%s", s);
  snprintf(sources, srcmax, "%s", g_ai.sources);
  snprintf(err, emax, "%s", g_aiErr);
  xSemaphoreGive(g_aiLock);
}
void plat_aiCancel() { g_aiCancel = true; }

// ---------------- BLE (same working flow as KeyboardScreenTest) ----------------
static NimBLEUUID kHidSvcUUID((uint16_t)0x1812);
static NimBLEUUID kReportUUID((uint16_t)0x2A4D);
static NimBLEAddress g_addr;
static NimBLEClient* g_client = nullptr;
static volatile bool g_hasTarget = false, g_connected = false, g_authReady = false, g_subscribed = false;

struct Report { uint8_t len; uint8_t data[16]; };
static QueueHandle_t g_reportQueue;

static void reportCB(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
  Report r;
  r.len = (len > sizeof(r.data)) ? sizeof(r.data) : len;
  memcpy(r.data, data, r.len);
  xQueueSend(g_reportQueue, &r, 0);
}

struct ClientCB : NimBLEClientCallbacks {
  void onConnect(NimBLEClient* c) override {
    g_connected = true; g_authReady = false; g_subscribed = false;
    int rc; NimBLEDevice::startSecurity(c->getConnHandle(), &rc);
  }
  void onDisconnect(NimBLEClient*, int reason) override {
    Serial.printf("[BLE] Disconnected (%d)\n", reason);
    g_connected = false; g_authReady = false; g_subscribed = false;
  }
  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    g_authReady = info.isAuthenticated() || info.isEncrypted();
    if (!g_authReady && g_client) g_client->disconnect();
  }
  bool onConnParamsUpdateRequest(NimBLEClient*, const ble_gap_upd_params*) override { return true; }
};

struct AdvCB : NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    if (g_hasTarget) return;
    bool nameMatch = d->getName().rfind("CardKB2", 0) == 0;
    if (!nameMatch && !d->isAdvertisingService(kHidSvcUUID)) return;
    Serial.printf("[BLE] Found: %s\n", d->getName().c_str());
    g_addr = d->getAddress(); g_hasTarget = true;
    NimBLEDevice::getScan()->stop();
  }
};

static void startScan() {
  NimBLEScan* s = NimBLEDevice::getScan();
  if (s->isScanning()) return;
  g_hasTarget = false;
  s->start(0, false);
}

static bool subscribeReports() {
  NimBLERemoteService* svc = g_client ? g_client->getService(kHidSvcUUID) : nullptr;
  if (!svc) return false;
  bool any = false;
  for (auto* c : svc->getCharacteristics(true))
    if (c->getUUID() == kReportUUID && (c->canNotify() || c->canIndicate()))
      if (c->subscribe(true, reportCB, true)) any = true;
  if (any) Serial.println("[BLE] Keyboard ready");
  return any;
}

// Runs on core 0 so connecting (which blocks) never freezes the animation on core 1
static void bleTask(void*) {
  NimBLEDevice::init("AltoidsGameboy");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setSecurityAuth(true, false, false);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEScan* s = NimBLEDevice::getScan();
  s->setScanCallbacks(new AdvCB(), true);
  s->setActiveScan(true);
  s->setInterval(100);
  s->setWindow(50);
  s->setDuplicateFilter(true);
  startScan();
  for (;;) {
    if (!g_connected) {
      if (!g_hasTarget) startScan();
      else {
        if (!g_client) {
          g_client = NimBLEDevice::createClient();
          g_client->setClientCallbacks(new ClientCB(), true);
        }
        if (!g_client->connect(g_addr, false)) { g_hasTarget = false; vTaskDelay(pdMS_TO_TICKS(500)); }
      }
    }
    if (g_connected && g_authReady && !g_subscribed) {
      g_subscribed = subscribeReports();
      if (!g_subscribed) vTaskDelay(pdMS_TO_TICKS(200));
    }
    g_kbState = g_subscribed ? 3 : g_authReady ? 2 : g_connected ? 1 : 0;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);
  delay(200);

  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.init(240, 320);
  tft.setSPISpeed(40000000);
  tft.invertDisplay(false);
  tft.setRotation(SCREEN_ROTATION);
  tft.fillScreen(ST77XX_BLACK);

  canvas = new Canvas(SW, SH);
  if (!canvas || !canvas->getBuffer()) {
    tft.setTextColor(ST77XX_RED); tft.setTextSize(2); tft.setCursor(10, 100);
    tft.print("No memory: enable");
    tft.setCursor(10, 122); tft.print("OPI PSRAM in Tools");
    for (;;) delay(1000);
  }
  Serial.printf("Frame buffer %s, free PSRAM %u\n",
                esp_ptr_external_ram(canvas->getBuffer()) ? "in PSRAM" : "in internal RAM", (unsigned)ESP.getFreePsram());

  prefs.begin("arcade", false);

  // WiFi (station mode; networks are saved by the app, not by the WiFi driver)
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(onWifiEvent);
  // AI buffers live in PSRAM; the TLS connection itself uses about 40 KB of internal RAM
  g_aiLine = (char*)ps_malloc(AI_LINE_CAP);
  g_aiOut  = (char*)ps_malloc(AI_OUT_CAP);
  g_aiBody = (char*)ps_malloc(Chat::BODY_CAP);
  g_aiLock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(aiTask, "ai", 16384, nullptr, 1, &g_aiTask, 0);

  g_reportQueue = xQueueCreate(32, sizeof(Report));
  xTaskCreatePinnedToCore(bleTask, "ble", 8192, nullptr, 1, nullptr, 0);
  app.begin();
}

void loop() {
  static int lastKb = -1;
  static uint32_t fpsT = 0, frames = 0, fps = 0;

  // 1) Keyboard reports -> buttons
  Report r;
  while (xQueueReceive(g_reportQueue, &r, 0) == pdTRUE) {
    const uint8_t* d = r.data; int len = r.len;
    if (len == 9 && d[0] == 0x01) { d++; len = 8; }   // report ID prefix
    if (len == 8) input.onHid(d[0], d + 2);
  }
  int kb = g_kbState;
  if (kb != lastKb) { if (kb != 3) input.releaseAll(); lastKb = kb; }

  // 2) Update and draw one frame
  uint32_t now = millis();
  input.frame(now);
  app.update(input, now);
  app.draw(*canvas, now);

  frames++;
  if (now - fpsT >= 1000) { fps = frames; frames = 0; fpsT = now; }
  if (app.showFps) {
    char b[8]; snprintf(b, sizeof(b), "%u", (unsigned)fps);
    canvas->fillRect(0, SH - 8, 20, 8, C_BG);
    text(*canvas, F_SMALL, 1, SH - 8, b, C_GREEN);
  }

  // 3) Send the finished frame to the screen in one go (no flicker)
  tft.drawRGBBitmap(0, 0, canvas->getBuffer(), SW, SH);
}
