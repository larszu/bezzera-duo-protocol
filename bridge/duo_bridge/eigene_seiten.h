#pragma once
// Eigene Seiten aus tools/seitenbau auf den freien Plaetzen 96-99, 196-199, 296-299.
//
// Tasten mit "Bridge-Aktion" schreiben ihren Wert in VP 0x6100 (Tastencode
// FD05, das Display meldet ihn nicht von selbst). Steht das Display auf einer
// eigenen Seite, liest die Bridge VP 0x6100 alle 150 ms, fuehrt die Aktion aus
// und setzt ihn auf 0 zurueck. Der Wert bleibt im Display stehen, bis die
// Bridge ihn abholt; ein Druck geht also auch dann nicht verloren, wenn sie
// den Seitenwechsel erst mit dem naechsten Nachlesen (2 s) bemerkt.
//
// Werte der Bridge fuer Zahlenanzeigen (alle 500 ms, nur auf eigenen Seiten):
//   0x6110 Gewicht der Waage, g x 10      0x6111 Bezugszeit, s x 10
//   0x6112 Bezuege seit Rueckspuelen      0x6113 Bezuege gesamt (bis 65535)
//   0x6114 Uhrzeit hhmm                   0x6115 aktives Profil (1-20, 0 = keins)
// Werte des Mainboards (z. B. 0x0053 Kaffeekessel) zeigt das Display ohnehin.

static const uint16_t ES_VP_AKTION = 0x6100, ES_VP_WERTE = 0x6110;

// Aktionen (Wert der Taste): siehe auch ES_AKTIONEN in tools/seitenbau/index.html
//   1 Ein   2 Standby   3 Bezug stoppen   4 Waage tarieren   5 Doom
//   10-29 Profil 1-20 anwenden
static bool eigeneSeite(int32_t s) {
  if (s < 0) return false;
  int r = s % 100;
  return r >= 96 && r <= 99 && s != doomEinst.seite;
}

static void esAktion(uint16_t w) {
  ereignis("- eigene Seite: Aktion %u", w);
  if (w == 1) aktion("an");
  else if (w == 2) aktion("aus");
  else if (w == 3) aktion("stopp");
  else if (w == 4) aktion("tara");
  else if (w == 5) doomStarten("eigene Seite");
  else if (w >= 10 && w < 10 + PROFILE_MAX) profilAnwenden(w - 10);
}

// aus rahmenVomDisplay: Antwort auf das Lesen von VP 0x6100
static void eigeneSeitenAntwort(const uint8_t *r, size_t n) {
  if (n < 9 || r[3] != 0x83 || ((r[4] << 8) | r[5]) != ES_VP_AKTION) return;
  uint16_t w = (r[7] << 8) | r[8];
  if (!w) return;
  uint8_t null[] = {KOPF0, KOPF1, 0x05, 0x82, ES_VP_AKTION >> 8, ES_VP_AKTION & 0xFF, 0, 0};
  uartB.write(null, sizeof null);
  esAktion(w);
}

static void eigeneSeitenLoop() {
  if (doomAktiv || emulation == 1 || !eigeneSeite(leitung.seite)) return;
  static uint32_t lesen = 0, schreiben = 0;
  uint32_t jetzt = millis();
  if (jetzt - lesen >= 150) {
    lesen = jetzt;
    uint8_t f[] = {KOPF0, KOPF1, 0x04, 0x83, ES_VP_AKTION >> 8, ES_VP_AKTION & 0xFF, 0x01};
    eigeneAnfrageMerken(f, sizeof f);
    uartB.write(f, sizeof f);
  }
  if (jetzt - schreiben >= 500) {
    schreiben = jetzt;
    struct tm t;
    bool zeit = jetztLokal(t);
    float g = bezugG;
    uint16_t w[6] = {
        (uint16_t)(isnan(g) || g < 0 ? 0 : lroundf(g * 10)),
        (uint16_t)(bezugZustand == BZ_LAEUFT ? (millis() - bezugStartMs) / 100 : 0),
        (uint16_t)zaehler.seitRueckspuelen,
        (uint16_t)min<uint32_t>(bezuegeMaschine(), 65535),
        (uint16_t)(zeit ? t.tm_hour * 100 + t.tm_min : 0),
        (uint16_t)(profilAktiv >= 0 ? profilAktiv + 1 : 0),
    };
    uint8_t f[6 + 12] = {KOPF0, KOPF1, 3 + 12, 0x82, ES_VP_WERTE >> 8, ES_VP_WERTE & 0xFF};
    for (int i = 0; i < 6; i++) {
      f[6 + 2 * i] = w[i] >> 8;
      f[7 + 2 * i] = w[i];
    }
    uartB.write(f, sizeof f);
  }
}
