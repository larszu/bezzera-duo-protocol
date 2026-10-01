#pragma once
// Netz: Heim-WLAN einrichten (Portal, USB, Improv), Passwortschutz, Uhrzeit.
//
// Ersteinrichtung ohne Rechner: Handy mit dem WLAN "duo-bridge" verbinden, die
// Seite oeffnet sich als Anmeldeportal von selbst, unter "Einrichten" das
// Heim-WLAN aus der Liste waehlen und das Passwort eingeben. Danach ist die
// Bridge im Heimnetz unter http://duo.local erreichbar.
// Direkt nach dem Flashen im Browser fragt die Flash-Seite (ESP Web Tools)
// die WLAN-Daten ueber Improv ab (improv.h).

#include <time.h>

static String apPass = "espresso1";  // bis zur Einrichtung; danach eigenes
static String webPass;               // leer = ohne Passwort (nur im eigenen Netz!)
static String webUser = "duo";        // Benutzername fuer HTTP Basic

static void netzLaden() {
  Preferences p;
  p.begin("netz", true);
  apPass = p.getString("ap_pass", "espresso1");
  webPass = p.getString("web_pass", "");
  webUser = p.getString("web_user", "duo");
  p.end();
  if (apPass.length() < 8) apPass = "espresso1";
}

// Heim-WLAN speichern und verbinden (Portal, USB-Befehl n, Improv)
static void heimWlanSpeichern(const String &ssid, const String &pass) {
  Preferences pref;
  pref.begin("netz", false);
  pref.putString("ssid", ssid);
  pref.putString("pass", pass);
  pref.end();
  heimWlanAn = ssid.length() > 0;
  heimWlanFehlschlaege = 0;
  heimWlanPause = 0;
  WiFi.disconnect();
  if (heimWlanAn) WiFi.begin(ssid.c_str(), pass.c_str());
  ereignis("- Heim-WLAN %s gespeichert", ssid.c_str());
}

// HTTP Basic: Benutzer (Standard "duo") und Passwort aus der Einrichtung. Ohne Passwort offen.
static bool zugang() {
  if (!webPass.length() || server.authenticate(webUser.c_str(), webPass.c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, "Bezzera Duo Bridge");
  return false;
}

// ─── WLAN-Suche fuer das Portal (asynchron, blockiert loop() nicht) ───────
static bool wlanSucheGestartet = false;

static String wlanJson() {
  String j = "{\"verbunden\":";
  j += WiFi.status() == WL_CONNECTED ? "true" : "false";
  j += ",\"ssid\":";
  jsonText(j, WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String(""));
  j += ",\"ip\":";
  jsonText(j, WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String(""));
  j += ",\"eth\":";
  jsonText(j, ETH.linkUp() ? ETH.localIP().toString() : String(""));
  j += ",\"ap\":\"duo-bridge\",\"ap_eigenes_passwort\":";
  j += apPass != "espresso1" ? "true" : "false";
  j += ",\"web_passwort\":";
  j += webPass.length() ? "true" : "false";
  j += ",\"sucht\":";
  int n = WiFi.scanComplete();
  j += n == WIFI_SCAN_RUNNING ? "true" : "false";
  j += ",\"netze\":[";
  if (n > 0) {
    // doppelte Namen (mehrere Zugangspunkte) nur einmal, staerkstes Signal
    String gesehen = "\n";
    bool erstes = true;
    for (int i = 0; i < n; i++) {
      String s = WiFi.SSID(i);
      if (!s.length() || gesehen.indexOf("\n" + s + "\n") >= 0) continue;
      gesehen += s + "\n";
      if (!erstes) j += ',';
      erstes = false;
      j += "{\"ssid\":";
      jsonText(j, s);
      j += ",\"rssi\":";
      j += WiFi.RSSI(i);
      j += ",\"offen\":";
      j += WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false";
      j += '}';
    }
  }
  j += "]}";
  return j;
}

// ─── Uhrzeit: NTP, sobald ein Netz da ist, sonst die Display-Uhr ──────────
static bool ntpGestartet = false;

static void zeitLoop() {
  bool netz = WiFi.status() == WL_CONNECTED || (ETH.linkUp() && ETH.localIP() != IPAddress(0, 0, 0, 0));
  if (netz && !ntpGestartet) {
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.cloudflare.com");
    ntpGestartet = true;
  }
}

// Lokale Zeit; false = unbekannt. Quelle: NTP, sonst Display-Uhr (BCD).
static bool jetztLokal(struct tm &t) {
  time_t now = time(nullptr);
  if (now > 1700000000) {
    localtime_r(&now, &t);
    return true;
  }
  if (!leitung.rtcGueltig) return false;
  auto b = [](uint8_t x) { return (x >> 4) * 10 + (x & 0x0F); };
  memset(&t, 0, sizeof t);
  t.tm_year = 100 + b(leitung.rtc[0]);
  t.tm_mon = b(leitung.rtc[1]) - 1;
  t.tm_mday = b(leitung.rtc[2]);
  t.tm_hour = b(leitung.rtc[4]);
  t.tm_min = b(leitung.rtc[5]);
  t.tm_sec = b(leitung.rtc[6]) + (millis() - leitung.rtcMs) / 1000;
  mktime(&t);  // normalisiert Sekundenueberlauf und setzt den Wochentag
  return true;
}
