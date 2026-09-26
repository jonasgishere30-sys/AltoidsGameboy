#pragma once
#include "Game.h"

// Pong in portrait: you are the bottom paddle, the CPU is on top. First to 7 wins.
struct PongGame : Game {
  static const int TOP = 30, BOT = 320, PW = 46, PH = 6, BS = 6;
  float pX, cX, bx, by, vx, vy, speed;
  int you, cpu; uint32_t serveAt; bool serving; int serveDir; bool won;
  const char* saveKey() override { return "pong"; }

  void serve(uint32_t now, int dir) {
    serving = true; serveAt = now; serveDir = dir;
    bx = SW / 2 - BS / 2; by = (TOP + BOT) / 2 - BS / 2; vx = vy = 0; speed = 170;
  }
  void begin() override {
    loadBest(); score = 0; phase = PLAY; you = cpu = 0; won = false;
    pX = cX = SW / 2 - PW / 2; serve(0, 1);
  }
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    float t = dt / 1000.0f;
    if (in.held[B_LEFT])  pX -= 260 * t;
    if (in.held[B_RIGHT]) pX += 260 * t;
    pX = fmaxf(0, fminf(SW - PW, pX));
    // CPU: follows the ball, a bit slower than it, reacts only when the ball comes at it
    float target = (vy < 0 ? bx + BS / 2 : SW / 2) - PW / 2;
    float cpuSpeed = 130 + 8 * (you + cpu);
    if (fabsf(target - cX) > 4) cX += (target > cX ? 1 : -1) * fminf(cpuSpeed * t, fabsf(target - cX));
    cX = fmaxf(0, fminf(SW - PW, cX));

    if (serving) {
      if (now - serveAt > 800) {
        serving = false;
        float ang = (plat_random(60) - 30) * 3.14159f / 180;
        vx = sinf(ang) * speed; vy = cosf(ang) * speed * serveDir;
      }
      return true;
    }
    for (int step = 0; step < 4; step++) {
      bx += vx * t / 4; by += vy * t / 4;
      if (bx < 0) { bx = 0; vx = fabsf(vx); }
      if (bx > SW - BS) { bx = SW - BS; vx = -fabsf(vx); }
      int pyY = BOT - 14, cyY = TOP + 8;
      if (vy > 0 && by + BS >= pyY && by + BS <= pyY + PH + 4 && bx + BS >= pX && bx <= pX + PW) bounce(pX, -1, true);
      if (vy < 0 && by <= cyY + PH && by >= cyY - 4 && bx + BS >= cX && bx <= cX + PW) bounce(cX, 1, false);
    }
    if (by > BOT) { cpu++; if (cpu >= 7) { won = false; gameOver(now); } else serve(now, 1); }
    if (by < TOP - BS) { you++; score += 100; if (you >= 7) { won = true; score += 500; gameOver(now); } else serve(now, -1); }
    return true;
  }
  void bounce(float padX, int dir, bool player) {
    float hit = ((bx + BS / 2) - (padX + PW / 2)) / (PW / 2);   // -1 .. 1
    hit = fmaxf(-1, fminf(1, hit));
    speed = fminf(speed * 1.06f, 400);
    float ang = hit * 60 * 3.14159f / 180;
    vx = sinf(ang) * speed; vy = cosf(ang) * speed * dir;
    if (player) score += 10;
  }
  void draw(Canvas& g, uint32_t now) override {
    g.fillScreen(C_BG);
    drawHud(g, "Pong", C_WHITE);
    int mid = (TOP + BOT) / 2;
    for (int x = 4; x < SW; x += 14) g.fillRect(x, mid - 1, 7, 2, C_LINE);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", cpu); textC(g, F_HUGE, 120, mid - 30, buf, C_LINE);
    snprintf(buf, sizeof(buf), "%d", you); textC(g, F_HUGE, 120, mid + 58, buf, C_LINE);
    rrect(g, cX, TOP + 8, PW, PH, 3, C_SOFT);
    rrect(g, pX, BOT - 14, PW, PH, 3, C_TEAL);
    if (!serving || (now / 150) % 2) rrect(g, bx, by, BS, BS, 2, C_WHITE);
    if (serving) textC(g, F_SMALL, 120, mid + 12, "LEFT/RIGHT to move", C_DIM);
    drawOverlays(g, now, C_TEAL);
    if (phase == OVER) textC(g, F_BOLD, 120, 88, won ? "You win!" : "CPU wins", won ? C_GREEN : C_RED);
  }
};
