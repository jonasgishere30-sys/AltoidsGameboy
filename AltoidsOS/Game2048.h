#pragma once
#include "Game.h"

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
