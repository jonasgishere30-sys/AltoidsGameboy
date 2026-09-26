#pragma once
// Boot animation -> playful transition -> menu -> games / settings / about
#include "Gfx.h"
#include "Input.h"
#include "Platform.h"
#include "GameSnake.h"
#include "GameBlocks.h"
#include "GamePong.h"
#include "GameBreakout.h"
#include "GameFlappy.h"
#include "Game2048.h"

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
    flip = plat_loadInt("flip", 0);
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
      if (setSel == 0) { flip = !flip; plat_saveInt("flip", flip); plat_setFlip(flip); }
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
    textC(g, F_SMALL, 218, 82, "ARCADE OS  v1.1", C_TEAL);
    const char* lines[] = {"ESP32-S3 N16R8", "ST7789 320x240 display", "CardKB2 over Bluetooth LE", "700mAh LiPo + MT3608 5V"};
    for (int i = 0; i < 4; i++) textC(g, F_SMALL, 218, 106 + i * 16, lines[i], C_SOFT);
    drawFooter(g, "", "ESC back");
  }
};
