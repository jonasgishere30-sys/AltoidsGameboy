#pragma once
// Functions the UI/games need from the hardware. Defined in AltoidsOS.ino (device)
// so the UI code itself stays hardware-independent.
#include <stdint.h>
uint32_t plat_millis();
long     plat_random(long n);                       // 0 .. n-1
int      plat_loadInt(const char* key, int def);
void     plat_saveInt(const char* key, int value);
int      plat_kbState();                            // 0 scanning, 1 connecting, 2 paired, 3 ready
