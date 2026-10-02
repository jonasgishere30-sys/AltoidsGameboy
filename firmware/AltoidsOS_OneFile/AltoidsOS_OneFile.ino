// Altoids Gameboy - Arcade OS v1.6 (landscape) - SINGLE FILE
// Paste this whole file into any sketch (any folder name). No other files needed.
// Boot animation, scrolling menu, 15 built-in games and a Notes app, controlled with the CardKB2 over BLE.
// Games: Snake, Blocks, Pong, Breakout, Flappy, Invaders, Asteroids, Dino, Racer, Tron,
//        Stack, Jumper, Mines, Connect 4, Simon.   Notes: 6 notes x 1000 characters, saved in flash.
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
//           ESC / BACKSPACE = back / pause,  P = pause,  F = flag (Mines)
// Notes:    type normally, ENTER = new line, BACKSPACE = delete, arrow keys = move cursor,
//           ESC = save and go back. Notes also auto-save 3 s after you stop typing.

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
// ================= UI + games (all in one file) =================
// Boot animation -> playful transition -> menu -> games / settings / about
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
static const uint16_t C_LIME   = rgb(163, 230, 53);
static const uint16_t C_CYAN   = rgb(34, 211, 238);
static const uint16_t C_AMBER  = rgb(251, 191, 36);
static const uint16_t C_INDIGO = rgb(129, 140, 248);
static const uint16_t C_ROSE   = rgb(251, 113, 133);
static const uint16_t C_SLATE  = rgb(148, 163, 184);
static const uint16_t C_NOTE   = rgb(254, 240, 138);

// ---- Types used by the drawing functions (kept above every function so the
//      sketch also compiles as one single .ino file) ----
enum Font { F_SMALL, F_REG, F_BOLD, F_BIG, F_HUGE };
enum Icon { IC_SNAKE, IC_BLOCKS, IC_PONG, IC_BREAKOUT, IC_FLAPPY,
            IC_INVADERS, IC_ASTEROIDS, IC_DINO, IC_RACER, IC_TRON, IC_STACK, IC_JUMPER, IC_MINES, IC_C4, IC_SIMON,
            IC_NOTES, IC_SETTINGS, IC_ABOUT };
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

// Turns CardKB2 HID key reports into game buttons.
// Arrows or D/X/C/Z = direction (D up, X down, Z left, C right),
// SPACE/ENTER = A, ESC/BACKSPACE = B, P = pause
#include <stdint.h>
#include <string.h>

enum Btn : uint8_t { B_UP, B_DOWN, B_LEFT, B_RIGHT, B_A, B_B, B_PAUSE, B_F, B_COUNT };

// Typed keys (used by the Notes editor). Printable characters are their ASCII code.
enum : uint16_t { K_BKSP = 8, K_TAB = 9, K_ENTER = 10, K_ESC = 27, K_DEL = 127,
                  K_LEFT = 0x101, K_RIGHT, K_UP, K_DOWN, K_HOME, K_END };

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

  static int map(uint8_t kc) {
    switch (kc) {
      case 0x52: case 0x07: return B_UP;      // Up arrow, D
      case 0x51: case 0x1B: return B_DOWN;    // Down arrow, X
      case 0x50: case 0x1D: return B_LEFT;    // Left arrow, Z
      case 0x4F: case 0x06: return B_RIGHT;   // Right arrow, C
      case 0x2C: case 0x28: return B_A;       // Space, Enter
      case 0x29: case 0x2A: return B_B;       // Esc, Backspace
      case 0x13:            return B_PAUSE;   // P
      case 0x09:            return B_F;       // F (flag in Mines)
    }
    return -1;
  }

  // ---- typed-character queue (Notes) ----
  uint16_t kq[32]; uint8_t kqHead = 0, kqTail = 0;
  uint8_t  repCode = 0; uint16_t repKey = 0; bool repArm = false; uint32_t repStart = 0, repLast = 0;
  bool     caps = false;
  void pushKey(uint16_t k) { uint8_t n = (kqHead + 1) & 31; if (n != kqTail) { kq[kqHead] = k; kqHead = n; } }
  bool popKey(uint16_t& k) { if (kqTail == kqHead) return false; k = kq[kqTail]; kqTail = (kqTail + 1) & 31; return true; }
  void clearKeys() { kqHead = kqTail = 0; repCode = 0; repKey = 0; repArm = false; }
  // HID usage code + modifier byte -> character (US layout). 0 = not a typing key.
  uint16_t toChar(uint8_t kc, uint8_t mods) {
    bool sh = mods & 0x22;   // left or right shift
    if (kc >= 0x04 && kc <= 0x1D) return ((sh != caps) ? 'A' : 'a') + (kc - 0x04);
    if (kc >= 0x1E && kc <= 0x27) { static const char n[] = "1234567890", s[] = "!@#$%^&*()"; return sh ? s[kc - 0x1E] : n[kc - 0x1E]; }
    switch (kc) {
      case 0x28: case 0x58: return K_ENTER;
      case 0x29: return K_ESC;   case 0x2A: return K_BKSP;  case 0x2B: return K_TAB;  case 0x2C: return ' ';
      case 0x2D: return sh ? '_' : '-';   case 0x2E: return sh ? '+' : '=';
      case 0x2F: return sh ? '{' : '[';   case 0x30: return sh ? '}' : ']';   case 0x31: return sh ? '|' : '\\';
      case 0x33: return sh ? ':' : ';';   case 0x34: return sh ? '"' : '\'';
      case 0x35: return sh ? '~' : '`';   case 0x36: return sh ? '<' : ',';
      case 0x37: return sh ? '>' : '.';   case 0x38: return sh ? '?' : '/';
      case 0x4A: return K_HOME;  case 0x4C: return K_DEL;   case 0x4D: return K_END;
      case 0x4F: return K_RIGHT; case 0x50: return K_LEFT;  case 0x51: return K_DOWN; case 0x52: return K_UP;
    }
    return 0;
  }

  // Called with each 8-byte keyboard report (mods = byte 0, keys = the 6 keycode bytes)
  void onHid(uint8_t mods, const uint8_t keys[6]) {
    bool now[B_COUNT] = {};
    bool repStill = false;
    for (int i = 0; i < 6; i++) {
      if (keys[i] == 0) continue;
      if (keys[i] == repCode) repStill = true;
      bool was = false;
      for (int j = 0; j < 6; j++) if (prevKeys[j] == keys[i]) was = true;
      if (!was) {
        if (keys[i] == 0x39) caps = !caps;   // Caps Lock
        uint16_t k = toChar(keys[i], mods);
        if (k) {
          pushKey(k);
          bool repeatable = k != K_ENTER && k != K_ESC && k != K_TAB;
          repCode = repeatable ? keys[i] : 0; repKey = k; repArm = repeatable; repStill = repeatable;
        }
      }
      int b = map(keys[i]);
      if (b < 0) continue;
      now[b] = true;
      if (!was) pending[b] = true;
    }
    if (!repStill) { repCode = 0; repArm = false; }
    memcpy(rawHeld, now, sizeof(now));
    memcpy(prevKeys, keys, 6);
  }

  void releaseAll() { memset(rawHeld, 0, sizeof(rawHeld)); memset(prevKeys, 0, 6); repCode = 0; repArm = false; }

  // Call once per frame before the UI/game update
  void frame(uint32_t now) {
    if (repArm) { repStart = repLast = now; repArm = false; }
    else if (repCode && now - repStart > 450 && now - repLast > 55) { pushKey(repKey); repLast = now; }
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
int      plat_loadStr(const char* key, char* buf, int maxLen);   // returns length (0 if missing)
bool     plat_saveStr(const char* key, const char* s);          // empty string deletes the key

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



// ======================================================================
//  Ten more games (v1.6). All original code written for this sketch,
//  using the same Game base class (pause, game over and best score).
// ======================================================================

// HSV -> RGB565 (h in degrees, s/v 0..1)
static uint16_t hsv565(float h, float s, float v) {
  h = fmodf(h, 360); if (h < 0) h += 360;
  float c = v * s, x = c * (1 - fabsf(fmodf(h / 60, 2) - 1)), m = v - c;
  float r, gg, b;
  if (h < 60)       { r = c; gg = x; b = 0; }
  else if (h < 120) { r = x; gg = c; b = 0; }
  else if (h < 180) { r = 0; gg = c; b = x; }
  else if (h < 240) { r = 0; gg = x; b = c; }
  else if (h < 300) { r = x; gg = 0; b = c; }
  else              { r = c; gg = 0; b = x; }
  int R = (int)((r + m) * 255), G = (int)((gg + m) * 255), B = (int)((b + m) * 255);
  return rgb(R, G, B);
}
// 8-pixel-wide bitmap, each bit drawn as an sc x sc square
static void drawBits8(Canvas& g, const uint8_t* bits, int rows, int x, int y, int sc, uint16_t c) {
  for (int r = 0; r < rows; r++) for (int b = 0; b < 8; b++)
    if (bits[r] & (0x80 >> b)) g.fillRect(x + b * sc, y + r * sc, sc, sc, c);
}

// ---------------------------------------------------------------- Invaders
// Z/C move, SPACE or D fire. Clear the wave before the aliens reach you.
struct InvadersGame : Game {
  static const int COLS = 9, ROWS = 5, AW = 16, AH = 16, SX = 24, SY = 20, PY = 220;
  static const int NB = 4, BC = 6, BROWS = 4, BUNK_Y = 184, MAXS = 4;
  uint8_t alive[ROWS][COLS]; int nAlive;
  float fx, fy; int dir, anim; uint32_t stepAcc;
  float px, bx, by; bool bOn;
  struct Shot { float x, y; bool on; } es[MAXS];
  uint8_t bunk[NB][BROWS][BC];
  int lives, wave; uint32_t dieAt, fireAcc, nextFire;
  float ux; int udir; bool uOn; uint32_t uNext, uHitAt; int uPts, uHitX;
  uint32_t boomAt; int boomX, boomY;
  const char* saveKey() override { return "invaders"; }

  void setupWave(uint32_t now) {
    memset(alive, 1, sizeof(alive)); nAlive = ROWS * COLS;
    fx = (SW - (COLS - 1) * SX - AW) / 2; fy = 40 + imin(wave - 1, 4) * 8;
    dir = 1; anim = 0; stepAcc = 0; bOn = false;
    for (auto& s : es) s.on = false;
    memset(bunk, 1, sizeof(bunk));
    for (int b = 0; b < NB; b++) { bunk[b][3][2] = bunk[b][3][3] = 0; }   // little arch
    fireAcc = 0; nextFire = 900; uOn = false; uNext = now + 12000; boomAt = 0;
  }
  void begin() override {
    loadBest(); score = 0; phase = PLAY; lives = 3; wave = 1; px = SW / 2; dieAt = 0; uHitAt = 0;
    setupWave(plat_millis());
  }
  bool hitBunker(float x, float y) {
    for (int b = 0; b < NB; b++) {
      int x0 = 40 + b * 80 - 12;
      if (x < x0 || x >= x0 + BC * 4 || y < BUNK_Y || y >= BUNK_Y + BROWS * 4) continue;
      int c = (int)(x - x0) / 4, r = (int)(y - BUNK_Y) / 4;
      if (bunk[b][r][c]) { bunk[b][r][c] = 0; return true; }
    }
    return false;
  }
  void marchStep(uint32_t now) {
    anim ^= 1;
    int minc = COLS, maxc = -1, maxr = -1;
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) if (alive[r][c]) {
      minc = imin(minc, c); maxc = imax(maxc, c); maxr = imax(maxr, r);
    }
    if (maxc < 0) return;
    float nx = fx + dir * 6;
    if (nx + minc * SX < 4 || nx + maxc * SX + AW > SW - 4) { fy += 8; dir = -dir; }
    else fx = nx;
    // aliens chew through the bunkers they touch
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) if (alive[r][c]) {
      float ax = fx + c * SX, ay = fy + r * SY;
      if (ay + AH < BUNK_Y) continue;
      for (int b = 0; b < NB; b++) for (int br = 0; br < BROWS; br++) for (int bc = 0; bc < BC; bc++) {
        float cx = 40 + b * 80 - 12 + bc * 4, cy = BUNK_Y + br * 4;
        if (cx + 4 > ax && cx < ax + AW && cy + 4 > ay && cy < ay + AH) bunk[b][br][bc] = 0;
      }
    }
    if (fy + maxr * SY + AH >= PY - 8) { lives = 0; dieAt = now; boomAt = now; boomX = ir(px); boomY = PY; }
  }
  void alienFire() {
    int cols[COLS], n = 0;
    for (int c = 0; c < COLS; c++) for (int r = 0; r < ROWS; r++) if (alive[r][c]) { cols[n++] = c; break; }
    if (!n) return;
    int c = cols[plat_random(n)], r = ROWS - 1;
    while (r >= 0 && !alive[r][c]) r--;
    if (r < 0) return;
    int maxShots = imin(MAXS, 2 + wave / 2);
    int active = 0; for (auto& s : es) if (s.on) active++;
    if (active >= maxShots) return;
    for (auto& s : es) if (!s.on) { s.on = true; s.x = fx + c * SX + AW / 2; s.y = fy + r * SY + AH; return; }
  }
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    float t = dt / 1000.0f;
    if (dieAt) {
      if (now - dieAt < 1200) return true;
      dieAt = 0;
      if (lives <= 0) { gameOver(now); return true; }
      px = SW / 2; for (auto& s : es) s.on = false;
    }
    if (in.held[B_LEFT])  px -= 170 * t;
    if (in.held[B_RIGHT]) px += 170 * t;
    px = fmaxf(12, fminf(SW - 12, px));
    if ((in.pressed[B_A] || in.pressed[B_UP]) && !bOn) { bOn = true; bx = px; by = PY - 8; }

    // player bullet (sub-steps so it never skips an alien)
    for (int k = 0; k < 3 && bOn; k++) {
      by -= 380 * t / 3;
      if (by < 28) { bOn = false; break; }
      if (hitBunker(bx, by)) { bOn = false; break; }
      if (uOn && fabsf(bx - ux) < 12 && by >= 32 && by <= 44) {
        uPts = 50 * (1 + plat_random(6)); score += uPts; uHitAt = now; uHitX = ir(ux); uOn = false;
        uNext = now + 15000 + plat_random(10000); bOn = false; break;
      }
      int c = (int)floorf((bx - fx) / SX), r = (int)floorf((by - fy) / SY);
      if (c >= 0 && c < COLS && r >= 0 && r < ROWS && alive[r][c]) {
        float ax = fx + c * SX, ay = fy + r * SY;
        if (bx >= ax && bx < ax + AW && by >= ay && by < ay + AH) {
          alive[r][c] = 0; nAlive--; bOn = false;
          score += r == 0 ? 30 : (r < 3 ? 20 : 10);
          boomAt = now; boomX = ir(ax + AW / 2); boomY = ir(ay + AH / 2);
        }
      }
    }
    // marching
    uint32_t interval = (uint32_t)imax(30, 40 + nAlive * 13 - (wave - 1) * 25);
    stepAcc += dt;
    if (stepAcc >= interval) { stepAcc = 0; marchStep(now); if (dieAt) return true; }
    // alien shots
    fireAcc += dt;
    if (fireAcc >= nextFire) { fireAcc = 0; nextFire = imax(250, 500 + plat_random(900) - wave * 40); alienFire(); }
    float es_v = 150 + wave * 10;
    for (auto& s : es) if (s.on) {
      s.y += es_v * t;
      if (s.y > SH) { s.on = false; continue; }
      if (hitBunker(s.x, s.y)) { s.on = false; continue; }
      if (fabsf(s.x - px) < 9 && s.y >= PY - 7 && s.y <= PY + 4) {
        s.on = false; lives--; dieAt = now; boomAt = now; boomX = ir(px); boomY = PY; return true;
      }
    }
    // mystery ship
    if (!uOn && now > uNext) { uOn = true; udir = plat_random(2) ? 1 : -1; ux = udir > 0 ? -20 : SW + 20; }
    if (uOn) { ux += udir * 70 * t; if (ux < -30 || ux > SW + 30) { uOn = false; uNext = now + 15000 + plat_random(10000); } }
    if (nAlive == 0) { wave++; score += 100; setupWave(now); }
    return true;
  }
  void draw(Canvas& g, uint32_t now) override {
    static const uint8_t A[3][2][8] = {
      {{0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0x5A, 0xA5}, {0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x5A, 0x81, 0x42}},   // squid
      {{0x42, 0x24, 0x7E, 0xDB, 0xFF, 0xBD, 0xA5, 0x24}, {0x42, 0xA5, 0xFF, 0xDB, 0xFF, 0x7E, 0x24, 0x42}},   // crab
      {{0x3C, 0x7E, 0xFF, 0x99, 0xFF, 0x66, 0xDB, 0x81}, {0x3C, 0x7E, 0xFF, 0x99, 0xFF, 0x24, 0x5A, 0x24}}};  // octopus
    static const uint16_t RC[ROWS] = {C_PINK, C_YELLOW, C_YELLOW, C_GREEN, C_GREEN};
    g.fillScreen(C_BG);
    for (int i = 0; i < 24; i++) g.drawPixel((i * 71 + 13) % SW, 30 + (i * 43) % 150, C_LINE);
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) if (alive[r][c]) {
      int type = r == 0 ? 0 : (r < 3 ? 1 : 2);
      drawBits8(g, A[type][anim], 8, ir(fx + c * SX), ir(fy + r * SY), 2, RC[r]);
    }
    for (int b = 0; b < NB; b++) for (int br = 0; br < BROWS; br++) for (int bc = 0; bc < BC; bc++)
      if (bunk[b][br][bc]) g.fillRect(40 + b * 80 - 12 + bc * 4, BUNK_Y + br * 4, 4, 4, C_GREEN);
    if (uOn) {
      int x = ir(ux);
      g.fillRoundRect(x - 11, 36, 22, 7, 3, C_RED); g.fillRoundRect(x - 5, 32, 10, 6, 3, C_RED);
      for (int i = -1; i <= 1; i++) g.drawPixel(x + i * 6, 39, C_YELLOW);
    }
    if (uHitAt && now - uHitAt < 900) { char b[8]; snprintf(b, sizeof(b), "%d", uPts); textC(g, F_SMALL, uHitX, 34, b, C_RED); }
    if (bOn) g.fillRect(ir(bx) - 1, ir(by) - 3, 2, 7, C_WHITE);
    for (auto& s : es) if (s.on) {
      int x = ir(s.x), y = ir(s.y), w = ((int)(s.y / 4)) % 2 ? 1 : -1;
      g.drawLine(x, y - 6, x + w, y - 3, C_WHITE); g.drawLine(x + w, y - 3, x, y, C_WHITE);
    }
    if (!dieAt) {
      int x = ir(px);
      g.fillRect(x - 9, PY - 2, 18, 6, C_TEAL); g.fillRect(x - 6, PY - 4, 12, 2, C_TEAL); g.fillRect(x - 1, PY - 7, 3, 3, C_TEAL);
    }
    if (boomAt && now - boomAt < (dieAt ? 900u : 160u)) {
      float u = seg(now, boomAt, boomAt + (dieAt ? 900 : 160));
      for (int i = 0; i < 8; i++) {
        float a = i * 0.785f; int rr = 3 + ir(u * (dieAt ? 16 : 7));
        g.fillRect(boomX + ir(cosf(a) * rr) - 1, boomY + ir(sinf(a) * rr) - 1, 2, 2, dieAt ? C_ORANGE : C_WHITE);
      }
    }
    g.drawFastHLine(0, 230, SW, C_LINE);
    drawHud(g, "Invaders", C_PINK);
    for (int i = 0; i < lives; i++) { int x = 104 + i * 14; g.fillRect(x, 14, 10, 4, C_TEAL); g.fillRect(x + 4, 11, 2, 3, C_TEAL); }
    char w[12]; snprintf(w, sizeof(w), "W%d", wave); text(g, F_SMALL, 150, 10, w, C_DIM);
    drawOverlays(g, now, C_PINK);
  }
};

// ---------------------------------------------------------------- Asteroids
// Z/C rotate, D thrust, SPACE fire, X hyperspace.
struct AsteroidsGame : Game {
  static const int MAXR = 28, MAXB = 5, MAXP = 24, TOPY = 27;
  struct Rock { float x, y, vx, vy, rot, spin; uint8_t size, v[8]; bool on; } rocks[MAXR];
  struct Bul { float x, y, vx, vy, life; bool on; } bul[MAXB];
  struct Part { float x, y, vx, vy, life; } parts[MAXP];
  float sx, sy, svx, svy, ang; bool thrust;
  int lives, wave; uint32_t invUntil, deadAt, waveAt;
  const char* saveKey() override { return "asteroids"; }

  static float rad(int s) { return s == 3 ? 20 : (s == 2 ? 11 : 6); }
  static void wrap(float& x, float& y) {
    const float H = SH - TOPY;
    if (x < 0) x += SW;
    if (x >= SW) x -= SW;
    if (y < TOPY) y += H;
    if (y >= SH) y -= H;
  }
  void spawnRock(float x, float y, int size) {
    for (auto& r : rocks) if (!r.on) {
      r.on = true; r.x = x; r.y = y; r.size = size;
      float a = plat_random(628) / 100.0f;
      float sp = (size == 3 ? 22 + plat_random(20) : size == 2 ? 38 + plat_random(30) : 55 + plat_random(45)) + wave * 3;
      r.vx = cosf(a) * sp; r.vy = sinf(a) * sp;
      for (int k = 0; k < 8; k++) r.v[k] = 70 + plat_random(31);
      r.rot = 0; r.spin = (plat_random(100) - 50) / 40.0f;
      return;
    }
  }
  void burst(float x, float y, int n) {
    for (int i = 0; i < MAXP && n > 0; i++) if (parts[i].life <= 0) {
      float a = plat_random(628) / 100.0f, sp = 30 + plat_random(90);
      parts[i] = {x, y, cosf(a) * sp, sinf(a) * sp, 0.4f + plat_random(40) / 100.0f}; n--;
    }
  }
  void newWave() {
    int n = imin(3 + wave, 8);
    for (int i = 0; i < n; i++) {
      float x = 0, y = 0;
      for (int tries = 0; tries < 30; tries++) {
        x = plat_random(SW); y = TOPY + plat_random(SH - TOPY);
        if ((x - sx) * (x - sx) + (y - sy) * (y - sy) > 90 * 90) break;
      }
      spawnRock(x, y, 3);
    }
  }
  void resetShip(uint32_t now) { sx = SW / 2; sy = (TOPY + SH) / 2; svx = svy = 0; ang = -1.5708f; invUntil = now + 2500; }
  void begin() override {
    loadBest(); score = 0; phase = PLAY; lives = 3; wave = 1; deadAt = waveAt = 0; thrust = false;
    for (auto& r : rocks) r.on = false;
    for (auto& b : bul) b.on = false;
    for (auto& p : parts) p.life = 0;
    resetShip(plat_millis()); newWave();
  }
  void splitRock(Rock& r) {
    score += r.size == 3 ? 20 : (r.size == 2 ? 50 : 100);
    r.on = false; burst(r.x, r.y, 6);
    if (r.size > 1) { spawnRock(r.x, r.y, r.size - 1); spawnRock(r.x, r.y, r.size - 1); }
  }
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    float t = dt / 1000.0f;
    if (deadAt) {
      if (now - deadAt > 1500) {
        if (lives <= 0) { gameOver(now); return true; }
        bool clear = true;
        for (auto& r : rocks) if (r.on) {
          float dx = r.x - SW / 2, dy = r.y - (TOPY + SH) / 2;
          if (dx * dx + dy * dy < 60 * 60) clear = false;
        }
        if (clear) { deadAt = 0; resetShip(now); }
      }
    } else {
      if (in.held[B_LEFT])  ang -= 4.2f * t;
      if (in.held[B_RIGHT]) ang += 4.2f * t;
      thrust = in.held[B_UP];
      if (thrust) { svx += cosf(ang) * 210 * t; svy += sinf(ang) * 210 * t; }
      svx -= svx * 0.45f * t; svy -= svy * 0.45f * t;
      float sp = sqrtf(svx * svx + svy * svy);
      if (sp > 230) { svx *= 230 / sp; svy *= 230 / sp; }
      sx += svx * t; sy += svy * t; wrap(sx, sy);
      if (in.pressed[B_A]) for (auto& b : bul) if (!b.on) {
        b.on = true; b.x = sx + cosf(ang) * 9; b.y = sy + sinf(ang) * 9;
        b.vx = svx + cosf(ang) * 320; b.vy = svy + sinf(ang) * 320; b.life = 0.85f; break;
      }
      if (in.pressed[B_DOWN]) { sx = plat_random(SW); sy = TOPY + plat_random(SH - TOPY); svx = svy = 0; invUntil = now + 300; }
    }
    for (auto& r : rocks) if (r.on) { r.x += r.vx * t; r.y += r.vy * t; r.rot += r.spin * t; wrap(r.x, r.y); }
    for (auto& p : parts) if (p.life > 0) { p.x += p.vx * t; p.y += p.vy * t; p.life -= t; }
    for (auto& b : bul) if (b.on) {
      b.x += b.vx * t; b.y += b.vy * t; wrap(b.x, b.y); b.life -= t;
      if (b.life <= 0) { b.on = false; continue; }
      for (auto& r : rocks) if (r.on) {
        float dx = b.x - r.x, dy = b.y - r.y, rr = rad(r.size) + 1;
        if (dx * dx + dy * dy < rr * rr) { b.on = false; splitRock(r); break; }
      }
    }
    if (!deadAt && now >= invUntil) for (auto& r : rocks) if (r.on) {
      float dx = sx - r.x, dy = sy - r.y, rr = rad(r.size) + 5;
      if (dx * dx + dy * dy < rr * rr) { lives--; deadAt = now; burst(sx, sy, 14); splitRock(r); break; }
    }
    bool any = false; for (auto& r : rocks) if (r.on) any = true;
    if (!any && !waveAt) waveAt = now;
    if (waveAt && now - waveAt > 1200) { wave++; waveAt = 0; newWave(); }
    return true;
  }
  void draw(Canvas& g, uint32_t now) override {
    g.fillScreen(C_BG);
    for (int i = 0; i < 30; i++) g.drawPixel((i * 89 + 7) % SW, TOPY + (i * 53) % (SH - TOPY), C_LINE);
    for (auto& r : rocks) if (r.on) {
      float R = rad(r.size); int px0 = 0, py0 = 0, fx0 = 0, fy0 = 0;
      for (int k = 0; k <= 8; k++) {
        int kk = k % 8; float a = kk * 0.7854f + r.rot, d = R * r.v[kk] / 100.0f;
        int x = ir(r.x + cosf(a) * d), y = ir(r.y + sinf(a) * d);
        if (k == 0) { fx0 = x; fy0 = y; } else g.drawLine(px0, py0, x, y, C_SOFT);
        px0 = x; py0 = y;
      }
      (void)fx0; (void)fy0;
    }
    for (auto& b : bul) if (b.on) g.fillRect(ir(b.x) - 1, ir(b.y) - 1, 2, 2, C_WHITE);
    for (auto& p : parts) if (p.life > 0) g.drawPixel(ir(p.x), ir(p.y), p.life > 0.3f ? C_ORANGE : C_DIM);
    if (!deadAt && (now >= invUntil || (now / 100) % 2)) {
      float c = cosf(ang), s = sinf(ang);
      int nx = ir(sx + c * 10), ny = ir(sy + s * 10);
      int lx = ir(sx + cosf(ang + 2.5f) * 8), ly = ir(sy + sinf(ang + 2.5f) * 8);
      int rx = ir(sx + cosf(ang - 2.5f) * 8), ry = ir(sy + sinf(ang - 2.5f) * 8);
      int bx = ir(sx - c * 4), by = ir(sy - s * 4);
      g.drawLine(nx, ny, lx, ly, C_WHITE); g.drawLine(nx, ny, rx, ry, C_WHITE);
      g.drawLine(lx, ly, bx, by, C_WHITE); g.drawLine(rx, ry, bx, by, C_WHITE);
      if (thrust && (now / 60) % 2) {
        int fx = ir(sx - c * (11 + plat_random(4))), fy = ir(sy - s * (11 + plat_random(4)));
        g.drawLine(ir(sx - c * 5 + s * 3), ir(sy - s * 5 - c * 3), fx, fy, C_ORANGE);
        g.drawLine(ir(sx - c * 5 - s * 3), ir(sy - s * 5 + c * 3), fx, fy, C_ORANGE);
      }
    }
    if (waveAt) { char b[24]; snprintf(b, sizeof(b), "WAVE %d", wave + 1); textC(g, F_BOLD, SW / 2, 120, b, C_BLUE); }
    drawHud(g, "Asteroids", C_BLUE);
    for (int i = 0; i < lives; i++) {
      int x = 112 + i * 12;
      g.drawLine(x, 7, x - 4, 19, C_WHITE); g.drawLine(x, 7, x + 4, 19, C_WHITE); g.drawLine(x - 4, 19, x + 4, 19, C_WHITE);
    }
    drawOverlays(g, now, C_BLUE);
  }
};

// ---------------------------------------------------------------- Dino run
// SPACE or D jump (hold = higher), X duck / drop fast.
struct DinoGame : Game {
  static const int GY = 206, DX = 44, NOB = 5;
  struct Ob { float x; int8_t type; bool on; } ob[NOB];
  float dy, vy, speed, dist, toNext; bool ducking, onGround, started, dead;
  float cloudX[3]; int cloudY[3]; uint32_t deadAt;
  const char* saveKey() override { return "dino"; }

  static void obBox(int type, float x, int& x0, int& y0, int& w, int& h) {
    switch (type) {
      case 0: w = 10; h = 20; y0 = GY - h; break;          // small cactus
      case 1: w = 14; h = 28; y0 = GY - h; break;          // big cactus
      case 2: w = 22; h = 20; y0 = GY - h; break;          // two small
      case 3: w = 34; h = 26; y0 = GY - h; break;          // three
      case 4: w = 20; h = 12; y0 = GY - 18; break;         // bird low  (jump)
      case 5: w = 20; h = 12; y0 = GY - 30; break;         // bird mid  (duck)
      default: w = 20; h = 12; y0 = GY - 56; break;        // bird high (just run)
    }
    x0 = ir(x);
  }
  void begin() override {
    loadBest(); score = 0; phase = PLAY;
    dy = vy = 0; speed = 190; dist = 0; toNext = 260; ducking = false; onGround = true; started = false; dead = false;
    for (auto& o : ob) o.on = false;
    for (int i = 0; i < 3; i++) { cloudX[i] = 60 + i * 110; cloudY[i] = 50 + plat_random(60); }
  }
  void spawn() {
    for (auto& o : ob) if (!o.on) {
      o.on = true; o.x = SW + 10;
      if (score > 250 && plat_random(4) == 0) o.type = 4 + plat_random(3);
      else o.type = plat_random(score > 100 ? 4 : 3);
      return;
    }
  }
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    float t = dt / 1000.0f;
    if (dead) { if (now - deadAt > 500) gameOver(now); return true; }
    bool jumpKey = in.held[B_A] || in.held[B_UP];
    if (!started) { if (in.pressed[B_A] || in.pressed[B_UP]) { started = true; vy = 470; onGround = false; } return true; }
    if (onGround && (in.pressed[B_A] || in.pressed[B_UP])) { vy = 470; onGround = false; }
    if (!onGround) {
      float grav = 1700;
      if (!jumpKey && vy > 0) grav = 3200;   // short hop when the key is released
      if (in.held[B_DOWN]) grav = 4500;      // fast drop
      vy -= grav * t; dy += vy * t;
      if (dy <= 0) { dy = 0; vy = 0; onGround = true; }
    }
    ducking = in.held[B_DOWN] && onGround;
    speed = fminf(470, 190 + dist * 0.012f);
    float mv = speed * t;
    dist += mv; score = (int)(dist / 12);
    for (int i = 0; i < 3; i++) { cloudX[i] -= mv * 0.15f; if (cloudX[i] < -40) { cloudX[i] = SW + plat_random(60); cloudY[i] = 45 + plat_random(70); } }
    toNext -= mv;
    if (toNext <= 0) { spawn(); toNext = speed * 0.62f + 70 + plat_random(230); }
    int dw = ducking ? 28 : 20, dh = ducking ? 14 : 26;
    int dx0 = DX + 3, dy0 = GY - ir(dy) - dh + 3, dx1 = DX + dw - 3, dy1 = GY - ir(dy) - 2;
    for (auto& o : ob) if (o.on) {
      o.x -= mv * (o.type >= 4 ? 1.12f : 1.0f);
      if (o.x < -40) { o.on = false; continue; }
      int x0, y0, w, h; obBox(o.type, o.x, x0, y0, w, h);
      if (dx1 > x0 + 2 && dx0 < x0 + w - 2 && dy1 > y0 + 2 && dy0 < y0 + h - 2) { dead = true; deadAt = now; }
    }
    return true;
  }
  void drawCactus(Canvas& g, int x, int y, int w, int h) {
    uint16_t c = rgb(34, 197, 94), d = rgb(21, 128, 61);
    int tw = imax(4, w / 3);
    rrect(g, x + w / 2 - tw / 2, y, tw, h, 2, c);
    g.fillRect(x + w / 2 + tw / 2 - 1, y + 2, 1, h - 2, d);
    if (w >= 10) {
      rrect(g, x, y + h / 3, 3, h / 3, 1, c); g.fillRect(x, y + h / 3 + h / 3 - 2, w / 2, 3, c);
      rrect(g, x + w - 3, y + h / 4, 3, h / 3, 1, c); g.fillRect(x + w / 2, y + h / 4 + h / 3 - 2, w / 2, 3, c);
    }
  }
  void draw(Canvas& g, uint32_t now) override {
    g.fillScreen(C_BG);
    for (int i = 0; i < 3; i++) { int x = ir(cloudX[i]), y = cloudY[i]; rrect(g, x, y, 34, 8, 4, C_LINE); rrect(g, x + 8, y - 5, 16, 8, 4, C_LINE); }
    g.drawFastHLine(0, GY, SW, C_SOFT);
    int off = (int)dist;
    for (int i = 0; i < 24; i++) { int x = ((i * 47 - off) % SW + SW) % SW; g.drawFastHLine(x, GY + 4 + (i * 7) % 12, 2 + i % 3, C_DIM); }
    for (auto& o : ob) if (o.on) {
      int x0, y0, w, h; obBox(o.type, o.x, x0, y0, w, h);
      if (o.type == 0 || o.type == 1) drawCactus(g, x0, y0, w, h);
      else if (o.type == 2) { drawCactus(g, x0, y0, 10, 20); drawCactus(g, x0 + 12, y0 + 4, 10, 16); }
      else if (o.type == 3) { drawCactus(g, x0, y0 + 6, 10, 20); drawCactus(g, x0 + 10, y0, 14, 26); drawCactus(g, x0 + 24, y0 + 6, 10, 20); }
      else {
        bool up = (now / 160) % 2;
        uint16_t bc = C_SOFT;
        rrect(g, x0 + 4, y0 + 4, 14, 5, 2, bc); g.fillRect(x0, y0 + 5, 5, 2, bc);   // body + beak
        if (up) g.fillTriangle(x0 + 7, y0 + 4, x0 + 14, y0 + 4, x0 + 10, y0 - 4, bc);
        else    g.fillTriangle(x0 + 7, y0 + 8, x0 + 14, y0 + 8, x0 + 10, y0 + 15, bc);
        g.drawPixel(x0 + 6, y0 + 5, C_BG);
      }
    }
    // dino
    int bx = DX, by = GY - ir(dy);
    uint16_t dc = C_LIME;
    bool step = onGround && started && !dead && ((int)(dist / 28)) % 2;
    if (ducking) {
      g.fillRect(bx, by - 13, 22, 9, dc); g.fillRect(bx + 18, by - 14, 10, 7, dc);
      g.fillRect(bx + 24, by - 12, 2, 2, C_BG);
      g.fillRect(bx + 4, by - 4, 3, step ? 4 : 2, dc); g.fillRect(bx + 13, by - 4, 3, step ? 2 : 4, dc);
    } else {
      g.fillRect(bx + 2, by - 18, 13, 12, dc);            // body
      g.fillRect(bx - 2, by - 16, 5, 4, dc);              // tail
      g.fillRect(bx + 9, by - 26, 11, 9, dc);             // head
      g.fillRect(bx + 15, by - 19, 5, 2, dc);             // jaw
      g.fillRect(bx + 12, by - 24, 2, 2, dead ? C_RED : C_BG);   // eye
      g.fillRect(bx + 14, by - 13, 4, 2, dc);             // arm
      g.fillRect(bx + 4, by - 6, 3, (step || !onGround) ? 6 : 4, dc);
      g.fillRect(bx + 10, by - 6, 3, (!step || !onGround) ? 6 : 4, dc);
    }
    drawHud(g, "Dino", C_LIME);
    if (!started && phase == PLAY) { textC(g, F_BOLD, SW / 2, 100, "Dino Run", C_WHITE); textC(g, F_SMALL, SW / 2, 120, "SPACE / D jump    X duck", C_SOFT); }
    drawOverlays(g, now, C_LIME);
  }
};

// ---------------------------------------------------------------- Racer (top-down traffic)
// Z/C steer, D faster, X brake. Don't crash.
struct RacerGame : Game {
  static const int RL = 70, RR = 250, PY = 184, CW = 22, CH = 36, NC = 8;
  struct Car { float x, y, sp; bool on, passed; uint16_t col; } cars[NC];
  float px, speed, dist, spawnAcc, nextGap; bool started, crashed; uint32_t crashAt; int passes;
  const char* saveKey() override { return "racer"; }

  void begin() override {
    loadBest(); score = 0; phase = PLAY; px = 160; speed = 0; dist = 0; spawnAcc = 0; nextGap = 120;
    started = false; crashed = false; passes = 0;
    for (auto& c : cars) c.on = false;
  }
  void spawnCar() {
    // lanes already used near the top of the screen
    bool used[3] = {false, false, false};
    for (auto& c : cars) if (c.on && c.y < 70) used[imax(0, imin(2, (int)((c.x - RL) / 60)))] = true;
    int freeL[3], n = 0; for (int l = 0; l < 3; l++) if (!used[l]) freeL[n++] = l;
    if (n <= 1) return;   // always leave at least one lane open
    int lane = freeL[plat_random(n)];
    static const uint16_t cols[] = {C_BLUE, C_YELLOW, C_GREEN, C_PURPLE, C_WHITE, C_ORANGE, C_CYAN};
    for (auto& c : cars) if (!c.on) {
      c.on = true; c.passed = false; c.x = RL + 30 + lane * 60; c.y = -CH - 4;
      c.sp = 80 + plat_random(90); c.col = cols[plat_random(7)]; return;
    }
  }
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    float t = dt / 1000.0f;
    if (crashed) { if (now - crashAt > 900) gameOver(now); return true; }
    if (!started) { if (in.anyPressed && !in.pressed[B_B] && !in.pressed[B_PAUSE]) started = true; else return true; }
    if (in.held[B_LEFT])  px -= 230 * t;
    if (in.held[B_RIGHT]) px += 230 * t;
    px = fmaxf(RL + CW / 2 + 2, fminf(RR - CW / 2 - 2, px));
    float target = fminf(520, 200 + dist * 0.004f);
    if (in.held[B_UP]) target *= 1.25f;
    if (in.held[B_DOWN]) target *= 0.55f;
    speed += (target > speed ? 260 : -420) * t;
    if (fabsf(speed - target) < 6) speed = target;
    float mv = speed * t;
    dist += mv; spawnAcc += mv;
    if (spawnAcc >= nextGap) { spawnAcc = 0; nextGap = fmaxf(70, 210 - dist * 0.002f) + plat_random(90); spawnCar(); }
    for (auto& c : cars) if (c.on) {
      c.y += (speed - c.sp) * t;
      if (c.y > SH + 10 || c.y < -200) { c.on = false; continue; }
      if (!c.passed && c.y > PY + CH) { c.passed = true; passes++; }
      if (fabsf(c.x - px) < CW - 3 && fabsf(c.y - PY) < CH - 4) { crashed = true; crashAt = now; }
    }
    score = (int)(dist / 30) + passes * 5;
    return true;
  }
  void drawCar(Canvas& g, int cx, int y, uint16_t col, bool player) {
    int x = cx - CW / 2;
    uint16_t k = rgb(15, 15, 18);
    g.fillRect(x - 2, y + 5, 3, 8, k); g.fillRect(x + CW - 1, y + 5, 3, 8, k);
    g.fillRect(x - 2, y + CH - 13, 3, 8, k); g.fillRect(x + CW - 1, y + CH - 13, 3, 8, k);
    rrect(g, x, y, CW, CH, 5, col);
    rrect(g, x + 3, y + (player ? 8 : 22), CW - 6, 6, 2, rgb(40, 60, 90));   // windshield
    rrect(g, x + 4, y + 14, CW - 8, 9, 2, blend(col, C_BG, 0.25f));          // roof
    if (player) g.fillRect(cx - 1, y + 1, 3, CH - 2, C_WHITE);
    else { g.fillRect(x + 2, y + CH - 2, 4, 2, C_RED); g.fillRect(x + CW - 6, y + CH - 2, 4, 2, C_RED); }
  }
  void draw(Canvas& g, uint32_t now) override {
    int off = ((int)dist) % 36;
    uint16_t grass = rgb(22, 101, 52), grass2 = rgb(18, 86, 44);
    g.fillRect(0, 27, RL, SH - 27, grass); g.fillRect(RR, 27, SW - RR, SH - 27, grass);
    for (int y = -36 + off; y < SH; y += 36) { g.fillRect(0, y, RL - 6, 18, grass2); g.fillRect(RR + 6, y, SW - RR - 6, 18, grass2); }
    for (int i = 0; i < 6; i++) {   // trees
      int ty = ((i * 83 + (int)dist) % 300) - 30;
      int tx = (i % 2) ? 26 + (i * 7) % 20 : RR + 22 + (i * 11) % 30;
      g.fillCircle(tx, ty, 9, rgb(16, 120, 60)); g.fillCircle(tx - 2, ty - 2, 4, rgb(40, 160, 80));
    }
    g.fillRect(RL, 27, RR - RL, SH - 27, rgb(55, 55, 62));
    for (int y = -24 + (off % 24); y < SH; y += 24) {
      g.fillRect(RL - 6, y, 6, 12, C_RED); g.fillRect(RL - 6, y + 12, 6, 12, C_WHITE);
      g.fillRect(RR, y, 6, 12, C_RED); g.fillRect(RR, y + 12, 6, 12, C_WHITE);
    }
    for (int y = -36 + off; y < SH; y += 36) { g.fillRect(RL + 59, y, 3, 18, rgb(220, 220, 220)); g.fillRect(RL + 119, y, 3, 18, rgb(220, 220, 220)); }
    for (auto& c : cars) if (c.on) drawCar(g, ir(c.x), ir(c.y), c.col, false);
    int shake = crashed && now - crashAt < 300 ? (int)plat_random(5) - 2 : 0;
    drawCar(g, ir(px) + shake, PY, C_ROSE, true);
    if (crashed) for (int i = 0; i < 10; i++) {
      float a = i * 0.628f; int r = 6 + ir(seg(now, crashAt, crashAt + 500) * 18);
      g.fillRect(ir(px) + ir(cosf(a) * r), PY + ir(sinf(a) * r), 2, 2, i % 2 ? C_YELLOW : C_ORANGE);
    }
    drawHud(g, "Racer", C_ROSE);
    char b[16]; snprintf(b, sizeof(b), "%d KM/H", (int)(speed * 0.45f)); text(g, F_SMALL, 80, 10, b, C_DIM);
    if (!started && phase == PLAY) {
      rrect(g, 80, 92, 160, 50, 10, C_CARD);
      textC(g, F_BOLD, SW / 2, 112, "Press to start", C_WHITE);
      textC(g, F_SMALL, SW / 2, 124, "Z C steer  D gas  X brake", C_SOFT);
    }
    drawOverlays(g, now, C_ROSE);
  }
};

// ---------------------------------------------------------------- Tron light cycles
// D/X/Z/C turn. Make the CPU crash into a wall or a trail. 3 lives.
struct TronGame : Game {
  static const int CS = 4, GW = 80, GH = 53, OY = 28, QMAX = 1600;
  uint8_t grid[GH][GW];
  uint8_t mark[GH][GW]; uint8_t stamp;
  uint16_t q[QMAX];
  int pxg, pyg, pdx, pdy, cxg, cyg, cdx, cdy;
  int8_t qdx[2], qdy[2]; int qn;
  uint32_t acc, rsAt; int rs, result, lives, round_;
  const char* saveKey() override { return "tron"; }

  void resetRound(uint32_t now) {
    memset(grid, 0, sizeof(grid));
    for (int x = 0; x < GW; x++) { grid[0][x] = 3; grid[GH - 1][x] = 3; }
    for (int y = 0; y < GH; y++) { grid[y][0] = 3; grid[y][GW - 1] = 3; }
    pxg = 14; pyg = GH / 2; pdx = 1; pdy = 0;
    cxg = GW - 15; cyg = GH / 2; cdx = -1; cdy = 0;
    grid[pyg][pxg] = 1; grid[cyg][cxg] = 2;
    qn = 0; acc = 0; rs = 0; rsAt = now;
  }
  void begin() override {
    loadBest(); score = 0; phase = PLAY; lives = 3; round_ = 1; stamp = 0; memset(mark, 0, sizeof(mark));
    resetRound(plat_millis());
  }
  void queueDir(int x, int y) {
    int lx = qn ? qdx[qn - 1] : pdx, ly = qn ? qdy[qn - 1] : pdy;
    if ((x == -lx && y == -ly) || (x == lx && y == ly) || qn >= 2) return;
    qdx[qn] = x; qdy[qn] = y; qn++;
  }
  int flood(int sx, int sy, int cap) {
    if (grid[sy][sx]) return 0;
    if (++stamp == 0) { memset(mark, 0, sizeof(mark)); stamp = 1; }
    int h = 0, tl = 0, n = 0;
    q[tl++] = sy * GW + sx; mark[sy][sx] = stamp;
    static const int8_t DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    while (h < tl && n < cap) {
      int v = q[h++]; n++;
      int x = v % GW, y = v / GW;
      for (int k = 0; k < 4; k++) {
        int nx = x + DX[k], ny = y + DY[k];
        if (grid[ny][nx] || mark[ny][nx] == stamp || tl >= QMAX) continue;
        mark[ny][nx] = stamp; q[tl++] = ny * GW + nx;
      }
    }
    return n;
  }
  void cpuThink() {
    int opts[3][2] = {{cdx, cdy}, {cdy, -cdx}, {-cdy, cdx}};
    int bestV = -1000000, bi = 0;
    int mistake = imax(0, 14 - round_ * 3);   // % chance of a sloppy choice, falls each round
    bool sloppy = (int)plat_random(100) < mistake;
    for (int i = 0; i < 3; i++) {
      int nx = cxg + opts[i][0], ny = cyg + opts[i][1];
      int v;
      if (grid[ny][nx]) v = -100000;
      else {
        v = flood(nx, ny, 400) * 10;
        if (i == 0) v += 4;
        int d0 = abs(cxg - pxg) + abs(cyg - pyg), d1 = abs(nx - pxg) + abs(ny - pyg);
        if (d1 < d0) v += 3;                          // a bit aggressive
        if (abs(nx - (pxg + pdx)) + abs(ny - (pyg + pdy)) == 0) v -= 2000;   // avoid head-on
        v += sloppy ? plat_random(4000) : plat_random(3);
      }
      if (v > bestV) { bestV = v; bi = i; }
    }
    cdx = opts[bi][0]; cdy = opts[bi][1];
  }
  void stepOnce(uint32_t now) {
    if (qn) { pdx = qdx[0]; pdy = qdy[0]; qdx[0] = qdx[1]; qdy[0] = qdy[1]; qn--; }
    cpuThink();
    int npx = pxg + pdx, npy = pyg + pdy, ncx = cxg + cdx, ncy = cyg + cdy;
    bool pDead = grid[npy][npx] != 0, cDead = grid[ncy][ncx] != 0;
    if (npx == ncx && npy == ncy) pDead = cDead = true;
    if (!pDead) { pxg = npx; pyg = npy; grid[pyg][pxg] = 1; }
    if (!cDead) { cxg = ncx; cyg = ncy; grid[cyg][cxg] = 2; }
    if (pDead || cDead) {
      rs = 2; rsAt = now;
      if (pDead && cDead) result = 3;
      else if (pDead) { result = 2; lives--; }
      else { result = 1; score += 100 + 25 * (round_ - 1); round_++; }
    }
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
    if (rs == 0) { if (now - rsAt > 1100) { rs = 1; acc = 0; } return true; }
    if (rs == 2) {
      if (now - rsAt > 1500) { if (lives <= 0) gameOver(now); else resetRound(now); }
      return true;
    }
    uint32_t interval = (uint32_t)imax(30, 62 - round_ * 3);
    acc += dt;
    while (acc >= interval && rs == 1) { acc -= interval; stepOnce(now); }
    return true;
  }
  void draw(Canvas& g, uint32_t now) override {
    g.fillScreen(C_BG);
    for (int x = 0; x < GW; x += 8) g.drawFastVLine(x * CS, OY, GH * CS, rgb(14, 22, 30));
    for (int y = 0; y < GH; y += 8) g.drawFastHLine(0, OY + y * CS, SW, rgb(14, 22, 30));
    uint16_t pc = C_CYAN, cc = C_ORANGE;
    uint16_t pt = blend(pc, C_BG, 0.35f), ct = blend(cc, C_BG, 0.35f);
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
      uint8_t v = grid[y][x];
      if (!v) continue;
      g.fillRect(x * CS, OY + y * CS, CS, CS, v == 1 ? pt : v == 2 ? ct : C_LINE);
    }
    g.fillRect(pxg * CS - 1, OY + pyg * CS - 1, CS + 2, CS + 2, C_WHITE);
    g.fillRect(cxg * CS - 1, OY + cyg * CS - 1, CS + 2, CS + 2, rgb(255, 220, 180));
    drawHud(g, "Tron", C_CYAN);
    for (int i = 0; i < lives; i++) g.fillCircle(72 + i * 10, 13, 3, C_CYAN);
    char b[24]; snprintf(b, sizeof(b), "ROUND %d", round_); text(g, F_SMALL, 106, 10, b, C_DIM);
    if (rs == 0 && phase == PLAY) {
      rrect(g, 90, 96, 140, 48, 10, C_CARD);
      textC(g, F_BOLD, SW / 2, 116, b, C_WHITE);
      textC(g, F_SMALL, SW / 2, 128, "YOU = CYAN   DXZC turn", C_SOFT);
    }
    if (rs == 2 && phase == PLAY) {
      const char* s = result == 1 ? "You win the round" : result == 2 ? "CPU wins the round" : "Draw";
      rrect(g, 70, 100, 180, 36, 10, C_CARD);
      textC(g, F_BOLD, SW / 2, 123, s, result == 1 ? C_GREEN : result == 2 ? C_RED : C_WHITE);
    }
    drawOverlays(g, now, C_CYAN);
  }
};

// ---------------------------------------------------------------- Stack
// SPACE (or X) drops the sliding block. Only the part that overlaps stays.
struct StackGame : Game {
  static const int LH = 12, BASEY = 228, RING = 32, START_W = 120;
  int16_t lx[RING], lw[RING]; int n;   // n = layers placed (layer 0 = base)
  float mx, mspeed, cam; int mw, mdir;
  struct Chip { float x, y, vy; int w, idx; bool on; } chip;
  int perfect; uint32_t perfAt, deadAt; bool dead;
  const char* saveKey() override { return "stack"; }

  static uint16_t layerCol(int i) { return hsv565(200 + i * 9, 0.55f, 0.95f); }
  float layerY(int i) { return BASEY - (i + 1) * LH + cam; }
  void nextBlock() {
    mdir = (n % 2) ? 1 : -1;
    mx = mdir > 0 ? -mw : SW;
    mspeed = fminf(300, 120 + (n - 1) * 5);
  }
  void begin() override {
    loadBest(); score = 0; phase = PLAY;
    n = 1; lx[0] = (SW - START_W) / 2; lw[0] = START_W; mw = START_W;
    cam = 0; chip.on = false; perfect = 0; perfAt = 0; dead = false;
    nextBlock();
  }
  void place(uint32_t now) {
    int px = lx[(n - 1) % RING], pw = lw[(n - 1) % RING];
    int x = ir(mx);
    if (abs(x - px) <= 3) {             // perfect
      x = px; perfect++; perfAt = now;
      if (perfect >= 3 && mw < START_W) { mw = imin(START_W, mw + 8); x -= 4; }
    } else perfect = 0;
    int L = imax(x, px), R = imin(x + mw, px + pw);
    if (perfect >= 3 && mw > pw) { L = x; R = x + mw; }   // grown block keeps its full width
    if (R <= L) {                        // missed completely
      chip = {(float)x, layerY(n), 0, mw, n, true};
      dead = true; deadAt = now; return;
    }
    if (x < L) chip = {(float)x, layerY(n), 0, L - x, n, true};
    else if (x + mw > R) chip = {(float)R, layerY(n), 0, x + mw - R, n, true};
    lx[n % RING] = L; lw[n % RING] = R - L; mw = R - L; n++;
    score = n - 1;
    nextBlock();
  }
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    float t = dt / 1000.0f;
    if (chip.on) { chip.vy += 900 * t; chip.y += chip.vy * t; if (chip.y > SH + 20) chip.on = false; }
    float camT = fmaxf(0, (n + 1) * LH - 120);
    cam = lerpf(cam, camT, fminf(1, t * 6));
    if (dead) { if (now - deadAt > 900) gameOver(now); return true; }
    mx += mdir * mspeed * t;
    if (mdir > 0 && mx + mw > SW) { mx = SW - mw; mdir = -1; }
    else if (mdir < 0 && mx < 0) { mx = 0; mdir = 1; }
    if (in.pressed[B_A] || in.pressed[B_DOWN]) place(now);
    return true;
  }
  void draw(Canvas& g, uint32_t now) override {
    float hue = 220 + n * 4;
    for (int y = 27; y < SH; y += 2) g.fillRect(0, y, SW, 2, hsv565(hue + y * 0.15f, 0.55f, 0.10f + 0.12f * y / SH));
    // pedestal
    int baseTop = ir(layerY(0));
    if (baseTop < SH) g.fillRect(lx[0], baseTop + LH, lw[0], SH, rgb(40, 40, 46));
    for (int i = imax(0, n - 22); i < n; i++) {
      int y = ir(layerY(i));
      if (y > SH || y + LH < 27) continue;
      uint16_t c = layerCol(i);
      g.fillRect(lx[i % RING], y, lw[i % RING], LH - 1, c);
      g.drawFastHLine(lx[i % RING], y, lw[i % RING], blend(c, C_WHITE, 0.45f));
    }
    if (!dead) {
      int y = ir(layerY(n));
      uint16_t c = layerCol(n);
      g.fillRect(ir(mx), y, mw, LH - 1, c);
      g.drawFastHLine(ir(mx), y, mw, blend(c, C_WHITE, 0.45f));
    }
    if (chip.on) g.fillRect(ir(chip.x), ir(chip.y), chip.w, LH - 1, blend(layerCol(chip.idx), C_BG, 0.25f));
    if (perfAt && now - perfAt < 700) {
      float u = seg(now, perfAt, perfAt + 700);
      textC(g, F_BOLD, SW / 2, ir(layerY(n - 1)) - 8 - ir(u * 14), perfect >= 3 ? "PERFECT x3+" : "PERFECT", blend(C_WHITE, C_BG, u));
    }
    drawHud(g, "Stack", C_AMBER);
    if (n == 1 && phase == PLAY && !dead) textC(g, F_SMALL, SW / 2, 60, "SPACE to drop the block", C_SOFT);
    drawOverlays(g, now, C_AMBER);
  }
};

// ---------------------------------------------------------------- Jumper (doodle-jump style)
// Z/C move. Bounce up on platforms. Blue = moving, brown = breaks, yellow coil = spring.
struct JumperGame : Game {
  static const int NP = 14, PWID = 46, PHT = 7;
  struct Plat { float x, y, vx, fall; uint8_t type; bool on, broken; } pl[NP];
  float px, py, vx, vy, camY, startY, topY, lastSolid, maxH; bool faceR; uint32_t springAt;
  const char* saveKey() override { return "jumper"; }

  void gen(Plat& p) {
    float d = clamp01(maxH / 6000.0f);
    float gap = 26 + plat_random(14 + (int)(30 * d));
    float y = topY - gap;
    int r = plat_random(100), type = 0;
    if (r < 6) type = 3;
    else if (r < 6 + 10 + (int)(30 * d)) type = 1;
    else if (r < 6 + 10 + (int)(30 * d) + 5 + (int)(12 * d)) type = 2;
    if (type == 2 && lastSolid - y > 40) type = 0;   // keep the path always jumpable
    if (lastSolid - y > 72) { y = lastSolid - 72; if (type == 2) type = 0; }   // max jump is ~81 px
    p.on = true; p.broken = false; p.fall = 0; p.type = type; p.y = y;
    p.x = plat_random(SW - PWID);
    p.vx = type == 1 ? (plat_random(2) ? 1 : -1) * (40 + 60 * d) : 0;
    topY = y;
    if (type != 2) lastSolid = y;
  }
  void begin() override {
    loadBest(); score = 0; phase = PLAY;
    px = SW / 2; py = 214; vx = 0; vy = -360; camY = 0; startY = py; maxH = 0; faceR = true; springAt = 0;
    pl[0].on = true; pl[0].broken = false; pl[0].type = 0; pl[0].x = SW / 2 - PWID / 2; pl[0].y = 220; pl[0].vx = 0; pl[0].fall = 0;
    topY = 220; lastSolid = 220;
    for (int i = 1; i < NP; i++) gen(pl[i]);
  }
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    float t = dt / 1000.0f;
    float target = in.held[B_LEFT] ? -210 : (in.held[B_RIGHT] ? 210 : 0);
    if (target < 0) faceR = false; else if (target > 0) faceR = true;
    vx += (target - vx) * fminf(1, t * 12);
    px += vx * t;
    if (px < 0) px += SW;
    if (px >= SW) px -= SW;
    float prev = py;
    vy += 800 * t; py += vy * t;
    for (auto& p : pl) if (p.on && p.type == 1) {
      p.x += p.vx * t;
      if (p.x < 0) { p.x = 0; p.vx = fabsf(p.vx); }
      if (p.x > SW - PWID) { p.x = SW - PWID; p.vx = -fabsf(p.vx); }
    }
    if (vy > 0) for (auto& p : pl) if (p.on && !p.broken) {
      if (prev <= p.y && py >= p.y && px + 7 > p.x && px - 7 < p.x + PWID) {
        if (p.type == 2) { p.broken = true; continue; }
        py = p.y; vy = p.type == 3 ? -640 : -360;
        if (p.type == 3) springAt = now;
        break;
      }
    }
    for (auto& p : pl) if (p.broken) p.fall += 300 * t;
    if (py - camY < 110) camY = py - 110;
    maxH = fmaxf(maxH, startY - py);
    score = (int)(maxH / 10);
    for (auto& p : pl) if (!p.on || p.y + p.fall - camY > SH + 20) { if (topY > camY - 300) gen(p); else p.on = false; }
    // make sure the area above the screen is always filled
    for (auto& p : pl) if (!p.on && topY > camY - 300) gen(p);
    if (py - camY > SH + 30) gameOver(now);
    return true;
  }
  void drawPlayer(Canvas& g, int x, int y, uint32_t now) {
    uint16_t body = C_LIME, dk = rgb(80, 130, 20);
    rrect(g, x - 8, y - 16, 16, 13, 5, body);
    g.fillRect(x - 6, y - 4, 3, 4, dk); g.fillRect(x + 3, y - 4, 3, 4, dk);
    int ex = faceR ? 2 : -2;
    g.fillRect(x - 4 + ex, y - 13, 3, 3, C_BG); g.fillRect(x + 1 + ex, y - 13, 3, 3, C_BG);
    g.fillRect(faceR ? x + 7 : x - 11, y - 10, 4, 3, body);   // snout
  }
  void draw(Canvas& g, uint32_t now) override {
    g.fillScreen(C_BG);
    uint16_t gl = rgb(16, 22, 34);
    int oy = ((int)floorf(-camY)) % 16; if (oy < 0) oy += 16;
    for (int y = oy; y < SH; y += 16) g.drawFastHLine(0, y, SW, gl);
    for (int x = 0; x < SW; x += 16) g.drawFastVLine(x, 27, SH - 27, gl);
    for (auto& p : pl) if (p.on) {
      int x = ir(p.x), y = ir(p.y + p.fall - camY);
      if (y < 20 || y > SH) continue;
      uint16_t c = p.type == 1 ? C_BLUE : p.type == 2 ? rgb(150, 100, 60) : C_GREEN;
      if (p.broken) { rrect(g, x, y, PWID / 2 - 1, PHT, 3, c); rrect(g, x + PWID / 2 + 1, y + 2, PWID / 2 - 1, PHT, 3, c); continue; }
      rrect(g, x, y, PWID, PHT, 3, c);
      g.drawFastHLine(x + 3, y + 1, PWID - 6, blend(c, C_WHITE, 0.4f));
      if (p.type == 2) g.drawLine(x + 20, y, x + 24, y + PHT - 1, rgb(80, 50, 30));
      if (p.type == 3) {
        bool sprung = springAt && now - springAt < 250;
        g.fillRect(x + 18, y - (sprung ? 9 : 5), 10, 2, C_YELLOW);
        for (int k = 0; k < (sprung ? 4 : 2); k++) g.drawFastHLine(x + 19, y - 2 - k * 2, 8, C_YELLOW);
      }
    }
    int sy = ir(py - camY);
    drawPlayer(g, ir(px), sy, now);
    if (px < 12) drawPlayer(g, ir(px) + SW, sy, now);
    if (px > SW - 12) drawPlayer(g, ir(px) - SW, sy, now);
    drawHud(g, "Jumper", C_INDIGO);
    if (maxH < 30 && phase == PLAY) textC(g, F_SMALL, SW / 2, 40, "Z / C to move left and right", C_SOFT);
    drawOverlays(g, now, C_INDIGO);
  }
};

// ---------------------------------------------------------------- Minesweeper
// DXZC move, SPACE reveal (on a number = open around it), F flag.
struct MinesGame : Game {
  static const int MC = 16, MROWS = 10, CSZ = 20, OY = 32, NM = 24;
  uint8_t mine[MROWS][MC], st[MROWS][MC], adj[MROWS][MC];   // st: 0 hidden, 1 open, 2 flag
  int cx, cy, revealed, flags, result, hitR, hitC; bool placed; uint32_t startAt, endAt;
  const char* saveKey() override { return "mines"; }

  void begin() override {
    loadBest(); score = 0; phase = PLAY;
    memset(mine, 0, sizeof(mine)); memset(st, 0, sizeof(st)); memset(adj, 0, sizeof(adj));
    cx = MC / 2; cy = MROWS / 2; revealed = flags = 0; result = 0; placed = false; startAt = 0; endAt = 0; hitR = hitC = -1;
  }
  void placeMines(int sr, int sc) {
    int n = 0;
    while (n < NM) {
      int r = plat_random(MROWS), c = plat_random(MC);
      if (mine[r][c] || (abs(r - sr) <= 1 && abs(c - sc) <= 1)) continue;
      mine[r][c] = 1; n++;
    }
    for (int r = 0; r < MROWS; r++) for (int c = 0; c < MC; c++) {
      int k = 0;
      for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) {
        int rr = r + dr, cc = c + dc;
        if (rr >= 0 && rr < MROWS && cc >= 0 && cc < MC && mine[rr][cc]) k++;
      }
      adj[r][c] = k;
    }
    placed = true;
  }
  void open(int r0, int c0, uint32_t now) {
    if (st[r0][c0] != 0) return;
    if (mine[r0][c0]) { st[r0][c0] = 1; result = 1; endAt = now; hitR = r0; hitC = c0; return; }
    static uint8_t stk[MROWS * MC][2]; int sp = 0;
    stk[sp][0] = r0; stk[sp][1] = c0; sp++; st[r0][c0] = 1; revealed++;
    while (sp) {
      sp--; int r = stk[sp][0], c = stk[sp][1];
      if (adj[r][c]) continue;
      for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) {
        int rr = r + dr, cc = c + dc;
        if (rr < 0 || rr >= MROWS || cc < 0 || cc >= MC || st[rr][cc] != 0 || mine[rr][cc]) continue;
        st[rr][cc] = 1; revealed++; stk[sp][0] = rr; stk[sp][1] = cc; sp++;
      }
    }
  }
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    if (result) { if (now - endAt > 1300) gameOver(now); return true; }
    if (in.rep[B_LEFT])  cx = (cx + MC - 1) % MC;
    if (in.rep[B_RIGHT]) cx = (cx + 1) % MC;
    if (in.rep[B_UP])    cy = (cy + MROWS - 1) % MROWS;
    if (in.rep[B_DOWN])  cy = (cy + 1) % MROWS;
    if (in.pressed[B_F] && st[cy][cx] != 1) { if (st[cy][cx] == 2) { st[cy][cx] = 0; flags--; } else { st[cy][cx] = 2; flags++; } }
    if (in.pressed[B_A]) {
      if (!placed) { placeMines(cy, cx); startAt = now; }
      if (st[cy][cx] == 0) open(cy, cx, now);
      else if (st[cy][cx] == 1 && adj[cy][cx]) {         // chord
        int f = 0;
        for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) {
          int rr = cy + dr, cc = cx + dc;
          if (rr >= 0 && rr < MROWS && cc >= 0 && cc < MC && st[rr][cc] == 2) f++;
        }
        if (f == adj[cy][cx])
          for (int dr = -1; dr <= 1 && !result; dr++) for (int dc = -1; dc <= 1 && !result; dc++) {
            int rr = cy + dr, cc = cx + dc;
            if (rr >= 0 && rr < MROWS && cc >= 0 && cc < MC && st[rr][cc] == 0) open(rr, cc, now);
          }
      }
      if (!result && revealed == MROWS * MC - NM) {
        result = 2; endAt = now;
        int secs = (now - startAt) / 1000;
        score = 500 + imax(0, 999 - secs);
        return true;
      }
    }
    if (!result) score = revealed;
    return true;
  }
  void draw(Canvas& g, uint32_t now) override {
    static const uint16_t NC[9] = {0, C_BLUE, C_GREEN, C_RED, C_PURPLE, C_ORANGE, C_TEAL, C_WHITE, C_SOFT};
    g.fillScreen(C_BG);
    for (int r = 0; r < MROWS; r++) for (int c = 0; c < MC; c++) {
      int x = c * CSZ, y = OY + r * CSZ;
      bool showMine = result && mine[r][c];
      if (st[r][c] == 1 || showMine) {
        g.fillRect(x + 1, y + 1, CSZ - 2, CSZ - 2, (r == hitR && c == hitC) ? rgb(150, 30, 30) : C_CARD);
        if (mine[r][c]) {
          g.fillCircle(x + 10, y + 10, 5, C_WHITE);
          g.drawFastHLine(x + 3, y + 10, 15, C_WHITE); g.drawFastVLine(x + 10, y + 3, 15, C_WHITE);
          g.fillRect(x + 8, y + 8, 2, 2, C_BG);
        } else if (adj[r][c]) {
          char b[2] = {(char)('0' + adj[r][c]), 0};
          textC(g, F_BOLD, x + 10, y + 16, b, NC[adj[r][c]]);
        }
      } else {
        rrect(g, x + 1, y + 1, CSZ - 2, CSZ - 2, 3, rgb(64, 68, 82));
        g.drawFastHLine(x + 3, y + 2, CSZ - 6, rgb(96, 100, 116));
        if (st[r][c] == 2) {
          bool wrong = result && !mine[r][c];
          g.drawFastVLine(x + 8, y + 4, 12, C_WHITE); g.fillRect(x + 5, y + 15, 8, 2, C_WHITE);
          g.fillTriangle(x + 9, y + 4, x + 16, y + 7, x + 9, y + 10, wrong ? C_SOFT : C_RED);
        }
      }
    }
    if (!result) {
      int x = cx * CSZ, y = OY + cy * CSZ;
      g.drawRoundRect(x, y, CSZ, CSZ, 4, C_WHITE); g.drawRoundRect(x + 1, y + 1, CSZ - 2, CSZ - 2, 3, C_WHITE);
    }
    drawHud(g, "Mines", C_SLATE);
    int secs = !placed ? 0 : (int)(((result ? endAt : now) - startAt) / 1000);
    char b[40];
    if (!placed) snprintf(b, sizeof(b), "SPACE open  F flag");
    else snprintf(b, sizeof(b), "%d LEFT  %ds", NM - flags, secs);
    text(g, F_SMALL, 82, 10, b, placed ? C_DIM : C_SOFT);
    drawOverlays(g, now, C_SLATE);
    if (phase == OVER) textC(g, F_BOLD, SW / 2, 48, result == 2 ? "Cleared!" : "Boom!", result == 2 ? C_GREEN : C_RED);
  }
};

// ---------------------------------------------------------------- Connect 4 vs CPU
// Z/C choose a column, SPACE (or X) drop. Win rounds in a row; one loss ends the run.
struct Connect4Game : Game {
  static const int NCOL = 7, NROW = 6, CEL = 28, BX = (SW - NCOL * CEL) / 2, BY = 60;
  int8_t b[NROW][NCOL];
  int curC, turn, firstTurn, wins, draws;
  bool anim; int aCol, aRow, aWho; float aY, aV;
  int rState, result; uint32_t endAt, cpuAt; int winCells[4][2];
  const char* saveKey() override { return "connect4"; }

  void newRound(uint32_t now) {
    memset(b, 0, sizeof(b)); curC = 3; anim = false; rState = 0; result = 0;
    turn = firstTurn; cpuAt = now + 500;
  }
  void begin() override {
    loadBest(); score = 0; phase = PLAY; wins = draws = 0; firstTurn = 1;
    newRound(plat_millis());
  }
  int dropRow(int c) { for (int r = NROW - 1; r >= 0; r--) if (!b[r][c]) return r; return -1; }
  bool winsAt(int r, int c, int who, int out[4][2]) {
    static const int D[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
    for (auto& d : D) {
      int cells[7][2], n = 0;
      for (int k = -3; k <= 3; k++) {
        int rr = r + d[0] * k, cc = c + d[1] * k;
        if (rr >= 0 && rr < NROW && cc >= 0 && cc < NCOL && b[rr][cc] == who) {
          cells[n][0] = rr; cells[n][1] = cc; n++;
          if (n >= 4) { if (out) for (int i = 0; i < 4; i++) { out[i][0] = cells[n - 4 + i][0]; out[i][1] = cells[n - 4 + i][1]; } return true; }
        } else n = 0;
      }
    }
    return false;
  }
  int evalFor(int who) {
    int other = 3 - who, s = 0;
    for (int r = 0; r < NROW; r++) { if (b[r][3] == who) s += 3; else if (b[r][3] == other) s -= 3; }
    static const int D[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
    for (int r = 0; r < NROW; r++) for (int c = 0; c < NCOL; c++) for (auto& d : D) {
      int er = r + d[0] * 3, ec = c + d[1] * 3;
      if (er < 0 || er >= NROW || ec < 0 || ec >= NCOL) continue;
      int mine = 0, theirs = 0;
      for (int k = 0; k < 4; k++) { int v = b[r + d[0] * k][c + d[1] * k]; if (v == who) mine++; else if (v == other) theirs++; }
      if (mine && theirs) continue;
      if (mine == 3) s += 5; else if (mine == 2) s += 2;
      if (theirs == 3) s -= 4; else if (theirs == 2) s -= 1;
    }
    return s;
  }
  int negamax(int depth, int alpha, int beta, int who) {
    static const int ORDER[7] = {3, 2, 4, 1, 5, 0, 6};
    if (depth == 0) return evalFor(who);
    int bv = -1000000; bool any = false;
    for (int i = 0; i < NCOL; i++) {
      int c = ORDER[i], r = dropRow(c);
      if (r < 0) continue;
      any = true;
      b[r][c] = who;
      int v = winsAt(r, c, who, nullptr) ? 100000 + depth : -negamax(depth - 1, -beta, -alpha, 3 - who);
      b[r][c] = 0;
      if (v > bv) bv = v;
      if (bv > alpha) alpha = bv;
      if (alpha >= beta) break;
    }
    return any ? bv : 0;
  }
  int cpuMove() {
    int bestV = -10000000, bestC = 3;
    for (int c = 0; c < NCOL; c++) {
      int r = dropRow(c);
      if (r < 0) continue;
      b[r][c] = 2;
      int v = winsAt(r, c, 2, nullptr) ? 1000000 : -negamax(4, -10000000, 10000000, 1);
      b[r][c] = 0;
      v += plat_random(3) - abs(c - 3);   // small tie-break: prefer the centre, a little randomness
      if (v > bestV) { bestV = v; bestC = c; }
    }
    return bestC;
  }
  void startDrop(int c, int who) { anim = true; aCol = c; aRow = dropRow(c); aWho = who; aY = BY - CEL / 2; aV = 0; }
  bool boardFull() { for (int c = 0; c < NCOL; c++) if (!b[0][c]) return false; return true; }
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    float t = dt / 1000.0f;
    if (rState) {
      if (now - endAt > 1700) { if (result == 2) gameOver(now); else { firstTurn = 3 - firstTurn; newRound(now); } }
      return true;
    }
    if (anim) {
      aV += 1800 * t; aY += aV * t;
      float target = BY + aRow * CEL + CEL / 2;
      if (aY >= target) {
        anim = false; b[aRow][aCol] = aWho;
        if (winsAt(aRow, aCol, aWho, winCells)) {
          rState = 1; endAt = now; result = aWho;
          if (aWho == 1) { wins++; score += 100; }
        } else if (boardFull()) { rState = 1; endAt = now; result = 3; draws++; score += 30; }
        else { turn = 3 - aWho; cpuAt = now + 350; }
      }
      return true;
    }
    if (turn == 1) {
      if (in.rep[B_LEFT])  curC = (curC + NCOL - 1) % NCOL;
      if (in.rep[B_RIGHT]) curC = (curC + 1) % NCOL;
      if ((in.pressed[B_A] || in.pressed[B_DOWN]) && dropRow(curC) >= 0) startDrop(curC, 1);
    } else if (now >= cpuAt) startDrop(cpuMove(), 2);
    return true;
  }
  void draw(Canvas& g, uint32_t now) override {
    g.fillScreen(C_BG);
    uint16_t pc = C_RED, cc = C_YELLOW, board = rgb(37, 99, 235);
    if (anim) g.fillCircle(BX + aCol * CEL + CEL / 2, ir(aY), 11, aWho == 1 ? pc : cc);
    rrect(g, BX - 4, BY - 4, NCOL * CEL + 8, NROW * CEL + 8, 8, board);
    for (int r = 0; r < NROW; r++) for (int c = 0; c < NCOL; c++) {
      int x = BX + c * CEL + CEL / 2, y = BY + r * CEL + CEL / 2;
      uint16_t col = b[r][c] == 1 ? pc : b[r][c] == 2 ? cc : C_BG;
      g.fillCircle(x, y, 11, col);
      if (b[r][c]) g.fillCircle(x - 3, y - 3, 3, blend(col, C_WHITE, 0.4f));
    }
    if (rState && result != 3 && (now / 200) % 2)
      for (auto& w : winCells) { int x = BX + w[1] * CEL + CEL / 2, y = BY + w[0] * CEL + CEL / 2; g.drawCircle(x, y, 12, C_WHITE); g.drawCircle(x, y, 13, C_WHITE); }
    if (!rState && !anim && turn == 1) {
      int x = BX + curC * CEL + CEL / 2;
      g.fillCircle(x, 43, 10, pc);
      g.fillTriangle(x - 4, 55, x + 4, 55, x, 59, C_SOFT);
    }
    drawHud(g, "Connect 4", C_RED);
    g.fillCircle(24, 80, 7, pc); textC(g, F_SMALL, 24, 94, "YOU", C_SOFT);
    g.fillCircle(SW - 24, 80, 7, cc); textC(g, F_SMALL, SW - 24, 94, "CPU", C_SOFT);
    char bf[16]; snprintf(bf, sizeof(bf), "%d", wins); textC(g, F_BIG, 24, 130, bf, C_WHITE);
    textC(g, F_SMALL, 24, 138, "WINS", C_DIM);
    if (!rState) textC(g, F_SMALL, turn == 1 ? 24 : SW - 24, 62, turn == 1 ? "TURN" : "THINK", turn == 1 ? pc : cc);
    else {
      const char* s = result == 1 ? "You win!" : result == 2 ? "CPU wins" : "Draw";
      rrect(g, 110, 118, 100, 30, 10, C_CARD);
      textC(g, F_BOLD, SW / 2, 139, s, result == 1 ? C_GREEN : result == 2 ? C_RED : C_WHITE);
    }
    drawOverlays(g, now, C_RED);
  }
};

// ---------------------------------------------------------------- Simon
// Watch the pattern, then repeat it with D (top) C (right) X (bottom) Z (left).
struct SimonGame : Game {
  static const int MAXN = 100, CXP = SW / 2, CYP = 134, PAD = 54, PO = 58;
  uint8_t seqv[MAXN]; int len, showI, inI, sstate;   // 0 pause, 1 show, 2 input, 3 fail
  uint32_t stAt, litUntil; int lit;
  const char* saveKey() override { return "simon"; }

  void begin() override {
    loadBest(); score = 0; phase = PLAY; len = 0; lit = -1; litUntil = 0;
    seqv[len++] = plat_random(4); sstate = 0; stAt = plat_millis() + 300;
  }
  uint32_t onMs() { return (uint32_t)imax(170, 460 - len * 14); }
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    if (now >= litUntil) lit = -1;
    if (sstate == 0) { if ((int32_t)(now - stAt) > 700) { sstate = 1; showI = 0; stAt = now; } return true; }
    if (sstate == 1) {
      uint32_t step = onMs() + 140, e = now - stAt;
      int i = e / step;
      if (i >= len) { sstate = 2; inI = 0; stAt = now; lit = -1; return true; }
      lit = (e % step) < onMs() ? seqv[i] : -1; litUntil = now + 30;
      return true;
    }
    if (sstate == 3) { if (now - stAt > 900) gameOver(now); return true; }
    int d = in.pressed[B_UP] ? 0 : in.pressed[B_RIGHT] ? 1 : in.pressed[B_DOWN] ? 2 : in.pressed[B_LEFT] ? 3 : -1;
    if (d >= 0) {
      lit = d; litUntil = now + 220;
      if (d == seqv[inI]) {
        inI++; stAt = now;
        if (inI == len) { score = len; if (len < MAXN) seqv[len++] = plat_random(4); sstate = 0; stAt = now; }
      } else { sstate = 3; stAt = now; lit = seqv[inI]; litUntil = now + 900; }
    } else if (now - stAt > 5000) { sstate = 3; stAt = now; lit = seqv[inI]; litUntil = now + 900; }
    return true;
  }
  void draw(Canvas& g, uint32_t now) override {
    static const uint16_t on[4]  = {rgb(74, 222, 128), rgb(248, 113, 113), rgb(96, 165, 250), rgb(250, 204, 21)};
    static const int px[4] = {0, 1, 0, -1}, py[4] = {-1, 0, 1, 0};
    static const char* keyName[4] = {"D", "C", "X", "Z"};
    g.fillScreen(C_BG);
    for (int i = 0; i < 4; i++) {
      int x = CXP + px[i] * PO - PAD / 2, y = CYP + py[i] * PO - PAD / 2;
      bool L = lit == i;
      uint16_t c = L ? on[i] : blend(on[i], C_BG, 0.72f);
      if (L) rrect(g, x - 4, y - 4, PAD + 8, PAD + 8, 14, blend(on[i], C_BG, 0.6f));
      rrect(g, x, y, PAD, PAD, 12, c);
      textC(g, F_BOLD, x + PAD / 2, y + PAD / 2 + 6, keyName[i], L ? C_BG : blend(on[i], C_BG, 0.3f));
    }
    char b[8]; snprintf(b, sizeof(b), "%d", len);
    g.fillCircle(CXP, CYP, 20, C_CARD);
    textC(g, F_BIG, CXP, CYP + 8, b, C_WHITE);
    const char* msg = sstate == 1 ? "WATCH" : sstate == 2 ? "YOUR TURN" : sstate == 3 ? "WRONG" : "GET READY";
    text(g, F_SMALL, 10, 40, msg, sstate == 3 ? C_RED : sstate == 2 ? C_GREEN : C_SOFT);
    drawHud(g, "Simon", C_SOFT);
    drawOverlays(g, now, C_SOFT);
  }
};


struct MenuItem { const char* name; const char* key; uint16_t color; Icon icon; };
// Games first (same order as App::games), then Notes, Settings, About
static const MenuItem MENU[] = {
  {"Snake",     "snake",     C_GREEN,  IC_SNAKE},
  {"Blocks",    "blocks",    C_PURPLE, IC_BLOCKS},
  {"Pong",      "pong",      C_TEAL,   IC_PONG},
  {"Breakout",  "breakout",  C_ORANGE, IC_BREAKOUT},
  {"Flappy",    "flappy",    C_YELLOW, IC_FLAPPY},
  {"Invaders",  "invaders",  C_PINK,   IC_INVADERS},
  {"Asteroids", "asteroids", C_BLUE,   IC_ASTEROIDS},
  {"Dino",      "dino",      C_LIME,   IC_DINO},
  {"Racer",     "racer",     C_ROSE,   IC_RACER},
  {"Tron",      "tron",      C_CYAN,   IC_TRON},
  {"Stack",     "stack",     C_AMBER,  IC_STACK},
  {"Jumper",    "jumper",    C_INDIGO, IC_JUMPER},
  {"Mines",     "mines",     C_SLATE,  IC_MINES},
  {"Connect 4", "connect4",  C_RED,    IC_C4},
  {"Simon",     "simon",     C_SOFT,   IC_SIMON},
  {"Notes",     nullptr,     C_NOTE,   IC_NOTES},
  {"Settings",  nullptr,     C_SOFT,   IC_SETTINGS},
  {"About",     nullptr,     C_SOFT,   IC_ABOUT},
};
static const int N_MENU = sizeof(MENU) / sizeof(MENU[0]);
static const int N_GAMES = 15;
static const int M_NOTES = N_GAMES, M_SETTINGS = N_GAMES + 1, M_ABOUT = N_GAMES + 2;

// Menu grid geometry (4 columns, 2 rows visible, scrolls vertically)
static const int TILE_W = 72, TILE_H = 78, TILE_X0 = 7, TILE_GAP = 6, TILE_Y0 = 48;
static const int ROW_H = TILE_H + TILE_GAP, MENU_ROWS = (N_MENU + 3) / 4;
static inline int tileX(int i) { return TILE_X0 + (i % 4) * (TILE_W + TILE_GAP); }
static inline int tileY(int i) { return TILE_Y0 + (i / 4) * ROW_H; }   // position in the scrolling list

// Boot mascot geometry (s = 0.62, centred at 160,100)
static const float BM_S = 0.62f, BM_CX = 160, BM_CY = 100;
static const float BM_W = 248 * BM_S, BM_H = 192 * BM_S;
static const float BM_TOP = BM_CY + 96 * BM_S - BM_H, BM_LEFT = BM_CX - BM_W / 2;
static const float BM_BAR_Y = BM_CY + 125 * BM_S, BM_BAR_H = 26 * BM_S, BM_BAR_W = 302 * BM_S;

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
    case IC_INVADERS: {
      static const uint8_t bits[8] = {0x42, 0x24, 0x7E, 0xDB, 0xFF, 0xBD, 0xA5, 0x24};
      drawBits8(g, bits, 8, x + 9, y + 9, 2, k); break; }
    case IC_ASTEROIDS:
      g.drawCircle(x + 22, y + 12, 7, k); g.drawCircle(x + 22, y + 12, 6, k);
      g.fillTriangle(x + 8, y + 28, x + 12, y + 16, x + 16, y + 28, k);
      g.fillRect(x + 11, y + 12, 2, 2, k); break;
    case IC_DINO:
      g.fillRect(x + 8, y + 14, 12, 10, k); g.fillRect(x + 16, y + 7, 11, 8, k);
      g.fillRect(x + 19, y + 9, 2, 2, col); g.fillRect(x + 5, y + 16, 4, 3, k);
      g.fillRect(x + 10, y + 24, 3, 5, k); g.fillRect(x + 16, y + 24, 3, 5, k); break;
    case IC_RACER:
      g.fillRect(x + 10, y + 8, 3, 5, k); g.fillRect(x + 21, y + 8, 3, 5, k);
      g.fillRect(x + 10, y + 21, 3, 5, k); g.fillRect(x + 21, y + 21, 3, 5, k);
      g.fillRoundRect(x + 12, y + 5, 10, 24, 3, k); g.fillRect(x + 14, y + 11, 6, 4, col); break;
    case IC_TRON:
      g.fillRect(x + 6, y + 24, 14, 3, k); g.fillRect(x + 17, y + 10, 3, 17, k); g.fillRect(x + 17, y + 10, 11, 3, k);
      g.fillRect(x + 26, y + 9, 4, 5, k); break;
    case IC_STACK:
      g.fillRect(x + 8, y + 23, 18, 5, k); g.fillRect(x + 10, y + 17, 15, 5, k);
      g.fillRect(x + 9, y + 11, 13, 5, k); g.fillRect(x + 16, y + 5, 14, 5, k); break;
    case IC_JUMPER:
      g.fillRect(x + 5, y + 26, 13, 3, k); g.fillRect(x + 19, y + 15, 11, 3, k);
      g.fillRoundRect(x + 9, y + 5, 9, 9, 3, k); g.fillRect(x + 11, y + 7, 2, 2, col); g.fillRect(x + 15, y + 7, 2, 2, col); break;
    case IC_MINES:
      g.fillCircle(cx, cy, 7, k); g.drawFastHLine(cx - 11, cy, 23, k); g.drawFastVLine(cx, cy - 11, 23, k);
      g.drawLine(cx - 8, cy - 8, cx + 8, cy + 8, k); g.drawLine(cx - 8, cy + 8, cx + 8, cy - 8, k);
      g.fillRect(cx - 3, cy - 3, 2, 2, col); break;
    case IC_C4:
      for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) g.fillCircle(x + 9 + c * 8, y + 10 + r * 8, 3, (r + c) % 2 ? k : C_WHITE);
      break;
    case IC_SIMON:
      g.fillRoundRect(x + 4, y + 4, 12, 12, 3, C_GREEN); g.fillRoundRect(x + 18, y + 4, 12, 12, 3, C_RED);
      g.fillRoundRect(x + 4, y + 18, 12, 12, 3, C_YELLOW); g.fillRoundRect(x + 18, y + 18, 12, 12, 3, C_BLUE); break;
    case IC_NOTES:
      g.fillRoundRect(x + 8, y + 5, 18, 24, 2, k);
      for (int i = 0; i < 4; i++) g.drawFastHLine(x + 11, y + 10 + i * 5, i == 3 ? 7 : 12, col);
      break;
    case IC_SETTINGS:
      for (int i = 0; i < 8; i++) {
        float a = i * 3.14159f / 4;
        g.fillCircle(ir(cx + cosf(a) * 9), ir(cy + sinf(a) * 9), 3, k);
      }
      g.fillCircle(cx, cy, 8, k); g.fillCircle(cx, cy, 3, col); break;
    case IC_ABOUT:
      g.fillCircle(cx, y + 9, 3, k); g.fillRoundRect(cx - 2, y + 14, 5, 13, 2, k); break;
  }
}

struct App {
  enum State { BOOT, INTRO, MENU_S, LAUNCH, GAME, SETTINGS, ABOUT, NOTES, NOTE_EDIT } st = BOOT;
  uint32_t t0 = 0;
  int sel = 0;
  float hx = TILE_X0, hy = TILE_Y0;   // gliding highlight position
  float scroll = 0; int topRow = 0;   // menu scrolling (pixels / first visible row)
  uint32_t menuAt = 0;
  int bests[N_GAMES] = {};
  Game* games[N_GAMES];
  Game* cur = nullptr;
  bool showFps = false, flip = false;
  int setSel = 0; uint32_t resetArmAt = 0, resetDoneAt = 0;
  uint32_t lastT = 0;
  static const uint32_t BOOT_LEN = 4150;

  SnakeGame snake; BlocksGame blocks; PongGame pong; BreakoutGame breakout; FlappyGame flappy;
  InvadersGame invaders; AsteroidsGame asteroids; DinoGame dino; RacerGame racer; TronGame tron;
  StackGame stack; JumperGame jumper; MinesGame mines; Connect4Game connect4; SimonGame simon;
  Input* inp = nullptr;   // set in update(), used by the Notes screens

  // ---- Notes ----
  static const int NOTE_N = 6, NOTE_MAX = 1000, NCOLS = 50, NROWS = 16, NTX = 8, NTY = 32, NLH = 11;
  char nbuf[NOTE_MAX + 1]; int nlen = 0, ncur = 0, ntop = 0, nprefCol = -1, nslot = 0;
  uint16_t nls[NOTE_MAX + 2], nll[NOTE_MAX + 2]; int nlines = 0;
  bool ndirty = false, nsaveFail = false; uint32_t nlastEdit = 0, nsavedAt = 0;
  int nsel = 0; char nprev[NOTE_N][36]; int nplen[NOTE_N]; uint32_t nclearArm = 0; int nclearSlot = -1;

  void begin() {
    games[0] = &snake; games[1] = &blocks; games[2] = &pong;
    games[3] = &breakout; games[4] = &flappy; games[5] = &invaders;
    games[6] = &asteroids; games[7] = &dino; games[8] = &racer; games[9] = &tron;
    games[10] = &stack; games[11] = &jumper; games[12] = &mines; games[13] = &connect4; games[14] = &simon;
    showFps = plat_loadInt("fps", 0);
    flip = plat_loadInt("flip2", 0);
    plat_setFlip(flip);
    loadBests();
    t0 = plat_millis();
  }
  void loadBests() { for (int i = 0; i < N_GAMES; i++) bests[i] = plat_loadInt(MENU[i].key, 0); }
  void go(State s, uint32_t now) { st = s; t0 = now; if (s == INTRO) menuAt = now + 700; }
  void openMenu(uint32_t now) { go(MENU_S, now); menuAt = now; }

  // ---------------- update ----------------
  void update(Input& in, uint32_t now) {
    inp = &in;
    uint32_t dt = lastT ? now - lastT : 16; if (dt > 50) dt = 50; lastT = now;
    uint32_t t = now - t0;
    switch (st) {
      case BOOT:  if (t > BOOT_LEN || (t > 400 && in.anyPressed)) go(INTRO, now); break;
      case INTRO: if (t > 1100) go(MENU_S, now); break;
      case MENU_S: updateMenu(in, now); break;
      case LAUNCH:
        if (t > 480) {
          if (sel < N_GAMES) { cur = games[sel]; cur->begin(); go(GAME, now); }
          else openNotesList(now);
        }
        break;
      case GAME:  if (!cur->update(in, now, dt)) { loadBests(); openMenu(now); } break;
      case SETTINGS: updateSettings(in, now); break;
      case ABOUT: if (in.pressed[B_B] || in.pressed[B_A]) openMenu(now); break;
      case NOTES: updateNotesList(in, now); break;
      case NOTE_EDIT: updateNoteEdit(in, now); break;
    }
    float k = 1 - powf(0.0005f, dt / 1000.0f * 1.6f);
    hx = lerpf(hx, tileX(sel), k);
    hy = lerpf(hy, tileY(sel), k);
    int row = sel / 4;
    if (row < topRow) topRow = row;
    if (row > topRow + 1) topRow = row - 1;
    scroll = lerpf(scroll, topRow * ROW_H, k);
    if (fabsf(scroll - topRow * ROW_H) < 0.5f) scroll = topRow * ROW_H;
  }
  void updateMenu(Input& in, uint32_t now) {
    if (in.rep[B_LEFT])  sel = (sel + N_MENU - 1) % N_MENU;
    if (in.rep[B_RIGHT]) sel = (sel + 1) % N_MENU;
    if (in.rep[B_DOWN]) {
      if (sel + 4 < N_MENU) sel += 4;
      else if (sel / 4 < MENU_ROWS - 1) sel = N_MENU - 1;   // short last row
      else sel = sel % 4;                                   // wrap to the top
    }
    if (in.rep[B_UP]) {
      if (sel - 4 >= 0) sel -= 4;
      else sel = imin(N_MENU - 1, (MENU_ROWS - 1) * 4 + sel % 4);   // wrap to the bottom
    }
    if (in.pressed[B_A]) {
      if (sel <= M_NOTES) go(LAUNCH, now);
      else if (sel == M_SETTINGS) { setSel = 0; resetArmAt = resetDoneAt = 0; go(SETTINGS, now); }
      else go(ABOUT, now);
    }
  }
  void updateSettings(Input& in, uint32_t now) {
    const int N = 4;
    if (in.rep[B_UP]) setSel = (setSel + N - 1) % N;
    if (in.rep[B_DOWN]) setSel = (setSel + 1) % N;
    if (in.pressed[B_B] || in.pressed[B_LEFT]) { openMenu(now); return; }
    if (in.pressed[B_A]) {
      if (setSel == 0) { flip = !flip; plat_saveInt("flip2", flip); plat_setFlip(flip); }
      else if (setSel == 1) { showFps = !showFps; plat_saveInt("fps", showFps); }
      else if (setSel == 3) {
        if (resetArmAt && now - resetArmAt < 3000) {
          for (int i = 0; i < N_GAMES; i++) plat_saveInt(MENU[i].key, 0);
          loadBests(); resetArmAt = 0; resetDoneAt = now;
        } else resetArmAt = now;
      }
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
      case SETTINGS: drawSettings(g, now); break;
      case ABOUT:  drawAbout(g, now); break;
      case NOTES:  drawNotesList(g, now); break;
      case NOTE_EDIT: drawNoteEdit(g, now); break;
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
  void drawHeader(Canvas& g, uint32_t now, const char* title) {
    g.fillRect(0, 0, SW, 42, C_BG);
    Mascot m; m.cx = 22; m.cy = 18; m.s = 0.1f;
    m.eyeOpen = 1 - bump(seg(now % 4200, 3900, 4100));
    float sw = sinf(now * 0.0011f);
    m.look = sw > 0.6f ? 1 : (sw < -0.6f ? -1 : 0);
    drawMascot(g, m);
    text(g, F_BIG, 42, 29, title, C_WHITE);
    int ks = plat_kbState();
    uint16_t dc = ks == 3 ? C_GREEN : (ks == 0 ? C_DIM : ((now / 300) % 2 ? C_YELLOW : C_BG));
    rrect(g, 262, 10, 48, 20, 10, C_CARD);
    g.fillCircle(274, 20, 4, dc);
    text(g, F_SMALL, 284, 16, "KB", ks == 3 ? C_WHITE : C_DIM);
    g.drawFastHLine(10, 39, 300, C_LINE);
  }
  void drawFooter(Canvas& g, const char* left, const char* right) {
    g.fillRect(0, 216, SW, SH - 216, C_BG);
    text(g, F_SMALL, 12, 224, left, C_SOFT);
    textR(g, F_SMALL, SW - 12, 224, right, C_DIM);
  }
  void drawMenu(Canvas& g, uint32_t now, bool highlight) {
    g.fillScreen(C_BG);
    int sc = ir(scroll);
    int firstRow = sc / ROW_H;
    for (int i = 0; i < N_MENU; i++) {
      int vis = (i / 4 - firstRow) * 4 + i % 4;   // stagger the pop-in by on-screen position
      if (vis < 0 || vis >= 12) continue;
      float in = seg(now, menuAt + vis * 40, menuAt + vis * 40 + 320);
      if (in <= 0) continue;
      int x = tileX(i), y = tileY(i) - sc + ir((1 - easeOutBack(in)) * 24);
      rrect(g, x, y, TILE_W, TILE_H, 12, blend(C_BG, C_CARD, in));
    }
    if (highlight) rrect(g, hx, hy - sc, TILE_W, TILE_H, 12, C_WHITE);
    for (int i = 0; i < N_MENU; i++) {
      int vis = (i / 4 - firstRow) * 4 + i % 4;
      if (vis < 0 || vis >= 12) continue;
      float in = seg(now, menuAt + vis * 40, menuAt + vis * 40 + 320);
      if (in <= 0) continue;
      int x = tileX(i), y = tileY(i) - sc + ir((1 - easeOutBack(in)) * 24);
      bool isSel = highlight && i == sel && fabsf(hx - x) < TILE_W / 2 && fabsf(hy - tileY(i)) < TILE_H / 2;
      drawTileContent(g, i, x, y, isSel);
    }
    // scroll bar (right edge)
    int trackY = TILE_Y0, trackH = 2 * ROW_H - TILE_GAP;
    int maxScroll = (MENU_ROWS - 2) * ROW_H;
    int thumbH = trackH * 2 / MENU_ROWS;
    int thumbY = trackY + (maxScroll > 0 ? ir((trackH - thumbH) * clamp01(scroll / maxScroll)) : 0);
    g.fillRect(SW - 3, trackY, 2, trackH, C_CARD);
    g.fillRect(SW - 3, thumbY, 2, thumbH, C_SOFT);
    drawHeader(g, now, "Arcade");
    char left[32];
    if (sel < N_GAMES) { if (bests[sel]) snprintf(left, sizeof(left), "%s   BEST %d", MENU[sel].name, bests[sel]); else snprintf(left, sizeof(left), "%s   NEW", MENU[sel].name); }
    else snprintf(left, sizeof(left), "%s", MENU[sel].name);
    drawFooter(g, left, "DXZC move   ENTER open");
  }
  void drawLaunch(Canvas& g, uint32_t t, uint32_t now) {
    drawMenu(g, now, true);
    float k = easeInOutCubic(seg(t, 0, 320));
    uint16_t c = blend(MENU[sel].color, C_BG, seg(t, 320, 480));
    rrect(g, lerpf(hx, 0, k), lerpf(hy - scroll, 0, k), lerpf(TILE_W, SW, k), lerpf(TILE_H, SH, k), lerpf(12, 0, k), c);
    float nk = seg(t, 120, 320) * (1 - seg(t, 330, 470));
    if (nk > 0) textC(g, F_HUGE, SW / 2, ir(132 + 12 * (1 - easeOutCubic(nk))), MENU[sel].name, blend(c, C_BG, nk));
  }

  // ======== Settings ========
  void toggle(Canvas& g, int x, int y, bool on) {
    rrect(g, x, y, 36, 18, 9, on ? C_TEAL : C_LINE);
    g.fillCircle(on ? x + 27 : x + 9, y + 9, 7, C_WHITE);
  }
  void drawSettings(Canvas& g, uint32_t now) {
    g.fillScreen(C_BG);
    const char* names[4] = {"Flip screen", "Show FPS", "Keyboard", "Reset scores"};
    int ks = plat_kbState();
    bool armed = resetArmAt && now - resetArmAt < 3000;
    for (int i = 0; i < 4; i++) {
      int y = 46 + i * 42;
      bool s = i == setSel;
      rrect(g, 10, y, 300, 36, 10, s ? C_WHITE : C_CARD);
      uint16_t tc = s ? C_BG : C_WHITE, sc = s ? rgb(90, 90, 96) : C_DIM;
      text(g, F_BOLD, 22, y + 17, names[i], tc);
      const char* sub = "";
      if (i == 0) sub = "USE IF THE PICTURE IS UPSIDE DOWN";
      if (i == 1) sub = showFps ? "ON" : "OFF";
      if (i == 2) sub = ks == 3 ? "CARDKB2 CONNECTED" : ks == 2 ? "PAIRED" : ks == 1 ? "CONNECTING..." : "SEARCHING...";
      if (i == 3) sub = resetDoneAt && now - resetDoneAt < 2000 ? "SCORES CLEARED" : (armed ? "PRESS AGAIN TO CONFIRM" : "CLEARS ALL BEST SCORES");
      text(g, F_SMALL, 22, y + 23, sub, (i == 3 && armed) ? C_RED : sc);
      if (i == 0) toggle(g, 262, y + 9, flip);
      if (i == 1) toggle(g, 262, y + 9, showFps);
      if (i == 2) g.fillCircle(280, y + 18, 5, ks == 3 ? C_GREEN : C_DIM);
    }
    drawHeader(g, now, "Settings");
    drawFooter(g, "", "ENTER select   ESC back");
  }


  // ======== Notes ========
  // 6 note slots, up to 1000 characters each, stored in flash (Preferences "note0".."note5").
  static void noteKey(int i, char* out) { snprintf(out, 8, "note%d", i); }
  void refreshPreviews() {
    for (int i = 0; i < NOTE_N; i++) {
      char k[8]; noteKey(i, k);
      int n = plat_loadStr(k, nbuf, sizeof(nbuf));
      nplen[i] = n;
      int j = 0;
      while (j < n && j < 34 && nbuf[j] != '\n') { nprev[i][j] = nbuf[j]; j++; }
      nprev[i][j] = 0;
      if (j == 34 && n > 34) { nprev[i][31] = nprev[i][32] = nprev[i][33] = '.'; }
    }
  }
  void openNotesList(uint32_t now) {
    refreshPreviews(); nclearArm = 0; nclearSlot = -1;
    if (inp) inp->clearKeys();
    go(NOTES, now);
  }
  void updateNotesList(Input& in, uint32_t now) {
    if (in.rep[B_UP])   nsel = (nsel + NOTE_N - 1) % NOTE_N;
    if (in.rep[B_DOWN]) nsel = (nsel + 1) % NOTE_N;
    uint16_t k; bool clearKey = false;
    while (in.popKey(k)) if (k == '-' || k == K_DEL) clearKey = true;
    if (clearKey && nplen[nsel] > 0) {
      if (nclearSlot == nsel && now - nclearArm < 3000) {
        char key[8]; noteKey(nsel, key); plat_saveStr(key, "");
        refreshPreviews(); nclearSlot = -1;
      } else { nclearSlot = nsel; nclearArm = now; }
    }
    if (in.pressed[B_B]) { openMenu(now); return; }
    if (in.pressed[B_A]) openEditor(nsel, now);
  }
  void openEditor(int slot, uint32_t now) {
    nslot = slot;
    char k[8]; noteKey(slot, k);
    nlen = plat_loadStr(k, nbuf, sizeof(nbuf));
    ncur = nlen; ntop = 0; nprefCol = -1; ndirty = false; nsaveFail = false; nsavedAt = 0; nlastEdit = now;
    layoutNote(); ensureVisible();
    if (inp) inp->clearKeys();   // drop the ENTER that opened the note
    go(NOTE_EDIT, now);
  }
  bool saveNote(uint32_t now) {
    char k[8]; noteKey(nslot, k);
    nbuf[nlen] = 0;
    bool ok = plat_saveStr(k, nbuf);
    nsaveFail = !ok; if (ok) { ndirty = false; nsavedAt = now; }
    return ok;
  }
  // Word-wrap the note into screen lines (NCOLS characters wide)
  void layoutNote() {
    nlines = 0; int pos = 0;
    for (;;) {
      int start = pos, j = start, col = 0, lastSpace = -1;
      while (j < nlen && nbuf[j] != '\n' && col < NCOLS) { if (nbuf[j] == ' ') lastSpace = j; j++; col++; }
      if (j >= nlen) { nls[nlines] = start; nll[nlines] = nlen - start; nlines++; break; }
      if (nbuf[j] == '\n') { nls[nlines] = start; nll[nlines] = j - start; nlines++; pos = j + 1; continue; }
      // line is full
      if (nbuf[j] == ' ') { nls[nlines] = start; nll[nlines] = j - start + 1; nlines++; pos = j + 1; }
      else if (lastSpace >= start) { nls[nlines] = start; nll[nlines] = lastSpace - start + 1; nlines++; pos = lastSpace + 1; }
      else { nls[nlines] = start; nll[nlines] = j - start; nlines++; pos = j; }
      if (nlines >= NOTE_MAX + 1) break;
    }
  }
  int lineOf(int idx) { int k = 0; for (int i = 0; i < nlines; i++) if (nls[i] <= idx) k = i; else break; return k; }
  // last cursor position that still belongs to line k
  int lineEnd(int k) {
    int e = nls[k] + nll[k];
    if (k == nlines - 1 || (e < nlen && nbuf[e] == '\n')) return e;
    return e - 1;   // wrapped line: its end is the start of the next one
  }
  void ensureVisible() {
    int l = lineOf(ncur);
    if (l < ntop) ntop = l;
    if (l >= ntop + NROWS) ntop = l - NROWS + 1;
  }
  void edited(uint32_t now) { ndirty = true; nlastEdit = now; nprefCol = -1; layoutNote(); }
  void updateNoteEdit(Input& in, uint32_t now) {
    uint16_t k;
    while (in.popKey(k)) {
      if (k == K_ESC) { saveNote(now); refreshPreviews(); in.clearKeys(); go(NOTES, now); return; }
      if (k == K_LEFT)  { if (ncur > 0) ncur--; nprefCol = -1; }
      else if (k == K_RIGHT) { if (ncur < nlen) ncur++; nprefCol = -1; }
      else if (k == K_HOME) { ncur = nls[lineOf(ncur)]; nprefCol = -1; }
      else if (k == K_END)  { ncur = lineEnd(lineOf(ncur)); nprefCol = -1; }
      else if (k == K_UP || k == K_DOWN) {
        int l = lineOf(ncur);
        if (nprefCol < 0) nprefCol = ncur - nls[l];
        int t = l + (k == K_UP ? -1 : 1);
        if (t < 0) ncur = 0;
        else if (t >= nlines) ncur = nlen;
        else ncur = imin(nls[t] + nprefCol, lineEnd(t));
      }
      else if (k == K_BKSP) { if (ncur > 0) { memmove(nbuf + ncur - 1, nbuf + ncur, nlen - ncur); nlen--; ncur--; edited(now); } }
      else if (k == K_DEL)  { if (ncur < nlen) { memmove(nbuf + ncur, nbuf + ncur + 1, nlen - ncur - 1); nlen--; edited(now); } }
      else {
        char c = k == K_ENTER ? '\n' : k == K_TAB ? ' ' : (k >= 32 && k < 127 ? (char)k : 0);
        if (c && nlen < NOTE_MAX) { memmove(nbuf + ncur + 1, nbuf + ncur, nlen - ncur); nbuf[ncur] = c; nlen++; ncur++; edited(now); }
      }
      nbuf[nlen] = 0;
      ensureVisible();
    }
    if (ndirty && now - nlastEdit > 3000) saveNote(now);   // auto-save after 3 s of no typing
  }
  void drawNotesList(Canvas& g, uint32_t now) {
    g.fillScreen(C_BG);
    for (int i = 0; i < NOTE_N; i++) {
      int y = 46 + i * 28;
      bool s = i == nsel;
      rrect(g, 10, y, 300, 24, 8, s ? C_WHITE : C_CARD);
      char num[4]; snprintf(num, sizeof(num), "%d", i + 1);
      text(g, F_BOLD, 20, y + 17, num, s ? C_BG : C_NOTE);
      if (nplen[i] == 0) text(g, F_SMALL, 40, y + 8, "Empty note", s ? rgb(90, 90, 96) : C_DIM);
      else {
        text(g, F_SMALL, 40, y + 8, nprev[i][0] ? nprev[i] : "(blank first line)", s ? C_BG : C_WHITE);
        char cnt[12]; snprintf(cnt, sizeof(cnt), "%d CH", nplen[i]);
        textR(g, F_SMALL, 302, y + 8, cnt, s ? rgb(90, 90, 96) : C_DIM);
      }
    }
    drawHeader(g, now, "Notes");
    bool armed = nclearSlot == nsel && now - nclearArm < 3000;
    drawFooter(g, armed ? "" : "ENTER open   - clear", "ESC back");
    if (armed) text(g, F_SMALL, 12, 224, "PRESS - AGAIN TO CLEAR", C_RED);
  }
  void drawNoteEdit(Canvas& g, uint32_t now) {
    g.fillScreen(C_BG);
    // top bar
    g.fillCircle(10, 13, 4, C_NOTE);
    char title[20]; snprintf(title, sizeof(title), "Note %d", nslot + 1);
    text(g, F_BOLD, 20, 19, title, C_WHITE);
    char cnt[16]; snprintf(cnt, sizeof(cnt), "%d/%d", nlen, NOTE_MAX);
    textR(g, F_SMALL, SW - 8, 10, cnt, nlen >= NOTE_MAX ? C_RED : C_DIM);
    const char* status = nsaveFail ? "SAVE FAILED" : ndirty ? "EDITING" : (nsavedAt && now - nsavedAt < 1500) ? "SAVED" : "";
    text(g, F_SMALL, 100, 10, status, nsaveFail ? C_RED : ndirty ? C_YELLOW : C_GREEN);
    g.drawFastHLine(0, 26, SW, C_LINE);
    // text
    char line[NCOLS + 2];
    for (int r = 0; r < NROWS; r++) {
      int l = ntop + r;
      if (l >= nlines) break;
      int n = imin(nll[l], NCOLS + 1), m = 0;
      for (int i = 0; i < n; i++) { char c = nbuf[nls[l] + i]; if (c != '\n') line[m++] = c; }
      line[m] = 0;
      text(g, F_SMALL, NTX, NTY + r * NLH, line, C_WHITE);
    }
    if (nlen == 0) text(g, F_SMALL, NTX + 8, NTY, "Start typing...", C_DIM);
    // cursor
    int cl = lineOf(ncur), cc = ncur - nls[cl];
    if (cl >= ntop && cl < ntop + NROWS && ((now - nlastEdit) < 600 || (now / 450) % 2))
      g.fillRect(NTX + cc * 6 - 1, NTY + (cl - ntop) * NLH - 1, 2, 10, C_TEAL);
    // scroll bar
    if (nlines > NROWS) {
      int th = NROWS * NLH, h = imax(10, th * NROWS / nlines);
      int y = NTY + (th - h) * ntop / (nlines - NROWS);
      g.fillRect(SW - 4, NTY, 2, th, C_CARD); g.fillRect(SW - 4, y, 2, h, C_SOFT);
    }
    drawFooter(g, "ESC save & back", "ENTER new line");
  }

  // ======== About ========
  void drawAbout(Canvas& g, uint32_t now) {
    g.fillScreen(C_BG);
    Mascot m; m.cx = 66; m.cy = 96; m.s = 0.34f;
    m.eyeOpen = 1 - bump(seg(now % 3500, 3200, 3380));
    m.lift = 4 * (0.5f + 0.5f * sinf(now * 0.004f));
    m.look = sinf(now * 0.0015f);
    drawMascot(g, m);
    textC(g, F_BOLD, 218, 72, "Altoids Gameboy", C_WHITE);
    textC(g, F_SMALL, 218, 82, "ARCADE OS  v1.6", C_TEAL);
    const char* lines[] = {"ESP32-S3 N16R8", "ST7789 320x240 display", "CardKB2 over Bluetooth LE", "700mAh LiPo + MT3608 5V", "15 games + Notes"};
    for (int i = 0; i < 5; i++) textC(g, F_SMALL, 218, 106 + i * 16, lines[i], C_SOFT);
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
int  plat_loadStr(const char* key, char* buf, int maxLen) {
  buf[0] = 0;
  if (!prefs.isKey(key)) return 0;
  size_t n = prefs.getString(key, buf, maxLen);   // fails (returns 0) if the stored text is too long
  if (n == 0) { buf[0] = 0; return 0; }
  buf[maxLen - 1] = 0;
  return (int)strlen(buf);
}
bool plat_saveStr(const char* key, const char* s) {
  if (!s[0]) return prefs.isKey(key) ? prefs.remove(key) : true;
  return prefs.putString(key, s) == strlen(s);
}

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
