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

enum SZustand : uint8_t { S_AUS, S_START, S_WARTEN, S_PRUEF_A, S_PRUEF_E, S_ANTWORT };
static SZustand sZ = S_AUS;
static bool sDatenbank = false;
static uint16_t sLib = 0;
static uint32_t sAdresse = 0, sEnde = 0, sSeit = 0, sAbfrage = 0, sAktivMs = 0;
static uint16_t sStueck = 0, sTeil = 0, sGroesse = 0, sVor = 0;
static uint16_t sErwartVp = 0, sPruefA = 0;
static uint8_t sErwartLen = 0, sVersuch = 0, sFehl = 0, sAntwort[2 * 96], sVorher[2 * 96];
static bool sAntwortDa = false, sRegDa = false, sHatVorher = false;
static uint8_t sRegWert = 0xFF;

// Das Display braucht je Lesevorgang ~50-100 ms, fast unabhaengig von der
// Menge. Deshalb bis 2048 Worte auf einmal in den Puffer (VP 0x1000-0x17FF)
// und in Teilen zu 96 Worten ueber die UART holen; jeder Teil wird zweimal
// geholt und nur bei gleichem Ergebnis ausgegeben.
//
// Gemessen (Display 2.2): Ein Lesevorgang, der genau bei Wort 0x1000 (bzw.
// 0x0FE0-0x1000) beginnt, laeuft gar nicht; einer, der 0x1000 ueberquert,
// bricht nach 256 Worten ab. Erkennung allgemein: Marken ins erste und letzte
// Pufferwort; steht eine danach noch da, wird der Block kleiner bzw. frueher
// beginnend wiederholt.
static const uint16_t S_PUFFER = 0x1000, S_STUECK = 2048, S_TEIL = 96, S_MARKE = 0xB17E;
static const uint8_t S_BLOCK = 32;  // Worte je Ausgabezeile

static bool sichernStandby() { return sZ != S_AUS || (sAktivMs && millis() - sAktivMs < 10000); }

// aus rahmenVomDisplay: Antworten auf eigene Lese- und Registeranfragen
static void sichernAntwort(const uint8_t *r, size_t n) {
  if (sZ == S_AUS || n < 7) return;
  if (r[3] == 0x83 && ((r[4] << 8) | r[5]) == sErwartVp && r[6] == sErwartLen && n >= 7 + 2u * sErwartLen) {
    memcpy(sAntwort, r + 7, 2 * sErwartLen);
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

static void sHolen(uint16_t vp, uint8_t len, SZustand z, uint32_t jetzt) {
  sErwartVp = vp;
  sErwartLen = len;
  sAntwortDa = false;
  sSenden("d c6 a5 04 83 %02x %02x %02x", vp >> 8, vp & 0xFF, len);
  sZ = z;
  sSeit = jetzt;
}

static void sTeilHolen(uint32_t jetzt) {
  sHolen(S_PUFFER + sVor + sTeil, min<uint16_t>(S_TEIL, sStueck - sTeil), S_ANTWORT, jetzt);
}

static void sMarke(uint16_t vp) { sSenden("d c6 a5 05 82 %02x %02x %02x %02x", vp >> 8, vp & 0xFF, S_MARKE >> 8, S_MARKE & 0xFF); }

static void sichernLoop() {
  if (sZ == S_AUS) return;
  uint32_t jetzt = millis();
  sAktivMs = jetzt;
  switch (sZ) {
    case S_START: {
      if (sVor > sAdresse) sVor = 0;
      sStueck = min<uint32_t>(sGroesse - sVor, sEnde - sAdresse);
      uint32_t a = sAdresse - sVor, len = sStueck + sVor;
      sTeil = 0;
      sHatVorher = false;
      sMarke(S_PUFFER + sVor);
      sMarke(S_PUFFER + sVor + sStueck - 1);
      if (sDatenbank)  // 80 56 5A A0 <adr 4> <vp 2> <len 2>
        sSenden("d c6 a5 0c 80 56 5a a0 %02x %02x %02x %02x 10 00 %02x %02x", (unsigned)(a >> 24) & 0xFF,
                (unsigned)(a >> 16) & 0xFF, (unsigned)(a >> 8) & 0xFF, (unsigned)a & 0xFF, len >> 8, len & 0xFF);
      else  // 80 40 5A A0 <lib> <adr 3> <vp 2> <len 2>
        sSenden("d c6 a5 0c 80 40 5a a0 %02x %02x %02x %02x 10 00 %02x %02x", sLib, (unsigned)(a >> 16) & 0xFF,
                (unsigned)(a >> 8) & 0xFF, (unsigned)a & 0xFF, len >> 8, len & 0xFF);
      sZ = S_WARTEN;
      sSeit = jetzt;
      sRegDa = false;
      break;
    }
    case S_WARTEN:  // bis En_Lib_OP / En_DBL_OP wieder 0 ist
      if (sRegDa && sRegWert == 0) {
        sHolen(S_PUFFER + sVor, 1, S_PRUEF_A, jetzt);
      } else if (jetzt - sSeit > 1000) {
        sZ = S_START;  // nochmal
        if (++sVersuch > 6) sFehler();
      } else if (jetzt - sSeit > 15 && jetzt - sAbfrage > 15) {
        sAbfrage = jetzt;
        sRegDa = false;
        sSenden("d c6 a5 03 81 %02x 01", sDatenbank ? 0x56 : 0x40);
      }
      break;
    case S_PRUEF_A:
    case S_PRUEF_E:
      if (sAntwortDa) {
        uint16_t w = (sAntwort[0] << 8) | sAntwort[1];
        if (sZ == S_PRUEF_A) {
          sPruefA = w;
          sHolen(S_PUFFER + sVor + sStueck - 1, 1, S_PRUEF_E, jetzt);
        } else if ((sPruefA == S_MARKE || w == S_MARKE) && ++sFehl < 7) {
          // Lesevorgang lief nicht oder nur teilweise: frueher beginnen, kleiner lesen
          static const uint16_t VOR[] = {0, 0, 64, 64, 64, 128, 128}, GROESSE[] = {2048, 2048, 2048, 256, 96, 256, 160};
          sVor = VOR[sFehl];
          sGroesse = GROESSE[sFehl];
          sZ = S_START;
        } else {
          sTeilHolen(jetzt);
        }
      } else if (jetzt - sSeit > 300) {
        if (++sVersuch > 8) sFehler();
        else sZ = S_START;
      }
      break;
    case S_ANTWORT:
      if (sAntwortDa) {
        if (sHatVorher && !memcmp(sAntwort, sVorher, 2 * sErwartLen)) {
          for (uint16_t b = 0; b < sErwartLen; b += S_BLOCK) {
            char h[2 * 2 * S_BLOCK + 1];
            for (size_t i = 0; i < 2 * S_BLOCK; i++) sprintf(h + 2 * i, "%02x", sAntwort[2 * b + i]);
            char z[200];
            snprintf(z, sizeof z, "#S %s %u %lu %s", sDatenbank ? "db" : "lib", sLib,
                     (unsigned long)(sAdresse + sTeil + b), h);
            usbAntwort(z);  // wartet kurz, verliert keine Daten
          }
          sHatVorher = false;
          sVersuch = 0;
          sTeil += sErwartLen;
          if (sTeil < sStueck) {
            sTeilHolen(jetzt);
            return;
          }
          sAdresse += sStueck;
          sVor = 0;
          sFehl = 0;
          sGroesse = S_STUECK;
          if (sAdresse >= sEnde) {
            Serial.println("#S fertig");
            sZ = S_AUS;
            return;
          }
          sZ = S_START;
        } else {  // erste Lesung oder Abweichung: (noch) einmal holen
          memcpy(sVorher, sAntwort, 2 * sErwartLen);
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
  sVor = 0;
  sFehl = 0;
  sGroesse = S_STUECK;
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
