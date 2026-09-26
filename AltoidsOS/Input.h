#pragma once
// Turns CardKB2 HID key reports into game buttons.
// Arrows or WASD = direction, SPACE/ENTER = A, ESC/BACKSPACE = B, P = pause
#include <stdint.h>
#include <string.h>

enum Btn : uint8_t { B_UP, B_DOWN, B_LEFT, B_RIGHT, B_A, B_B, B_PAUSE, B_COUNT };

struct Input {
  bool     held[B_COUNT]    = {};
  bool     pressed[B_COUNT] = {};   // true for exactly one frame when pressed
  bool     rep[B_COUNT]     = {};   // pressed + auto-repeat while held (menus)
  bool     anyPressed = false;
  uint32_t holdStart[B_COUNT] = {};
  uint32_t lastRep[B_COUNT]   = {};
  bool     pending[B_COUNT]   = {};
  bool     rawHeld[B_COUNT]   = {};
  uint8_t  prevKeys[6] = {};

  static int map(uint8_t kc) {
    switch (kc) {
      case 0x52: case 0x1A: return B_UP;      // Up arrow, W
      case 0x51: case 0x16: return B_DOWN;    // Down arrow, S
      case 0x50: case 0x04: return B_LEFT;    // Left arrow, A
      case 0x4F: case 0x07: return B_RIGHT;   // Right arrow, D
      case 0x2C: case 0x28: return B_A;       // Space, Enter
      case 0x29: case 0x2A: return B_B;       // Esc, Backspace
      case 0x13:            return B_PAUSE;   // P
    }
    return -1;
  }

  // Called with each 8-byte keyboard report (keys = the 6 keycode bytes)
  void onHid(const uint8_t keys[6]) {
    bool now[B_COUNT] = {};
    for (int i = 0; i < 6; i++) {
      int b = map(keys[i]);
      if (b < 0) continue;
      now[b] = true;
      bool was = false;
      for (int j = 0; j < 6; j++) if (prevKeys[j] == keys[i]) was = true;
      if (!was) pending[b] = true;
    }
    memcpy(rawHeld, now, sizeof(now));
    memcpy(prevKeys, keys, 6);
  }

  void releaseAll() { memset(rawHeld, 0, sizeof(rawHeld)); memset(prevKeys, 0, 6); }

  // Call once per frame before the UI/game update
  void frame(uint32_t now) {
    anyPressed = false;
    for (int b = 0; b < B_COUNT; b++) {
      pressed[b] = pending[b];
      pending[b] = false;
      if (pressed[b]) { holdStart[b] = now; lastRep[b] = now; anyPressed = true; }
      held[b] = rawHeld[b] || pressed[b];
      rep[b] = pressed[b];
      if (!pressed[b] && rawHeld[b] && now - holdStart[b] > 320 && now - lastRep[b] > 110) {
        rep[b] = true; lastRep[b] = now;
      }
    }
  }
  uint32_t heldMs(Btn b, uint32_t now) const { return held[b] ? now - holdStart[b] : 0; }
};
