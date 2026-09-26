#pragma once
#include "Game.h"

struct FlappyGame : Game {
  static const int GROUND = 296, PIPEW = 38, GAP = 96, NP = 3, SPACING = 150;
  float birdY, vel, scroll, pipeX[NP]; int gapY[NP]; bool passed[NP];
  bool started;
  const char* saveKey() override { return "flappy"; }

  void newPipe(int i, float x) { pipeX[i] = x; gapY[i] = 70 + plat_random(GROUND - 70 - GAP - 30); passed[i] = false; }
  void begin() override {
    loadBest(); score = 0; phase = PLAY; started = false;
    birdY = 150; vel = 0; scroll = 0;
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
      birdY = 150 + sinf(now * 0.006f) * 6;
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
      int sx = (i * 53 + 17) % SW, sy = 40 + (i * 97) % 200;
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
    if (!started && phase == PLAY) { textC(g, F_BOLD, 120, 110, "Get ready", C_WHITE); textC(g, F_SMALL, 120, 200, "SPACE or UP to flap", C_SOFT); }
    drawOverlays(g, now, C_YELLOW);
  }
};
