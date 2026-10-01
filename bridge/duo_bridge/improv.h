#pragma once
// Improv Serial (https://www.improv-wifi.com/serial/): Direkt nach dem Flashen
// im Browser fragt die Flash-Seite (ESP Web Tools) Name und Passwort des
// Heim-WLANs ab und schickt sie ueber USB. Pakete beginnen mit "IMPROV" und
// laufen neben den Textbefehlen; leseBefehle() gibt jedes Byte erst hierher.

static const char *const IMPROV_KOPF = "IMPROV";
static uint8_t improvPuffer[160];
static size_t improvN = 0;
static bool improvWartetAufWlan = false;
static uint32_t improvWlanSeit = 0;

static void improvSenden(uint8_t typ, const uint8_t *daten, size_t n) {
  uint8_t p[180];
  size_t k = 0;
  memcpy(p, IMPROV_KOPF, 6);
  k = 6;
  p[k++] = 1;  // Version
  p[k++] = typ;
  p[k++] = (uint8_t)n;
  memcpy(p + k, daten, n);
  k += n;
  uint8_t sum = 0;
  for (size_t i = 0; i < k; i++) sum += p[i];
  p[k++] = sum;
  p[k++] = '\n';
  Serial.write(p, k);
}

static void improvZustand(uint8_t z) { improvSenden(0x01, &z, 1); }  // 2 bereit, 3 verbindet, 4 eingerichtet
static void improvFehler(uint8_t f) { improvSenden(0x02, &f, 1); }   // 0 keiner, 1 Paket, 2 Befehl, 3 keine Verbindung

// RPC-Ergebnis: Befehl, dann Liste von Texten
static void improvErgebnis(uint8_t cmd, std::initializer_list<String> texte) {
  uint8_t d[170];
  size_t k = 2;
  for (const String &t : texte) {
    size_t n = min<size_t>(t.length(), 60);
    if (k + 1 + n > sizeof d) break;
    d[k++] = n;
    memcpy(d + k, t.c_str(), n);
    k += n;
  }
  d[0] = cmd;
  d[1] = k - 2;
  improvSenden(0x04, d, k);
}

static String improvUrl() {
  return WiFi.status() == WL_CONNECTED ? "http://" + WiFi.localIP().toString() + "/" : String("http://duo.local/");
}

static void improvPaket(const uint8_t *p, size_t n) {
  // p: ab Version; [ver, typ, len, daten..., summe]
  if (n < 4 || p[0] != 1) return;
  uint8_t typ = p[1], len = p[2];
  if (n < (size_t)len + 4) return;
  uint8_t sum = 0;
  for (size_t i = 0; i < 6; i++) sum += IMPROV_KOPF[i];
  for (size_t i = 0; i < (size_t)len + 3; i++) sum += p[i];
  if (sum != p[len + 3]) {
    improvFehler(1);
    return;
  }
  if (typ != 0x03 || len < 2) return;
  const uint8_t *d = p + 3;
  uint8_t cmd = d[0];
  if (cmd == 0x01) {  // WLAN-Daten
    uint8_t sl = d[2];
    String ssid((const char *)d + 3, sl);
    uint8_t pl = d[3 + sl];
    String pass((const char *)d + 4 + sl, pl);
    improvZustand(3);
    heimWlanSpeichern(ssid, pass);
    improvWartetAufWlan = true;
    improvWlanSeit = millis();
  } else if (cmd == 0x02) {  // aktueller Zustand
    if (WiFi.status() == WL_CONNECTED) {
      improvZustand(4);
      improvErgebnis(0x02, {improvUrl()});
    } else improvZustand(2);
  } else if (cmd == 0x03) {  // Geraeteinfo
    improvErgebnis(0x03, {"duo_bridge", "2026.10", "ESP32-S3", "Bezzera Duo Bridge"});
  } else if (cmd == 0x04) {  // Netze
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++)
      improvErgebnis(0x04, {WiFi.SSID(i), String(WiFi.RSSI(i)), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "NO" : "YES"});
    improvErgebnis(0x04, {});
    WiFi.scanDelete();
  } else {
    improvFehler(2);
  }
}

// true = Byte gehoert zu einem Improv-Paket
static bool improvByte(uint8_t c) {
  if (improvN < 6) {
    if (c == (uint8_t)IMPROV_KOPF[improvN]) {
      improvN++;
      return true;
    }
    improvN = 0;
    return false;  // Achtung: bereits verschluckte Kopfbytes sind dann verloren (kommt in Textbefehlen nicht vor)
  }
  improvPuffer[improvN - 6] = c;
  improvN++;
  size_t n = improvN - 6;
  if (n >= 3 && n == (size_t)improvPuffer[2] + 4) {
    improvPaket(improvPuffer, n);
    improvN = 0;
  } else if (n >= sizeof improvPuffer) improvN = 0;
  return true;
}

static void improvLoop() {
  if (!improvWartetAufWlan) return;
  if (WiFi.status() == WL_CONNECTED) {
    improvWartetAufWlan = false;
    improvZustand(4);
    improvErgebnis(0x01, {improvUrl()});
  } else if (millis() - improvWlanSeit > 20000) {
    improvWartetAufWlan = false;
    improvFehler(3);
    improvZustand(2);
  }
}
