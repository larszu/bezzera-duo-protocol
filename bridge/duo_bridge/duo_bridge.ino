// Man-in-the-middle fuer die Leitung Mainboard <-> Display der Bezzera Duo.
// Der ESP32 sitzt IN der Leitung: Er reicht jeden DGUS-Rahmen weiter,
// schreibt ihn mit und kann auf Befehl eigene Rahmen einschieben oder
// Antworten des Displays ueberschreiben (Tastendruck emulieren).
//
// Board: Waveshare ESP32-S3-ETH (Arduino-ESP32 3.x, "ESP32S3 Dev Module",
// USB CDC On Boot: Enabled). Andere ESP32-S3 gehen auch, Pins unten pruefen.
// FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=custom,PSRAM=opi
// (eigene Aufteilung in partitions.csv; Flashen: ../../docs/flashen.md)
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
//   d <hex ...>          Rohbytes an das Display; Antworten auf eigene Lese-
//                        anfragen (0x81, 0x83 ausser VP 0/1) gehen nicht ans Mainboard
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
//   z <pfad> [rumpf]     Zusatz-API wie im Web, z. B. "z /api/aktion an",
//                        "z /api/zusatz"; Antwort als Zeile "#Z ..."
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

// Rahmenkopf des Displays. Mainboard und Display 2.0 sprechen C6 A5, ein
// Ersatz-Display 2.2 dagegen DWIN-Standard 5A A5 (beide Version 0x25 in
// Register 0x00). Die Bridge erkennt den Kopf an den Antworten des Displays
// und uebersetzt in beide Richtungen; intern ist alles C6 A5.
static const uint8_t KOPF_DWIN = 0x5A;
static uint8_t kopfDisplay = KOPF0;
static uint32_t kopfUebersetzt = 0;  // Rahmen mit uebersetztem Kopf (Display -> Mainboard)
static uint32_t kopfRepariert = 0;   // Mainboard-Rahmen mit gestoertem erstem Byte

// Schreiben zum Display: Kopf C6 A5 bei Bedarf durch den des Displays ersetzen.
class ZumDisplay : public HardwareSerial {
 public:
  using HardwareSerial::HardwareSerial;
  using HardwareSerial::write;
  size_t write(const uint8_t *b, size_t n) override {
    if (n >= 2 && b[0] == KOPF0 && b[1] == KOPF1 && kopfDisplay != KOPF0) {
      HardwareSerial::write(kopfDisplay);
      return 1 + HardwareSerial::write(b + 1, n - 1);
    }
    return HardwareSerial::write(b, n);
  }
  size_t roh(const uint8_t *b, size_t n) { return HardwareSerial::write(b, n); }
};
static ZumDisplay uartBObj(2);

// A: liest vom Display, sendet ans Mainboard. B: liest vom Mainboard, sendet ans Display.
static HardwareSerial &uartA = Serial1;  // RX 15, TX 16
static HardwareSerial &uartB = uartBObj;  // RX 17, TX 18 (UART 2)

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
static VpWert vpTabelle[96];
static size_t vpAnzahl = 0;

struct Leitung {
  int32_t seite = -1;     // Seite, die das Display zeigt
  int32_t seiteMb = -1;   // Seite, die das Mainboard zuletzt wollte (Bezug x06 auch, wenn die Bruehkurve stehen bleibt)
  uint8_t rtc[7] = {0};
  bool rtcGueltig = false;
  uint32_t rtcMs = 0;  // wann die Uhr zuletzt gestellt oder gelesen wurde
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
  if (!e && vpAnzahl < 96) {
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
    if (quelle == 'B') leitung.seiteMb = s;
  } else if (cmd == 0x80 && pn >= 9 && p[0] == 0x1F && p[1] == 0x5A && zumDisplay) {
    // Im Standby stellt das Mainboard die Uhr jede Sekunde; ins Protokoll nur,
    // wenn die Zeit springt (nicht im Sekundentakt weiterlaeuft)
    auto sek = [](const uint8_t *r) {
      auto b = [](uint8_t x) { return (x >> 4) * 10 + (x & 0x0F); };
      return b(r[4]) * 3600 + b(r[5]) * 60 + b(r[6]);
    };
    int erwartet = leitung.rtcGueltig ? sek(leitung.rtc) + (millis() - leitung.rtcMs + 500) / 1000 : -100;
    int neu = sek(p + 2);
    bool sprung = !leitung.rtcGueltig || abs(neu - erwartet) > 2 || memcmp(leitung.rtc, p + 2, 3);
    memcpy(leitung.rtc, p + 2, 7);
    leitung.rtcGueltig = true;
    leitung.rtcMs = millis();
    if (sprung) ereignis("%c Uhr gestellt %02x:%02x:%02x", quelle, p[6], p[7], p[8]);
  } else if (cmd == 0x81 && pn >= 4 && !zumDisplay) {
    if (p[0] == 0x03 && pn >= 4) leitung.seite = (p[2] << 8) | p[3];
    if (p[0] == 0x20 && pn >= 9) {
      memcpy(leitung.rtc, p + 2, 7);
      leitung.rtcGueltig = true;
      leitung.rtcMs = millis();
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
static uint32_t vpMainboardMs[0x100];  // wann das Mainboard VP 0x00..0xFF zuletzt geschrieben hat

void webSetup();
void webLoop();
void heimWlan(char *s);
void zusatzBefehl(char *s);
void sichernBefehl(char *s);
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

// Eigene Leseanfragen der Bridge an das Display (Befehl d, Weboberflaeche):
// Die Antwort des Displays geht nur ins Protokoll, nicht ans Mainboard. Sonst
// saehe das Mainboard Antworten auf Fragen, die es nie gestellt hat.
// VP 0x0000/0x0001 fragt das Mainboard selbst alle 100 ms ab; dort laesst
// sich eine eigene Antwort nicht von seiner unterscheiden, sie bleibt aussen vor.
struct EigeneAnfrage {
  uint8_t cmd = 0;  // 0x81 Register, 0x83 VP; 0 = frei
  uint16_t adresse = 0;
  uint32_t ms = 0;
};
static EigeneAnfrage eigene[4];

static void eigeneAnfrageMerken(const uint8_t *b, size_t n) {
  if (n < 6 || b[0] != KOPF0 || b[1] != KOPF1) return;
  uint16_t adr;
  if (b[3] == 0x83 && n >= 7) adr = (b[4] << 8) | b[5];
  else if (b[3] == 0x81) adr = b[4];
  else return;
  if (b[3] == 0x83 && adr <= 1) return;
  EigeneAnfrage *frei = &eigene[0];
  for (auto &e : eigene)
    if (!e.cmd || millis() - e.ms > 500) frei = &e;
  *frei = {b[3], adr, millis()};
}

// true = Antwort gehoert zu einer eigenen Anfrage (und wird verbraucht)
static bool eigeneAntwort(const uint8_t *r, size_t n) {
  if (n < 6) return false;
  uint16_t adr = r[3] == 0x83 ? (r[4] << 8) | r[5] : r[4];
  for (auto &e : eigene) {
    if (e.cmd && e.cmd == r[3] && e.adresse == adr && millis() - e.ms < 500) {
      e.cmd = 0;
      return true;
    }
  }
  return false;
}

// Seiten-Durchlauf
static bool scanAktiv = false;
static int scanSeite = 0, scanBis = 0;
static uint32_t scanMs = 3000, scanNaechster = 0;

// Antworten auf Befehle (j, z, ?) muessen vollstaendig ankommen: in Stuecken
// schreiben und je Stueck kurz warten, aber nie haengen bleiben.
static void usbAntwort(const String &s) {
  if (!Serial) return;
  size_t i = 0;
  while (i < s.length()) {
    uint32_t t0 = millis();
    while (Serial.availableForWrite() < 1 && millis() - t0 < 50) delay(1);
    size_t frei = Serial.availableForWrite();
    if (!frei) return;  // niemand liest mit
    size_t n = min(frei, s.length() - i);
    Serial.write((const uint8_t *)s.c_str() + i, n);
    i += n;
  }
  Serial.write('\n');
}

static void logZeile(char richtung, const uint8_t *d, size_t n) {
  beobachte(richtung, d, n);
  if (!ausgabe || !Serial || Serial.availableForWrite() < 64) return;  // niemand liest mit: verwerfen
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
static void sichernAntwort(const uint8_t *r, size_t n);  // sichern.h
static void eigeneSeitenAntwort(const uint8_t *r, size_t n);  // eigene_seiten.h

static void rahmenVomDisplay(uint8_t *r, size_t n) {
  sichernAntwort(r, n);
  eigeneSeitenAntwort(r, n);
  if (emulation == 1) {  // das Mainboard bekommt nur die Antworten des Emulators
    if (eigeneAntwort(r, n)) logZeile('A', r, n);  // eigene Anfrage: sichtbar machen
    else beobachte('A', r, n);
    return;
  }
  if (eigeneAntwort(r, n)) {  // Antwort auf eine Frage der Bridge: nicht ans Mainboard
    logZeile('A', r, n);
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

static void anzeigeAnpassen(uint8_t *r, size_t n);  // zusatz.h: Werte der Bridge aufs Display
static bool kurveHalten(const uint8_t *r, size_t n);  // eigene_seiten.h

static void rahmenVomMainboard(uint8_t *r, size_t n) {
  if (kurveHalten(r, n)) return;
  anzeigeAnpassen(r, n);
  uartB.write(r, n);
  logZeile('B', r, n);
  if (emulation == 1) emuliereAntwort(r, n);
}

// Liest Bytes, setzt Rahmen zusammen. Bytes ausserhalb eines Rahmens werden
// sofort weitergereicht (z. B. das einzelne FD beim Einschalten).
static void pumpe(HardwareSerial &quelle, Parser &p, HardwareSerial &ziel, char richtung,
                  void (*fertig)(uint8_t *, size_t)) {
  const bool vomDisplay = richtung == 'A';
  // Moeglicher Rahmenanfang. Vom Display C6 oder 5A (siehe kopfDisplay). Vom
  // Mainboard jedes Byte: Mainboard 2.1 schickt das erste Byte nach einer
  // Pause manchmal gestoert (E2/E6 statt C6); A5, Laenge und Befehl 80-83
  // dahinter entscheiden, ob es ein Rahmen ist.
  auto anfang = [&](uint8_t b) { return vomDisplay ? (b == KOPF0 || b == KOPF_DWIN) : true; };
  while (quelle.available()) {
    uint8_t b = quelle.read();
    p.letztesUs = micros();
    if (p.n == 0 && !anfang(b)) {
      ziel.write(b);
      logZeile(richtung, &b, 1);
      continue;
    }
    if (p.n == 1 && b != KOPF1) {
      ziel.write(p.buf, 1);
      logZeile(richtung, p.buf, 1);
      p.n = 0;
      if (anfang(b)) {
        p.buf[p.n++] = b;
      } else {
        ziel.write(b);
        logZeile(richtung, &b, 1);
      }
      continue;
    }
    if (p.n == 3 && p.buf[0] != KOPF0 && !vomDisplay && (b < 0x80 || b > 0x83 || p.buf[2] < 2)) {
      p.buf[p.n++] = b;  // kein Rahmen: gestoertes Byte war doch nur ein Byte
      ziel.write(p.buf, p.n);
      logZeile(richtung, p.buf, p.n);
      p.n = 0;
      continue;
    }
    p.buf[p.n++] = b;
    if (p.n >= 3 && p.n == (size_t)p.buf[2] + 3) {
      if (p.buf[0] != KOPF0) {
        if (vomDisplay) {
          if (kopfDisplay != p.buf[0]) {
            kopfDisplay = p.buf[0];
            ereignis("- Display spricht Kopf %02X A5, Bridge uebersetzt", kopfDisplay);
          }
          kopfUebersetzt++;
        } else {
          kopfRepariert++;
        }
        p.buf[0] = KOPF0;
      } else if (vomDisplay && kopfDisplay != KOPF0) {
        kopfDisplay = KOPF0;
        ereignis("- Display spricht Kopf C6 A5");
      }
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
  Serial.printf("# display_kopf=%02X uebersetzt=%lu repariert=%lu\n", kopfDisplay, (unsigned long)kopfUebersetzt,
                (unsigned long)kopfRepariert);
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
    if (c == 'd') eigeneAnfrageMerken(b, n);
    if (c == 'd' && n >= 6 && b[3] == 0x81 && (b[4] == 0x20 || b[4] == 0x03)) {  // Nachlesen: nicht ins Protokoll
      uartB.write(b, n);
      return;
    }
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
  } else if (c == 'S') {  // Display-Flash sichern (sichern.h)
    sichernBefehl(s);
  } else if (c == 'z') {  // Zusatz-API (Ein/Aus, Waage, Verlauf, Einstellungen)
    zusatzBefehl(s);
  } else if (c == 'j') {  // Zustand als JSON fuer die lokale Oberflaeche
    long seit = zahl(s, ok);
    usbAntwort("#J " + statusText(ok ? seit : 0));
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

static bool improvByte(uint8_t c);  // improv.h

static void leseBefehle() {
  static char zeile[400];
  static size_t n = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (improvByte(c)) continue;
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
  // Liest am Rechner niemand mit, darf die Ausgabe ueber USB nie warten: Jede
  // blockierte Zeile liess die UARTs ueberlaufen, Antworten gingen verloren und
  // das Mainboard begann seinen Start (Uhr stellen) immer wieder von vorn.
  Serial.setTxTimeoutMs(0);
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

// Schweigt das Display, obwohl das Mainboard fragt, abwechselnd mit beiden
// Koepfen Register 0x00 (Version) lesen. Die Antwort stellt kopfDisplay ein
// und geht nicht ans Mainboard (eigene Anfrage).
static void kopfSuchen() {
  static uint32_t zuletzt = 0;
  static bool dwin = true;
  uint32_t jetzt = millis();
  if (jetzt - zuletzt < 2000 || jetzt - leitung.zuletztDisplay < 3000 || jetzt - leitung.zuletztMainboard > 1000) return;
  zuletzt = jetzt;
  uint8_t f[] = {KOPF0, KOPF1, 0x03, 0x81, 0x00, 0x01};
  eigeneAnfrageMerken(f, sizeof f);
  if (dwin) f[0] = KOPF_DWIN;
  dwin = !dwin;
  uartBObj.roh(f, sizeof f);
}

void loop() {
  pumpe(uartA, pDisplay, uartA, 'A', rahmenVomDisplay);
  pumpe(uartB, pMainboard, uartB, 'B', rahmenVomMainboard);
  kopfSuchen();
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
      if (quelle == 'B' && vp < 0x100) vpMainboardMs[vp] = millis();
    }
  }
}

// Leseanfragen des Mainboards beantworten wie das Display: 81 reg n / 83 vp n.
static bool sichernStandby();  // sichern.h

static void emuliereAntwort(const uint8_t *r, size_t n) {
  if (n < 6) return;
  const uint8_t cmd = r[3];
  uint8_t a[260];
  size_t k = 0;
  // Waehrend der Display-Sicherung sieht das Mainboard immer Standby: Seite
  // x00 und Tastencode 0, egal was Display und Touch gerade melden.
  const bool standby = sichernStandby();
  const uint16_t standbySeite = leitung.seite >= 0 && leitung.seite < 300 ? (leitung.seite / 100) * 100 : 100;
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
      else if (standby && rg == 0x03) a[k++] = standbySeite >> 8;
      else if (standby && rg == 0x04) a[k++] = standbySeite & 0xFF;
      else a[k++] = rg < 256 ? regRam[rg] : 0;
    }
  } else if (cmd == 0x83 && n >= 7) {
    uint16_t vp = (r[4] << 8) | r[5];
    uint8_t anz = r[6];
    if (anz == 0 || anz > 32) return;  // das echte Display kann hoechstens 32 Worte
    a[k++] = 0x83; a[k++] = r[4]; a[k++] = r[5]; a[k++] = anz;
    for (uint8_t i = 0; i < anz; i++) {
      uint16_t w = (vp + i) < VP_ANZAHL ? vpRam[vp + i] : 0;
      if (standby && vp + i == 0) w = 0;
      a[k++] = w >> 8;
      a[k++] = w & 0xFF;
    }
  } else {
    return;
  }
  // Tastendruck-Ueberschreibung (Befehl o) gilt auch im Emulationsmodus
  if (a[0] == 0x83 && a[3] == 1 && !standby) {
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
