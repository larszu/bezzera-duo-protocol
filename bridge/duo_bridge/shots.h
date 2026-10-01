#pragma once
// Shot-Log, Bereit-Anzeige, Zaehler und Erinnerungen.
//
// Shot-Erkennung ohne Waage: Waehrend eines Bezugs schaltet das Mainboard
// auf den Ausgabezaehler (Seite x06, CRONO an) und schreibt in VP 0x0059 die
// Sekunden seit Start, in VP 0x0055 den Pumpendruck (x0,5 bar). Mit Waage
// kommen Gewicht und Durchfluss aus zusatz.h dazu.
//
// Zaehler: Die Gesamtbezuege der Maschine stehen in VP 0x0082 (Technikmenue,
// "Abgaben gesamt"); die Bridge merkt sich den zuletzt gesehenen Wert und
// zaehlt eigene erkannte Bezuege dazu. Rueckspuelen wird erkannt (Seiten
// x67/x87 "Waschen …"), dann beginnt der Zaehler bis zur naechsten Erinnerung neu.

static const int SHOT_LOG = 50;      // Eintraege im NVS
static const int SHOT_KURVEN = 10;   // Kurven im PSRAM
static const int KURVE_MAX = 900;    // 90 s bei 10 Punkten je Sekunde

struct ShotPunkt {
  uint16_t t10;
  int16_t druck10, temp, g10;  // INT16_MIN = unbekannt
};
struct ShotEintrag {
  char zeit[17];   // "2026-09-30 20:14" aus der Display-Uhr
  float dauer, gewicht, druckMax, temp;
  int8_t profil;
  uint8_t sterne;  // 0 = unbewertet, 1..5
  char notiz[48];
  uint32_t nr;     // laufende Nummer, auch Schluessel der Kurve
};

static ShotEintrag shotLog[SHOT_LOG];
static int shotLogN = 0;  // belegt, neueste zuerst
static uint32_t shotNr = 0;
static ShotPunkt *kurven = nullptr;  // SHOT_KURVEN x KURVE_MAX
static uint16_t kurvenLaenge[SHOT_KURVEN];
static uint32_t kurvenNr[SHOT_KURVEN];

struct Zaehler {
  uint32_t maschineBasis;   // letzter Wert aus VP 0x0082
  uint32_t seitBasis;       // seitdem von der Bridge gezaehlte Bezuege
  uint32_t seitRueckspuelen;
  uint16_t rueckspuelenAlle;  // Erinnerung nach so vielen Bezuegen, 0 = aus
};
static Zaehler zaehler = {0, 0, 0, 60};

static void shotsSpeichern() {
  Preferences p;
  p.begin("shots", false);
  p.putBytes("log", shotLog, sizeof(ShotEintrag) * shotLogN);
  p.putUInt("nr", shotNr);
  p.putBytes("zaehler", &zaehler, sizeof zaehler);
  p.end();
}

static void shotsSetup() {
  Preferences p;
  p.begin("shots", true);
  size_t n = p.getBytesLength("log");
  if (n % sizeof(ShotEintrag) == 0 && n <= sizeof shotLog) {
    p.getBytes("log", shotLog, n);
    shotLogN = n / sizeof(ShotEintrag);
  }
  shotNr = p.getUInt("nr", 0);
  if (p.getBytesLength("zaehler") == sizeof zaehler) p.getBytes("zaehler", &zaehler, sizeof zaehler);
  p.end();
  if (psramFound()) kurven = (ShotPunkt *)ps_calloc(SHOT_KURVEN * KURVE_MAX, sizeof(ShotPunkt));
}

// ─── laufender Shot ───────────────────────────────────────────────────────
static bool shotLaeuft = false;
static uint32_t shotStartMs = 0;
static int shotSlot = 0;
static float shotDruckMax = 0, shotTempSumme = 0;
static int shotTempN = 0;

static uint32_t bezuegeMaschine() { return zaehler.maschineBasis ? zaehler.maschineBasis + zaehler.seitBasis : 0; }

static void shotBeginnen() {
  shotLaeuft = true;
  shotStartMs = millis();
  shotSlot = (shotNr) % SHOT_KURVEN;
  kurvenLaenge[shotSlot] = 0;
  kurvenNr[shotSlot] = shotNr + 1;
  shotDruckMax = 0;
  shotTempSumme = 0;
  shotTempN = 0;
  ereignis("- Shot beginnt");
}

static void shotBeenden() {
  shotLaeuft = false;
  float dauer = vpRam[0x59] ? vpRam[0x59] : (millis() - shotStartMs) / 1000.0f;
  if (dauer < 5) {  // Spuelen der Gruppe o. ae., kein Shot
    ereignis("- Ausgabe unter 5 s, nicht gezaehlt");
    return;
  }
  ShotEintrag e = {};
  if (leitung.rtcGueltig)
    snprintf(e.zeit, sizeof e.zeit, "20%02x-%02x-%02x %02x:%02x", leitung.rtc[0], leitung.rtc[1], leitung.rtc[2], leitung.rtc[4],
             leitung.rtc[5]);
  e.dauer = dauer;
  const BezugInfo *l = bezugListeN ? &bezugListe[(bezugListeN - 1) % 10] : nullptr;
  e.gewicht = l && millis() - l->endeMs < 20000 ? l->g : NAN;
  e.druckMax = shotDruckMax;
  e.temp = shotTempN ? shotTempSumme / shotTempN : NAN;
  e.profil = profilAktiv;
  e.nr = ++shotNr;
  memmove(&shotLog[1], &shotLog[0], sizeof(ShotEintrag) * (SHOT_LOG - 1));
  shotLog[0] = e;
  if (shotLogN < SHOT_LOG) shotLogN++;
  zaehler.seitBasis++;
  zaehler.seitRueckspuelen++;
  shotsSpeichern();
  ereignis("- Shot %lu: %.0f s, max %.1f bar", (unsigned long)e.nr, e.dauer, e.druckMax);
}

static void shotsLoop() {
  static uint32_t zuletzt = 0;
  // Gesamtbezuege uebernehmen, sobald das Mainboard sie schreibt (Technikmenue)
  if (vpMainboardMs[0x82] && vpRam[0x82] && vpRam[0x82] != zaehler.maschineBasis &&
      vpRam[0x82] >= zaehler.maschineBasis) {
    zaehler.maschineBasis = vpRam[0x82];
    zaehler.seitBasis = 0;
    shotsSpeichern();
  }
  bool aufAusgabe = mainboardLebt() && leitung.seiteMb >= 0 && leitung.seiteMb % 100 == 6;
  static uint32_t wegSeit = 0;
  if (aufAusgabe) wegSeit = 0;
  if (!shotLaeuft && aufAusgabe) shotBeginnen();
  if (shotLaeuft && !aufAusgabe) {  // kurz weg (Seitenwechsel) ist noch kein Ende
    if (!wegSeit) wegSeit = millis();
    if (millis() - wegSeit > 1500) shotBeenden();
  }
  // Rueckspuelen erkannt: "Waschen …" (x67 kurz, x87 komplett)
  static bool spuelt = false;
  bool jetzt = leitung.seite >= 0 && (leitung.seite % 100 == 67 || leitung.seite % 100 == 87);
  if (jetzt && !spuelt) {
    zaehler.seitRueckspuelen = 0;
    shotsSpeichern();
    ereignis("- Rückspülen erkannt, Zähler zurückgesetzt");
  }
  spuelt = jetzt;
  if (!shotLaeuft || millis() - zuletzt < 100) return;
  zuletzt = millis();
  float druck = vpRam[0x55] / 2.0f;
  float temp = (int16_t)vpRam[0x53];
  if (druck > shotDruckMax) shotDruckMax = druck;
  if (temp > 0) {
    shotTempSumme += temp;
    shotTempN++;
  }
  if (!kurven || kurvenLaenge[shotSlot] >= KURVE_MAX) return;
  ShotPunkt &p = kurven[shotSlot * KURVE_MAX + kurvenLaenge[shotSlot]++];
  p.t10 = (millis() - shotStartMs) / 100;
  p.druck10 = lroundf(druck * 10);
  p.temp = temp > 0 ? (int16_t)temp : INT16_MIN;
  p.g10 = bezugZustand == BZ_LAEUFT ? (int16_t)lroundf(bezugG * 10) : INT16_MIN;
}

// ─── Bereit ───────────────────────────────────────────────────────────────
// Kaffeekessel seit 60 s innerhalb 1 °C am Sollwert (VP 0x0060; unbekannt: 93)
static bool maschineBereit() {
  static uint32_t nahSeit = 0;
  float t = tempKaffee();
  int soll = vpRam[0x60] >= 85 && vpRam[0x60] <= 100 ? vpRam[0x60] : 93;
  bool nah = maschineAn() && !isnan(t) && fabsf(t - soll) <= 1;
  if (!nah) nahSeit = 0;
  else if (!nahSeit) nahSeit = millis();
  return nahSeit && millis() - nahSeit > 60000;
}

static String shotsJson() {
  String j;
  j.reserve(200 + shotLogN * 180);
  j += "{\"laeuft\":";
  j += shotLaeuft ? "true" : "false";
  j += ",\"t\":";
  j += shotLaeuft ? String(vpRam[0x59] ? vpRam[0x59] : (millis() - shotStartMs) / 1000) : String("null");
  j += ",\"bezuege_maschine\":";
  j += bezuegeMaschine();
  j += ",\"seit_rueckspuelen\":";
  j += zaehler.seitRueckspuelen;
  j += ",\"rueckspuelen_alle\":";
  j += zaehler.rueckspuelenAlle;
  j += ",\"log\":[";
  for (int i = 0; i < shotLogN; i++) {
    const ShotEintrag &e = shotLog[i];
    if (i) j += ',';
    j += "{\"nr\":";
    j += e.nr;
    j += ",\"zeit\":";
    jsonText(j, e.zeit);
    j += ",\"dauer\":";
    jsonZahl(j, e.dauer, 0);
    j += ",\"gewicht\":";
    jsonZahl(j, e.gewicht, 1);
    j += ",\"druck_max\":";
    jsonZahl(j, e.druckMax, 1);
    j += ",\"temp\":";
    jsonZahl(j, e.temp, 0);
    j += ",\"profil\":";
    if (e.profil >= 0 && e.profil < PROFILE_MAX && profile[e.profil].belegt) jsonText(j, profile[e.profil].name);
    else j += "null";
    j += ",\"sterne\":";
    j += e.sterne;
    j += ",\"notiz\":";
    jsonText(j, e.notiz);
    bool kurve = false;
    for (int k = 0; k < SHOT_KURVEN; k++)
      if (kurven && kurvenNr[k] == e.nr && kurvenLaenge[k]) kurve = true;
    j += ",\"kurve\":";
    j += kurve ? "true" : "false";
    j += '}';
  }
  j += "]}";
  return j;
}

static String shotKurveJson(uint32_t nr) {
  String j = "[";
  for (int k = 0; kurven && k < SHOT_KURVEN; k++) {
    if (kurvenNr[k] != nr) continue;
    for (int i = 0; i < kurvenLaenge[k]; i++) {
      const ShotPunkt &p = kurven[k * KURVE_MAX + i];
      if (i) j += ',';
      j += '[';
      j += p.t10 / 10.0f;
      j += ',';
      if (p.druck10 == INT16_MIN) j += "null"; else j += p.druck10 / 10.0f;
      j += ',';
      if (p.temp == INT16_MIN) j += "null"; else j += p.temp;
      j += ',';
      if (p.g10 == INT16_MIN) j += "null"; else j += p.g10 / 10.0f;
      j += ']';
    }
  }
  return j + "]";
}

// "bewerten <nr> <sterne> <notiz>" | "rueckspuelen_erledigt" | "rueckspuelen_alle <n>"
static bool shotAktion(const String &a) {
  if (a.startsWith("bewerten ")) {
    int s1 = a.indexOf(' ', 9), s2 = s1 < 0 ? -1 : a.indexOf(' ', s1 + 1);
    uint32_t nr = a.substring(9, s1 < 0 ? a.length() : s1).toInt();
    for (int i = 0; i < shotLogN; i++) {
      if (shotLog[i].nr != nr) continue;
      if (s1 > 0) shotLog[i].sterne = constrain(a.substring(s1 + 1, s2 < 0 ? a.length() : s2).toInt(), 0, 5);
      if (s2 > 0) strlcpy(shotLog[i].notiz, a.c_str() + s2 + 1, sizeof shotLog[i].notiz);
      shotsSpeichern();
      return true;
    }
    return false;
  }
  if (a == "rueckspuelen_erledigt") {
    zaehler.seitRueckspuelen = 0;
    shotsSpeichern();
    return true;
  }
  if (a.startsWith("rueckspuelen_alle ")) {
    zaehler.rueckspuelenAlle = constrain(a.substring(18).toInt(), 0, 500);
    shotsSpeichern();
    return true;
  }
  return false;
}

static bool shotLaeuftExtern() { return shotLaeuft; }
