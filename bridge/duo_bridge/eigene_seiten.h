#pragma once
// Eigene Seiten aus tools/seitenbau auf den freien Plaetzen 96-99, 196-199, 296-299.
//
// Tasten mit "Bridge-Aktion" schreiben ihren Wert in VP 0x0300 (Tastencode
// FD05, das Display meldet ihn nicht von selbst). Steht das Display auf einer
// eigenen Seite, liest die Bridge VP 0x0300 alle 150 ms, fuehrt die Aktion aus
// und setzt ihn auf 0 zurueck. Der Wert bleibt im Display stehen, bis die
// Bridge ihn abholt; ein Druck geht also auch dann nicht verloren, wenn sie
// den Seitenwechsel erst mit dem naechsten Nachlesen (2 s) bemerkt.
//
// Werte der Bridge fuer Zahlenanzeigen (alle 500 ms, nur auf eigenen Seiten):
//   0x0310 Gewicht der Waage, g x 10      0x0311 Bezugszeit, s x 10
//   0x0312 Bezuege seit Rueckspuelen      0x0313 Bezuege gesamt (bis 65535)
//   0x0314 Uhrzeit hhmm                   0x0315 aktives Profil (1-20, 0 = keins)
//   0x0316 Hoechstdruck, bar x 10         0x0317 mittlere Bruehtemperatur, °C x 10
//   (beide vom laufenden bzw. letzten Bezug)
// Werte des Mainboards (z. B. 0x0053 Kaffeekessel) zeigt das Display ohnehin.

// Das Display kennt nur VPs 0x0000-0x3FFF (hoehere werden abgeschnitten); Bezzera nutzt bis 0x010A.
static const uint16_t ES_VP_AKTION = 0x0300, ES_VP_WERTE = 0x0310;

// Aktionen (Wert der Taste): siehe auch ES_AKTIONEN in tools/seitenbau/index.html
//   1 Ein   2 Standby   3 Bezug stoppen   4 Waage tarieren
//   10-29 Profil 1-20 anwenden
static bool eigeneSeite(int32_t s) {
  if (s < 0) return false;
  int r = s % 100;
  return r >= 96 && r <= 99;
}

static void esAktion(uint16_t w) {
  ereignis("- eigene Seite: Aktion %u", w);
  if (w == 1) aktion("an");
  else if (w == 2) aktion("aus");
  else if (w == 3) aktion("stopp");
  else if (w == 4) aktion("tara");
  else if (w >= 10 && w < 10 + PROFILE_MAX) profilAnwenden(w - 10);
}

// aus rahmenVomDisplay: Antwort auf das Lesen von VP 0x0300
static void eigeneSeitenAntwort(const uint8_t *r, size_t n) {
  if (n < 9 || r[3] != 0x83 || ((r[4] << 8) | r[5]) != ES_VP_AKTION) return;
  uint16_t w = (r[7] << 8) | r[8];
  if (!w) return;
  uint8_t null[] = {KOPF0, KOPF1, 0x05, 0x82, ES_VP_AKTION >> 8, ES_VP_AKTION & 0xFF, 0, 0};
  uartB.write(null, sizeof null);
  esAktion(w);
}

// ─── Bruehkurve ────────────────────────────────────────────────────────────
// Druck und Temperatur des laufenden bzw. letzten Bezugs (shots.h, 10 Punkte
// je Sekunde) als zwei Linien. Die Seite braucht dafuer zwei Zeichenflaechen
// (Basic Graphics 0x21) auf VP 0x0400 (Druck) und 0x0480 (Temperatur); Achsen
// und Beschriftung stehen im Hintergrundbild (tools/seitenbau/beispiele/
// bruehkurve.json). Linienbefehl 0x0002: Anzahl Strecken, Farbe, Punkte.
static const uint16_t BK_VP_DRUCK = 0x0400, BK_VP_TEMP = 0x0480;
static const int32_t BK_SEITE = 196;

// Steht das Display auf der Bruehkurve, bleibt es dort, wenn das Mainboard beim
// Bezug auf seine Ausgabeseite (x06) und danach zur Startseite (x01) schalten
// will; der Rahmen geht nicht ans Display. Alarme und Standby kommen durch. Das
// Mainboard liest die Seite nie zurueck (alle Mitschnitte), merkt es also nicht.
static bool kurveHalten(const uint8_t *r, size_t n) {
  if (n != 7 || r[3] != 0x80 || r[4] != 0x03 || leitung.seite != BK_SEITE) return false;
  uint16_t s = (r[5] << 8) | r[6];
  if (s % 100 != 6 && s % 100 != 1) return false;
  leitung.seiteMb = s;  // Bezugserkennung (shots.h, zusatz.h) folgt dem Mainboard
  ereignis("- Bruehkurve bleibt (Mainboard wollte Seite %u)", s);
  return true;
}
static const int BK_X0 = 36, BK_Y0 = 34, BK_B = 248, BK_H = 150;  // Plotbereich, muss zum Bild passen
static const int BK_SEK = 45, BK_PUNKTE = 60;
static const float BK_DRUCK_MAX = 12, BK_T_MIN = 80, BK_T_MAX = 100;
static const uint16_t BK_FARBE_DRUCK = 0x3D7F, BK_FARBE_TEMP = 0xFC60;  // Blau, Orange (RGB565)

static void bkLinie(uint16_t vp, uint16_t farbe, const uint16_t *pkt, int n) {
  uint8_t f[6 + 2 * (3 + 2 * BK_PUNKTE)] = {KOPF0, KOPF1, 0, 0x82, (uint8_t)(vp >> 8), (uint8_t)vp};
  uint16_t w[3 + 2 * BK_PUNKTE] = {0x0002, (uint16_t)(n > 1 ? n - 1 : 0), farbe};
  memcpy(w + 3, pkt, 4 * n);
  int k = 3 + 2 * n;
  for (int i = 0; i < k; i++) {
    f[6 + 2 * i] = w[i] >> 8;
    f[7 + 2 * i] = w[i];
  }
  f[2] = 3 + 2 * k;
  uartB.write(f, 6 + 2 * k);
}

static void bruehkurveZeichnen(bool erzwingen) {
  static uint32_t stand = UINT32_MAX;
  int slot = shotSlot;
  int len = kurven ? kurvenLaenge[slot] : 0;
  uint32_t neu = (shotNr << 12) ^ len;
  if (neu == stand && !erzwingen) return;  // nichts Neues: Display behaelt die Linien
  stand = neu;
  uint16_t druck[2 * BK_PUNKTE], temp[2 * BK_PUNKTE];
  int nd = 0, nt = 0, letzt = -1000;
  for (int i = 0; i < len && nd < BK_PUNKTE; i++) {
    const ShotPunkt &p = kurven[slot * KURVE_MAX + i];
    if (p.t10 > BK_SEK * 10) break;
    if (p.t10 - letzt < BK_SEK * 10 / BK_PUNKTE && i != len - 1) continue;
    letzt = p.t10;
    uint16_t x = BK_X0 + p.t10 * BK_B / (BK_SEK * 10);
    float d = constrain(p.druck10 / 10.0f, 0.0f, BK_DRUCK_MAX);
    druck[2 * nd] = x;
    druck[2 * nd + 1] = BK_Y0 + BK_H - lroundf(d * BK_H / BK_DRUCK_MAX);
    nd++;
    if (p.temp != INT16_MIN && nt < BK_PUNKTE) {
      float t = constrain((float)p.temp, BK_T_MIN, BK_T_MAX);
      temp[2 * nt] = x;
      temp[2 * nt + 1] = BK_Y0 + BK_H - lroundf((t - BK_T_MIN) * BK_H / (BK_T_MAX - BK_T_MIN));
      nt++;
    }
  }
  bkLinie(BK_VP_DRUCK, BK_FARBE_DRUCK, druck, nd);
  bkLinie(BK_VP_TEMP, BK_FARBE_TEMP, temp, nt);
}

// Taste zur Bruehkurve auf der Startseite (x01), im Stil der Menuetaste: cyaner
// Rahmen (Basic Graphics 0x0003, zwei Rechtecke = 2 px) mit weisser Kurve
// (0x0002). Touch-Flaeche und Zeichenflaechen VP 0x0500/0x0580 kommen aus
// tools/seitenbau/beispiele/start_*.json.
static const uint16_t FARBE_CYAN = 0x3E7F, FARBE_WEISS = 0xFFFF;
static void startSymbol() {
  static int32_t seite = -1;
  static uint32_t zuletzt = 0;
  if (leitung.seite < 0 || leitung.seite % 100 != 1) {
    seite = -1;
    return;
  }
  if (seite == leitung.seite && millis() - zuletzt < 5000) return;
  seite = leitung.seite;
  zuletzt = millis();
  const uint16_t rahmen[] = {0x0003, 2, 254, 8, 306, 48, FARBE_CYAN, 255, 9, 305, 47, FARBE_CYAN};
  uint8_t f[6 + sizeof rahmen] = {KOPF0, KOPF1, (uint8_t)(3 + sizeof rahmen), 0x82, 0x05, 0x00};
  for (size_t i = 0; i < sizeof rahmen / 2; i++) {
    f[6 + 2 * i] = rahmen[i] >> 8;
    f[7 + 2 * i] = rahmen[i];
  }
  uartB.write(f, sizeof f);
  static const uint16_t kurve[] = {260, 40, 266, 40, 270, 22, 277, 16, 286, 16, 292, 20, 300, 20};
  bkLinie(0x0580, FARBE_WEISS, kurve, 7);
}

static void eigeneSeitenLoop() {
  if (emulation == 1) return;
  startSymbol();
  if (!eigeneSeite(leitung.seite)) return;
  static uint32_t lesen = 0, schreiben = 0;
  uint32_t jetzt = millis();
  if (jetzt - lesen >= 150) {
    lesen = jetzt;
    uint8_t f[] = {KOPF0, KOPF1, 0x04, 0x83, ES_VP_AKTION >> 8, ES_VP_AKTION & 0xFF, 0x01};
    eigeneAnfrageMerken(f, sizeof f);
    uartB.write(f, sizeof f);
  }
  static int32_t seiteVorher = -1;
  if (leitung.seite != seiteVorher) {  // neu auf der Seite: Linien neu schicken
    seiteVorher = leitung.seite;
    schreiben = 0;
  }
  if (jetzt - schreiben >= 500) {
    bruehkurveZeichnen(!schreiben);
    schreiben = jetzt;
    struct tm t;
    bool zeit = jetztLokal(t);
    float g = bezugG;
    uint16_t w[8] = {
        (uint16_t)(isnan(g) || g < 0 ? 0 : lroundf(g * 10)),
        (uint16_t)(bezugZustand == BZ_LAEUFT ? (millis() - bezugStartMs) / 100 : 0),
        (uint16_t)zaehler.seitRueckspuelen,
        (uint16_t)min<uint32_t>(bezuegeMaschine(), 65535),
        (uint16_t)(zeit ? t.tm_hour * 100 + t.tm_min : 0),
        (uint16_t)(profilAktiv >= 0 ? profilAktiv + 1 : 0),
        (uint16_t)lroundf(shotDruckMax * 10),
        (uint16_t)(shotTempN ? lroundf(shotTempSumme / shotTempN * 10) : 0),
    };
    uint8_t f[6 + 16] = {KOPF0, KOPF1, 3 + 16, 0x82, ES_VP_WERTE >> 8, ES_VP_WERTE & 0xFF};
    for (int i = 0; i < 8; i++) {
      f[6 + 2 * i] = w[i] >> 8;
      f[7 + 2 * i] = w[i];
    }
    uartB.write(f, sizeof f);
  }
}
