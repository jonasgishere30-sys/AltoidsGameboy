#pragma once
#include "Game.h"

struct BreakoutGame : Game {
  static const int COLS = 8, ROWS = 6, BRW = 28, BRH = 10, BX0 = 1, BY0 = 52;
  static const int PADY = 296, PADH = 6, BALL_R = 3;
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
    padW = 48; padX = SW / 2 - padW / 2; buildLevel(); resetBall();
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
    if (in.held[B_LEFT])  padX -= 280 * t;
    if (in.held[B_RIGHT]) padX += 280 * t;
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
    textR(g, F_SMALL, 234, 33, buf, C_DIM);
    if (stuck && phase == PLAY) textC(g, F_SMALL, 120, 250, "SPACE to launch", C_SOFT);
    drawOverlays(g, now, C_ORANGE);
  }
};
