#pragma once
// Ein-/Ausschalten nach Wochenplan, Kalender und Leerlauf, ausgefuehrt von der
// Bridge (nicht vom Auto Ein/Aus des Mainboards, dessen Speicherformat noch
// nicht entschluesselt ist). Ein/Aus gehen denselben Weg wie die Tasten
// (maschineSchalten in zusatz.h). Uhrzeit: NTP, sonst die Display-Uhr.
//
// Wochenplan: bis zu 10 Zeilen "Tage, Ein, Aus".
// Kalender: iCal-Adresse (z. B. die geheime Adresse eines Google-, iCloud-
//   oder Nextcloud-Kalenders). Termine, deren Titel das Stichwort enthaelt
//   (leer = alle), schalten die Maschine <vorlauf> Minuten vor Beginn ein und
//   am Ende aus. Wiederholungen: FREQ=DAILY/WEEKLY mit BYDAY und UNTIL; alles
//   andere (Ausnahmen, monatlich) wird nicht ausgewertet.
// Leerlauf: nach <n> Minuten an ohne Bezug in Standby (0 = aus).

#include <WiFiClientSecure.h>

struct PlanZeile {
  uint8_t tage;  // Bit 0 = Montag … Bit 6 = Sonntag
  uint8_t einH, einM, ausH, ausM;
  bool aktiv;
};
struct PlanEinstellungen {
  PlanZeile zeilen[10];
  uint8_t n;
  bool planAktiv;
  bool kalenderAktiv;
  uint8_t vorlaufMin;
  uint16_t leerlaufMin;
  char kalenderUrl[200];
  char stichwort[32];
};
static PlanEinstellungen plan = {};

struct Termin {
  time_t start, ende;
  char titel[40];
};
static Termin termine[16];
static volatile int termineN = 0;
static char kalenderStatus[64] = "aus";
static uint32_t letzteAktivitaetMs = 0;  // Einschalten oder Bezug

static void planLaden() {
  Preferences p;
  p.begin("zeitplan", true);
  if (p.getBytesLength("plan") == sizeof plan) p.getBytes("plan", &plan, sizeof plan);
  else {
    plan.vorlaufMin = 20;
    plan.leerlaufMin = 0;
  }
  p.end();
  if (plan.n > 10) plan.n = 0;
}
static void planSpeichern() {
  Preferences p;
  p.begin("zeitplan", false);
  p.putBytes("plan", &plan, sizeof plan);
  p.end();
}

// ─── iCal lesen ───────────────────────────────────────────────────────────
static int64_t tageSeit1970(int y, unsigned m, unsigned d) {  // Howard Hinnant, days_from_civil
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int)doe - 719468;
}

// "20261001T070000Z" (UTC) oder "20261001T070000" (lokal); Tagestermine -> 0
static time_t icsZeit(const String &wert, bool utc) {
  if (wert.length() < 15 || wert[8] != 'T') return 0;
  int y = wert.substring(0, 4).toInt(), mo = wert.substring(4, 6).toInt(), d = wert.substring(6, 8).toInt();
  int h = wert.substring(9, 11).toInt(), mi = wert.substring(11, 13).toInt(), s = wert.substring(13, 15).toInt();
  if (utc || wert.endsWith("Z")) return (time_t)(tageSeit1970(y, mo, d) * 86400 + h * 3600 + mi * 60 + s);
  struct tm t = {};
  t.tm_year = y - 1900;
  t.tm_mon = mo - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = mi;
  t.tm_sec = s;
  t.tm_isdst = -1;
  return mktime(&t);
}

static void terminDazu(time_t s, time_t e, const String &titel, time_t von, time_t bis) {
  if (e <= von || s >= bis || termineN >= 16) return;
  Termin &t = termine[termineN++];
  t.start = s;
  t.ende = e > s ? e : s + 3600;
  strlcpy(t.titel, titel.c_str(), sizeof t.titel);
}

// Ein Ereignis (mit einfacher Wiederholung) in die Liste der naechsten 7 Tage
static void ereignisAuswerten(time_t s, time_t e, const String &titel, const String &rrule, time_t von, time_t bis) {
  if (!s) return;
  String st = plan.stichwort;
  st.trim();
  if (st.length()) {
    String a = titel, b = st;
    a.toLowerCase();
    b.toLowerCase();
    if (a.indexOf(b) < 0) return;
  }
  if (!rrule.length()) {
    terminDazu(s, e, titel, von, bis);
    return;
  }
  bool taeglich = rrule.indexOf("FREQ=DAILY") >= 0, woechentlich = rrule.indexOf("FREQ=WEEKLY") >= 0;
  if (!taeglich && !woechentlich) return;
  time_t bisWdh = bis;
  int u = rrule.indexOf("UNTIL=");
  if (u >= 0) {
    time_t ut = icsZeit(rrule.substring(u + 6, u + 22), false);
    if (ut && ut < bisWdh) bisWdh = ut;
  }
  static const char *const KUERZEL[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
  int bd = rrule.indexOf("BYDAY=");
  String tage = bd >= 0 ? rrule.substring(bd + 6, rrule.indexOf(';', bd) < 0 ? rrule.length() : rrule.indexOf(';', bd)) : "";
  time_t dauer = e > s ? e - s : 3600;
  for (time_t t = s; t < bisWdh && termineN < 16; t += 86400) {
    if (t + dauer <= von) continue;
    struct tm lt;
    localtime_r(&t, &lt);
    if (woechentlich) {
      if (tage.length()) {
        if (tage.indexOf(KUERZEL[lt.tm_wday]) < 0) continue;
      } else {
        struct tm ls;
        localtime_r(&s, &ls);
        if (ls.tm_wday != lt.tm_wday) continue;
      }
    }
    terminDazu(t, t + dauer, titel, von, bis);
  }
}

static void kalenderLaden() {
  if (!plan.kalenderAktiv || !plan.kalenderUrl[0]) {
    strlcpy(kalenderStatus, "aus", sizeof kalenderStatus);
    termineN = 0;
    return;
  }
  if (WiFi.status() != WL_CONNECTED && !ETH.linkUp()) {
    strlcpy(kalenderStatus, "kein Internet", sizeof kalenderStatus);
    return;
  }
  time_t jetzt = time(nullptr);
  if (jetzt < 1700000000) {
    strlcpy(kalenderStatus, "Uhrzeit noch unbekannt (NTP)", sizeof kalenderStatus);
    return;
  }
  String url = plan.kalenderUrl;
  url.replace("webcal://", "https://");
  WiFiClientSecure tls;
  tls.setInsecure();  // nur lesend, keine Zugangsdaten im Spiel ausser der geheimen Adresse
  HTTPClient h;
  h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  h.setTimeout(15000);
  if (!h.begin(tls, url)) {
    strlcpy(kalenderStatus, "Adresse ungültig", sizeof kalenderStatus);
    return;
  }
  int code = h.GET();
  if (code != 200) {
    snprintf(kalenderStatus, sizeof kalenderStatus, "Abruf fehlgeschlagen (HTTP %d)", code);
    h.end();
    return;
  }
  termineN = 0;
  time_t von = jetzt - 3600, bis = jetzt + 7 * 86400;
  WiFiClient *st = h.getStreamPtr();
  String zeile, vorige, titel, rrule;
  time_t s = 0, e = 0;
  bool imEvent = false;
  uint32_t ende = millis() + 20000;
  auto verarbeiten = [&](const String &z) {
    if (z == "BEGIN:VEVENT") {
      imEvent = true;
      s = e = 0;
      titel = rrule = "";
    } else if (z == "END:VEVENT") {
      if (imEvent) ereignisAuswerten(s, e, titel, rrule, von, bis);
      imEvent = false;
    } else if (imEvent) {
      int k = z.indexOf(':');
      if (k < 0) return;
      String name = z.substring(0, k), wert = z.substring(k + 1);
      if (name.startsWith("DTSTART")) s = icsZeit(wert, false);
      else if (name.startsWith("DTEND")) e = icsZeit(wert, false);
      else if (name == "SUMMARY") titel = wert;
      else if (name == "RRULE") rrule = wert;
    }
  };
  while ((h.connected() || st->available()) && millis() < ende) {
    if (!st->available()) {
      delay(5);
      continue;
    }
    char c = st->read();
    if (c == '\r') continue;
    if (c != '\n') {
      if (zeile.length() < 400) zeile += c;
      continue;
    }
    if (zeile.startsWith(" ") || zeile.startsWith("\t")) vorige += zeile.substring(1);  // gefaltete Zeile
    else {
      if (vorige.length()) verarbeiten(vorige);
      vorige = zeile;
    }
    zeile = "";
  }
  if (vorige.length()) verarbeiten(vorige);
  h.end();
  std::sort(termine, termine + termineN, [](const Termin &a, const Termin &b) { return a.start < b.start; });
  snprintf(kalenderStatus, sizeof kalenderStatus, "%d Termine in den nächsten 7 Tagen", (int)termineN);
}

static volatile bool kalenderNeuLaden = true;
static void kalenderTask(void *) {
  uint32_t zuletzt = 0;
  for (;;) {
    if (kalenderNeuLaden || millis() - zuletzt > 15 * 60000UL) {
      kalenderNeuLaden = false;
      zuletzt = millis();
      kalenderLaden();
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ─── Ausfuehren ───────────────────────────────────────────────────────────
static void zeitplanLoop() {
  static uint32_t zuletzt = 0;
  static int letzteMinute = -1;
  if (maschineAn() && !letzteAktivitaetMs) letzteAktivitaetMs = millis();
  if (!maschineAn()) letzteAktivitaetMs = 0;
  if (shotLaeuft) letzteAktivitaetMs = millis();
  if (millis() - zuletzt < 5000) return;
  zuletzt = millis();
  if (!mainboardLebt() || ablaufLaeuft()) return;
  // Leerlauf
  if (plan.leerlaufMin && maschineAn() && letzteAktivitaetMs && !shotLaeuft &&
      millis() - letzteAktivitaetMs > plan.leerlaufMin * 60000UL) {
    ereignis("- Leerlauf %u min: Standby", plan.leerlaufMin);
    maschineSchalten(false);
    return;
  }
  struct tm t;
  if (!jetztLokal(t)) return;
  int minute = t.tm_hour * 60 + t.tm_min;
  if (minute == letzteMinute) return;
  letzteMinute = minute;
  int wtag = (t.tm_wday + 6) % 7;  // 0 = Montag
  if (plan.planAktiv) {
    for (int i = 0; i < plan.n; i++) {
      const PlanZeile &z = plan.zeilen[i];
      if (!z.aktiv || !(z.tage & (1 << wtag))) continue;
      if (minute == z.einH * 60 + z.einM && !maschineAn()) {
        ereignis("- Wochenplan: ein");
        maschineSchalten(true);
        letzteAktivitaetMs = millis();
      } else if (minute == z.ausH * 60 + z.ausM && maschineAn() && !shotLaeuft) {
        ereignis("- Wochenplan: aus");
        maschineSchalten(false);
      }
    }
  }
  if (plan.kalenderAktiv) {
    time_t jetzt = time(nullptr);
    for (int i = 0; i < termineN; i++) {
      const Termin &k = termine[i];
      time_t ein = k.start - plan.vorlaufMin * 60;
      if (jetzt >= ein && jetzt < ein + 60 && !maschineAn()) {
        ereignis("- Kalender „%s“: ein", k.titel);
        maschineSchalten(true);
        letzteAktivitaetMs = millis();
      } else if (jetzt >= k.ende && jetzt < k.ende + 60 && maschineAn() && !shotLaeuft) {
        ereignis("- Kalender „%s“ vorbei: aus", k.titel);
        maschineSchalten(false);
      }
    }
  }
}

static String zeitplanJson() {
  String j = "{\"plan_aktiv\":";
  j += plan.planAktiv ? "true" : "false";
  j += ",\"kalender_aktiv\":";
  j += plan.kalenderAktiv ? "true" : "false";
  j += ",\"vorlauf\":";
  j += plan.vorlaufMin;
  j += ",\"leerlauf\":";
  j += plan.leerlaufMin;
  j += ",\"kalender_url\":";
  jsonText(j, plan.kalenderUrl);
  j += ",\"stichwort\":";
  jsonText(j, plan.stichwort);
  j += ",\"kalender_status\":";
  jsonText(j, kalenderStatus);
  struct tm t;
  j += ",\"uhrzeit\":";
  if (jetztLokal(t)) {
    char b[24];
    strftime(b, sizeof b, "%Y-%m-%d %H:%M", &t);
    jsonText(j, b);
  } else j += "null";
  j += ",\"ntp\":";
  j += time(nullptr) > 1700000000 ? "true" : "false";
  j += ",\"zeilen\":[";
  for (int i = 0; i < plan.n; i++) {
    const PlanZeile &z = plan.zeilen[i];
    if (i) j += ',';
    char b[96];
    snprintf(b, sizeof b, "{\"tage\":%u,\"ein\":\"%02u:%02u\",\"aus\":\"%02u:%02u\",\"aktiv\":%s}", z.tage, z.einH, z.einM,
             z.ausH, z.ausM, z.aktiv ? "true" : "false");
    j += b;
  }
  j += "],\"termine\":[";
  for (int i = 0; i < termineN; i++) {
    if (i) j += ',';
    j += "{\"titel\":";
    jsonText(j, termine[i].titel);
    char b[48];
    struct tm s, e;
    localtime_r(&termine[i].start, &s);
    localtime_r(&termine[i].ende, &e);
    strftime(b, sizeof b, "\"%a %d.%m. %H:%M", &s);
    j += ",\"start\":";
    j += b;
    strftime(b, sizeof b, "–%H:%M\"", &e);
    j += b;
    j += '}';
  }
  j += "]}";
  return j;
}

// Formular: plan_aktiv, kalender_aktiv, vorlauf, leerlauf, kalender_url, stichwort,
// zeilen = "tage,HH:MM,HH:MM,aktiv;…"
static void zeitplanFeld(const String &k, const String &v) {
  if (k == "plan_aktiv") plan.planAktiv = v == "1";
  else if (k == "kalender_aktiv") plan.kalenderAktiv = v == "1";
  else if (k == "vorlauf") plan.vorlaufMin = constrain(v.toInt(), 0, 120);
  else if (k == "leerlauf") plan.leerlaufMin = constrain(v.toInt(), 0, 600);
  else if (k == "kalender_url") strlcpy(plan.kalenderUrl, v.c_str(), sizeof plan.kalenderUrl);
  else if (k == "stichwort") strlcpy(plan.stichwort, v.c_str(), sizeof plan.stichwort);
  else if (k == "zeilen") {
    plan.n = 0;
    int a = 0;
    while (a < (int)v.length() && plan.n < 10) {
      int e = v.indexOf(';', a);
      if (e < 0) e = v.length();
      String z = v.substring(a, e);
      a = e + 1;
      int tage, eh, em, ah, am, akt;
      if (sscanf(z.c_str(), "%d,%d:%d,%d:%d,%d", &tage, &eh, &em, &ah, &am, &akt) != 6) continue;
      plan.zeilen[plan.n++] = {(uint8_t)(tage & 0x7F), (uint8_t)constrain(eh, 0, 23), (uint8_t)constrain(em, 0, 59),
                               (uint8_t)constrain(ah, 0, 23), (uint8_t)constrain(am, 0, 59), akt == 1};
    }
  }
}

static void zeitplanSetup() {
  planLaden();
  xTaskCreatePinnedToCore(kalenderTask, "kalender", 12288, nullptr, 1, nullptr, 0);
}
