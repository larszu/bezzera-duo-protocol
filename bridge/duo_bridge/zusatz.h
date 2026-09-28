#pragma once
// Funktionen, die das Display nicht hat: Ein/Aus aus der Ferne, Verlauf von
// Temperatur und Druck im ESP32 (auch ohne offene Weboberflaeche), Brew by
// Weight mit Bluetooth- oder WLAN-Waage (waage.h) und Home Assistant ueber
// MQTT (ha_mqtt.h). Eingebunden von web.h, nutzt den Zustand aus duo_bridge.ino.
//
// Einstellungen liegen im NVS (Namensraum "zusatz"), nie im Quelltext.

#include <Preferences.h>

// ─── Einstellungen ──────────────────────────────────────────────────────────

struct Einstellungen {
  String mqttUri, mqttUser, mqttPass, haPrefix = "homeassistant";
  uint8_t waageArt = 0;  // 0 aus, 1 Bluetooth, 2 WLAN: URL abfragen, 3 WLAN: Waage meldet selbst
  String waageUrl, waageTopic, waageBle;  // waageBle: Adresse oder leer = erste bekannte Waage
  float ziel = 36, vorlauf = 2;
  bool lernen = true;
  int8_t stoppPin = -1;
  bool stoppHigh = true;
  uint16_t stoppPulsMs = 300;  // so lange "drueckt" der Ausgang die Taste
  int8_t druckPWort = -1, druckKWort = -1;  // Wort in VP 0x0050, -1 = noch unbekannt
  uint16_t druckPTeiler = 10, druckKTeiler = 10;
};
static Einstellungen cfg;

// Frei auf dem ESP32-S3-ETH: nicht W5500, SD, UART-Bridge, LED, Strapping-Pins
// oder Octal-PSRAM (33-37).
static bool stoppPinErlaubt(int p) {
  static const int8_t frei[] = {1, 2, 38, 39, 40, 41, 42, 47, 48};
  for (int8_t f : frei)
    if (f == p) return true;
  return false;
}

static void einstellungenLaden() {
  Preferences p;
  p.begin("zusatz", true);
  cfg.mqttUri = p.getString("mqtt_uri", "");
  cfg.mqttUser = p.getString("mqtt_user", "");
  cfg.mqttPass = p.getString("mqtt_pass", "");
  cfg.haPrefix = p.getString("ha_prefix", "homeassistant");
  cfg.waageArt = p.getUChar("waage_art", 0);
  cfg.waageUrl = p.getString("waage_url", "");
  cfg.waageTopic = p.getString("waage_topic", "");
  cfg.waageBle = p.getString("waage_ble", "");
  cfg.ziel = p.getFloat("ziel", 36);
  cfg.vorlauf = p.getFloat("vorlauf", 2);
  cfg.lernen = p.getBool("lernen", true);
  cfg.stoppPin = p.getChar("stopp_pin", -1);
  cfg.stoppHigh = p.getBool("stopp_high", true);
  cfg.stoppPulsMs = p.getUShort("stopp_puls", 300);
  cfg.druckPWort = p.getChar("druck_p_wort", -1);
  cfg.druckKWort = p.getChar("druck_k_wort", -1);
  cfg.druckPTeiler = p.getUShort("druck_p_teil", 10);
  cfg.druckKTeiler = p.getUShort("druck_k_teil", 10);
  p.end();
  if (cfg.waageArt > 3) cfg.waageArt = 0;
  if (!stoppPinErlaubt(cfg.stoppPin)) cfg.stoppPin = -1;
}

static void einstellungenSpeichern() {
  Preferences p;
  p.begin("zusatz", false);
  p.putString("mqtt_uri", cfg.mqttUri);
  p.putString("mqtt_user", cfg.mqttUser);
  p.putString("mqtt_pass", cfg.mqttPass);
  p.putString("ha_prefix", cfg.haPrefix);
  p.putUChar("waage_art", cfg.waageArt);
  p.putString("waage_url", cfg.waageUrl);
  p.putString("waage_topic", cfg.waageTopic);
  p.putString("waage_ble", cfg.waageBle);
  p.putFloat("ziel", cfg.ziel);
  p.putFloat("vorlauf", cfg.vorlauf);
  p.putBool("lernen", cfg.lernen);
  p.putChar("stopp_pin", cfg.stoppPin);
  p.putBool("stopp_high", cfg.stoppHigh);
  p.putUShort("stopp_puls", cfg.stoppPulsMs);
  p.putChar("druck_p_wort", cfg.druckPWort);
  p.putChar("druck_k_wort", cfg.druckKWort);
  p.putUShort("druck_p_teil", cfg.druckPTeiler);
  p.putUShort("druck_k_teil", cfg.druckKTeiler);
  p.end();
}

static float zahlAus(const String &s, float sonst) {
  char *e;
  float v = strtof(s.c_str(), &e);
  return e == s.c_str() ? sonst : v;
}

// Ein Feld aus dem Formular der Weboberflaeche setzen. false = unbekannt.
static bool setzeEinstellung(const String &k, const String &v) {
  if (k == "mqtt_uri") cfg.mqttUri = v;
  else if (k == "mqtt_user") cfg.mqttUser = v;
  else if (k == "mqtt_pass") { if (v.length()) cfg.mqttPass = v == "-" ? "" : v; }  // leer = unveraendert
  else if (k == "ha_prefix") cfg.haPrefix = v.length() ? v : "homeassistant";
  else if (k == "waage_art") cfg.waageArt = constrain(v.toInt(), 0, 3);
  else if (k == "waage_url") cfg.waageUrl = v;
  else if (k == "waage_topic") cfg.waageTopic = v;
  else if (k == "waage_ble") cfg.waageBle = v;
  else if (k == "ziel") cfg.ziel = constrain(zahlAus(v, cfg.ziel), 0.0f, 200.0f);
  else if (k == "vorlauf") cfg.vorlauf = constrain(zahlAus(v, cfg.vorlauf), 0.0f, 10.0f);
  else if (k == "lernen") cfg.lernen = v == "1" || v == "on" || v == "true";
  else if (k == "stopp_pin") cfg.stoppPin = stoppPinErlaubt(v.toInt()) ? v.toInt() : -1;
  else if (k == "stopp_high") cfg.stoppHigh = v == "1" || v == "on" || v == "true";
  else if (k == "stopp_puls") cfg.stoppPulsMs = constrain(v.toInt(), 50, 3000);
  else if (k == "druck_p_wort") cfg.druckPWort = constrain(v.toInt(), -1, 8);
  else if (k == "druck_k_wort") cfg.druckKWort = constrain(v.toInt(), -1, 8);
  else if (k == "druck_p_teil") cfg.druckPTeiler = max(1L, v.toInt());
  else if (k == "druck_k_teil") cfg.druckKTeiler = max(1L, v.toInt());
  else return false;
  return true;
}

static void jsonText(String &j, const String &s) {
  j += '"';
  for (char c : s) {
    if (c == '"' || c == '\\') j += '\\';
    if ((uint8_t)c >= 0x20) j += c;
  }
  j += '"';
}

static void jsonZahl(String &j, float v, int stellen) {
  if (isnan(v)) j += "null";
  else j += String(v, stellen);
}

// ─── Maschinenzustand aus der Leitung ──────────────────────────────────────

static bool mainboardLebt() { return leitung.rahmenMainboard && millis() - leitung.zuletztMainboard < 3000; }

// VP 0x0050 (9 Worte Status) aktuell? Dann gelten vpRam[0x50..0x58].
static bool statusAktuell() {
  for (size_t i = 0; i < vpAnzahl; i++)
    if (vpTabelle[i].vp == 0x50) return millis() - vpTabelle[i].ms < 5000;
  return false;
}

static float tempKaffee() { return statusAktuell() ? (int16_t)vpRam[0x53] : NAN; }
static float tempService() { return statusAktuell() ? (int16_t)vpRam[0x54] : NAN; }
static float druckPumpe() {
  return statusAktuell() && cfg.druckPWort >= 0 ? (int16_t)vpRam[0x50 + cfg.druckPWort] / (float)cfg.druckPTeiler : NAN;
}
static float druckKessel() {
  return statusAktuell() && cfg.druckKWort >= 0 ? (int16_t)vpRam[0x50 + cfg.druckKWort] / (float)cfg.druckKTeiler : NAN;
}

static const char *alarmText(int b) {
  switch (b) {
    case 2: return "Ladezeit überschritten";
    case 3: return "Tank füllen";
    case 4: return "Fehler Sonde NTC";
    case 56: return "Wartung erforderlich";
    case 77: return "Kein volumetrisches Signal";
    case 89: return "Wasserfilter wechseln";
    case 91: return "Spülen abgebrochen";
  }
  return nullptr;
}

// Tastenwert VP 0x0000, wie ihn das Display dem Mainboard alle 100 ms meldet
// (Antworten auf 0x83). Gemessen: 1 nach dem Einschalten (schreibt das
// Mainboard selbst), 5 nach "OK"; laut Flash 0 nach "Standby". Zuverlaessiger
// als die Seite: Seitenwechsel per Touch macht das Display ohne Meldung.
static int32_t tastenwert() {
  for (size_t i = 0; i < vpAnzahl; i++)
    if (vpTabelle[i].vp == 0 && vpTabelle[i].len >= 2 && millis() - vpTabelle[i].ms < 3000)
      return (vpTabelle[i].d[0] << 8) | vpTabelle[i].d[1];
  return -1;
}

static bool imStandby() {
  int32_t t = tastenwert();
  return t >= 0 ? t == 0 : leitung.seite >= 0 && leitung.seite % 100 == 0;
}

// Ausgabezaehler: Seite x06 zeigt nur den Pumpendruck-Zeiger und in der Mitte
// eine grosse Zahl. Laut Handbuch (5.4.4) erscheint waehrend der Ausgabe ein
// Bildschirm mit Pumpendruck und Ausgabedauer. Vermutung: das ist x06, und das
// Mainboard schaltet ihn wie die Alarmseiten selbst. Noch nicht mitgeschnitten.
static bool ausgabeSeite() { return mainboardLebt() && leitung.seite >= 0 && leitung.seite % 100 == 6; }

// Kurzer Zustand fuer Weboberflaeche und Home Assistant.
static String maschinenStatus() {
  if (!mainboardLebt()) return "keine Verbindung";
  int b = leitung.seite >= 0 ? leitung.seite % 100 : -1;
  if (b == 90) return "startet";
  if (const char *a = b >= 0 ? alarmText(b) : nullptr) return String("Alarm: ") + a;
  if (imStandby()) return "Standby";
  if (b == 6) return "Ausgabe";
  return leitung.seite >= 0 || tastenwert() >= 0 ? "an" : "unbekannt";
}
static bool maschineAn() { return mainboardLebt() && !imStandby() && (leitung.seite >= 0 || tastenwert() >= 0); }
static const char *aktuellerAlarm() { return mainboardLebt() && leitung.seite >= 0 ? alarmText(leitung.seite % 100) : nullptr; }

// Ein/Aus wie die Tasten am Display (Touch-Konfiguration 13.bin):
// Standby „Für Start drücken“ schreibt VP 0x0000 = 1 und geht auf Seite x01,
// Seitenmenue „Standby“ schreibt VP 0x0000 = 0 und geht auf Seite x00.
// Das Mainboard liest VP 0x0000 alle 100 ms und reagiert wie auf den Touch.
static void maschineSchalten(bool an) {
  int32_t s = leitung.seite;
  int sprache = s >= 0 && s < 300 ? (s / 100) * 100 : 100;
  char b[24];
  snprintf(b, sizeof b, "w 0x0000 %d", an ? 1 : 0);
  befehl(b);
  snprintf(b, sizeof b, "p %d", sprache + (an ? 1 : 0));
  befehl(b);
  ereignis("- Maschine %s (Fernbedienung)", an ? "ein" : "aus");
}

// ─── Verlauf: VP 0x0050 einmal je Sekunde, im PSRAM bis 24 h ───────────────

struct Probe {
  uint32_t ms;
  int16_t w[9];
  int16_t seite;
};
static Probe *verlaufBuf = nullptr;
static uint32_t verlaufGroesse = 0, verlaufN = 0;  // verlaufN: insgesamt geschrieben

static void verlaufSetup() {
  verlaufGroesse = psramFound() ? 86400 : 1800;
  verlaufBuf = (Probe *)(psramFound() ? ps_malloc(verlaufGroesse * sizeof(Probe)) : malloc(verlaufGroesse * sizeof(Probe)));
  if (!verlaufBuf) verlaufGroesse = 0;
}

static void verlaufLoop() {
  static uint32_t zuletzt = 0;
  if (!verlaufGroesse || millis() - zuletzt < 1000 || !statusAktuell()) return;
  zuletzt = millis();
  Probe &p = verlaufBuf[verlaufN % verlaufGroesse];
  p.ms = zuletzt;
  for (int i = 0; i < 9; i++) p.w[i] = vpRam[0x50 + i];
  p.seite = leitung.seite;
  verlaufN++;
}

// JSON: {"jetzt":ms,"p":[[alter_s,seite,w0..w8],...]} vom aeltesten zum neuesten,
// hoechstens "max" Punkte ueber die letzten "sek" Sekunden.
static String verlaufJson(uint32_t sek, uint32_t maxPunkte) {
  uint32_t jetzt = millis(), vorhanden = min(verlaufN, verlaufGroesse), anz = 0;
  while (anz < vorhanden && jetzt - verlaufBuf[(verlaufN - 1 - anz) % verlaufGroesse].ms <= sek * 1000UL) anz++;
  uint32_t schritt = maxPunkte && anz > maxPunkte ? (anz + maxPunkte - 1) / maxPunkte : 1;
  String j;
  j.reserve(64 + anz / schritt * 48);
  j += "{\"jetzt\":";
  j += jetzt;
  j += ",\"groesse\":";
  j += verlaufGroesse;
  j += ",\"p\":[";
  bool erstes = true;
  for (int64_t i = (int64_t)anz - 1; i >= 0; i -= schritt) {
    const Probe &p = verlaufBuf[(verlaufN - 1 - i) % verlaufGroesse];
    if (!erstes) j += ',';
    erstes = false;
    j += '[';
    j += (jetzt - p.ms) / 1000;
    j += ',';
    j += p.seite;
    for (int k = 0; k < 9; k++) {
      j += ',';
      j += p.w[k];
    }
    j += ']';
  }
  j += "]}";
  return j;
}

// ─── Waage: Gewicht kommt aus einer anderen Task (BLE, HTTP, MQTT) ─────────

static portMUX_TYPE waageMux = portMUX_INITIALIZER_UNLOCKED;
static volatile float waageRoh = NAN;  // letzter Wert der Waage in g
static volatile uint32_t waageMs = 0, waageNr = 0;
static float waageNull = 0;  // Software-Tara fuer WLAN-Waagen
static char waageStatus[64] = "aus";

static void waageMelden(float g) {
  if (isnan(g) || g < -5000 || g > 5000) return;
  portENTER_CRITICAL(&waageMux);
  waageRoh = g;
  waageMs = millis();
  waageNr++;
  portEXIT_CRITICAL(&waageMux);
}
static void waageStatusSetzen(const char *s) {
  portENTER_CRITICAL(&waageMux);
  strlcpy(waageStatus, s, sizeof waageStatus);
  portEXIT_CRITICAL(&waageMux);
}
static String waageStatusText() {
  char b[64];
  portENTER_CRITICAL(&waageMux);
  memcpy(b, waageStatus, sizeof b);
  portEXIT_CRITICAL(&waageMux);
  return b;
}
static volatile bool waageTaraBle = false;  // die Waage-Task schickt Tara an die BLE-Waage

// ─── Brew by Weight ────────────────────────────────────────────────────────
// Bezug erkennt die Bridge selbst: Das Gewicht steigt gleichmaessig (erste
// Tropfen), der Pumpendruck steigt (sobald das Druckwort bekannt ist) oder
// das Mainboard zeigt den Ausgabezaehler (Seite x06, Vermutung).
// Ziel erreicht (Gewicht + Vorlauf >= Ziel) heisst: Meldung an Web und Home
// Assistant und, wenn eingerichtet, "drueckt" der Stopp-Ausgang kurz die Taste
// Dauerausgabe/Stop am Tastenfeld der DE; das Mainboard beendet die Ausgabe
// dann selbst (Handbuch 5.4). Der Vorlauf
// (was nach dem Stopp noch nachtropft) lernt sich aus jedem Bezug.

struct BezugPunkt {
  uint16_t t10;  // 0,1 s seit Start
  int16_t g10, fluss100, druck10, temp;
};
static const int BEZUG_MAX = 1500;  // 150 s bei 10 Werten je Sekunde
static BezugPunkt bezugPunkte[BEZUG_MAX];
static int bezugN = 0;

struct BezugInfo {
  uint32_t endeMs;
  float g, ziel, dauer;
  bool gestoppt;
};
static BezugInfo bezugListe[10];
static uint32_t bezugListeN = 0, bezuegeGesamt = 0;

enum { BZ_BEREIT, BZ_LAEUFT };
static uint8_t bezugZustand = BZ_BEREIT;
static uint32_t bezugStartMs = 0, bezugRuhigSeit = 0, stoppSeitMs = 0;
static float bezugBasis = 0, bezugG = NAN, fluss = 0;
static bool stoppGesendet = false, stoppAktiv = false;
static uint32_t bezugNr = 0;  // zaehlt Starts, damit das Web neue Bezuege erkennt

// Letzte Waagenwerte fuer Durchfluss und Starterkennung (etwa 3 s).
static struct { uint32_t ms; float g; } waageHist[48];
static uint32_t waageHistN = 0;

static float waageVor(uint32_t ms) {  // Gewicht vor ms Millisekunden (naechster Wert)
  if (!waageHistN) return NAN;
  uint32_t jetzt = waageHist[(waageHistN - 1) % 48].ms;
  for (uint32_t i = 0; i < min<uint32_t>(waageHistN, 48); i++) {
    auto &h = waageHist[(waageHistN - 1 - i) % 48];
    if (jetzt - h.ms >= ms) return h.g;
  }
  return NAN;
}

// Steigung ueber die letzte Sekunde (lineare Regression), g/s
static float flussBerechnen() {
  uint32_t n = min<uint32_t>(waageHistN, 48);
  if (n < 3) return 0;
  uint32_t t0 = waageHist[(waageHistN - 1) % 48].ms;
  double sx = 0, sy = 0, sxx = 0, sxy = 0;
  int k = 0;
  for (uint32_t i = 0; i < n; i++) {
    auto &h = waageHist[(waageHistN - 1 - i) % 48];
    if (t0 - h.ms > 1000) break;
    double x = -(double)(t0 - h.ms) / 1000.0;
    sx += x; sy += h.g; sxx += x * x; sxy += x * h.g;
    k++;
  }
  double nenner = k * sxx - sx * sx;
  return k >= 3 && nenner > 1e-9 ? (float)((k * sxy - sx * sy) / nenner) : 0;
}

static void stoppAusgang(bool an) {
  stoppAktiv = an;
  if (cfg.stoppPin >= 0) digitalWrite(cfg.stoppPin, an == cfg.stoppHigh ? HIGH : LOW);
}

static void mqttEreignis(const char *art);  // ha_mqtt.h

static void bezugBeenden(bool abgebrochen) {
  float dauer = (millis() - bezugStartMs) / 1000.0f;
  bezugZustand = BZ_BEREIT;
  bezugRuhigSeit = millis();
  if (abgebrochen) {
    ereignis("- Bezug abgebrochen");
    return;
  }
  BezugInfo &b = bezugListe[bezugListeN++ % 10];
  b = {millis(), bezugG, cfg.ziel, dauer, stoppGesendet};
  if (stoppGesendet && cfg.lernen && cfg.ziel > 0) {
    // Nachtropfen lernen: halber Schritt Richtung Abweichung
    cfg.vorlauf = constrain(cfg.vorlauf + 0.5f * (bezugG - cfg.ziel), 0.0f, 8.0f);
    einstellungenSpeichern();
  }
  bezuegeGesamt++;
  Preferences p;
  p.begin("zusatz", false);
  p.putUInt("bezuege", bezuegeGesamt);
  p.end();
  ereignis("- Bezug fertig: %.1f g in %.1f s (Ziel %.1f, Vorlauf jetzt %.1f)", bezugG, dauer, cfg.ziel, cfg.vorlauf);
  mqttEreignis("bezug_fertig");
}

static void bezugStarten(uint32_t vorlaufMs) {
  bezugZustand = BZ_LAEUFT;
  bezugStartMs = millis() - vorlaufMs;
  bezugN = 0;
  bezugNr++;
  stoppGesendet = false;
  bezugG = 0;
  ereignis("- Bezug erkannt, Ziel %.1f g", cfg.ziel);
  mqttEreignis("bezug_start");
}

static void bezugPunkt() {
  if (bezugN >= BEZUG_MAX) return;
  float p = druckPumpe(), t = tempKaffee();
  uint32_t t10 = (millis() - bezugStartMs) / 100;
  if (bezugN && bezugPunkte[bezugN - 1].t10 == t10) return;  // hoechstens 10 je Sekunde
  bezugPunkte[bezugN++] = {(uint16_t)t10, (int16_t)lroundf(bezugG * 10), (int16_t)lroundf(fluss * 100),
                           (int16_t)(isnan(p) ? INT16_MIN : lroundf(p * 10)), (int16_t)(isnan(t) ? INT16_MIN : t)};
}

// Laeuft in loop(): neue Waagenwerte uebernehmen, Bezug fuehren, Stopp-Tastendruck beenden.
static void bezugLoop() {
  static uint32_t gesehenNr = 0;
  uint32_t nr, ms;
  float roh;
  portENTER_CRITICAL(&waageMux);
  nr = waageNr; ms = waageMs; roh = waageRoh;
  portEXIT_CRITICAL(&waageMux);

  if (stoppAktiv && millis() - stoppSeitMs > cfg.stoppPulsMs) stoppAusgang(false);
  float p = druckPumpe();
  if (nr == gesehenNr) {
    // ohne neue Waagenwerte: Bezug nach 5 s ohne Waage beenden
    if (bezugZustand == BZ_LAEUFT && millis() - ms > 5000) bezugBeenden(false);
    return;
  }
  gesehenNr = nr;
  float g = roh - (cfg.waageArt >= 2 ? waageNull : 0);
  waageHist[waageHistN++ % 48] = {ms, g};
  fluss = flussBerechnen();

  if (bezugZustand == BZ_BEREIT) {
    if (fabsf(fluss) > 0.3f) bezugRuhigSeit = millis();
    else if (millis() - bezugRuhigSeit > 1500) bezugBasis = g;  // ruhig: neue Nulllinie (Tasse steht)
    float d1 = g - waageVor(500), d2 = waageVor(500) - waageVor(1000);
    bool tropft = g - bezugBasis > 0.5f && d1 > 0.15f && d2 > 0.15f && d1 + d2 < 12;  // gleichmaessig, kein Sprung
    bool druck = (!isnan(p) && p >= 2.0f) || ausgabeSeite();
    if (tropft || druck) bezugStarten(tropft && !druck ? 1000 : 0);
    else return;
  }
  bezugG = g - bezugBasis;
  bezugPunkt();
  uint32_t dauer = millis() - bezugStartMs;
  if (!stoppGesendet && cfg.ziel > 0 && bezugG + cfg.vorlauf >= cfg.ziel) {
    stoppGesendet = true;
    stoppSeitMs = millis();
    stoppAusgang(true);
    ereignis("- Ziel erreicht: %.1f g + Vorlauf %.1f g, Stopp%s", bezugG, cfg.vorlauf, cfg.stoppPin >= 0 ? "-Ausgang an" : "-Meldung");
    mqttEreignis("ziel_erreicht");
  }
  // Ende: 3 s kein Durchfluss (und kein Pumpendruck, falls bekannt)
  static uint32_t stillSeit = 0;
  bool still = dauer > 5000 && fabsf(fluss) < 0.15f && (isnan(p) || p < 0.5f) && !ausgabeSeite();
  if (!still) stillSeit = 0;
  else if (!stillSeit) stillSeit = millis();
  if (bezugG < -5) bezugBeenden(true);  // Tasse weggenommen
  else if (dauer > 150000 || (stillSeit && millis() - stillSeit > 3000)) {
    stillSeit = 0;
    bezugBeenden(false);
  }
}

static void waageTara() {
  if (cfg.waageArt == 1) waageTaraBle = true;
  else if (!isnan(waageRoh)) waageNull = waageRoh;
  bezugBasis = 0;
  bezugRuhigSeit = millis();
  waageHistN = 0;
}

// Aktionen fuer Web, USB und MQTT: "an", "aus", "tara", "abbruch", "stopp", "ziel <g>"
static bool aktion(const String &a) {
  if (a == "an" || a == "aus") maschineSchalten(a == "an");
  else if (a == "tara") waageTara();
  else if (a == "abbruch") { if (bezugZustand == BZ_LAEUFT) bezugBeenden(true); }
  else if (a == "stopp") {  // Stopp-Taste von Hand, z. B. aus Home Assistant
    if (cfg.stoppPin < 0) return false;
    stoppSeitMs = millis();
    stoppAusgang(true);
    ereignis("- Stopp-Taste gedrueckt (von Hand)");
  }
  else if (a.startsWith("ziel ")) {
    cfg.ziel = constrain(zahlAus(a.substring(5), cfg.ziel), 0.0f, 200.0f);
    einstellungenSpeichern();
  } else return false;
  return true;
}

static String zusatzJson();  // unten, braucht mqttStatus aus ha_mqtt.h

static void zusatzSetup() {
  einstellungenLaden();
  Preferences p;
  p.begin("zusatz", true);
  bezuegeGesamt = p.getUInt("bezuege", 0);
  p.end();
  verlaufSetup();
  if (cfg.stoppPin >= 0) {
    pinMode(cfg.stoppPin, OUTPUT);
    stoppAusgang(false);
  }
}

// Kurve des laufenden bzw. letzten Bezugs
static String bezugJson() {
  String j;
  j.reserve(64 + bezugN * 28);
  j += "{\"nr\":";
  j += bezugNr;
  j += ",\"p\":[";
  for (int i = 0; i < bezugN; i++) {
    const BezugPunkt &b = bezugPunkte[i];
    if (i) j += ',';
    j += '[';
    j += b.t10 / 10.0f;
    j += ',';
    j += b.g10 / 10.0f;
    j += ',';
    j += b.fluss100 / 100.0f;
    j += ',';
    if (b.druck10 == INT16_MIN) j += "null";
    else j += b.druck10 / 10.0f;
    j += ',';
    if (b.temp == INT16_MIN) j += "null";
    else j += b.temp;
    j += ']';
  }
  j += "]}";
  return j;
}
