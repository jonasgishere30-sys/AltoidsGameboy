#pragma once
#include "Game.h"

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
