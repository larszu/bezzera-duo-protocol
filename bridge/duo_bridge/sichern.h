#pragma once
// Display-Flash lesen, nur lesend, ohne loop() anzuhalten (Zustandsmaschine).
// USB-Befehl:
//   S lib <id> <startwort> <worte>   Font-/Konfigurationsbibliothek (LibOP, Register
//                                    0x40-0x49, Modus 0xA0 = lesen)
//   S db  <wortadresse> <worte>      Datenbank/Bildspeicher (Register 0x56-0x5F,
//                                    Modus 0xA0 = lesen); Adresse 0 = 64 MB im Flash
//   S                                abbrechen
// Ausgabe je 32 Worte: "#S <lib|db> <id> <wortadresse> <hex>", am Ende "#S fertig".
// Jeder Block wird zweimal gelesen; nur gleiche Lesungen werden ausgegeben.
// Gedacht fuer tools/display_sichern.py; am sichersten im Modus 1 (die Bridge
// beantwortet das Mainboard selbst, waehrend das Display liest).

enum SZustand : uint8_t { S_AUS, S_START, S_WARTEN, S_LESEN, S_ANTWORT };
static SZustand sZ = S_AUS;
static bool sDatenbank = false;
static uint16_t sLib = 0;
static uint32_t sAdresse = 0, sEnde = 0, sSeit = 0, sAbfrage = 0;
static uint8_t sVersuch = 0, sAntwort[64], sVorher[64];
static bool sAntwortDa = false, sRegDa = false, sHatVorher = false;
static uint8_t sRegWert = 0xFF;

static const uint16_t S_PUFFER = 0x1000;
static const uint8_t S_BLOCK = 32;

// aus rahmenVomDisplay: Antworten auf eigene Lese- und Registeranfragen
static void sichernAntwort(const uint8_t *r, size_t n) {
  if (sZ == S_AUS || n < 7) return;
  if (r[3] == 0x83 && ((r[4] << 8) | r[5]) == S_PUFFER && r[6] == S_BLOCK && n >= 7 + 2 * S_BLOCK) {
    memcpy(sAntwort, r + 7, 2 * S_BLOCK);
    sAntwortDa = true;
  } else if (r[3] == 0x81 && n >= 7 && (r[4] == 0x40 || r[4] == 0x56)) {
    sRegWert = r[6];
    sRegDa = true;
  }
}

static void sSenden(const char *fmt, ...) {
  char b[96];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(b, sizeof b, fmt, ap);
  va_end(ap);
  befehl(b);
}

static void sichernLoop() {
  if (sZ == S_AUS) return;
  uint32_t jetzt = millis();
  switch (sZ) {
    case S_START: {
      uint32_t a = sAdresse;
      if (sDatenbank)  // 80 56 5A A0 <adr 4> <vp 2> <len 2>
        sSenden("d c6 a5 0c 80 56 5a a0 %02x %02x %02x %02x 10 00 00 %02x", (unsigned)(a >> 24) & 0xFF, (unsigned)(a >> 16) & 0xFF,
                (unsigned)(a >> 8) & 0xFF, (unsigned)a & 0xFF, S_BLOCK);
      else  // 80 40 5A A0 <lib> <adr 3> <vp 2> <len 2>
        sSenden("d c6 a5 0c 80 40 5a a0 %02x %02x %02x %02x 10 00 00 %02x", sLib, (unsigned)(a >> 16) & 0xFF,
                (unsigned)(a >> 8) & 0xFF, (unsigned)a & 0xFF, S_BLOCK);
      sZ = S_WARTEN;
      sSeit = jetzt;
      sRegDa = false;
      break;
    }
    case S_WARTEN:  // bis En_Lib_OP / En_DBL_OP wieder 0 ist
      if (sRegDa && sRegWert == 0) {
        sAntwortDa = false;
        sSenden("d c6 a5 04 83 10 00 %02x", S_BLOCK);
        sZ = S_ANTWORT;
        sSeit = jetzt;
      } else if (jetzt - sSeit > 400) {
        sZ = S_START;  // nochmal
        if (++sVersuch > 6) {
          Serial.printf("#S fehler %s %u %lu\n", sDatenbank ? "db" : "lib", sLib, (unsigned long)sAdresse);
          sZ = S_AUS;
        }
      } else if (jetzt - sSeit > 15 && jetzt - sAbfrage > 20) {
        sAbfrage = jetzt;
        sRegDa = false;
        sSenden("d c6 a5 03 81 %02x 01", sDatenbank ? 0x56 : 0x40);
      }
      break;
    case S_ANTWORT:
      if (sAntwortDa) {
        if (sHatVorher && !memcmp(sAntwort, sVorher, sizeof sAntwort)) {
          char h[2 * sizeof sAntwort + 1];
          for (size_t i = 0; i < sizeof sAntwort; i++) sprintf(h + 2 * i, "%02x", sAntwort[i]);
          char z[200];
          snprintf(z, sizeof z, "#S %s %u %lu %s", sDatenbank ? "db" : "lib", sLib, (unsigned long)sAdresse, h);
          usbAntwort(z);  // wartet kurz, verliert keine Daten
          sHatVorher = false;
          sVersuch = 0;
          sAdresse += S_BLOCK;
          if (sAdresse >= sEnde) {
            Serial.println("#S fertig");
            sZ = S_AUS;
            return;
          }
        } else {  // erste Lesung oder Abweichung: (noch) einmal lesen
          memcpy(sVorher, sAntwort, sizeof sVorher);
          sHatVorher = true;
          if (++sVersuch > 8) {
            Serial.printf("#S fehler %s %u %lu\n", sDatenbank ? "db" : "lib", sLib, (unsigned long)sAdresse);
            sZ = S_AUS;
            return;
          }
        }
        sZ = S_START;
      } else if (jetzt - sSeit > 300) sZ = S_START;
      break;
    default:
      break;
  }
}

// USB "S lib <id> <start> <worte>" | "S db <adr> <worte>" | "S"
void sichernBefehl(char *s) {
  char art[8] = "";
  unsigned long a = 0, b = 0, c = 0;
  int n = sscanf(s, "%7s %lu %lu %lu", art, &a, &b, &c);
  sHatVorher = false;
  sVersuch = 0;
  if (n >= 4 && !strcmp(art, "lib")) {
    sDatenbank = false;
    sLib = a;
    sAdresse = b;
    sEnde = b + c;
    sZ = S_START;
  } else if (n >= 3 && !strcmp(art, "db")) {
    sDatenbank = true;
    sLib = 0;
    sAdresse = a;
    sEnde = a + b;
    sZ = S_START;
  } else {
    sZ = S_AUS;
    Serial.println("#S aus");
  }
}
