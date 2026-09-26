#pragma once
#include "Game.h"

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
