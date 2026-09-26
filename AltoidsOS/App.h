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

enum Icon { IC_SNAKE, IC_BLOCKS, IC_PONG, IC_BREAKOUT, IC_FLAPPY, IC_2048, IC_SETTINGS, IC_ABOUT };
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

// Menu geometry
static const int LIST_TOP = 58, LIST_BOT = 294, PITCH = 56, CARD_H = 48, CARD_X = 10, CARD_W = 220;

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
  uint32_t t0 = 0;          // start time of the current state
  int sel = 0;              // selected menu item
  float scroll = 0, hl = 0; // smooth scroll & highlight position (list coords)
  uint32_t menuAt = 0;      // when the menu last appeared (stagger animation)
  int bests[N_GAMES] = {};
  Game* games[N_GAMES];
  Game* cur = nullptr;
  bool showFps = false;
  int setSel = 0; uint32_t resetArmAt = 0, resetDoneAt = 0;
  uint32_t lastT = 0;

  SnakeGame snake; BlocksGame blocks; PongGame pong; BreakoutGame breakout; FlappyGame flappy; Game2048 g2048;

  void begin() {
    games[0] = &snake; games[1] = &blocks; games[2] = &pong;
    games[3] = &breakout; games[4] = &flappy; games[5] = &g2048;
    showFps = plat_loadInt("fps", 0);
    loadBests();
    t0 = plat_millis();
  }
  void loadBests() { for (int i = 0; i < N_GAMES; i++) bests[i] = plat_loadInt(MENU[i].key, 0); }
  void go(State s, uint32_t now) { st = s; t0 = now; if (s == INTRO) menuAt = now + 700; }

  // ---------------- update ----------------
  void update(Input& in, uint32_t now) {
    uint32_t dt = lastT ? now - lastT : 16; if (dt > 50) dt = 50; lastT = now;
    uint32_t t = now - t0;
    switch (st) {
      case BOOT:  if (t > 3600 || (t > 400 && in.anyPressed)) go(INTRO, now); break;
      case INTRO: if (t > 1100) { go(MENU_S, now); } break;
      case MENU_S: updateMenu(in, now); break;
      case LAUNCH:
        if (t > 480) { cur = games[sel]; cur->begin(); go(GAME, now); }
        break;
      case GAME:
        if (!cur->update(in, now, dt)) { loadBests(); openMenu(now); }
        break;
      case SETTINGS: updateSettings(in, now); break;
      case ABOUT: if (in.pressed[B_B] || in.pressed[B_A]) openMenu(now); break;
    }
    // smooth scroll & highlight (frame-rate independent-ish)
    float k = 1 - powf(0.0005f, dt / 1000.0f * 1.6f);
    hl = lerpf(hl, sel * PITCH, k);
    float selTop = sel * PITCH, target = scroll;
    if (selTop - target < 0) target = selTop;
    if (selTop + CARD_H - target > LIST_BOT - LIST_TOP) target = selTop + CARD_H - (LIST_BOT - LIST_TOP);
    scroll = lerpf(scroll, target, k);
  }
  void openMenu(uint32_t now) { go(MENU_S, now); menuAt = now; }

  void updateMenu(Input& in, uint32_t now) {
    if (in.rep[B_UP])   sel = (sel + N_MENU - 1) % N_MENU;
    if (in.rep[B_DOWN]) sel = (sel + 1) % N_MENU;
    if (in.pressed[B_A] || in.pressed[B_RIGHT]) {
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
      if (setSel == 0) { showFps = !showFps; plat_saveInt("fps", showFps); }
      else if (setSel == 2) {
        if (resetArmAt && now - resetArmAt < 3000) {
          for (int i = 0; i < N_GAMES; i++) plat_saveInt(MENU[i].key, 0);
          loadBests(); resetArmAt = 0; resetDoneAt = now;
        } else resetArmAt = now;
      } else if (setSel == 3) openMenu(now);
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

  // Mascot pose for boot time t (ms). Shared by boot and the start of the intro.
  Mascot bootPose(uint32_t t) {
    Mascot m; m.cx = 120; m.cy = 118; m.s = 0.5f;
    m.barW = easeOutCubic(seg(t, 0, 380));
    m.dropY = t < 250 ? -400 : lerpf(-190, 0, easeInCubic(seg(t, 250, 640)));
    m.squash = 0.9f * bump(seg(t, 640, 800)) - 0.35f * bump(seg(t, 800, 1080)) + 0.35f * bump(seg(t, 1080, 1220));
    m.lift = 16 * bump(seg(t, 800, 1080));
    m.eyeScale = easeOutBack(seg(t, 1180, 1420));
    m.look = easeInOutCubic(seg(t, 1550, 1750)) - 2 * easeInOutCubic(seg(t, 1950, 2200)) + easeInOutCubic(seg(t, 2400, 2560));
    m.eyeOpen = 1 - bump(seg(t, 2650, 2800));
    float hop = seg(t, 2950, 3300);
    if (hop > 0 && hop < 1) { m.happy = 1; m.lift += 20 * bump(hop); m.squash += -0.3f * bump(hop); }
    return m;
  }
  void drawBootText(Canvas& g, uint32_t t, float fade) {
    const char* word = "ALTOIDS";
    int w = textW(g, F_HUGE, word);
    int n = (int)ceilf(7 * seg(t, 1400, 1950));
    char buf[8]; memcpy(buf, word, n); buf[n] = 0;
    // typed from a fixed left edge so letters do not jump around
    setFont(g, F_HUGE); int16_t x1, y1; uint16_t tw, th; g.getTextBounds(word, 0, 240, &x1, &y1, &tw, &th);
    if (n > 0) text(g, F_HUGE, 120 - w / 2 - x1, 240, buf, blend(C_WHITE, C_BG, fade));
    if (n > 0 && n < 7 && (t / 120) % 2) g.fillRect(120 - w / 2 + textW(g, F_HUGE, buf) + 3, 214, 3, 28, C_TEAL);
    float gb = easeOutCubic(seg(t, 1980, 2380));
    if (gb > 0) textC(g, F_BIG, 120, ir(278 - 10 * gb), "GAMEBOY", blend(C_BG, C_TEAL, gb * (1 - fade)));
    float tg = seg(t, 2400, 2800);
    if (tg > 0) textC(g, F_SMALL, 120, 306, "ESP32-S3  ARCADE", blend(C_BG, C_DIM, tg * (1 - fade)));
  }
  void drawBoot(Canvas& g, uint32_t t) {
    g.fillScreen(C_BG);
    drawMascot(g, bootPose(t));
    drawBootText(g, t, 0);
  }

  // Intro: eyes close, the screen outline grows to fill the display, flashes white,
  // then the white shrinks down into the menu's selection highlight.
  void drawIntro(Canvas& g, uint32_t t, uint32_t now) {
    if (t < 700) {
      g.fillScreen(C_BG);
      Mascot m = bootPose(3600);
      float close = seg(t, 0, 180);
      m.eyeOpen = 1 - close;
      m.eyeScale = 1 - seg(t, 180, 300);
      float grow = easeInOutCubic(seg(t, 150, 560));
      float fill = easeInCubic(seg(t, 520, 700));
      // bar drops away
      float bd = easeInCubic(seg(t, 100, 400));
      if (bd < 1) {
        float bw = 151, bh = 13, by = 118 + 48 + 14.5f + bd * 150;
        rrect(g, 120 - bw / 2, by, bw, bh, bh / 2, C_WHITE);
      }
      float L = lerpf(58, -2, grow), T = lerpf(70, -2, grow), W = lerpf(124, 244, grow), H = lerpf(96, 324, grow);
      float R = lerpf(23, 18, grow), st = lerpf(14, 8, grow);
      st = lerpf(st, 170, fill);
      rrect(g, L, T, W, H, R, C_WHITE);
      if (W - 2 * st > 1 && H - 2 * st > 1) rrect(g, L + st, T + st, W - 2 * st, H - 2 * st, fmaxf(R - st, 2), C_BG);
      // eyes ride along with the box until they close
      if (m.eyeScale > 0.02f) {
        m.showBar = false; m.body = C_WHITE;
        float eh = fmaxf(28 * m.eyeOpen, 2) * m.eyeScale;
        for (int i = -1; i <= 1; i += 2) rrect(g, L + W / 2 + i * 13 - 6, T + H / 2 - 2 - eh / 2, 12, eh, 6, C_TEAL);
      }
      if (t < 200) drawBootText(g, 3600, seg(t, 0, 200));
    } else {
      drawMenu(g, now, false);
      float k = easeInOutCubic(seg(t, 700, 1100));
      float y = LIST_TOP + hl - scroll;
      rrect(g, lerpf(0, CARD_X, k), lerpf(0, y, k), lerpf(SW, CARD_W, k), lerpf(SH, CARD_H, k), lerpf(0, 12, k), C_WHITE);
      if (k >= 1) drawCardContent(g, 0, CARD_X, ir(y), true);
    }
  }

  void drawCardContent(Canvas& g, int i, int x, int y, bool selected) {
    const MenuItem& it = MENU[i];
    drawIcon(g, it.icon, x + 8, y + 7, it.color);
    text(g, F_BOLD, x + 54, y + 22, it.name, selected ? C_BG : C_WHITE);
    char buf[24];
    if (i < N_GAMES) { if (bests[i]) snprintf(buf, sizeof(buf), "BEST  %d", bests[i]); else snprintf(buf, sizeof(buf), "NEW"); }
    else snprintf(buf, sizeof(buf), i == N_GAMES ? "OPTIONS" : "INFO");
    text(g, F_SMALL, x + 54, y + 30, buf, selected ? rgb(90, 90, 96) : C_DIM);
    if (selected) g.fillTriangle(x + CARD_W - 20, y + 18, x + CARD_W - 13, y + 24, x + CARD_W - 20, y + 30, C_BG);
  }

  void drawHeader(Canvas& g, uint32_t now, const char* title) {
    g.fillRect(0, 0, SW, LIST_TOP - 4, C_BG);
    Mascot m; m.cx = 24; m.cy = 22; m.s = 0.11f;
    uint32_t cyc = now % 4200;
    m.eyeOpen = 1 - bump(seg(cyc, 3900, 4100));
    m.look = sinf(now * 0.0011f) > 0.6f ? 1 : (sinf(now * 0.0011f) < -0.6f ? -1 : 0);
    drawMascot(g, m);
    text(g, F_BIG, 48, 32, title, C_WHITE);
    // keyboard status pill
    int ks = plat_kbState();
    uint16_t dc = ks == 3 ? C_GREEN : (ks == 0 ? C_DIM : ((now / 300) % 2 ? C_YELLOW : C_BG));
    rrect(g, 184, 13, 48, 20, 10, C_CARD);
    g.fillCircle(196, 23, 4, dc);
    text(g, F_SMALL, 206, 19, "KB", ks == 3 ? C_WHITE : C_DIM);
    g.drawFastHLine(10, LIST_TOP - 6, 220, C_LINE);
  }
  void drawFooter(Canvas& g, const char* hint) {
    g.fillRect(0, LIST_BOT + 2, SW, SH - LIST_BOT - 2, C_BG);
    textC(g, F_SMALL, 120, 305, hint, C_DIM);
  }

  void drawMenu(Canvas& g, uint32_t now, bool highlight) {
    g.fillScreen(C_BG);
    for (int i = 0; i < N_MENU; i++) {
      float in = easeOutCubic(seg(now, menuAt + i * 45, menuAt + i * 45 + 320));
      int x = CARD_X + ir((1 - in) * 240);
      int y = LIST_TOP + i * PITCH - ir(scroll);
      if (y > LIST_BOT || y + CARD_H < LIST_TOP - 10) continue;
      bool isSel = highlight && i == sel;
      if (!isSel) rrect(g, x, y, CARD_W, CARD_H, 12, C_CARD);
      if (isSel) {
        int hy = LIST_TOP + ir(hl - scroll);
        rrect(g, x, hy, CARD_W, CARD_H, 12, C_WHITE);
      }
      drawCardContent(g, i, x, y, isSel && fabsf(hl - i * PITCH) < PITCH / 2);
    }
    // scroll bar
    float total = N_MENU * PITCH - (PITCH - CARD_H), view = LIST_BOT - LIST_TOP;
    if (total > view) {
      float h = view * view / total, y = LIST_TOP + (view - h) * (scroll / (total - view));
      rrect(g, 234, y, 3, h, 1, C_LINE);
    }
    drawHeader(g, now, "Arcade");
    drawFooter(g, "UP/DOWN  move    ENTER  open");
  }

  void drawLaunch(Canvas& g, uint32_t t, uint32_t now) {
    drawMenu(g, now, true);
    float k = easeInOutCubic(seg(t, 0, 320));
    float y = LIST_TOP + hl - scroll;
    uint16_t c = blend(MENU[sel].color, C_BG, seg(t, 320, 480));
    rrect(g, lerpf(CARD_X, 0, k), lerpf(y, 0, k), lerpf(CARD_W, SW, k), lerpf(CARD_H, SH, k), lerpf(12, 0, k), c);
    float nk = seg(t, 120, 320) * (1 - seg(t, 330, 470));
    if (nk > 0) textC(g, F_HUGE, 120, ir(172 + 12 * (1 - easeOutCubic(nk))), MENU[sel].name, blend(c, C_BG, nk));
  }

  void drawSettings(Canvas& g, uint32_t now) {
    g.fillScreen(C_BG);
    const char* names[4] = {"Show FPS", "Keyboard", "Reset scores", "Back"};
    for (int i = 0; i < 4; i++) {
      int y = LIST_TOP + i * PITCH;
      bool s = i == setSel;
      rrect(g, CARD_X, y, CARD_W, CARD_H, 12, s ? C_WHITE : C_CARD);
      uint16_t tc = s ? C_BG : C_WHITE, sc = s ? rgb(90, 90, 96) : C_DIM;
      text(g, F_BOLD, CARD_X + 16, y + 22, names[i], tc);
      const char* sub = "";
      int ks = plat_kbState();
      if (i == 0) sub = showFps ? "ON" : "OFF";
      if (i == 1) sub = ks == 3 ? "CARDKB2 CONNECTED" : ks == 2 ? "PAIRED" : ks == 1 ? "CONNECTING..." : "SEARCHING...";
      if (i == 2) sub = resetDoneAt && now - resetDoneAt < 2000 ? "SCORES CLEARED" :
                        (resetArmAt && now - resetArmAt < 3000 ? "PRESS AGAIN TO CONFIRM" : "CLEARS ALL BEST SCORES");
      if (i == 3) sub = "RETURN TO MENU";
      text(g, F_SMALL, CARD_X + 16, y + 30, sub, (i == 2 && resetArmAt && now - resetArmAt < 3000) ? C_RED : sc);
      if (i == 0) {   // toggle switch
        int sx = CARD_X + CARD_W - 50, sy = y + 15;
        rrect(g, sx, sy, 36, 18, 9, showFps ? C_TEAL : C_LINE);
        g.fillCircle(showFps ? sx + 27 : sx + 9, sy + 9, 7, C_WHITE);
      }
    }
    drawHeader(g, now, "Settings");
    drawFooter(g, "ENTER  select    ESC  back");
  }

  void drawAbout(Canvas& g, uint32_t now) {
    g.fillScreen(C_BG);
    Mascot m; m.cx = 120; m.cy = 78; m.s = 0.32f;
    uint32_t cyc = now % 3500;
    m.eyeOpen = 1 - bump(seg(cyc, 3200, 3380));
    m.lift = 4 * (0.5f + 0.5f * sinf(now * 0.004f));
    m.look = sinf(now * 0.0015f);
    drawMascot(g, m);
    textC(g, F_BIG, 120, 160, "Altoids Gameboy", C_WHITE);
    textC(g, F_SMALL, 120, 170, "ARCADE OS  v1.0", C_TEAL);
    const char* lines[] = {"ESP32-S3 N16R8", "ST7789 240x320 display", "CardKB2 over Bluetooth LE", "700mAh LiPo + MT3608 5V"};
    for (int i = 0; i < 4; i++) textC(g, F_SMALL, 120, 196 + i * 16, lines[i], C_SOFT);
    textC(g, F_SMALL, 120, 305, "ESC  back", C_DIM);
  }
};
