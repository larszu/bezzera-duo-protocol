// Man-in-the-middle fuer die Leitung Mainboard <-> Display der Bezzera Duo.
// Der ESP32 sitzt IN der Leitung: Er reicht jeden DGUS-Rahmen weiter,
// schreibt ihn mit und kann auf Befehl eigene Rahmen einschieben oder
// Antworten des Displays ueberschreiben (Tastendruck emulieren).
//
// Board: Waveshare ESP32-S3-ETH (Arduino-ESP32 3.x, "ESP32S3 Dev Module",
// USB CDC On Boot: Enabled). Andere ESP32-S3 gehen auch, Pins unten pruefen.
// Die Pins sind so gewaehlt, dass W5500 (9-14), SD (4-7), LED (21) und
// GPIO33-37 frei bleiben. 15 und 18 sind Kamera-Datenpins, ohne Kamera frei.
//
// Verdrahtung (Details in ../../README.md, Abschnitt ESP32-Bridge):
//   Display TXD (gelb, 3,3 V) ------------------> GPIO15  (RX von Display)
//   GPIO16 -------------------------------------> Mainboard RX (dort, wo gelb war)
//   Mainboard TX (weiss, 4,8 V) --10k--+--------> GPIO17  (RX von Mainboard)
//                                      20k
//                                      GND
//   GPIO18 -------------------------------------> Display RXD (dort, wo weiss war)
//   GND Maschine -------------------------------> GND ESP32
//   +5 V (gruen) bleibt direkt Mainboard -> Display, NICHT an den ESP32.
//
// Ausgabe ueber USB, eine Zeile je Rahmen, Format wie ../../tools/duo_sniff.py:
//   <millis> <A|B> <hex hex ...>       A = vom Display, B = vom Mainboard
//   <millis> <a|b> <hex hex ...>       eingeschoben/veraendert: a = an Mainboard,
//                                      b = an Display
// Zeilen mit '#' sind Meldungen.
//
// Befehle (Zeile mit Enter abschliessen, Zahlen dezimal oder 0x...):
//   p <seite>            Display auf Seite schalten (Register 0x03)
//   w <vp> <wort> ...    VP im Display schreiben (0x82)
//   o <vp> <wert> [n]    die naechsten n Antworten (Standard 1) des Displays
//                        auf "VP lesen <vp>" mit <wert> ueberschreiben
//                        -> so sieht das Mainboard einen Tastendruck
//   o <vp> -             Ueberschreiben fuer <vp> aufheben
//   d <hex ...>          Rohbytes an das Display
//   m <hex ...>          Rohbytes an das Mainboard
//   s <von> <bis> [ms]   Seiten von..bis durchschalten, je ms (Standard 3000)
//   s                    Durchschalten abbrechen
//   n <ssid> <passwort>  Heim-WLAN speichern (nur ueber USB), "n -" loescht es
//   j [seit]             Zustand als JSON-Zeile "#J {...}" (tools/web_lokal.py)
//   M <hex ...>          Test: Rahmen behandeln, als kaeme er vom Mainboard
//   D <hex ...>          Test: Rahmen behandeln, als kaeme er vom Display
//   e [0|1|2]            Modus: 0 durchreichen, 1 Display emulieren (der ESP32
//                        beantwortet die Leseanfragen selbst, Display optional),
//                        2 Hybrid (Display antwortet, per Web/Befehl gesetzte VPs
//                        werden in seinen Antworten ersetzt)
//   x                    Mitschnitt-Ausgabe an/aus (Durchreichen laeuft immer)
//   ?                    Zustand

#include <Arduino.h>
#include <Preferences.h>
#include <driver/gpio.h>

// Vorwaertsdeklaration: die Arduino-IDE setzt Funktionsprototypen vor die Typen.
struct Parser;
static void modellSchreiben(const uint8_t *r, size_t n, char quelle);
static void emuliereAntwort(const uint8_t *r, size_t n);

static const int PIN_RX_DISPLAY = 15;
static const int PIN_TX_MAINBOARD = 16;
static const int PIN_RX_MAINBOARD = 17;
static const int PIN_TX_DISPLAY = 18;

static const uint32_t BAUD = 115200;
static const uint8_t KOPF0 = 0xC6;  // Duo: C6 A5 statt DWIN-Standard 5A A5
static const uint8_t KOPF1 = 0xA5;

// A: liest vom Display, sendet ans Mainboard. B: liest vom Mainboard, sendet ans Display.
static HardwareSerial &uartA = Serial1;  // RX 15, TX 16
static HardwareSerial &uartB = Serial2;  // RX 17, TX 18

static bool ausgabe = true;

// ─── Zustand fuer die Weboberflaeche (web.ino) ─────────────────────────────
// Alles, was auf der Leitung zu sehen ist, wird hier mitgefuehrt: aktuelle
// Seite, jede VP mit letztem Wert, Uhr, Lebenszeichen je Richtung und die
// letzten Ereignisse als Text.

struct VpWert {
  uint16_t vp;
  uint8_t len;
  uint8_t d[64];
  uint32_t ms;
};
static VpWert vpTabelle[48];
static size_t vpAnzahl = 0;

struct Leitung {
  int32_t seite = -1;
  uint8_t rtc[7] = {0};
  bool rtcGueltig = false;
  uint32_t rahmenDisplay = 0, rahmenMainboard = 0;
  uint32_t zuletztDisplay = 0, zuletztMainboard = 0;
};
static Leitung leitung;

static char ereignisse[40][96];
static uint32_t ereignisNr = 0;  // fortlaufend, Web holt nur neue

static void ereignis(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char *z = ereignisse[ereignisNr % 40];
  int k = snprintf(z, 96, "%lu ", (unsigned long)millis());
  vsnprintf(z + k, 96 - k, fmt, ap);
  va_end(ap);
  ereignisNr++;
}

// Stoerbytes (kein gueltiger Rahmen) hoechstens einmal je 5 s melden.
static uint32_t muellAnzahl[2] = {0, 0};
static void ereignisMuell(char quelle) {
  static uint32_t zuletzt = 0;
  muellAnzahl[quelle == 'B']++;
  if (millis() - zuletzt > 5000) {
    zuletzt = millis();
    ereignis("%c Stoerbytes, kein gueltiger Rahmen (%lu bisher)", quelle, (unsigned long)muellAnzahl[quelle == 'B']);
  }
}

static void merkeVp(uint16_t vp, const uint8_t *d, size_t n, char quelle) {
  if (n > sizeof(vpTabelle[0].d)) n = sizeof(vpTabelle[0].d);
  VpWert *e = nullptr;
  for (size_t i = 0; i < vpAnzahl; i++)
    if (vpTabelle[i].vp == vp) e = &vpTabelle[i];
  if (!e && vpAnzahl < 48) {
    e = &vpTabelle[vpAnzahl++];
    e->vp = vp;
    e->len = 0;
    ereignis("%c neue VP 0x%04X", quelle, vp);
  }
  if (!e) return;
  // VP 0x0050 aendert sich staendig (Temperaturen): nur Tabelle, kein Ereignis
  if (vp != 0x0050 && (e->len != n || memcmp(e->d, d, n))) {
    char h[64];
    int k = 0;
    for (size_t i = 0; i + 1 < n && k < 56; i += 2) k += snprintf(h + k, sizeof h - k, " %u", (d[i] << 8) | d[i + 1]);
    ereignis("%c VP 0x%04X =%s", quelle, vp, h);
  }
  memcpy(e->d, d, n);
  e->len = n;
  e->ms = millis();
}

// Wertet jeden vollstaendigen Rahmen aus. quelle: A/a zum Mainboard (vom
// Display bzw. eingeschoben), B/b zum Display (vom Mainboard bzw. eingeschoben).
static void beobachte(char quelle, const uint8_t *r, size_t n) {
  bool zumDisplay = quelle == 'B' || quelle == 'b';
  if (n < 5 || r[0] != KOPF0 || r[1] != KOPF1 || r[2] + 3u != n) {
    if (quelle == 'A' || quelle == 'B') ereignisMuell(quelle);
    return;
  }
  // Lebenszeichen nur fuer gueltige Rahmen: Stoerbytes zaehlen nicht
  if (quelle == 'A') {
    leitung.rahmenDisplay++;
    leitung.zuletztDisplay = millis();
  } else if (quelle == 'B') {
    leitung.rahmenMainboard++;
    leitung.zuletztMainboard = millis();
  }
  if (zumDisplay) modellSchreiben(r, n, quelle);
  const uint8_t cmd = r[3];
  const uint8_t *p = r + 4;
  size_t pn = n - 4;
  if (cmd == 0x80 && pn >= 3 && p[0] == 0x03 && zumDisplay) {
    int32_t s = (p[1] << 8) | p[2];
    if (s != leitung.seite) ereignis("%c Seite %ld", quelle, (long)s);
    leitung.seite = s;
  } else if (cmd == 0x80 && pn >= 9 && p[0] == 0x1F && p[1] == 0x5A && zumDisplay) {
    memcpy(leitung.rtc, p + 2, 7);
    leitung.rtcGueltig = true;
    ereignis("%c Uhr gestellt", quelle);
  } else if (cmd == 0x81 && pn >= 4 && !zumDisplay) {
    if (p[0] == 0x03 && pn >= 4) leitung.seite = (p[2] << 8) | p[3];
    if (p[0] == 0x20 && pn >= 9) {
      memcpy(leitung.rtc, p + 2, 7);
      leitung.rtcGueltig = true;
    }
  } else if (cmd == 0x82 && pn >= 4 && zumDisplay) {
    merkeVp((p[0] << 8) | p[1], p + 2, pn - 2, quelle);
  } else if (cmd == 0x83 && pn >= 5 && !zumDisplay) {
    merkeVp((p[0] << 8) | p[1], p + 3, pn - 3, quelle);
  }
}

// ─── Display-Emulation ──────────────────────────────────────────────────────
// Modell des Display-Speichers: alle VPs (Worte) und Register (Bytes), wie sie
// Mainboard, Weboberflaeche und Befehle geschrieben haben. Im Emulationsmodus
// ("e 1") beantwortet der ESP32 die Leseanfragen des Mainboards (0x81, 0x83)
// selbst aus diesem Modell; Antworten eines angeschlossenen Displays werden
// dann verworfen. Das echte Display ist optional und zeigt weiter an.
static const uint16_t VP_ANZAHL = 0x1000;  // 0x0000..0x0FFF, mehr nutzt das Projekt nicht
static uint16_t vpRam[VP_ANZAHL];
static uint8_t regRam[256];
static uint8_t emulation = 0;  // 0 durchreichen, 1 Display emulieren, 2 Hybrid
// Hybrid: das echte Display antwortet, gesetzte VPs werden in seinen Antworten
// ersetzt. Gesetzt = von der Weboberflaeche/Befehlen geschrieben ('b'); das
// Mainboard hebt es auf, indem es selbst in die VP schreibt ('B').
static uint8_t vpGesetzt[0x1000 / 8];
static uint32_t rtcBasisMs = 0;  // millis() beim letzten Stellen der Uhr

void webSetup();
void webLoop();
void heimWlan(char *s);
String statusText(uint32_t seit);

// ─── Rahmen-Parser je Richtung ─────────────────────────────────────────────

struct Parser {
  uint8_t buf[260];
  size_t n = 0;
  uint32_t letztesUs = 0;
};

static Parser pDisplay, pMainboard;

// Ueberschreiben von Display-Antworten auf 0x83
struct Override {
  bool aktiv = false;
  uint16_t vp = 0;
  uint16_t wert = 0;
  int32_t rest = 0;
};
static Override overrides[8];

// Seiten-Durchlauf
static bool scanAktiv = false;
static int scanSeite = 0, scanBis = 0;
static uint32_t scanMs = 3000, scanNaechster = 0;

static void logZeile(char richtung, const uint8_t *d, size_t n) {
  beobachte(richtung, d, n);
  if (!ausgabe) return;
  char zeile[16 + 3 * 260];
  int k = snprintf(zeile, sizeof zeile, "%lu %c", (unsigned long)millis(), richtung);
  for (size_t i = 0; i < n && k < (int)sizeof zeile - 4; i++) k += snprintf(zeile + k, sizeof zeile - k, " %02x", d[i]);
  Serial.println(zeile);
}

static void sendeRahmen(HardwareSerial &ziel, char logRichtung, const uint8_t *nutz, size_t n) {
  uint8_t r[260];
  r[0] = KOPF0;
  r[1] = KOPF1;
  r[2] = (uint8_t)n;
  memcpy(r + 3, nutz, n);
  ziel.write(r, n + 3);
  logZeile(logRichtung, r, n + 3);
}

static void seite(uint16_t s) {
  uint8_t c[] = {0x80, 0x03, (uint8_t)(s >> 8), (uint8_t)s};
  sendeRahmen(uartB, 'b', c, sizeof c);
}

// Ein vollstaendiger Rahmen vom Display: ggf. Antwort ueberschreiben, dann
// an das Mainboard weiterreichen.
static void rahmenVomDisplay(uint8_t *r, size_t n) {
  if (emulation == 1) {  // das Mainboard bekommt nur die Antworten des Emulators
    beobachte('A', r, n);
    return;
  }
  if (emulation == 2 && n >= 9 && r[3] == 0x83) {  // Hybrid: gesetzte VPs ersetzen
    uint16_t vp = (r[4] << 8) | r[5];
    uint8_t anz = r[6];
    bool geaendert = false;
    for (uint8_t i = 0; i < anz && 7u + 2 * i + 1 < n; i++) {
      uint16_t v = vp + i;
      if (v < VP_ANZAHL && (vpGesetzt[v / 8] & (1 << (v % 8)))) {
        if (!geaendert) logZeile('A', r, n);  // Original protokollieren
        r[7 + 2 * i] = vpRam[v] >> 8;
        r[8 + 2 * i] = vpRam[v] & 0xFF;
        geaendert = true;
      }
    }
    if (geaendert) {
      uartA.write(r, n);
      logZeile('a', r, n);
      return;
    }
  }
  // C6 A5 06 83 vpH vpL 01 wH wL
  if (n == 9 && r[3] == 0x83 && r[6] == 0x01) {
    uint16_t vp = (r[4] << 8) | r[5];
    for (auto &o : overrides) {
      if (o.aktiv && o.vp == vp) {
        logZeile('A', r, n);  // Original trotzdem protokollieren
        r[7] = o.wert >> 8;
        r[8] = o.wert & 0xFF;
        uartA.write(r, n);
        logZeile('a', r, n);
        if (o.rest > 0 && --o.rest == 0) {
          o.aktiv = false;
          Serial.printf("# override 0x%04X beendet\n", vp);
        }
        return;
      }
    }
  }
  uartA.write(r, n);
  logZeile('A', r, n);
}

static void rahmenVomMainboard(uint8_t *r, size_t n) {
  uartB.write(r, n);
  logZeile('B', r, n);
  if (emulation == 1) emuliereAntwort(r, n);
}

// Liest Bytes, setzt Rahmen zusammen. Bytes ausserhalb eines Rahmens werden
// sofort weitergereicht (z. B. das einzelne FD beim Einschalten).
static void pumpe(HardwareSerial &quelle, Parser &p, HardwareSerial &ziel, char richtung,
                  void (*fertig)(uint8_t *, size_t)) {
  while (quelle.available()) {
    uint8_t b = quelle.read();
    p.letztesUs = micros();
    if (p.n == 0 && b != KOPF0) {
      ziel.write(b);
      logZeile(richtung, &b, 1);
      continue;
    }
    if (p.n == 1 && b != KOPF1) {
      ziel.write(p.buf, 1);
      p.n = 0;
      if (b == KOPF0) {
        p.buf[p.n++] = b;
      } else {
        ziel.write(b);
      }
      continue;
    }
    p.buf[p.n++] = b;
    if (p.n >= 3 && p.n == (size_t)p.buf[2] + 3) {
      fertig(p.buf, p.n);
      p.n = 0;
    } else if (p.n >= sizeof p.buf) {
      ziel.write(p.buf, p.n);
      p.n = 0;
    }
  }
  // Angefangener Rahmen, der nicht fertig wird: nach 5 ms roh weiterreichen,
  // damit nichts haengen bleibt.
  if (p.n && micros() - p.letztesUs > 5000) {
    ziel.write(p.buf, p.n);
    logZeile(richtung, p.buf, p.n);
    p.n = 0;
  }
}

// ─── Befehle ────────────────────────────────────────────────────────────────

static long zahl(char *&s, bool &ok) {
  while (*s == ' ') s++;
  if (!*s) {
    ok = false;
    return 0;
  }
  char *ende;
  long v = strtol(s, &ende, 0);
  ok = ende != s;
  s = ende;
  return v;
}

static size_t hexBytes(char *s, uint8_t *out, size_t max) {
  size_t n = 0;
  while (*s && n < max) {
    while (*s == ' ') s++;
    if (!isxdigit((unsigned char)s[0]) || !isxdigit((unsigned char)s[1])) break;
    char h[3] = {s[0], s[1], 0};
    out[n++] = strtol(h, nullptr, 16);
    s += 2;
  }
  return n;
}

static void zustand() {
  Serial.printf("# baud=%lu kopf=%02X %02X ausgabe=%d scan=%s emulation=%d\n", (unsigned long)BAUD, KOPF0, KOPF1,
                ausgabe, scanAktiv ? "an" : "aus", emulation);
  for (auto &o : overrides)
    if (o.aktiv) Serial.printf("# override vp=0x%04X wert=0x%04X rest=%ld\n", o.vp, o.wert, (long)o.rest);
}

static void befehl(char *z) {
  char c = z[0];
  char *s = z + 1;
  bool ok = true;
  if (c == 'p') {
    long n = zahl(s, ok);
    if (ok) seite(n);
  } else if (c == 'w') {
    long vp = zahl(s, ok);
    if (!ok) return;
    uint8_t cmd[3 + 2 * 60] = {0x82, (uint8_t)(vp >> 8), (uint8_t)vp};
    size_t n = 3;
    while (n < sizeof cmd - 1) {
      long w = zahl(s, ok);
      if (!ok) break;
      cmd[n++] = w >> 8;
      cmd[n++] = w & 0xFF;
    }
    if (n > 3) sendeRahmen(uartB, 'b', cmd, n);
  } else if (c == 'o') {
    long vp = zahl(s, ok);
    if (!ok) return;
    while (*s == ' ') s++;
    if (*s == '-') {
      for (auto &o : overrides)
        if (o.aktiv && o.vp == vp) o.aktiv = false;
      Serial.printf("# override 0x%04lX aus\n", vp);
      return;
    }
    long wert = zahl(s, ok);
    if (!ok) return;
    long anzahl = zahl(s, ok);
    if (!ok) anzahl = 1;
    Override *frei = nullptr;
    for (auto &o : overrides)
      if (o.aktiv && o.vp == vp) frei = &o;
    for (auto &o : overrides)
      if (!frei && !o.aktiv) frei = &o;
    if (!frei) {
      Serial.println("# kein freier override-Platz");
      return;
    }
    *frei = {true, (uint16_t)vp, (uint16_t)wert, anzahl};
    Serial.printf("# override vp=0x%04lX wert=0x%04lX fuer %ld Antworten (0 = dauerhaft)\n", vp, wert, anzahl);
  } else if (c == 'd' || c == 'm') {
    uint8_t b[256];
    size_t n = hexBytes(s, b, sizeof b);
    if (!n) return;
    (c == 'd' ? uartB : uartA).write(b, n);
    logZeile(c == 'd' ? 'b' : 'a', b, n);
  } else if (c == 's') {
    long von = zahl(s, ok);
    if (!ok) {
      scanAktiv = false;
      Serial.println("# scan aus");
      return;
    }
    long bis = zahl(s, ok);
    if (!ok) bis = von;
    long ms = zahl(s, ok);
    scanMs = ok ? ms : 3000;
    scanSeite = von;
    scanBis = bis;
    scanAktiv = true;
    scanNaechster = 0;
  } else if (c == 'n') {
    heimWlan(s);
  } else if (c == 'j') {  // Zustand als JSON fuer die lokale Oberflaeche
    long seit = zahl(s, ok);
    Serial.print("#J ");
    Serial.println(statusText(ok ? seit : 0));
  } else if (c == 'D') {  // Test: Rahmen verarbeiten, als kaeme er vom Display
    uint8_t b[256];
    size_t n = hexBytes(s, b, sizeof b);
    if (n >= 4) rahmenVomDisplay(b, n);
  } else if (c == 'M') {  // Test: Rahmen verarbeiten, als kaeme er vom Mainboard
    uint8_t b[256];
    size_t n = hexBytes(s, b, sizeof b);
    if (n >= 4) rahmenVomMainboard(b, n);
  } else if (c == 'e') {
    long an = zahl(s, ok);
    emulation = ok ? (an >= 0 && an <= 2 ? an : 0) : (emulation ? 0 : 1);
    Preferences pref;  // dauerhaft: USB-Verbindungsaufbau startet den ESP32 neu
    pref.begin("bridge", false);
    pref.putUChar("modus", emulation);
    pref.end();
    static const char *namen[] = {"durchreichen", "Emulation", "Hybrid"};
    Serial.printf("# Modus %s (gespeichert)\n", namen[emulation]);
    ereignis("- Modus %s", namen[emulation]);
  } else if (c == 'x') {
    ausgabe = !ausgabe;
    Serial.printf("# ausgabe %s\n", ausgabe ? "an" : "aus");
  } else if (c == '?') {
    zustand();
  }
}

static void leseBefehle() {
  static char zeile[400];
  static size_t n = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      zeile[n] = 0;
      if (n) befehl(zeile);
      n = 0;
    } else if (n < sizeof zeile - 1) {
      zeile[n++] = c;
    }
  }
}

void setup() {
  Serial.begin(921600);
  uartA.setRxBufferSize(2048);
  uartB.setRxBufferSize(2048);
  uartA.begin(BAUD, SERIAL_8N1, PIN_RX_DISPLAY, PIN_TX_MAINBOARD);
  uartB.begin(BAUD, SERIAL_8N1, PIN_RX_MAINBOARD, PIN_TX_DISPLAY);
  // Offene Eingaenge (Kabel noch nicht dran) sollen Ruhepegel sehen statt
  // Rauschen, das sonst als Bytes weitergereicht wuerde. ~45 kOhm, stoert
  // die echte Leitung nicht.
  gpio_pullup_en((gpio_num_t)PIN_RX_DISPLAY);
  gpio_pullup_en((gpio_num_t)PIN_RX_MAINBOARD);
  // Kurze FIFO-Wartezeit: Rahmen sollen ohne Verzoegerung weitergehen.
  uartA.setRxFIFOFull(1);
  uartB.setRxFIFOFull(1);
  delay(200);
  // Beim Einschalten der UARTs entsteht ein Stoerbyte je Kanal. Verwerfen,
  // damit es weder beim Display noch beim Mainboard ankommt.
  while (uartA.available()) uartA.read();
  while (uartB.available()) uartB.read();
  {
    Preferences pref;
    pref.begin("bridge", true);
    emulation = pref.getUChar("modus", 0);
    if (emulation > 2) emulation = 0;
    pref.end();
  }
  Serial.println("# duo_bridge bereit, ? fuer Zustand");
  zustand();
  webSetup();
}

void loop() {
  pumpe(uartA, pDisplay, uartA, 'A', rahmenVomDisplay);
  pumpe(uartB, pMainboard, uartB, 'B', rahmenVomMainboard);
  leseBefehle();
  webLoop();
  if (scanAktiv && millis() >= scanNaechster) {
    if (scanSeite > scanBis) {
      scanAktiv = false;
      Serial.println("# scan fertig");
    } else {
      Serial.printf("# seite %d\n", scanSeite);
      seite(scanSeite++);
      scanNaechster = millis() + scanMs;
    }
  }
}

// ─── Display-Emulation: Implementierung ─────────────────────────────────────

static uint8_t bcd(uint32_t v) { return ((v / 10) << 4) | (v % 10); }
static uint32_t vonBcd(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }

// Aktuelle Uhrzeit aus dem zuletzt gestellten Wert (Register 0x20..0x26,
// BCD: JJ MM TT Wochentag hh mm ss) plus vergangener Zeit.
static void rtcAktuell(uint8_t *aus) {
  memcpy(aus, regRam + 0x20, 7);
  uint32_t sek = vonBcd(aus[4]) * 3600 + vonBcd(aus[5]) * 60 + vonBcd(aus[6]) + (millis() - rtcBasisMs) / 1000;
  sek %= 86400;  // Datumswechsel wird nicht nachgefuehrt
  aus[4] = bcd(sek / 3600);
  aus[5] = bcd(sek / 60 % 60);
  aus[6] = bcd(sek % 60);
}

// Schreibzugriffe Richtung Display ins Modell uebernehmen.
static void modellSchreiben(const uint8_t *r, size_t n, char quelle) {
  const uint8_t cmd = r[3];
  const uint8_t *p = r + 4;
  size_t pn = n - 4;
  if (cmd == 0x80 && pn >= 2) {
    uint8_t reg = p[0];
    if (reg == 0x1F && pn >= 9 && p[1] == 0x5A) {  // Uhr stellen: 1F 5A JJ MM TT WT hh mm ss
      memcpy(regRam + 0x20, p + 2, 7);
      rtcBasisMs = millis();
    } else {
      for (size_t i = 1; i < pn && reg + i - 1 < 256; i++) regRam[reg + i - 1] = p[i];
    }
  } else if (cmd == 0x82 && pn >= 4) {
    uint16_t vp = (p[0] << 8) | p[1];
    for (size_t i = 2; i + 1 < pn; i += 2, vp++) {
      if (vp >= VP_ANZAHL) continue;
      vpRam[vp] = (p[i] << 8) | p[i + 1];
      if (quelle == 'b') vpGesetzt[vp / 8] |= 1 << (vp % 8);
      else vpGesetzt[vp / 8] &= ~(1 << (vp % 8));
    }
  }
}

// Leseanfragen des Mainboards beantworten wie das Display: 81 reg n / 83 vp n.
static void emuliereAntwort(const uint8_t *r, size_t n) {
  if (n < 6) return;
  const uint8_t cmd = r[3];
  uint8_t a[260];
  size_t k = 0;
  if (cmd == 0x81 && n >= 6) {
    uint8_t reg = r[4], anz = r[5];
    if (anz > 60) return;
    a[k++] = 0x81; a[k++] = reg; a[k++] = anz;
    uint8_t rtc[7];
    rtcAktuell(rtc);
    for (uint8_t i = 0; i < anz; i++) {
      uint16_t rg = reg + i;
      if (rg >= 0x20 && rg <= 0x26) a[k++] = rtc[rg - 0x20];
      else if (rg == 0x00) a[k++] = 0x22;  // Firmware-Version des echten Displays
      else a[k++] = rg < 256 ? regRam[rg] : 0;
    }
  } else if (cmd == 0x83 && n >= 7) {
    uint16_t vp = (r[4] << 8) | r[5];
    uint8_t anz = r[6];
    if (anz == 0 || anz > 32) return;  // das echte Display kann hoechstens 32 Worte
    a[k++] = 0x83; a[k++] = r[4]; a[k++] = r[5]; a[k++] = anz;
    for (uint8_t i = 0; i < anz; i++) {
      uint16_t w = (vp + i) < VP_ANZAHL ? vpRam[vp + i] : 0;
      a[k++] = w >> 8;
      a[k++] = w & 0xFF;
    }
  } else {
    return;
  }
  // Tastendruck-Ueberschreibung (Befehl o) gilt auch im Emulationsmodus
  if (a[0] == 0x83 && a[3] == 1) {
    uint16_t vp = (a[1] << 8) | a[2];
    for (auto &o : overrides) {
      if (o.aktiv && o.vp == vp) {
        a[4] = o.wert >> 8;
        a[5] = o.wert & 0xFF;
        if (o.rest > 0 && --o.rest == 0) o.aktiv = false;
      }
    }
  }
  sendeRahmen(uartA, 'a', a, k);
}

// Weboberflaeche am Ende eingebunden: nutzt Zustand und befehl() von oben.
#include "web.h"
