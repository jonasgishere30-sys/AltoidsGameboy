#pragma once
#include "Gfx.h"
#include "Input.h"
#include "Platform.h"
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
    textR(g, F_BOLD, 234, 19, sc, C_WHITE);
    int bsW = (int)strlen(bs) * 6;
    text(g, F_SMALL, 234 - scW - 8 - bsW, 10, bs, C_DIM);
    g.drawFastHLine(0, 26, SW, C_LINE);
  }
  void drawOverlays(Canvas& g, uint32_t now, uint16_t accent) {
    if (phase == PAUSED) {
      dimScreen(g);
      rrect(g, 30, 110, 180, 100, 14, C_CARD);
      g.drawRoundRect(30, 110, 180, 100, 14, C_LINE);
      textC(g, F_BIG, 120, 148, "Paused", C_WHITE);
      textC(g, F_SMALL, 120, 170, "ENTER  resume", C_SOFT);
      textC(g, F_SMALL, 120, 186, "ESC    menu", C_SOFT);
    } else if (phase == OVER) {
      dimScreen(g);
      float t = easeOutBack(seg(now, overAt, overAt + 350));
      int y = ir(lerpf(340, 96, t));
      rrect(g, 24, y, 192, 128, 16, C_CARD);
      g.drawRoundRect(24, y, 192, 128, 16, accent);
      textC(g, F_BIG, 120, y + 34, "Game Over", C_WHITE);
      char buf[32];
      snprintf(buf, sizeof(buf), "%d", score);
      textC(g, F_HUGE, 120, y + 76, buf, accent);
      bool rec = score > 0 && score >= best;
      textC(g, F_SMALL, 120, y + 88, rec ? "NEW BEST!" : "", C_YELLOW);
      snprintf(buf, sizeof(buf), "ENTER again   ESC menu");
      textC(g, F_SMALL, 120, y + 108, buf, C_SOFT);
    }
  }
};
