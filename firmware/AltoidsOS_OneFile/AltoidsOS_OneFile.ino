// Altoids Gameboy - Arcade OS v1.2 (landscape) - SINGLE FILE
// Paste this whole file into any sketch (any folder name). No other files needed.
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
// Controls: Arrows or W A S D = move,  SPACE / ENTER = select / action,
//           ESC / BACKSPACE = back / pause,  P = pause

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

// ---- Types used by the drawing functions (kept above every function so the
//      sketch also compiles as one single .ino file) ----
enum Font { F_SMALL, F_REG, F_BOLD, F_BIG, F_HUGE };
enum Icon { IC_SNAKE, IC_BLOCKS, IC_PONG, IC_BREAKOUT, IC_FLAPPY, IC_2048, IC_SETTINGS, IC_ABOUT };
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
// Arrows or WASD = direction, SPACE/ENTER = A, ESC/BACKSPACE = B, P = pause
#include <stdint.h>
#include <string.h>

enum Btn : uint8_t { B_UP, B_DOWN, B_LEFT, B_RIGHT, B_A, B_B, B_PAUSE, B_COUNT };

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
      case 0x52: case 0x1A: return B_UP;      // Up arrow, W
      case 0x51: case 0x16: return B_DOWN;    // Down arrow, S
      case 0x50: case 0x04: return B_LEFT;    // Left arrow, A
      case 0x4F: case 0x07: return B_RIGHT;   // Right arrow, D
      case 0x2C: case 0x28: return B_A;       // Space, Enter
      case 0x29: case 0x2A: return B_B;       // Esc, Backspace
      case 0x13:            return B_PAUSE;   // P
    }
    return -1;
  }

  // Called with each 8-byte keyboard report (keys = the 6 keycode bytes)
  void onHid(const uint8_t keys[6]) {
    bool now[B_COUNT] = {};
    for (int i = 0; i < 6; i++) {
      int b = map(keys[i]);
      if (b < 0) continue;
      now[b] = true;
      bool was = false;
      for (int j = 0; j < 6; j++) if (prevKeys[j] == keys[i]) was = true;
      if (!was) pending[b] = true;
    }
    memcpy(rawHeld, now, sizeof(now));
    memcpy(prevKeys, keys, 6);
  }

  void releaseAll() { memset(rawHeld, 0, sizeof(rawHeld)); memset(prevKeys, 0, 6); }

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


struct Game2048 : Game {
  static const int TS = 48, GAPP = 4, GX = 12, GY = 31;
  uint16_t t[4][4]; uint32_t popAt[4][4]; bool won, keepGoing;
  const char* saveKey() override { return "2048"; }

  void addTile(uint32_t now) {
    int free[16], n = 0;
    for (int i = 0; i < 16; i++) if (!t[i / 4][i % 4]) free[n++] = i;
    if (!n) return;
    int k = free[plat_random(n)];
    t[k / 4][k % 4] = plat_random(10) == 0 ? 4 : 2; popAt[k / 4][k % 4] = now;
  }
  void begin() override {
    loadBest(); score = 0; phase = PLAY; won = keepGoing = false;
    memset(t, 0, sizeof(t)); memset(popAt, 0, sizeof(popAt));
    addTile(0); addTile(0);
  }
  // slide one line of 4 toward index 0
  bool slideLine(uint16_t* v[4], uint32_t now, int idx[4][2]) {
    uint16_t out[4] = {0, 0, 0, 0}; int n = 0; bool moved = false, merged = false;
    int last = -1;
    for (int i = 0; i < 4; i++) {
      if (!*v[i]) continue;
      if (last >= 0 && out[last] == *v[i] && !merged) { out[last] *= 2; score += out[last]; merged = true; popAt[idx[last][0]][idx[last][1]] = now; if (out[last] == 2048) won = true; }
      else { out[n] = *v[i]; last = n; n++; merged = false; }
    }
    for (int i = 0; i < 4; i++) { if (*v[i] != out[i]) moved = true; *v[i] = out[i]; }
    return moved;
  }
  bool move(int dir, uint32_t now) {   // 0 left 1 right 2 up 3 down
    bool moved = false;
    for (int line = 0; line < 4; line++) {
      uint16_t* v[4]; int idx[4][2];
      for (int i = 0; i < 4; i++) {
        int r, c;
        if (dir == 0) { r = line; c = i; } else if (dir == 1) { r = line; c = 3 - i; }
        else if (dir == 2) { r = i; c = line; } else { r = 3 - i; c = line; }
        v[i] = &t[r][c]; idx[i][0] = r; idx[i][1] = c;
      }
      if (slideLine(v, now, idx)) moved = true;
    }
    return moved;
  }
  bool canMove() {
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
      if (!t[r][c]) return true;
      if (c < 3 && t[r][c] == t[r][c + 1]) return true;
      if (r < 3 && t[r][c] == t[r + 1][c]) return true;
    }
    return false;
  }
  bool update(Input& in, uint32_t now, uint32_t) override {
    if (won && !keepGoing && phase == PLAY) {
      if (in.pressed[B_A]) keepGoing = true;
      else if (in.pressed[B_B]) { gameOver(now); }
      return true;
    }
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    int dir = in.pressed[B_LEFT] ? 0 : in.pressed[B_RIGHT] ? 1 : in.pressed[B_UP] ? 2 : in.pressed[B_DOWN] ? 3 : -1;
    if (dir >= 0 && move(dir, now)) { addTile(now); if (!canMove()) gameOver(now); }
    return true;
  }
  static uint16_t tileColor(int v) {
    switch (v) {
      case 2: return rgb(236, 236, 236); case 4: return rgb(200, 200, 205);
      case 8: return rgb(253, 186, 116); case 16: return C_ORANGE;
      case 32: return C_RED; case 64: return rgb(239, 68, 68);
      case 128: return rgb(253, 230, 138); case 256: return C_YELLOW;
      case 512: return rgb(163, 230, 53); case 1024: return rgb(34, 211, 238);
      case 2048: return C_PURPLE; default: return C_PINK;
    }
  }
  void draw(Canvas& g, uint32_t now) override {
    g.fillScreen(C_BG);
    drawHud(g, "2048", C_PINK);
    rrect(g, GX - 2, GY - 2, 4 * TS + 5 * GAPP + 4 - 2, 4 * TS + 5 * GAPP + 4 - 2, 10, C_CARD);
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
      int x = GX + GAPP + c * (TS + GAPP), y = GY + GAPP + r * (TS + GAPP);
      int v = t[r][c];
      if (!v) { rrect(g, x, y, TS, TS, 8, rgb(30, 30, 34)); continue; }
      float p = popAt[r][c] ? easeOutBack(seg(now, popAt[r][c], popAt[r][c] + 160)) : 1;
      float sz = TS * (0.6f + 0.4f * p);
      rrect(g, x + (TS - sz) / 2, y + (TS - sz) / 2, sz, sz, 8, tileColor(v));
      char buf[8]; snprintf(buf, sizeof(buf), "%d", v);
      uint16_t tc = v <= 4 ? rgb(40, 40, 44) : C_BG;
      textC(g, v >= 1000 ? F_SMALL : (v >= 100 ? F_BOLD : F_BIG), x + TS / 2, y + TS / 2 + (v >= 1000 ? -3 : (v >= 100 ? 6 : 8)), buf, tc);
    }
    textC(g, F_SMALL, 272, 90, "ARROWS", C_DIM);
    textC(g, F_SMALL, 272, 102, "or WASD", C_DIM);
    textC(g, F_SMALL, 272, 114, "to slide", C_DIM);
    textC(g, F_SMALL, 272, 150, "ESC pause", C_DIM);
    if (won && !keepGoing && phase == PLAY) {
      dimScreen(g);
      rrect(g, SW / 2 - 90, 75, 180, 90, 14, C_CARD);
      textC(g, F_BIG, SW / 2, 111, "2048!", C_PURPLE);
      textC(g, F_SMALL, SW / 2, 131, "ENTER keep going", C_SOFT);
      textC(g, F_SMALL, SW / 2, 145, "ESC   finish", C_SOFT);
    }
    drawOverlays(g, now, C_PINK);
  }
};


struct MenuItem { const char* name; const char* key; uint16_t color; Icon icon; };
static const MenuItem MENU[] = {
  {"Snake",    "snake",    C_GREEN,  IC_SNAKE},
  {"Blocks",   "blocks",   C_PURPLE, IC_BLOCKS},
  {"Pong",     "pong",     C_TEAL,   IC_PONG},
  {"Breakout", "breakout", C_ORANGE, IC_BREAKOUT},
  {"Flappy",   "flappy",   C_YELLOW, IC_FLAPPY},
  {"2048",     "2048",     C_PINK,   IC_2048},
  {"Settings", nullptr,    C_SOFT,   IC_SETTINGS},
  {"About",    nullptr,    C_SOFT,   IC_ABOUT},
};
static const int N_MENU = sizeof(MENU) / sizeof(MENU[0]);
static const int N_GAMES = 6;

// Menu grid geometry (4 x 2 tiles, landscape)
static const int TILE_W = 72, TILE_H = 78, TILE_X0 = 7, TILE_GAP = 6, TILE_Y0 = 48;
static inline int tileX(int i) { return TILE_X0 + (i % 4) * (TILE_W + TILE_GAP); }
static inline int tileY(int i) { return TILE_Y0 + (i / 4) * (TILE_H + TILE_GAP); }

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
    case IC_2048:
      text(g, F_SMALL, x + 5, y + 13, "2048", k); break;
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
  enum State { BOOT, INTRO, MENU_S, LAUNCH, GAME, SETTINGS, ABOUT } st = BOOT;
  uint32_t t0 = 0;
  int sel = 0;
  float hx = TILE_X0, hy = TILE_Y0;   // gliding highlight position
  uint32_t menuAt = 0;
  int bests[N_GAMES] = {};
  Game* games[N_GAMES];
  Game* cur = nullptr;
  bool showFps = false, flip = false;
  int setSel = 0; uint32_t resetArmAt = 0, resetDoneAt = 0;
  uint32_t lastT = 0;
  static const uint32_t BOOT_LEN = 4150;

  SnakeGame snake; BlocksGame blocks; PongGame pong; BreakoutGame breakout; FlappyGame flappy; Game2048 g2048;

  void begin() {
    games[0] = &snake; games[1] = &blocks; games[2] = &pong;
    games[3] = &breakout; games[4] = &flappy; games[5] = &g2048;
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
    uint32_t dt = lastT ? now - lastT : 16; if (dt > 50) dt = 50; lastT = now;
    uint32_t t = now - t0;
    switch (st) {
      case BOOT:  if (t > BOOT_LEN || (t > 400 && in.anyPressed)) go(INTRO, now); break;
      case INTRO: if (t > 1100) go(MENU_S, now); break;
      case MENU_S: updateMenu(in, now); break;
      case LAUNCH: if (t > 480) { cur = games[sel]; cur->begin(); go(GAME, now); } break;
      case GAME:  if (!cur->update(in, now, dt)) { loadBests(); openMenu(now); } break;
      case SETTINGS: updateSettings(in, now); break;
      case ABOUT: if (in.pressed[B_B] || in.pressed[B_A]) openMenu(now); break;
    }
    float k = 1 - powf(0.0005f, dt / 1000.0f * 1.6f);
    hx = lerpf(hx, tileX(sel), k);
    hy = lerpf(hy, tileY(sel), k);
  }
  void updateMenu(Input& in, uint32_t now) {
    if (in.rep[B_LEFT])  sel = (sel + N_MENU - 1) % N_MENU;
    if (in.rep[B_RIGHT]) sel = (sel + 1) % N_MENU;
    if (in.rep[B_UP] || in.rep[B_DOWN]) sel = (sel + 4) % N_MENU;
    if (in.pressed[B_A]) {
      if (sel < N_GAMES) go(LAUNCH, now);
      else if (sel == N_GAMES) { setSel = 0; resetArmAt = resetDoneAt = 0; go(SETTINGS, now); }
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
    char left[32];
    if (sel < N_GAMES) { if (bests[sel]) snprintf(left, sizeof(left), "%s   BEST %d", MENU[sel].name, bests[sel]); else snprintf(left, sizeof(left), "%s   NEW", MENU[sel].name); }
    else snprintf(left, sizeof(left), "%s", MENU[sel].name);
    drawFooter(g, left, "ARROWS move   ENTER open");
  }
  void drawLaunch(Canvas& g, uint32_t t, uint32_t now) {
    drawMenu(g, now, true);
    float k = easeInOutCubic(seg(t, 0, 320));
    uint16_t c = blend(MENU[sel].color, C_BG, seg(t, 320, 480));
    rrect(g, lerpf(hx, 0, k), lerpf(hy, 0, k), lerpf(TILE_W, SW, k), lerpf(TILE_H, SH, k), lerpf(12, 0, k), c);
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

  // ======== About ========
  void drawAbout(Canvas& g, uint32_t now) {
    g.fillScreen(C_BG);
    Mascot m; m.cx = 66; m.cy = 96; m.s = 0.34f;
    m.eyeOpen = 1 - bump(seg(now % 3500, 3200, 3380));
    m.lift = 4 * (0.5f + 0.5f * sinf(now * 0.004f));
    m.look = sinf(now * 0.0015f);
    drawMascot(g, m);
    textC(g, F_BOLD, 218, 72, "Altoids Gameboy", C_WHITE);
    textC(g, F_SMALL, 218, 82, "ARCADE OS  v1.2", C_TEAL);
    const char* lines[] = {"ESP32-S3 N16R8", "ST7789 320x240 display", "CardKB2 over Bluetooth LE", "700mAh LiPo + MT3608 5V"};
    for (int i = 0; i < 4; i++) textC(g, F_SMALL, 218, 106 + i * 16, lines[i], C_SOFT);
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
    if (len == 8) input.onHid(d + 2);
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
