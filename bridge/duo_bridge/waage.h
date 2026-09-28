#pragma once
// Waagen fuer Brew by Weight. Laeuft in einer eigenen Task auf Kern 0: Ein
// BLE-Verbindungsaufbau oder eine langsame HTTP-Antwort darf die Leitung
// Mainboard <-> Display nie aufhalten. Werte gehen ueber waageMelden() an loop().
//
// Bluetooth (cfg.waageArt 1), erkannt am Namen, Protokolle wie in
// AcaiaArduinoBLE (tatemazer), BooKoo- und Decent-Dokumentation:
//   Acaia (Lunar, Pearl, Pyxis, Cinco, Proch), BOOKOO Themis/Mini,
//   Felicita Arc/Incline, Decent Scale / Half Decent Scale
// WLAN (cfg.waageArt 2): URL regelmaessig abfragen, Antwort ist eine Zahl
//   oder JSON mit "value", "weight" oder "gewicht" (z. B. ESPHome-REST:
//   http://waage.local/sensor/gewicht)
// WLAN (cfg.waageArt 3): die Waage meldet selbst, per HTTP POST /api/waage
//   (Zahl im Rumpf) oder per MQTT auf cfg.waageTopic (ha_mqtt.h).

#include <BLEDevice.h>
#include <HTTPClient.h>

// Zahl aus Text oder JSON ("value": 12.3 / "weight": / "gewicht":)
static float waageZahl(const char *s) {
  for (const char *k : {"\"value\"", "\"weight\"", "\"gewicht\""}) {
    const char *p = strstr(s, k);
    if (p && (p = strchr(p, ':'))) return strtof(p + 1, nullptr);
  }
  while (*s == ' ' || *s == '"') s++;
  char *e;
  float v = strtof(s, &e);
  return e == s ? NAN : v;
}

enum WaagenTyp { W_KEINE, W_ACAIA_ALT, W_ACAIA_NEU, W_BOOKOO, W_FELICITA, W_DECENT };
static const char *const WAAGEN_NAME[] = {"", "Acaia", "Acaia", "BOOKOO", "Felicita", "Decent"};

static BLEClient *bleClient = nullptr;
static BLERemoteCharacteristic *bleSchreib = nullptr;
static WaagenTyp bleTyp = W_KEINE;

static bool istWaagenName(const String &n) {
  String k = n.substring(0, 5);
  k.toUpperCase();
  return k == "ACAIA" || k == "LUNAR" || k == "PEARL" || k == "PYXIS" || k == "CINCO" || k == "PROCH" ||
         k == "BOOKO" || k == "FELIC" || k == "DECEN";
}

static void bleSchreiben(const uint8_t *d, size_t n) {
  if (bleSchreib) bleSchreib->writeValue((uint8_t *)d, n, false);
}

static void bleDaten(BLERemoteCharacteristic *, uint8_t *d, size_t n, bool) {
  float g = NAN;
  switch (bleTyp) {
    case W_ACAIA_NEU:
      if ((n == 13 || n == 17) && d[4] == 0x05) g = ((d[6] << 8) | d[5]) / powf(10, d[9]) * ((d[10] & 0x02) ? -1 : 1);
      break;
    case W_ACAIA_ALT:
      if (n == 10 || n == 14) g = ((d[3] << 8) | d[2]) / powf(10, d[6]) * ((d[7] & 0x02) ? -1 : 1);
      break;
    case W_BOOKOO:  // 03 0B ... [6] Vorzeichen '+'/'-', [7..9] Gewicht x100
      if (n == 20 && d[0] == 0x03 && d[1] == 0x0B) g = ((d[7] << 16) | (d[8] << 8) | d[9]) / 100.0f * (d[6] == '-' ? -1 : 1);
      break;
    case W_FELICITA:  // ASCII: [2] Vorzeichen, [3..8] Ziffern, 2 Nachkommastellen
      if (n == 18) {
        long v = 0;
        for (int i = 3; i <= 8; i++) v = v * 10 + (d[i] - '0');
        g = v / 100.0f * (d[2] == '-' ? -1 : 1);
      }
      break;
    case W_DECENT:  // 03 CE/CA hi lo ...: int16 in 0,1 g
      if (n >= 7 && d[0] == 0x03 && (d[1] == 0xCE || d[1] == 0xCA)) g = (int16_t)((d[2] << 8) | d[3]) / 10.0f;
      break;
    default:
      break;
  }
  waageMelden(g);
}

static void bleTrennen() {
  if (bleClient) {
    if (bleClient->isConnected()) bleClient->disconnect();
    delete bleClient;
    bleClient = nullptr;
  }
  bleSchreib = nullptr;
  bleTyp = W_KEINE;
}

// Sucht die passende Charakteristik in allen Diensten der Waage.
static BLERemoteCharacteristic *bleSuche(const char *uuid) {
  BLEUUID u(uuid);
  for (auto &d : *bleClient->getServices())
    for (auto &c : *d.second->getCharacteristics())
      if (c.second->getUUID().equals(u)) return c.second;
  return nullptr;
}

static bool bleVerbinden() {
  waageStatusSetzen("Bluetooth: suche Waage …");
  BLEScan *scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  BLEScanResults *r = scan->start(4, false);
  BLEAdvertisedDevice *treffer = nullptr;
  static BLEAdvertisedDevice gefunden;
  for (int i = 0; r && i < r->getCount(); i++) {
    BLEAdvertisedDevice d = r->getDevice(i);
    bool passt = cfg.waageBle.length() ? d.getAddress().toString().equalsIgnoreCase(cfg.waageBle) : istWaagenName(d.getName());
    if (passt) {
      gefunden = d;
      treffer = &gefunden;
      break;
    }
  }
  scan->clearResults();
  if (!treffer) {
    waageStatusSetzen("Bluetooth: keine Waage gefunden");
    return false;
  }
  String name = treffer->getName();
  char st[64];
  snprintf(st, sizeof st, "Bluetooth: verbinde %s …", name.c_str());
  waageStatusSetzen(st);
  bleClient = BLEDevice::createClient();
  if (!bleClient->connect(treffer)) {
    bleTrennen();
    waageStatusSetzen("Bluetooth: Verbindung fehlgeschlagen");
    return false;
  }
  BLERemoteCharacteristic *lesen = nullptr;
  if ((lesen = bleSuche("49535343-1e4d-4bd9-ba61-23c647249616"))) {
    bleTyp = W_ACAIA_NEU;
    bleSchreib = bleSuche("49535343-8841-43f4-a8d4-ecbe34729bb3");
  } else if ((lesen = bleSuche("00002a80-0000-1000-8000-00805f9b34fb"))) {
    bleTyp = W_ACAIA_ALT;
    bleSchreib = lesen;
  } else if ((lesen = bleSuche("0000ff11-0000-1000-8000-00805f9b34fb"))) {
    bleTyp = W_BOOKOO;
    bleSchreib = bleSuche("0000ff12-0000-1000-8000-00805f9b34fb");
  } else if ((lesen = bleSuche("0000ffe1-0000-1000-8000-00805f9b34fb"))) {
    bleTyp = W_FELICITA;
    bleSchreib = lesen;
  } else if ((lesen = bleSuche("0000fff4-0000-1000-8000-00805f9b34fb"))) {
    bleTyp = W_DECENT;
    bleSchreib = bleSuche("000036f5-0000-1000-8000-00805f9b34fb");
  }
  if (!lesen || !lesen->canNotify()) {
    bleTrennen();
    waageStatusSetzen("Bluetooth: Protokoll unbekannt");
    return false;
  }
  lesen->registerForNotify(bleDaten);
  if (bleTyp == W_ACAIA_ALT || bleTyp == W_ACAIA_NEU) {
    static const uint8_t IDENT[20] = {0xef, 0xdd, 0x0b, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
                                      0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x9a, 0x6d};
    static const uint8_t NOTIF[14] = {0xef, 0xdd, 0x0c, 0x09, 0x00, 0x01, 0x01, 0x02, 0x02, 0x05, 0x03, 0x04, 0x15, 0x06};
    bleSchreiben(IDENT, sizeof IDENT);
    bleSchreiben(NOTIF, sizeof NOTIF);
  } else if (bleTyp == W_FELICITA) {
    static const uint8_t MODUS[1] = {0x32};  // Gewicht+Timer-Modus
    bleSchreiben(MODUS, 1);
  }
  snprintf(st, sizeof st, "Bluetooth: %s (%s)", name.c_str(), WAAGEN_NAME[bleTyp]);
  waageStatusSetzen(st);
  return true;
}

static void bleSchritt() {
  static uint32_t herz = 0, naechsterVersuch = 0;
  if (!bleClient || !bleClient->isConnected()) {
    bleTrennen();
    if (millis() < naechsterVersuch) return;
    if (!bleVerbinden()) naechsterVersuch = millis() + 5000;
    herz = millis();
    return;
  }
  if (waageTaraBle) {
    waageTaraBle = false;
    static const uint8_t TARA_ACAIA[6] = {0xef, 0xdd, 0x04, 0x00, 0x00, 0x00};
    static const uint8_t TARA_BOOKOO[6] = {0x03, 0x0a, 0x01, 0x00, 0x00, 0x08};
    static const uint8_t TARA_FELICITA[1] = {0x54};
    static const uint8_t TARA_DECENT[7] = {0x03, 0x0f, 0x01, 0x00, 0x00, 0x01, 0x0c};
    if (bleTyp == W_ACAIA_ALT || bleTyp == W_ACAIA_NEU) bleSchreiben(TARA_ACAIA, 6);
    else if (bleTyp == W_BOOKOO) bleSchreiben(TARA_BOOKOO, 6);
    else if (bleTyp == W_FELICITA) bleSchreiben(TARA_FELICITA, 1);
    else if (bleTyp == W_DECENT) bleSchreiben(TARA_DECENT, 7);
  }
  // Herzschlag: Acaia alle 2,75 s, Decent alle 4 s, sonst trennt die Waage
  if (bleTyp == W_ACAIA_ALT || bleTyp == W_ACAIA_NEU) {
    if (millis() - herz > 2750) {
      static const uint8_t HB[7] = {0xef, 0xdd, 0x00, 0x02, 0x00, 0x02, 0x00};
      bleSchreiben(HB, 7);
      herz = millis();
    }
  } else if (bleTyp == W_DECENT && millis() - herz > 4000) {
    static const uint8_t HB[7] = {0x03, 0x0a, 0x03, 0xff, 0xff, 0x00, 0x0a};
    bleSchreiben(HB, 7);
    herz = millis();
  }
}

static void httpSchritt() {
  static uint32_t fehler = 0;
  if (!cfg.waageUrl.length()) {
    waageStatusSetzen("WLAN: keine URL eingetragen");
    vTaskDelay(pdMS_TO_TICKS(1000));
    return;
  }
  HTTPClient h;
  h.setConnectTimeout(400);
  h.setTimeout(400);
  h.setReuse(true);
  if (h.begin(cfg.waageUrl) && h.GET() == 200) {
    float g = waageZahl(h.getString().c_str());
    waageMelden(g);
    waageStatusSetzen(isnan(g) ? "WLAN: Antwort ohne Zahl" : "WLAN: verbunden");
    fehler = 0;
  } else if (++fehler >= 5) {
    waageStatusSetzen("WLAN: Waage antwortet nicht");
  }
  h.end();
  vTaskDelay(pdMS_TO_TICKS(fehler >= 5 ? 2000 : 120));
}

static void waageTask(void *) {
  bool bleAn = false;
  for (;;) {
    uint8_t art = cfg.waageArt;
    if (art != 1 && bleClient) bleTrennen();  // BLE bleibt initialisiert, nur die Verbindung geht
    if (art == 1) {
      if (!bleAn) {
        BLEDevice::init("duo-bridge");
        bleAn = true;
      }
      bleSchritt();
      vTaskDelay(pdMS_TO_TICKS(50));
    } else if (art == 2) {
      httpSchritt();
    } else {
      if (art == 0) waageStatusSetzen("aus");
      else if (millis() - waageMs > 3000) waageStatusSetzen("WLAN: wartet auf Werte (POST /api/waage oder MQTT)");
      else waageStatusSetzen("WLAN: Werte kommen an");
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }
}

static void waageSetup() { xTaskCreatePinnedToCore(waageTask, "waage", 8192, nullptr, 1, nullptr, 0); }
