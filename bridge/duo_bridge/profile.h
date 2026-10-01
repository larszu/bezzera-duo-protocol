#pragma once
// Bruehprofile: je Kaffeesorte die Einstellungen, die die Duo selbst kennt
// (Bruehtemperatur, Vorbruehen, Kesselprioritaet, Dampfkessel), dazu die der
// Bridge (Zielgewicht fuer Brew by Weight) und Notizen (Roester, Mahlgrad,
// Dosis). Aufbau angelehnt an Beanconqueror (Bohne + Bruehparameter) und
// GaggiMate (Name, Temperatur, Ziel als Abbruchbedingung, Favorit).
//
// Gespeichert im NVS der Bridge (Namensraum "profile", ein Blob je Profil).
// "Auf die Maschine schreiben" geht denselben Weg wie ein Finger am Display:
//   Startbildschirm -> Tastencode 7 (Einstellungen Kaffee, Seite x07)
//   -> Mainboard schreibt seine aktuellen Werte -> Bridge ueberschreibt
//   VP 0x0060 (Temperatur), 0x005C (Vorbruehen), 0x005A (Prioritaet)
//   -> OK (VP 0x0002 = 1) -> Mainboard speichert.
//   Dasselbe mit Tastencode 8 (Seite x08) fuer VP 0x0061 (Dampfkessel).
//   Danach betritt die Bridge die Seiten noch einmal und prueft, was das
//   Mainboard zurueckschreibt (Kontrolle).
// Nur vom Startbildschirm aus und nicht waehrend eines Bezugs.

static const int PROFILE_MAX = 20;

struct Profil {
  char name[32];
  char roester[32];
  char mahlgrad[16];
  char notiz[96];
  float dosis;     // g Kaffeemehl
  float ziel;      // g in der Tasse (Brew by Weight)
  uint8_t temp;    // Kaffeekessel °C, 89..96 laut Handbuch
  uint8_t vorb;    // Vorbruehen in Zehntelsekunden, 0..50
  uint8_t dampf;   // Servicekessel °C, 0 = nicht aendern
  uint8_t prio;    // 0 Kaffee, 1 Services, 2 keine, 255 = nicht aendern
  bool belegt;
};

static Profil profile[PROFILE_MAX];
static int8_t profilAktiv = -1;

static void profileLaden() {
  Preferences p;
  p.begin("profile", true);
  for (int i = 0; i < PROFILE_MAX; i++) {
    char k[8];
    snprintf(k, sizeof k, "p%d", i);
    memset(&profile[i], 0, sizeof(Profil));
    if (p.getBytesLength(k) == sizeof(Profil)) p.getBytes(k, &profile[i], sizeof(Profil));
  }
  profilAktiv = p.getChar("aktiv", -1);
  p.end();
}

static void profilSpeichern(int i) {
  Preferences p;
  p.begin("profile", false);
  char k[8];
  snprintf(k, sizeof k, "p%d", i);
  if (profile[i].belegt) p.putBytes(k, &profile[i], sizeof(Profil));
  else p.remove(k);
  p.putChar("aktiv", profilAktiv);
  p.end();
}

// ─── Ablauf: Befehle mit Wartezeit, ohne loop() anzuhalten ────────────────
// Ein blockierendes delay() liesse Antworten des Displays liegen; das
// Mainboard sieht sie dann zu spaet (siehe Uhr-Schleife vom 30.09.).
struct Schritt {
  uint32_t wartenMs;
  char befehl[48];
};
static Schritt ablauf[48];
static int ablaufN = 0, ablaufPos = 0;
static uint32_t ablaufNaechster = 0;
static char ablaufName[40] = "";
static void (*ablaufFertig)() = nullptr;

static bool ablaufLaeuft() { return ablaufPos < ablaufN; }
static void kontrolleStart();

static void ablaufNeu(const char *name, void (*fertig)() = nullptr) {
  ablaufN = ablaufPos = 0;
  strlcpy(ablaufName, name, sizeof ablaufName);
  ablaufFertig = fertig;
  ablaufNaechster = millis();
}

static void ablaufDazu(uint32_t wartenMs, const char *fmt, ...) {
  if (ablaufN >= 48) return;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(ablauf[ablaufN].befehl, sizeof ablauf[0].befehl, fmt, ap);
  va_end(ap);
  ablauf[ablaufN++].wartenMs = wartenMs;
}

static void ablaufLoop() {
  if (!ablaufLaeuft() || (int32_t)(millis() - ablaufNaechster) < 0) return;
  Schritt &s = ablauf[ablaufPos++];
  char b[48];
  strlcpy(b, s.befehl, sizeof b);
  unsigned vp, ziel;
  if (!strcmp(b, "#kontrolle")) kontrolleStart();
  else if (sscanf(b, "#stufen %x %u", &vp, &ziel) == 2) {
    // Temperatur: das Mainboard aendert sie selbst auf ±; je Druck ein Grad.
    // Aktuellen Wert hat das Mainboard beim Oeffnen der Seite geschrieben.
    int diff = (int)ziel - (int)vpRam[vp];
    int n = min(abs(diff), 12);
    for (int i = n - 1; i >= 0; i--) {  // hinter dem aktuellen Schritt einfuegen, rueckwaerts
      if (ablaufN >= 46) break;
      memmove(&ablauf[ablaufPos + 2], &ablauf[ablaufPos], sizeof(Schritt) * (ablaufN - ablaufPos));
      ablaufN += 2;
      ablauf[ablaufPos] = {350, ""};
      snprintf(ablauf[ablaufPos].befehl, sizeof ablauf[0].befehl, "w 0x0015 %d", diff > 0 ? 2 : 1);
      ablauf[ablaufPos + 1] = {250, "w 0x0015 0"};  // loslassen
    }
  } else if (b[0]) befehl(b);
  if (ablaufPos < ablaufN) ablaufNaechster = millis() + ablauf[ablaufPos].wartenMs;
  else if (ablaufFertig) ablaufFertig();
}

// ─── Profil auf die Maschine ──────────────────────────────────────────────
static int profilZiel = -1;
static char profilErgebnis[96] = "";

static int spracheBasis() {
  int32_t s = leitung.seite;
  return s >= 0 && s < 300 ? (s / 100) * 100 : 100;
}

static uint16_t vpWert(uint16_t vp) { return vp < VP_ANZAHL ? vpRam[vp] : 0; }

// Kontrolle: nach dem zweiten Betreten haben die Werte in vpRam das, was das
// Mainboard gespeichert hat.
static uint32_t kontrolleAb = 0;
static void kontrolleStart() { kontrolleAb = millis(); }
// nur Werte zaehlen, die das Mainboard seit Beginn der Kontrolle selbst geschrieben hat
static bool vomMainboard(uint16_t vp) { return vpMainboardMs[vp] && (int32_t)(vpMainboardMs[vp] - kontrolleAb) >= 0; }

static void profilKontrolle() {
  const Profil &p = profile[profilZiel];
  bool gesehen = vomMainboard(0x60);
  bool ok = gesehen && vpWert(0x60) == p.temp && (!vomMainboard(0x5C) || vpWert(0x5C) == p.vorb) &&
            (p.prio == 255 || !vomMainboard(0x5A) || vpWert(0x5A) == p.prio) &&
            (!p.dampf || !vomMainboard(0x61) || vpWert(0x61) == p.dampf);
  if (!gesehen) {
    snprintf(profilErgebnis, sizeof profilErgebnis, "%s: geschrieben, Kontrolle unmöglich (Mainboard hat die Werte nicht zurückgeschrieben)", p.name);
    ereignis("- Profil %s", profilErgebnis);
    return;
  }
  snprintf(profilErgebnis, sizeof profilErgebnis, "%s: %s (Mainboard: %u °C, %.1f s, Prio %u, Dampf %u °C)", p.name,
           ok ? "übernommen" : "NICHT vollständig übernommen", vpWert(0x60), vpWert(0x5C) / 10.0, vpWert(0x5A),
           vpWert(0x61));
  ereignis("- Profil %s", profilErgebnis);
}

static const char *profilAnwenden(int i) {
  if (i < 0 || i >= PROFILE_MAX || !profile[i].belegt) return "Profil gibt es nicht";
  if (ablaufLaeuft()) return "es läuft schon ein Ablauf";
  if (!mainboardLebt()) return "Mainboard antwortet nicht";
  if (bezugZustand == BZ_LAEUFT) return "während eines Bezugs nicht";
  if (leitung.seite < 0 || leitung.seite % 100 != 1) return "nur vom Startbildschirm aus";
  const Profil &p = profile[i];
  int s = spracheBasis();
  profilZiel = i;
  profilAktiv = i;
  cfg.ziel = p.ziel > 0 ? p.ziel : cfg.ziel;
  einstellungenSpeichern();
  profilSpeichern(i);
  snprintf(profilErgebnis, sizeof profilErgebnis, "%s: wird geschrieben …", p.name);
  ablaufNeu("Profil", profilKontrolle);
  // Kaffee: Seite x07 betreten, Werte des Mainboards abwarten, ueberschreiben, OK
  ablaufDazu(0, "w 0x0000 7");
  ablaufDazu(0, "p %d", s + 7);
  ablaufDazu(800, "#stufen 60 %u", p.temp);
  ablaufDazu(100, "w 0x005C %u", p.vorb);
  if (p.prio != 255) ablaufDazu(50, "w 0x005A %u", p.prio);
  ablaufDazu(150, "w 0x0002 1");
  // Dampfkessel: Seite x08
  if (p.dampf) {
    ablaufDazu(1200, "w 0x0000 8");
    ablaufDazu(0, "p %d", s + 8);
    ablaufDazu(800, "#stufen 61 %u", p.dampf);
    ablaufDazu(150, "w 0x0002 1");
  }
  // Kontrolle: Seite x07 erneut betreten, das Mainboard schreibt seine Werte,
  // mit OK verlassen (die Werte sind dann die des Mainboards, es aendert sich nichts)
  ablaufDazu(1200, "#kontrolle");
  ablaufDazu(0, "w 0x0000 7");
  ablaufDazu(0, "p %d", s + 7);
  ablaufDazu(800, "");
  ablaufDazu(0, "w 0x0002 1");
  if (p.dampf) {
    ablaufDazu(1200, "w 0x0000 8");
    ablaufDazu(0, "p %d", s + 8);
    ablaufDazu(800, "");
    ablaufDazu(0, "w 0x0002 1");
  }
  ablaufDazu(300, "");
  ereignis("- Profil %s wird geschrieben", p.name);
  return nullptr;
}

static bool shotLaeuftExtern();  // shots.h

static void uhrStellen(int jj, int mm, int tt, int hh, int mi) {
  if (ablaufLaeuft()) return;
  ablaufNeu("Uhr");
  ablaufDazu(0, "w 0x0000 23");
  ablaufDazu(0, "p %d", spracheBasis() + 57);
  ablaufDazu(400, "w 0x002E %d %d %d %d %d", hh, mi, tt, mm, jj);  // Mainboard hat die alten Werte geschrieben
  ablaufDazu(150, "w 0x0002 1");
  ereignis("- Uhr stellen: 20%02d-%02d-%02d %02d:%02d", jj, mm, tt, hh, mi);
}

static void jsonProfil(String &j, const Profil &p) {
  j += "{\"name\":";
  jsonText(j, p.name);
  j += ",\"roester\":";
  jsonText(j, p.roester);
  j += ",\"mahlgrad\":";
  jsonText(j, p.mahlgrad);
  j += ",\"notiz\":";
  jsonText(j, p.notiz);
  j += ",\"dosis\":";
  jsonZahl(j, p.dosis, 1);
  j += ",\"ziel\":";
  jsonZahl(j, p.ziel, 1);
  j += ",\"temp\":";
  j += p.temp;
  j += ",\"vorb\":";
  j += p.vorb;
  j += ",\"dampf\":";
  j += p.dampf;
  j += ",\"prio\":";
  j += p.prio;
  j += '}';
}

static String profileJson() {
  String j = "{\"aktiv\":";
  j += profilAktiv;
  j += ",\"ablauf\":";
  j += ablaufLaeuft() ? "true" : "false";
  j += ",\"ergebnis\":";
  jsonText(j, profilErgebnis);
  j += ",\"profile\":[";
  bool erstes = true;
  for (int i = 0; i < PROFILE_MAX; i++) {
    if (!profile[i].belegt) continue;
    if (!erstes) j += ',';
    erstes = false;
    j += "{\"nr\":";
    j += i;
    j += ",\"p\":";
    jsonProfil(j, profile[i]);
    j += '}';
  }
  j += "]}";
  return j;
}

// Formularfelder (URL-kodiert, schon dekodiert uebergeben) in ein Profil
static void profilFeld(Profil &p, const String &k, const String &v) {
  if (k == "name") strlcpy(p.name, v.c_str(), sizeof p.name);
  else if (k == "roester") strlcpy(p.roester, v.c_str(), sizeof p.roester);
  else if (k == "mahlgrad") strlcpy(p.mahlgrad, v.c_str(), sizeof p.mahlgrad);
  else if (k == "notiz") strlcpy(p.notiz, v.c_str(), sizeof p.notiz);
  else if (k == "dosis") p.dosis = constrain(zahlAus(v, p.dosis), 0.0f, 40.0f);
  else if (k == "ziel") p.ziel = constrain(zahlAus(v, p.ziel), 0.0f, 200.0f);
  else if (k == "temp") p.temp = constrain(v.toInt(), 89, 96);
  else if (k == "vorb") p.vorb = constrain(lroundf(zahlAus(v, p.vorb / 10.0f) * 10), 0L, 50L);
  else if (k == "dampf") p.dampf = v.toInt() <= 0 ? 0 : constrain(v.toInt(), 100, 135);
  else if (k == "prio") p.prio = v.toInt() < 0 ? 255 : constrain(v.toInt(), 0, 2);
}

// ─── Einzelne Maschinendaten aendern (Weboberflaeche, Akkordeon) ──────────
// Weg je Variable: welche Seite sie fuehrt und wie man hinkommt.
//   KAFFEE   Startbildschirm -> Tastencode 7 (Seite x07), OK
//   TEE      Startbildschirm -> Tastencode 8 (Seite x08), OK
//   EINST    Startbildschirm -> Tastencode 68 (Einstellungen, Seite x16), OK
//   TECHNIK  Standby -> Schluessel (Tastencode 53, Seite x35) -> Passwort in
//            VP 0x0004 -> Mainboard oeffnet das Technikmenue und schreibt alle
//            Werte -> Wert setzen -> OK -> OK (zurueck in den Standby); war die
//            Maschine an, schaltet die Bridge sie danach wieder ein.
// Temperaturen gehen ueber ± (VP 0x0015), alles andere wird direkt geschrieben.
enum { W_KAFFEE = 1, W_TEE, W_EINST, W_TECHNIK };
struct EditWeg {
  uint16_t vp;
  uint8_t weg;
  int16_t min, max;
};
static const EditWeg EDIT_WEGE[] = {
    {0x60, W_KAFFEE, 89, 96},   {0x5E, W_KAFFEE, 0, 1},    {0x5B, W_KAFFEE, 0, 1},    {0x5A, W_KAFFEE, 0, 2},
    {0x5C, W_KAFFEE, 0, 50},    {0x61, W_TEE, 100, 135},   {0x5F, W_TEE, 0, 1},       {0x20, W_EINST, 0, 1},
    {0x25, W_EINST, 0, 1},      {0x2B, W_EINST, 0, 1},     {0x2A, W_EINST, 0, 1},     {0x2C, W_EINST, 0, 1},
    {0x2D, W_EINST, 0, 1},      {0x21, W_EINST, 1, 5},     {0x29, W_EINST, 1, 5},     {0x27, W_EINST, 0, 9000},
    {0x28, W_EINST, 0, 350},    {0x70, W_TECHNIK, 0, 1},   {0x71, W_TECHNIK, 10, 99}, {0x72, W_TECHNIK, 0, 999},
    {0x73, W_TECHNIK, 0, 999},  {0x74, W_TECHNIK, 0, 10},  {0x76, W_TECHNIK, 10, 99}, {0x77, W_TECHNIK, 0, 999},
    {0x78, W_TECHNIK, 0, 999},  {0x79, W_TECHNIK, 0, 10},  {0x7B, W_TECHNIK, 10, 99}, {0x7C, W_TECHNIK, 0, 999},
    {0x7D, W_TECHNIK, 0, 999},  {0x7E, W_TECHNIK, 0, 10},  {0x80, W_TECHNIK, 1, 4},   {0x81, W_TECHNIK, 1, 3},
};
static uint16_t editVp = 0, editZiel = 0;
static char editErgebnis[96] = "";
static String technikPasswort = "1906";

static void editKontrolle() {
  bool gesehen = vomMainboard(editVp);
  if (!gesehen) snprintf(editErgebnis, sizeof editErgebnis, "VP 0x%04X = %u gesendet, Mainboard hat nicht zurückgeschrieben", editVp, editZiel);
  else snprintf(editErgebnis, sizeof editErgebnis, "VP 0x%04X: %s (Mainboard meldet %u)", editVp,
                vpRam[editVp] == editZiel ? "übernommen" : "NICHT übernommen", vpRam[editVp]);
  ereignis("- Einstellung %s", editErgebnis);
}
static void editTechnikFertig() {
  snprintf(editErgebnis, sizeof editErgebnis, "VP 0x%04X = %u im Technikmenü gespeichert", editVp, editZiel);
  ereignis("- Einstellung %s", editErgebnis);
}

static const char *maschinendatumSetzen(uint16_t vp, int wert) {
  const EditWeg *w = nullptr;
  for (const auto &e : EDIT_WEGE)
    if (e.vp == vp) w = &e;
  if (!w) return "dieser Wert lässt sich nicht ändern";
  if (wert < w->min || wert > w->max) return "Wert außerhalb des erlaubten Bereichs";
  if (ablaufLaeuft()) return "es läuft schon ein Ablauf";
  if (!mainboardLebt()) return "Mainboard antwortet nicht";
  if (bezugZustand == BZ_LAEUFT || shotLaeuftExtern()) return "während eines Bezugs nicht";
  int b = leitung.seite >= 0 ? leitung.seite % 100 : -1;
  int s = spracheBasis();
  editVp = vp;
  editZiel = wert;
  snprintf(editErgebnis, sizeof editErgebnis, "VP 0x%04X wird auf %d gesetzt …", vp, wert);
  if (w->weg == W_TECHNIK) {
    if (b != 1 && b != 0) return "nur vom Startbildschirm oder Standby aus";
    bool warAn = b == 1;
    ablaufNeu("Technik", editTechnikFertig);
    if (warAn) {
      ablaufDazu(0, "w 0x0000 0");
      ablaufDazu(0, "p %d", s);
    }
    ablaufDazu(warAn ? 2000 : 0, "w 0x0000 53");
    ablaufDazu(0, "p %d", s + 35);
    ablaufDazu(900, "w 0x0004 %s", technikPasswort.c_str());
    ablaufDazu(2500, "p %d", s + 20);
    ablaufDazu(300, "w 0x%04X %d", vp, wert);
    ablaufDazu(200, "w 0x0002 1");
    ablaufDazu(1500, "w 0x0002 1");
    if (warAn) {
      ablaufDazu(2000, "w 0x0000 1");
      ablaufDazu(0, "p %d", s + 1);
    }
    ablaufDazu(500, "");
    return nullptr;
  }
  if (b != 1) return "nur vom Startbildschirm aus";
  int code = w->weg == W_KAFFEE ? 7 : w->weg == W_TEE ? 8 : 68;
  int seite = w->weg == W_KAFFEE ? 7 : w->weg == W_TEE ? 8 : 16;
  bool stufen = vp == 0x60 || vp == 0x61;
  ablaufNeu("Einstellung", editKontrolle);
  ablaufDazu(0, "w 0x0000 %d", code);
  ablaufDazu(0, "p %d", s + seite);
  if (stufen) ablaufDazu(900, "#stufen %x %d", vp, wert);
  else ablaufDazu(900, "w 0x%04X %d", vp, wert);
  ablaufDazu(300, "w 0x0002 1");
  // Kontrolle: Seite neu oeffnen, Mainboard schreibt, mit OK unveraendert verlassen
  ablaufDazu(1500, "#kontrolle");
  ablaufDazu(0, "w 0x0000 %d", code);
  ablaufDazu(0, "p %d", s + seite);
  ablaufDazu(900, "");
  ablaufDazu(0, "w 0x0002 1");
  ablaufDazu(300, "");
  return nullptr;
}
