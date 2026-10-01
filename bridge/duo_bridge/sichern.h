#pragma once
// Display-Flash lesen, nur lesend, ohne loop() anzuhalten (Zustandsmaschine).
// USB-Befehl:
//   S lib <id> <startwort> <worte>   Font-/Konfigurationsbibliothek (LibOP, Register
//                                    0x40-0x49, Modus 0xA0 = lesen)
//   S db  <wortadresse> <worte>      Datenbank/Bildspeicher (Register 0x56-0x5F,
//                                    Modus 0xA0 = lesen); Adresse 0 = 64 MB im Flash
//   S                                abbrechen
// Ausgabe je 32 Worte: "#S <lib|db> <id> <wortadresse> <hex>", am Ende "#S fertig".
// Je 2048 Worte ein Lesevorgang; jeder Teil wird zweimal ueber die UART geholt.
// Gedacht fuer tools/display_sichern.py; am sichersten im Modus 1 (die Bridge
// beantwortet das Mainboard selbst, waehrend das Display liest).

enum SZustand : uint8_t { S_AUS, S_START, S_WARTEN, S_ANTWORT };
static SZustand sZ = S_AUS;
static bool sDatenbank = false;
static uint16_t sLib = 0;
static uint32_t sAdresse = 0, sEnde = 0, sSeit = 0, sAbfrage = 0;
static uint16_t sStueck = 0, sTeil = 0, sTeilLen = 0;  // Worte im Puffer, Leseposition darin
static uint8_t sVersuch = 0, sAntwort[2 * 96], sVorher[2 * 96];
static bool sAntwortDa = false, sRegDa = false, sHatVorher = false;
static uint8_t sRegWert = 0xFF;

// Das Display braucht je Lesevorgang ~50-100 ms, fast unabhaengig von der
// Menge. Deshalb 2048 Worte auf einmal in den Puffer (VP 0x1000-0x17FF) und
// dann in Teilen zu 96 Worten ueber die UART holen. Jeder Teil wird zweimal
// geholt und nur bei gleichem Ergebnis ausgegeben.
static const uint16_t S_PUFFER = 0x1000, S_STUECK = 2048, S_TEIL = 96;
static const uint8_t S_BLOCK = 32;  // Worte je Ausgabezeile

// aus rahmenVomDisplay: Antworten auf eigene Lese- und Registeranfragen
static void sichernAntwort(const uint8_t *r, size_t n) {
  if (sZ == S_AUS || n < 7) return;
  if (r[3] == 0x83 && ((r[4] << 8) | r[5]) == S_PUFFER + sTeil && r[6] == sTeilLen && n >= 7 + 2u * sTeilLen) {
    memcpy(sAntwort, r + 7, 2 * sTeilLen);
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

static void sFehler() {
  Serial.printf("#S fehler %s %u %lu\n", sDatenbank ? "db" : "lib", sLib, (unsigned long)(sAdresse + sTeil));
  sZ = S_AUS;
}

static void sTeilHolen(uint32_t jetzt) {
  sTeilLen = min<uint16_t>(S_TEIL, sStueck - sTeil);
  sAntwortDa = false;
  sSenden("d c6 a5 04 83 %02x %02x %02x", (S_PUFFER + sTeil) >> 8, (S_PUFFER + sTeil) & 0xFF, sTeilLen);
  sZ = S_ANTWORT;
  sSeit = jetzt;
}

static void sichernLoop() {
  if (sZ == S_AUS) return;
  uint32_t jetzt = millis();
  switch (sZ) {
    case S_START: {
      uint32_t a = sAdresse;
      sStueck = min<uint32_t>(S_STUECK, sEnde - sAdresse);
      sTeil = 0;
      sHatVorher = false;
      if (sDatenbank)  // 80 56 5A A0 <adr 4> <vp 2> <len 2>
        sSenden("d c6 a5 0c 80 56 5a a0 %02x %02x %02x %02x 10 00 %02x %02x", (unsigned)(a >> 24) & 0xFF,
                (unsigned)(a >> 16) & 0xFF, (unsigned)(a >> 8) & 0xFF, (unsigned)a & 0xFF, sStueck >> 8, sStueck & 0xFF);
      else  // 80 40 5A A0 <lib> <adr 3> <vp 2> <len 2>
        sSenden("d c6 a5 0c 80 40 5a a0 %02x %02x %02x %02x 10 00 %02x %02x", sLib, (unsigned)(a >> 16) & 0xFF,
                (unsigned)(a >> 8) & 0xFF, (unsigned)a & 0xFF, sStueck >> 8, sStueck & 0xFF);
      sZ = S_WARTEN;
      sSeit = jetzt;
      sRegDa = false;
      break;
    }
    case S_WARTEN:  // bis En_Lib_OP / En_DBL_OP wieder 0 ist
      if (sRegDa && sRegWert == 0) {
        sTeilHolen(jetzt);
      } else if (jetzt - sSeit > 1000) {
        sZ = S_START;  // nochmal
        if (++sVersuch > 6) sFehler();
      } else if (jetzt - sSeit > 15 && jetzt - sAbfrage > 15) {
        sAbfrage = jetzt;
        sRegDa = false;
        sSenden("d c6 a5 03 81 %02x 01", sDatenbank ? 0x56 : 0x40);
      }
      break;
    case S_ANTWORT:
      if (sAntwortDa) {
        if (sHatVorher && !memcmp(sAntwort, sVorher, 2 * sTeilLen)) {
          for (uint16_t b = 0; b < sTeilLen; b += S_BLOCK) {
            char h[2 * 2 * S_BLOCK + 1];
            for (size_t i = 0; i < 2 * S_BLOCK; i++) sprintf(h + 2 * i, "%02x", sAntwort[2 * b + i]);
            char z[200];
            snprintf(z, sizeof z, "#S %s %u %lu %s", sDatenbank ? "db" : "lib", sLib,
                     (unsigned long)(sAdresse + sTeil + b), h);
            usbAntwort(z);  // wartet kurz, verliert keine Daten
          }
          sHatVorher = false;
          sVersuch = 0;
          sTeil += sTeilLen;
          if (sTeil < sStueck) {
            sTeilHolen(jetzt);
            return;
          }
          sAdresse += sStueck;
          if (sAdresse >= sEnde) {
            Serial.println("#S fertig");
            sZ = S_AUS;
            return;
          }
          sZ = S_START;
        } else {  // erste Lesung oder Abweichung: (noch) einmal holen
          memcpy(sVorher, sAntwort, 2 * sTeilLen);
          sHatVorher = true;
          if (++sVersuch > 8) {
            sFehler();
            return;
          }
          sTeilHolen(jetzt);
        }
      } else if (jetzt - sSeit > 300) {
        if (++sVersuch > 8) sFehler();
        else sTeilHolen(jetzt);
      }
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
