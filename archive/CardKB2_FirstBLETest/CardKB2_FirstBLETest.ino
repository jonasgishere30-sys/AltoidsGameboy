#include <NimBLEDevice.h>

void notifyCB(NimBLERemoteCharacteristic* c, uint8_t* d, size_t len, bool) {
  Serial.printf("[h%d] ", c->getHandle());
  for (size_t i = 0; i < len; i++) Serial.printf("%02X ", d[i]);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  NimBLEDevice::init("");
  NimBLEDevice::deleteAllBonds();                       // remove after first success
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  // scan 10 s, blocking, then pick CardKB2
  NimBLEScan* s = NimBLEDevice::getScan();
  s->setActiveScan(true);
  NimBLEScanResults res = s->getResults(10000, false);

  const NimBLEAdvertisedDevice* target = nullptr;
  for (int i = 0; i < res.getCount(); i++) {
    const NimBLEAdvertisedDevice* d = res.getDevice(i);
    if (d->getName().rfind("CardKB2", 0) == 0) { target = d; break; }
  }
  if (!target) { Serial.println("CardKB2 not found"); return; }

  NimBLEClient* cl = NimBLEDevice::createClient();
  if (!cl->connect(target)) { Serial.println("connect failed"); return; }
  Serial.println("connected");

  if (!cl->secureConnection()) { Serial.println("encrypt failed"); return; }
  Serial.println("encrypted");

  NimBLERemoteService* hid = cl->getService(NimBLEUUID((uint16_t)0x1812));
  if (!hid) { Serial.println("no HID service"); return; }

  NimBLERemoteCharacteristic* pm = hid->getCharacteristic(NimBLEUUID((uint16_t)0x2A4E));
  if (pm && pm->canWrite()) pm->writeValue<uint8_t>(0x01, true);   // report mode

  for (auto* c : hid->getCharacteristics(true)) {
    if (c->getUUID() == NimBLEUUID((uint16_t)0x2A4D) && c->canNotify()) {
      bool ok = c->subscribe(true, notifyCB);
      Serial.printf("subscribed h%d: %s\n", c->getHandle(), ok ? "ok" : "FAIL");
    }
  }
  Serial.println("press keys");
}

void loop() { delay(1000); }
