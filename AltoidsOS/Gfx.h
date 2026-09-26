#pragma once
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
