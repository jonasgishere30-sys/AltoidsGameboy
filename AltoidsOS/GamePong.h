#pragma once
#include "Game.h"

// Classic Pong: you are the left paddle (UP/DOWN), the CPU is on the right. First to 7 wins.
struct PongGame : Game {
  static const int TOP = 28, BOT = 240, PW = 6, PH = 42, BS = 6, PX = 10, CXP = 304;
  float pY, cY, bx, by, vx, vy, speed;
  int you, cpu; uint32_t serveAt; bool serving; int serveDir; bool won;
  const char* saveKey() override { return "pong"; }

  void serve(uint32_t now, int dir) {
    serving = true; serveAt = now; serveDir = dir;
    bx = SW / 2 - BS / 2; by = (TOP + BOT) / 2 - BS / 2; vx = vy = 0; speed = 190;
  }
  void begin() override {
    loadBest(); score = 0; phase = PLAY; you = cpu = 0; won = false;
    pY = cY = (TOP + BOT) / 2 - PH / 2; serve(0, 1);
  }
  bool update(Input& in, uint32_t now, uint32_t dt) override {
    int m = handleMeta(in, now);
    if (m == 2) return false;
    if (m == 1) { begin(); return true; }
    if (phase != PLAY) return true;
    float t = dt / 1000.0f;
    if (in.held[B_UP])   pY -= 240 * t;
    if (in.held[B_DOWN]) pY += 240 * t;
    pY = fmaxf(TOP, fminf(BOT - PH, pY));
    // CPU: follows the ball while it is coming, drifts to centre otherwise
    float target = (vx > 0 ? by + BS / 2 : (TOP + BOT) / 2) - PH / 2;
    float cpuSpeed = 120 + 8 * (you + cpu);
    if (fabsf(target - cY) > 4) cY += (target > cY ? 1 : -1) * fminf(cpuSpeed * t, fabsf(target - cY));
    cY = fmaxf(TOP, fminf(BOT - PH, cY));

    if (serving) {
      if (now - serveAt > 800) {
        serving = false;
        float ang = (plat_random(60) - 30) * 3.14159f / 180;
        vx = cosf(ang) * speed * serveDir; vy = sinf(ang) * speed;
      }
      return true;
    }
    for (int step = 0; step < 4; step++) {
      bx += vx * t / 4; by += vy * t / 4;
      if (by < TOP) { by = TOP; vy = fabsf(vy); }
      if (by > BOT - BS) { by = BOT - BS; vy = -fabsf(vy); }
      if (vx < 0 && bx <= PX + PW && bx >= PX - 4 && by + BS >= pY && by <= pY + PH) bounce(pY, 1, true);
      if (vx > 0 && bx + BS >= CXP && bx + BS <= CXP + PW + 4 && by + BS >= cY && by <= cY + PH) bounce(cY, -1, false);
    }
    if (bx < -BS) { cpu++; if (cpu >= 7) { won = false; gameOver(now); } else serve(now, 1); }
    if (bx > SW) { you++; score += 100; if (you >= 7) { won = true; score += 500; gameOver(now); } else serve(now, -1); }
    return true;
  }
  void bounce(float padY, int dir, bool player) {
    float hit = ((by + BS / 2) - (padY + PH / 2)) / (PH / 2);   // -1 .. 1
    hit = fmaxf(-1, fminf(1, hit));
    speed = fminf(speed * 1.06f, 420);
    float ang = hit * 55 * 3.14159f / 180;
    vx = cosf(ang) * speed * dir; vy = sinf(ang) * speed;
    if (player) score += 10;
  }
  void draw(Canvas& g, uint32_t now) override {
    g.fillScreen(C_BG);
    drawHud(g, "Pong", C_WHITE);
    int mid = SW / 2;
    for (int y = TOP + 4; y < BOT; y += 14) g.fillRect(mid - 1, y, 2, 7, C_LINE);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", you); textC(g, F_HUGE, mid - 40, 70, buf, C_LINE);
    snprintf(buf, sizeof(buf), "%d", cpu); textC(g, F_HUGE, mid + 40, 70, buf, C_LINE);
    rrect(g, PX, pY, PW, PH, 3, C_TEAL);
    rrect(g, CXP, cY, PW, PH, 3, C_SOFT);
    if (!serving || (now / 150) % 2) rrect(g, bx, by, BS, BS, 2, C_WHITE);
    if (serving) textC(g, F_SMALL, mid, 200, "UP/DOWN to move", C_DIM);
    drawOverlays(g, now, C_TEAL);
    if (phase == OVER) textC(g, F_BOLD, mid, 48, won ? "You win!" : "CPU wins", won ? C_GREEN : C_RED);
  }
};
