// Altoids Gameboy - Arcade OS v1.1 (landscape)
// Boot animation, menu and 6 built-in games, controlled with the CardKB2 over BLE.
//
// Board:    ESP32-S3 N16R8 -> Tools: "ESP32S3 Dev Module", Flash Size 16MB,
//           PSRAM "OPI PSRAM", Partition "16M Flash (3MB APP/9.9MB FATFS)"
// Display:  GMT024-10 ST7789 240x320 used sideways as 320x240 (SCK 12, SDA 11, RST 10, DC 9, CS 8)
//           Pins on the LEFT. If the picture is upside down: Settings > Flip screen
//           (or change SCREEN_ROTATION below from 1 to 3).
// Keyboard: M5Stack Unit CardKB2 in BLE HID mode (Fn + Sym + 4)
// Libraries: "Adafruit ST7735 and ST7789 Library" (+ Adafruit GFX), "NimBLE-Arduino" 2.x
//
// Controls: Arrows or W A S D = move,  SPACE / ENTER = select / action,
//           ESC / BACKSPACE = back / pause,  P = pause

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include "App.h"

// ---------------- Display ----------------
#define TFT_SCK   12
#define TFT_MOSI  11
#define TFT_RST   10
#define TFT_DC     9
#define TFT_CS     8
#define SCREEN_ROTATION 1   // landscape. 3 = landscape turned 180 degrees
Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);
Canvas* canvas = nullptr;   // full-screen frame buffer (320x240x2 = 150 KB, lives in PSRAM)

App app;
Input input;
Preferences prefs;

// ---------------- Platform functions used by the UI ----------------
static volatile int g_kbState = 0;
uint32_t plat_millis() { return millis(); }
long plat_random(long n) { return n > 0 ? (long)(esp_random() % (uint32_t)n) : 0; }
int  plat_loadInt(const char* key, int def) { return prefs.getInt(key, def); }
void plat_saveInt(const char* key, int v) { prefs.putInt(key, v); }
int  plat_kbState() { return g_kbState; }
void plat_setFlip(bool flip) { tft.setRotation(flip ? (SCREEN_ROTATION + 2) % 4 : SCREEN_ROTATION); }

// ---------------- BLE (same working flow as KeyboardScreenTest) ----------------
static NimBLEUUID kHidSvcUUID((uint16_t)0x1812);
static NimBLEUUID kReportUUID((uint16_t)0x2A4D);
static NimBLEAddress g_addr;
static NimBLEClient* g_client = nullptr;
static volatile bool g_hasTarget = false, g_connected = false, g_authReady = false, g_subscribed = false;

struct Report { uint8_t len; uint8_t data[16]; };
static QueueHandle_t g_reportQueue;

static void reportCB(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
  Report r;
  r.len = (len > sizeof(r.data)) ? sizeof(r.data) : len;
  memcpy(r.data, data, r.len);
  xQueueSend(g_reportQueue, &r, 0);
}

struct ClientCB : NimBLEClientCallbacks {
  void onConnect(NimBLEClient* c) override {
    g_connected = true; g_authReady = false; g_subscribed = false;
    int rc; NimBLEDevice::startSecurity(c->getConnHandle(), &rc);
  }
  void onDisconnect(NimBLEClient*, int reason) override {
    Serial.printf("[BLE] Disconnected (%d)\n", reason);
    g_connected = false; g_authReady = false; g_subscribed = false;
  }
  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    g_authReady = info.isAuthenticated() || info.isEncrypted();
    if (!g_authReady && g_client) g_client->disconnect();
  }
  bool onConnParamsUpdateRequest(NimBLEClient*, const ble_gap_upd_params*) override { return true; }
};

struct AdvCB : NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    if (g_hasTarget) return;
    bool nameMatch = d->getName().rfind("CardKB2", 0) == 0;
    if (!nameMatch && !d->isAdvertisingService(kHidSvcUUID)) return;
    Serial.printf("[BLE] Found: %s\n", d->getName().c_str());
    g_addr = d->getAddress(); g_hasTarget = true;
    NimBLEDevice::getScan()->stop();
  }
};

static void startScan() {
  NimBLEScan* s = NimBLEDevice::getScan();
  if (s->isScanning()) return;
  g_hasTarget = false;
  s->start(0, false);
}

static bool subscribeReports() {
  NimBLERemoteService* svc = g_client ? g_client->getService(kHidSvcUUID) : nullptr;
  if (!svc) return false;
  bool any = false;
  for (auto* c : svc->getCharacteristics(true))
    if (c->getUUID() == kReportUUID && (c->canNotify() || c->canIndicate()))
      if (c->subscribe(true, reportCB, true)) any = true;
  if (any) Serial.println("[BLE] Keyboard ready");
  return any;
}

// Runs on core 0 so connecting (which blocks) never freezes the animation on core 1
static void bleTask(void*) {
  NimBLEDevice::init("AltoidsGameboy");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setSecurityAuth(true, false, false);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEScan* s = NimBLEDevice::getScan();
  s->setScanCallbacks(new AdvCB(), true);
  s->setActiveScan(true);
  s->setInterval(100);
  s->setWindow(50);
  s->setDuplicateFilter(true);
  startScan();
  for (;;) {
    if (!g_connected) {
      if (!g_hasTarget) startScan();
      else {
        if (!g_client) {
          g_client = NimBLEDevice::createClient();
          g_client->setClientCallbacks(new ClientCB(), true);
        }
        if (!g_client->connect(g_addr, false)) { g_hasTarget = false; vTaskDelay(pdMS_TO_TICKS(500)); }
      }
    }
    if (g_connected && g_authReady && !g_subscribed) {
      g_subscribed = subscribeReports();
      if (!g_subscribed) vTaskDelay(pdMS_TO_TICKS(200));
    }
    g_kbState = g_subscribed ? 3 : g_authReady ? 2 : g_connected ? 1 : 0;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);
  delay(200);

  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.init(240, 320);
  tft.setSPISpeed(40000000);
  tft.invertDisplay(false);
  tft.setRotation(SCREEN_ROTATION);
  tft.fillScreen(ST77XX_BLACK);

  canvas = new Canvas(SW, SH);
  if (!canvas || !canvas->getBuffer()) {
    tft.setTextColor(ST77XX_RED); tft.setTextSize(2); tft.setCursor(10, 100);
    tft.print("No memory: enable");
    tft.setCursor(10, 122); tft.print("OPI PSRAM in Tools");
    for (;;) delay(1000);
  }
  Serial.printf("Frame buffer %s, free PSRAM %u\n",
                esp_ptr_external_ram(canvas->getBuffer()) ? "in PSRAM" : "in internal RAM", (unsigned)ESP.getFreePsram());

  prefs.begin("arcade", false);
  g_reportQueue = xQueueCreate(32, sizeof(Report));
  xTaskCreatePinnedToCore(bleTask, "ble", 8192, nullptr, 1, nullptr, 0);
  app.begin();
}

void loop() {
  static int lastKb = -1;
  static uint32_t fpsT = 0, frames = 0, fps = 0;

  // 1) Keyboard reports -> buttons
  Report r;
  while (xQueueReceive(g_reportQueue, &r, 0) == pdTRUE) {
    const uint8_t* d = r.data; int len = r.len;
    if (len == 9 && d[0] == 0x01) { d++; len = 8; }   // report ID prefix
    if (len == 8) input.onHid(d + 2);
  }
  int kb = g_kbState;
  if (kb != lastKb) { if (kb != 3) input.releaseAll(); lastKb = kb; }

  // 2) Update and draw one frame
  uint32_t now = millis();
  input.frame(now);
  app.update(input, now);
  app.draw(*canvas, now);

  frames++;
  if (now - fpsT >= 1000) { fps = frames; frames = 0; fpsT = now; }
  if (app.showFps) {
    char b[8]; snprintf(b, sizeof(b), "%u", (unsigned)fps);
    canvas->fillRect(0, SH - 8, 20, 8, C_BG);
    text(*canvas, F_SMALL, 1, SH - 8, b, C_GREEN);
  }

  // 3) Send the finished frame to the screen in one go (no flicker)
  tft.drawRGBBitmap(0, 0, canvas->getBuffer(), SW, SH);
}
