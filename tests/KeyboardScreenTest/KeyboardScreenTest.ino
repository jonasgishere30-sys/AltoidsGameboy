// Altoids Gameboy - Keyboard + Screen Test
// Board:    ESP32-S3 N16R8 (ESP32S3 Dev Module, 16MB flash, OPI PSRAM)
// Display:  GMT024-10 V2.1 2.4" ST7789 240x320 (same wiring as ScreenTest)
// Keyboard: M5Stack Unit CardKB2 in BLE HID mode (Fn + Sym + 4)
// Libraries: "Adafruit ST7735 and ST7789 Library", "NimBLE-Arduino" (2.x)
// BLE part follows M5Stack's official CardKB2 BLE HID receiver example.

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <NimBLEDevice.h>

// ---------------- Display ----------------
#define TFT_SCK   12
#define TFT_MOSI  11
#define TFT_RST   10
#define TFT_DC     9
#define TFT_CS     8
Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);

// Screen layout (portrait 240x320)
const int STATUS_H = 28;              // top status bar
const int TEXT_Y   = 34;              // typing area
const int TEXT_H   = 196;
const int CHAR_W   = 12, CHAR_H = 16; // text size 2
const int COLS     = 240 / CHAR_W;    // 20
const int ROWS     = TEXT_H / CHAR_H; // 12
const int INFO_Y   = 236;             // bottom info panel

// ---------------- BLE ----------------
static NimBLEUUID kHidSvcUUID((uint16_t)0x1812);
static NimBLEUUID kReportUUID((uint16_t)0x2A4D);

static NimBLEAddress g_addr;
static NimBLEClient* g_client = nullptr;
static volatile bool g_hasTarget = false, g_connected = false,
                     g_authReady = false, g_subscribed = false;

// Reports are copied here by the BLE task and drawn by loop()
struct Report { uint8_t len; uint8_t data[16]; };
static QueueHandle_t g_reportQueue;

// HID keycode -> ASCII (same table as M5Stack's example)
static const char kNormal[]  = "\0\0\0\0abcdefghijklmnopqrstuvwxyz1234567890\n\x1b\b\t -=[]\\" "\0;'`,./";
static const char kShifted[] = "\0\0\0\0ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()\n\x1b\b\t _+{}|" "\0:\"~<>?";

// HID arrow keycodes (standard USB HID usage table)
#define HID_RIGHT 0x4F
#define HID_LEFT  0x50
#define HID_DOWN  0x51
#define HID_UP    0x52

static void reportCB(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
  Report r;
  r.len = (len > sizeof(r.data)) ? sizeof(r.data) : len;
  memcpy(r.data, data, r.len);
  xQueueSend(g_reportQueue, &r, 0);   // never block the BLE task
}

struct ClientCB : NimBLEClientCallbacks {
  void onConnect(NimBLEClient* c) override {
    Serial.println("[BLE] Connected");
    g_connected = true; g_authReady = false; g_subscribed = false;
    int rc; NimBLEDevice::startSecurity(c->getConnHandle(), &rc);
  }
  void onDisconnect(NimBLEClient*, int reason) override {
    Serial.printf("[BLE] Disconnected (%d)\n", reason);
    g_connected = false; g_authReady = false; g_subscribed = false;
  }
  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    g_authReady = info.isAuthenticated() || info.isEncrypted();
    Serial.printf("[BLE] Auth: %s\n", g_authReady ? "OK" : "FAIL");
    if (!g_authReady && g_client) g_client->disconnect();
  }
  bool onConnParamsUpdateRequest(NimBLEClient*, const ble_gap_upd_params*) override { return true; }
};

struct AdvCB : NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    if (g_hasTarget) return;
    bool nameMatch = d->getName().rfind("CardKB2", 0) == 0;
    if (!nameMatch && !d->isAdvertisingService(kHidSvcUUID)) return;
    Serial.printf("[BLE] Found: %s %s\n", d->getName().c_str(), d->getAddress().toString().c_str());
    g_addr = d->getAddress(); g_hasTarget = true;
    NimBLEDevice::getScan()->stop();
  }
};

static void startScan() {
  NimBLEScan* s = NimBLEDevice::getScan();
  if (s->isScanning()) return;
  g_hasTarget = false;
  Serial.println("[BLE] Scanning...");
  s->start(0, false);                 // 0 = scan until we stop it
}

static bool subscribeReports() {
  NimBLERemoteService* svc = g_client ? g_client->getService(kHidSvcUUID) : nullptr;
  if (!svc) return false;
  bool any = false;
  for (auto* c : svc->getCharacteristics(true)) {
    if (c->getUUID() == kReportUUID && (c->canNotify() || c->canIndicate())) {
      if (c->subscribe(true, reportCB, true)) any = true;
    }
  }
  if (any) Serial.println("[BLE] Subscribed - press keys");
  return any;
}

// ---------------- Drawing ----------------
int curCol = 0, curRow = 0;
int dotX = 205, dotY = 290;           // arrow-key test dot
uint8_t prevKeys[6] = {0};

void drawStatus(const char* msg, uint16_t color) {
  tft.fillRect(0, 0, 240, STATUS_H, color);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(6, 6);
  tft.print(msg);
}

void clearTextArea() {
  tft.fillRect(0, TEXT_Y, 240, TEXT_H, ST77XX_BLACK);
  curCol = 0; curRow = 0;
}

void newLine() {
  curCol = 0; curRow++;
  if (curRow >= ROWS) clearTextArea();   // full: start over at the top
}

void typeChar(char ch) {
  if (ch == '\n') { newLine(); return; }
  if (ch == '\b') {                      // backspace
    if (curCol == 0 && curRow == 0) return;
    if (curCol == 0) { curRow--; curCol = COLS - 1; } else curCol--;
    tft.fillRect(curCol * CHAR_W, TEXT_Y + curRow * CHAR_H, CHAR_W, CHAR_H, ST77XX_BLACK);
    return;
  }
  if (ch == 0x1B) { clearTextArea(); return; }   // ESC clears the screen
  if (ch == '\t') ch = ' ';
  if (ch < 0x20 || ch > 0x7E) return;
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(curCol * CHAR_W, TEXT_Y + curRow * CHAR_H);
  tft.print(ch);
  curCol++;
  if (curCol >= COLS) newLine();
}

void drawInfo(const char* keyName, const Report& r) {
  tft.fillRect(0, INFO_Y, 180, 84, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(4, INFO_Y + 4);
  tft.print("Key: ");
  tft.print(keyName);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setCursor(4, INFO_Y + 30);
  tft.print("Raw:");
  for (int i = 0; i < r.len && i < 8; i++) tft.printf(" %02X", r.data[i]);
  tft.setCursor(4, INFO_Y + 46);
  tft.setTextColor(0x8410, ST77XX_BLACK);   // grey
  tft.print("ESC = clear   arrows = dot");
}

void drawDotBox() {
  tft.drawRect(182, 262, 56, 56, ST77XX_WHITE);
  tft.fillRect(183, 263, 54, 54, ST77XX_BLACK);
  tft.fillCircle(dotX, dotY, 4, ST77XX_GREEN);
}

void moveDot(int dx, int dy) {
  tft.fillCircle(dotX, dotY, 4, ST77XX_BLACK);
  dotX = constrain(dotX + dx, 188, 232);
  dotY = constrain(dotY + dy, 268, 312);
  tft.fillCircle(dotX, dotY, 4, ST77XX_GREEN);
}

void handleReport(const Report& rep) {
  Serial.printf("+BLE:RX,%u", rep.len);
  for (int i = 0; i < rep.len; i++) Serial.printf(",%02X", rep.data[i]);
  Serial.println();

  // Keyboard reports: 8 bytes, or 9 bytes starting with report ID 0x01
  const uint8_t* r = rep.data;
  int len = rep.len;
  if (len == 9 && r[0] == 0x01) { r++; len = 8; }
  if (len != 8) { drawInfo("(other)", rep); return; }

  uint8_t mod = r[0];
  const uint8_t* keys = r + 2;
  char name[12] = "-";

  for (int i = 0; i < 6; i++) {
    uint8_t kc = keys[i];
    if (kc == 0) continue;
    bool wasDown = false;
    for (int j = 0; j < 6; j++) if (prevKeys[j] == kc) wasDown = true;
    if (wasDown) continue;                       // only act on new presses

    if      (kc == HID_UP)    { moveDot(0, -4); strcpy(name, "UP"); }
    else if (kc == HID_DOWN)  { moveDot(0,  4); strcpy(name, "DOWN"); }
    else if (kc == HID_LEFT)  { moveDot(-4, 0); strcpy(name, "LEFT"); }
    else if (kc == HID_RIGHT) { moveDot( 4, 0); strcpy(name, "RIGHT"); }
    else if (kc < sizeof(kNormal)) {
      char ch = (mod & 0x22) ? kShifted[kc] : kNormal[kc];
      if (ch == '\n') strcpy(name, "ENTER");
      else if (ch == '\b') strcpy(name, "BKSP");
      else if (ch == 0x1B) strcpy(name, "ESC");
      else if (ch == '\t') strcpy(name, "TAB");
      else if (ch == ' ')  strcpy(name, "SPACE");
      else if (ch)         { name[0] = ch; name[1] = 0; }
      else                 snprintf(name, sizeof(name), "0x%02X", kc);
      if (ch) typeChar(ch);
    } else {
      snprintf(name, sizeof(name), "0x%02X", kc);
    }
  }
  memcpy(prevKeys, keys, 6);
  if (strcmp(name, "-") != 0) drawInfo(name, rep);   // key release: keep showing the last key
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);
  delay(300);

  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.init(240, 320);
  tft.setSPISpeed(40000000);
  tft.invertDisplay(false);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.drawFastHLine(0, INFO_Y - 3, 240, 0x8410);
  drawDotBox();
  drawStatus("Scanning...", ST77XX_ORANGE);

  g_reportQueue = xQueueCreate(32, sizeof(Report));

  NimBLEDevice::init("AltoidsGameboy");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setSecurityAuth(true, false, false);          // bonding, no MITM
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

  NimBLEScan* s = NimBLEDevice::getScan();
  s->setScanCallbacks(new AdvCB(), true);
  s->setActiveScan(true);            // active scan so we can read the "CardKB2" name
  s->setInterval(100);
  s->setWindow(50);
  s->setDuplicateFilter(true);
  startScan();
}

int lastState = -1;

void loop() {
  // 1) Keep the BLE connection going (reconnects automatically)
  if (!g_connected) {
    if (!g_hasTarget) {
      startScan();
    } else {
      if (!g_client) {
        g_client = NimBLEDevice::createClient();
        g_client->setClientCallbacks(new ClientCB(), true);
      }
      if (!g_client->connect(g_addr, false)) {
        Serial.println("[BLE] Connect failed, rescanning");
        g_hasTarget = false;
        delay(500);
      }
    }
  }
  if (g_connected && g_authReady && !g_subscribed) {
    g_subscribed = subscribeReports();
    if (!g_subscribed) delay(200);
  }

  // 2) Update the status bar only when the state changes
  int state = g_subscribed ? 3 : g_authReady ? 2 : g_connected ? 1 : 0;
  if (state != lastState) {
    lastState = state;
    if      (state == 3) drawStatus("Keyboard ready", ST77XX_GREEN);
    else if (state == 2) drawStatus("Paired...", ST77XX_YELLOW);
    else if (state == 1) drawStatus("Connecting...", ST77XX_YELLOW);
    else { drawStatus("Scanning...", ST77XX_ORANGE); memset(prevKeys, 0, 6); }
  }

  // 3) Draw any key reports that arrived
  Report r;
  while (xQueueReceive(g_reportQueue, &r, 0) == pdTRUE) handleReport(r);

  delay(5);
}
