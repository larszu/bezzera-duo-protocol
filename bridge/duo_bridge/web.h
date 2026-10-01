#pragma once
// Weboberflaeche der Bridge. Erreichbar ueber
//   - Ethernet (W5500 auf dem Waveshare ESP32-S3-ETH),
//   - das Heim-WLAN, wenn per USB-Befehl "n <ssid> <passwort>" hinterlegt
//     (gespeichert in NVS, nicht im Quelltext),
//   - das eigene WLAN "duo-bridge" / espresso1: dort beantwortet die Bridge
//     jede DNS-Anfrage mit sich selbst, also http://espresso.maschine oder
//     jeder andere Name; Handys oeffnen die Seite als Anmeldeportal von selbst.
// Im Heimnetz: http://duo.local oder die IP, die beim Start ueber USB kommt.
//
// GET  /                  Oberflaeche (web_ui.h)
// GET  /api/status        JSON: Seite, Uhr, Lebenszeichen, alle VPs, neue Ereignisse
// POST /api/cmd           eine Befehlszeile wie ueber USB (p, w, o, d, m, s)
// GET  /api/zusatz        JSON: Maschine, Waage, Bezug, MQTT, Einstellungen
// GET  /api/verlauf       ?sek=3600&max=720: VP 0x0050 je Sekunde (zusatz.h)
// GET  /api/bezug         Kurve des laufenden bzw. letzten Bezugs
// POST /api/aktion        an | aus | tara | abbruch | stopp | ziel <g>
// POST /api/einstellungen feld=wert&... (URL-kodiert)
// POST /api/waage         Gewicht in g von einer WLAN-Waage (auch GET ?g=)
//
// Kein Login: nur im eigenen Netz betreiben.

#include <ETH.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <WebServer.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Update.h>
#include <esp_wifi.h>

#include "tasten.h"
#include "web_ui.h"
#include "zusatz.h"
#include "waage.h"
#include "profile.h"
#include "shots.h"
#include "ha_mqtt.h"

static const int ETH_CS = 14, ETH_IRQ = 10, ETH_RST = 9;
static const int ETH_SCK = 13, ETH_MISO = 12, ETH_MOSI = 11;

static WebServer server(80);
static DNSServer dns;  // im eigenen WLAN: jeder Name fuehrt zur Bridge (Captive Portal)

// Staendige Verbindungsversuche mit einem unerreichbaren Heim-WLAN lassen den
// Funk die Kanaele wechseln; das eigene WLAN "duo-bridge" wird dann unsichtbar.
// Deshalb: nach einigen Fehlversuchen aufhoeren und alle 5 min neu probieren.
static volatile uint32_t heimWlanFehlschlaege = 0;
static uint32_t heimWlanPause = 0;
static bool heimWlanAn = false;
static void ereignis(const char *fmt, ...);

#include "netz.h"
#include "zeitplan.h"
#include "improv.h"
#include "matter_home.h"
#include "benachrichtigung.h"
#include "sichern.h"
#include "eigene_seiten.h"

static void netzEreignis(arduino_event_id_t e) {
  if (e == ARDUINO_EVENT_ETH_GOT_IP) {
    Serial.printf("# Ethernet: http://%s  (http://duo.local)\n", ETH.localIP().toString().c_str());
    ereignis("- Ethernet %s", ETH.localIP().toString().c_str());
  } else if (e == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    Serial.printf("# WLAN %s: http://%s  (http://duo.local)\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    ereignis("- WLAN %s", WiFi.localIP().toString().c_str());
  } else if (e == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    static uint32_t zuletzt = 0;
    if (millis() - zuletzt > 10000) {  // nicht bei jedem Neuversuch
      zuletzt = millis();
      Serial.println("# WLAN nicht verbunden (Name/Passwort pruefen, 'n ?' listet Netze)");
    }
    heimWlanFehlschlaege++;
  } else if (e == ARDUINO_EVENT_ETH_DISCONNECTED) {
    Serial.println("# Ethernet getrennt");
  }
}

// Zustand als JSON; genutzt von /api/status und vom USB-Befehl "j" (lokale
// Oberflaeche am Rechner, tools/web_lokal.py).
String statusText(uint32_t seit) {
  uint32_t jetzt = millis();
  String j;
  j.reserve(6000);
  j += "{\"ms\":";
  j += jetzt;
  j += ",\"seite\":";
  j += leitung.seite;
  j += ",\"rtc\":";
  if (leitung.rtcGueltig) {
    char b[48];
    snprintf(b, sizeof b, "\"20%02x-%02x-%02x %02x:%02x:%02x\"", leitung.rtc[0], leitung.rtc[1], leitung.rtc[2],
             leitung.rtc[4], leitung.rtc[5], leitung.rtc[6]);
    j += b;
  } else {
    j += "null";
  }
  j += ",\"display\":{\"rahmen\":";
  j += leitung.rahmenDisplay;
  j += ",\"still_ms\":";
  j += leitung.rahmenDisplay ? (long)(jetzt - leitung.zuletztDisplay) : -1L;
  j += "},\"mainboard\":{\"rahmen\":";
  j += leitung.rahmenMainboard;
  j += ",\"still_ms\":";
  j += leitung.rahmenMainboard ? (long)(jetzt - leitung.zuletztMainboard) : -1L;
  j += "},\"vps\":[";
  for (size_t i = 0; i < vpAnzahl; i++) {
    const VpWert &v = vpTabelle[i];
    if (i) j += ',';
    j += "{\"vp\":";
    j += v.vp;
    j += ",\"alter_ms\":";
    j += jetzt - v.ms;
    j += ",\"w\":[";
    for (size_t k = 0; k + 1 < v.len; k += 2) {
      if (k) j += ',';
      j += (v.d[k] << 8) | v.d[k + 1];
    }
    j += "]}";
  }
  j += "],\"overrides\":[";
  bool erstes = true;
  for (auto &o : overrides) {
    if (!o.aktiv) continue;
    if (!erstes) j += ',';
    erstes = false;
    j += "{\"vp\":";
    j += o.vp;
    j += ",\"wert\":";
    j += o.wert;
    j += ",\"rest\":";
    j += o.rest;
    j += '}';
  }
  j += "],\"ereignis_nr\":";
  j += ereignisNr;
  j += ",\"ereignisse\":[";
  uint32_t ab = ereignisNr > 40 ? ereignisNr - 40 : 0;
  if (seit > ab) ab = seit;
  for (uint32_t i = ab; i < ereignisNr; i++) {
    if (i != ab) j += ',';
    j += '"';
    for (const char *c = ereignisse[i % 40]; *c; c++) {
      if (*c == '"' || *c == '\\') j += '\\';
      j += *c;
    }
    j += '"';
  }
  j += "],\"emulation\":";
  j += emulation;
  j += "}";
  return j;
}

static bool zugang();
static void statusJson() {
  if (!zugang()) return;
  uint32_t seit = server.hasArg("seit") ? server.arg("seit").toInt() : 0;
  server.send(200, "application/json", statusText(seit));
}

static void befehlWeb() {
  if (!zugang()) return;
  String z = server.arg("plain");
  z.trim();
  if (!z.length() || z.length() > 380 || strchr("pwodmse", z[0]) == nullptr) {
    server.send(400, "text/plain", "unbekannter Befehl");
    return;
  }
  char buf[400];
  z.toCharArray(buf, sizeof buf);
  ereignis("- Web: %s", buf);
  befehl(buf);
  server.send(200, "text/plain", "ok");
}

static String urlDecode(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '+') o += ' ';
    else if (c == '%' && i + 2 < s.length()) {
      o += (char)strtol(s.substring(i + 1, i + 3).c_str(), nullptr, 16);
      i += 2;
    } else o += c;
  }
  return o;
}

static String queryWert(const String &q, const char *k) {
  String such = String(k) + "=";
  int i = ("&" + q).indexOf("&" + such);
  if (i < 0) return "";
  int e = q.indexOf('&', i);
  return urlDecode(q.substring(i + such.length(), e < 0 ? q.length() : e));
}

// Zusatz-API, gemeinsam fuer den Webserver und den USB-Befehl "z" (tools/web_lokal.py).
static int zusatzApi(const String &pfad, const String &query, const String &rumpf, String &antwort) {
  if (pfad == "/api/zusatz") antwort = zusatzJson();
  else if (pfad == "/api/verlauf") {
    String s = queryWert(query, "sek"), m = queryWert(query, "max");
    antwort = verlaufJson(s.length() ? s.toInt() : 3600, m.length() ? m.toInt() : 720);
  } else if (pfad == "/api/bezug") antwort = bezugJson();
  else if (pfad == "/api/aktion") {
    String a = rumpf;
    a.trim();
    if (a.startsWith("uhr ") && !statusAktuell()) {  // im Standby nimmt das Mainboard keine Einstellungen an (gemessen)
      antwort = "Uhr stellen geht nur bei eingeschalteter Maschine";
      return 409;
    }
    if (!aktion(a)) {
      antwort = "unbekannte Aktion";
      return 400;
    }
    ereignis("- Web: %s", a.c_str());
    antwort = "ok";
  } else if (pfad == "/api/einstellungen") {
    bool mqttNeuStart = false;
    int a = 0;
    while (a < (int)rumpf.length()) {
      int e = rumpf.indexOf('&', a);
      if (e < 0) e = rumpf.length();
      String paar = rumpf.substring(a, e);
      a = e + 1;
      int g = paar.indexOf('=');
      if (g < 0) continue;
      String k = urlDecode(paar.substring(0, g)), v = urlDecode(paar.substring(g + 1));
      v.trim();
      if (!setzeEinstellung(k, v)) continue;
      if (k.startsWith("mqtt_") || k == "ha_prefix" || k.startsWith("druck_") || k.startsWith("waage_") || k == "stopp_pin") mqttNeuStart = true;
      if (k == "stopp_pin" && cfg.stoppPin >= 0) pinMode(cfg.stoppPin, OUTPUT);
    }
    einstellungenSpeichern();
    stoppAusgang(stoppAktiv);
    if (mqttNeuStart) mqttStarten();  // Discovery kommt beim Verbinden neu
    ereignis("- Einstellungen gespeichert");
    antwort = "ok";
  } else if (pfad == "/api/profile") {
    antwort = profileJson();
  } else if (pfad == "/api/profil") {  // speichern: nr=-1 legt neu an
    String nrText = queryWert(rumpf, "nr");
    int nr = nrText.length() ? nrText.toInt() : -1;
    if (nr < 0)
      for (int i = 0; i < PROFILE_MAX && nr < 0; i++)
        if (!profile[i].belegt) nr = i;
    if (nr < 0 || nr >= PROFILE_MAX) {
      antwort = "kein Platz (höchstens 20 Profile)";
      return 400;
    }
    Profil &p = profile[nr];
    if (!p.belegt) {
      memset(&p, 0, sizeof p);
      p.temp = 93;
      p.vorb = 0;
      p.prio = 255;
    }
    int a = 0;
    while (a < (int)rumpf.length()) {
      int e = rumpf.indexOf('&', a);
      if (e < 0) e = rumpf.length();
      String paar = rumpf.substring(a, e);
      a = e + 1;
      int g = paar.indexOf('=');
      if (g > 0) profilFeld(p, urlDecode(paar.substring(0, g)), urlDecode(paar.substring(g + 1)));
    }
    if (!p.name[0]) snprintf(p.name, sizeof p.name, "Profil %d", nr + 1);
    p.belegt = true;
    profilSpeichern(nr);
    antwort = String(nr);
  } else if (pfad == "/api/profil_aktion") {  // "anwenden N" | "loeschen N"
    String a = rumpf;
    a.trim();
    int nr = a.substring(a.indexOf(' ') + 1).toInt();
    if (a.startsWith("anwenden ")) {
      const char *f = profilAnwenden(nr);
      antwort = f ? f : "ok";
      return f ? 409 : 200;
    } else if (a.startsWith("loeschen ") && nr >= 0 && nr < PROFILE_MAX) {
      profile[nr].belegt = false;
      if (profilAktiv == nr) profilAktiv = -1;
      profilSpeichern(nr);
      antwort = "ok";
    } else {
      antwort = "unbekannt";
      return 400;
    }
  } else if (pfad == "/api/shots") {
    antwort = shotsJson();
  } else if (pfad == "/api/bruehkurve") {  // laufender bzw. letzter Bezug, wie auf dem Display
    antwort = "{\"laeuft\":";
    antwort += shotLaeuft ? "true" : "false";
    antwort += ",\"nr\":";
    antwort += kurvenNr[shotSlot];
    antwort += ",\"druck_max\":";
    antwort += shotDruckMax;
    antwort += ",\"temp_mittel\":";
    antwort += shotTempN ? shotTempSumme / shotTempN : 0.0f;
    antwort += ",\"p\":";
    antwort += shotKurveJson(kurvenNr[shotSlot]);
    antwort += '}';
  } else if (pfad == "/api/shot") {
    antwort = shotKurveJson(queryWert(query, "nr").toInt());
  } else if (pfad == "/api/shot_aktion") {
    String a = urlDecode(rumpf);
    a.trim();
    if (!shotAktion(a)) {
      antwort = "unbekannt";
      return 400;
    }
    antwort = "ok";
  } else if (pfad == "/api/ble_suche") {
    bleSucheAnfordern = true;
    bleFundeN = 0;
    antwort = "ok";
  } else if (pfad == "/api/ble_geraete") {
    antwort = "{\"laeuft\":";
    antwort += (bleSucheLaeuft || bleSucheAnfordern) ? "true" : "false";
    antwort += ",\"geraete\":[";
    for (int i = 0; i < bleFundeN; i++) {
      if (i) antwort += ',';
      antwort += "{\"name\":";
      jsonText(antwort, bleFunde[i].name);
      antwort += ",\"adresse\":";
      jsonText(antwort, bleFunde[i].adresse);
      antwort += ",\"rssi\":";
      antwort += bleFunde[i].rssi;
      antwort += ",\"bekannt\":";
      antwort += bleFunde[i].bekannt ? "true" : "false";
      antwort += '}';
    }
    antwort += "]}";
  } else if (pfad == "/api/maschine_setzen") {  // "vp=96&wert=94"
    String vpT = queryWert(rumpf, "vp"), wT = queryWert(rumpf, "wert");
    if (queryWert(rumpf, "technik_pw").length()) technikPasswort = queryWert(rumpf, "technik_pw");
    const char *f = vpT.length() && wT.length() ? maschinendatumSetzen(vpT.toInt(), wT.toInt()) : "vp und wert fehlen";
    antwort = f ? f : "ok";
    if (f) return 409;
  } else if (pfad == "/api/maschine") {  // alles, was das Mainboard in den Variablenspeicher geschrieben hat
    antwort = "{\"vps\":{";
    bool erstes = true;
    for (uint16_t vp = 0; vp < 0x100; vp++) {
      if (!vpMainboardMs[vp]) continue;
      if (!erstes) antwort += ',';
      erstes = false;
      antwort += "\"" + String(vp) + "\":[";
      antwort += vpRam[vp];
      antwort += ',';
      antwort += (millis() - vpMainboardMs[vp]) / 1000;
      antwort += ']';
    }
    antwort += "},\"aenderbar\":{";
    bool e1 = true;
    for (const auto &e : EDIT_WEGE) {
      if (!e1) antwort += ',';
      e1 = false;
      antwort += "\"" + String(e.vp) + "\":[" + String(e.min) + "," + String(e.max) + "," + String(e.weg) + "]";
    }
    antwort += "},\"ablauf\":";
    antwort += ablaufLaeuft() ? "true" : "false";
    antwort += ",\"ergebnis\":";
    jsonText(antwort, editErgebnis);
    antwort += ",\"bezuege_maschine\":";
    antwort += bezuegeMaschine();
    antwort += '}';
  } else if (pfad == "/api/wlan") {
    if (queryWert(query, "suche") == "1" && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
      WiFi.scanDelete();
      WiFi.scanNetworks(true, true, false, 400);  // asynchron, auch versteckte, 400 ms je Kanal
    }
    antwort = wlanJson();
  } else if (pfad == "/api/wlan_setzen") {
    heimWlanSpeichern(queryWert(rumpf, "ssid"), queryWert(rumpf, "pass"));
    antwort = "ok";
  } else if (pfad == "/api/sicherheit") {  // ap_pass (mind. 8 Zeichen), web_pass ("-" = entfernen)
    String ap = queryWert(rumpf, "ap_pass"), web = queryWert(rumpf, "web_pass");
    Preferences pr;
    pr.begin("netz", false);
    if (ap.length() >= 8) {
      apPass = ap;
      pr.putString("ap_pass", ap);
    }
    if (web.length()) {
      webPass = web == "-" ? "" : web;
      pr.putString("web_pass", webPass);
    }
    String benutzer = queryWert(rumpf, "web_user");
    if (benutzer.length()) {
      webUser = benutzer;
      pr.putString("web_user", webUser);
    }
    pr.end();
    antwort = ap.length() && ap.length() < 8 ? "WLAN-Passwort braucht mindestens 8 Zeichen" : "ok";
    if (ap.length() >= 8) WiFi.softAP("duo-bridge", apPass.c_str());  // gilt sofort
  } else if (pfad == "/api/melden") {  // Felder speichern; "test=1" schickt eine Probemeldung
    if (rumpf.length()) {
      int a = 0;
      while (a < (int)rumpf.length()) {
        int e = rumpf.indexOf('&', a);
        if (e < 0) e = rumpf.length();
        String paar = rumpf.substring(a, e);
        a = e + 1;
        int g = paar.indexOf('=');
        if (g > 0) benFeld(urlDecode(paar.substring(0, g)), urlDecode(paar.substring(g + 1)));
      }
      benSpeichern();
      if (queryWert(rumpf, "test") == "1") melden("Bezzera Duo", "Probemeldung: %s", kurzStatus().c_str());
    }
    antwort = benJson();
  } else if (pfad == "/api/matter") {
    if (rumpf.length() && !matterAktion(rumpf)) {
      antwort = "unbekannt";
      return 400;
    }
    antwort = matterJson();
  } else if (pfad == "/api/zeitplan") {
    if (rumpf.length()) {
      int a = 0;
      while (a < (int)rumpf.length()) {
        int e = rumpf.indexOf('&', a);
        if (e < 0) e = rumpf.length();
        String paar = rumpf.substring(a, e);
        a = e + 1;
        int g = paar.indexOf('=');
        if (g > 0) zeitplanFeld(urlDecode(paar.substring(0, g)), urlDecode(paar.substring(g + 1)));
      }
      planSpeichern();
      kalenderNeuLaden = true;
    }
    antwort = zeitplanJson();
  } else if (pfad == "/api/waage") {
    float g = waageZahl((rumpf.length() ? rumpf : queryWert(query, "g")).c_str());
    if (isnan(g)) {
      antwort = "keine Zahl";
      return 400;
    }
    waageMelden(g);
    antwort = "ok";
  } else {
    antwort = "unbekannt";
    return 404;
  }
  return 200;
}

static void zusatzWeb() {
  if (server.uri() != "/api/waage" && !zugang()) return;  // WLAN-Waagen melden ohne Passwort
  String q, antwort;
  for (int i = 0; i < server.args(); i++) {
    if (server.argName(i) == "plain") continue;
    if (q.length()) q += '&';
    q += server.argName(i) + "=" + server.arg(i);
  }
  int code = zusatzApi(server.uri(), q, server.arg("plain"), antwort);
  server.send(code, antwort.startsWith("{") ? "application/json" : "text/plain", antwort);
}

// USB-Befehl "z <pfad>[?query] [rumpf]": Antwort als Zeile "#Z <antwort>"
void zusatzBefehl(char *s) {
  while (*s == ' ') s++;
  String z = s, pfad = z, rumpf;
  int sp = z.indexOf(' ');
  if (sp >= 0) {
    pfad = z.substring(0, sp);
    rumpf = z.substring(sp + 1);
  }
  String query;
  int f = pfad.indexOf('?');
  if (f >= 0) {
    query = pfad.substring(f + 1);
    pfad = pfad.substring(0, f);
  }
  String antwort;
  zusatzApi(pfad, query, rumpf, antwort);
  usbAntwort("#Z " + antwort);
}

void webSetup() {
  Network.onEvent(netzEreignis);
  ETH.begin(ETH_PHY_W5500, 1, ETH_CS, ETH_IRQ, ETH_RST, SPI2_HOST, ETH_SCK, ETH_MISO, ETH_MOSI);
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_set_country_code("DE", false);  // Kanaele 1-13 fest, nicht von Nachbar-Routern (802.11d) einschraenken lassen
  netzLaden();
  WiFi.softAP("duo-bridge", apPass.c_str());
  Preferences pref;
  pref.begin("netz", true);
  String ssid = pref.getString("ssid", ""), pass = pref.getString("pass", "");
  pref.end();
  if (ssid.length()) {
    heimWlanAn = true;
    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.printf("# verbinde mit WLAN %s ...\n", ssid.c_str());
  }
  Serial.printf("# WLAN duo-bridge: http://%s\n", WiFi.softAPIP().toString().c_str());
  MDNS.begin("duo");
  MDNS.addService("http", "tcp", 80);
  server.on("/", HTTP_GET, [] {
    if (zugang()) server.send_P(200, "text/html; charset=utf-8", WEB_UI);
  });
  server.on("/tasten.js", HTTP_GET, [] {
    if (zugang()) server.send_P(200, "text/javascript; charset=utf-8", TASTEN_JS);
  });
  server.on("/api/status", HTTP_GET, statusJson);
  server.on("/api/cmd", HTTP_POST, befehlWeb);
  // Captive-Portal-Pruefadressen von Android, Apple und Windows und alles
  // Unbekannte: zur Oberflaeche umleiten
  server.onNotFound([] {
    if (server.uri().startsWith("/api/")) {
      server.send(404, "text/plain", "unbekannt");
      return;
    }
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
  });
  dns.start(53, "*", WiFi.softAPIP());
  for (const char *p : {"/api/zusatz", "/api/verlauf", "/api/bezug", "/api/aktion", "/api/einstellungen", "/api/waage",
                        "/api/profile", "/api/profil", "/api/profil_aktion", "/api/shots", "/api/shot", "/api/shot_aktion", "/api/bruehkurve",
                        "/api/ble_suche", "/api/ble_geraete", "/api/maschine", "/api/maschine_setzen", "/api/wlan", "/api/wlan_setzen",
                        "/api/sicherheit", "/api/zeitplan", "/api/matter", "/api/melden"})
    server.on(p, zusatzWeb);
  // Firmware ueber die Weboberflaeche: duo_bridge.ino.bin (nur das Programm,
  // nicht die Flash-Datei mit Bootloader) in den freien OTA-Platz, dann Neustart.
  server.on(
      "/api/firmware", HTTP_POST,
      [] {
        if (!zugang()) return;
        bool ok = !Update.hasError();
        server.send(ok ? 200 : 500, "text/plain", ok ? "ok, Neustart" : Update.errorString());
        if (ok) {
          delay(300);
          ESP.restart();
        }
      },
      [] {
        if (!zugang()) return;
        HTTPUpload &u = server.upload();
        if (u.status == UPLOAD_FILE_START) {
          ereignis("- Firmware-Update: %s", u.filename.c_str());
          Update.begin(UPDATE_SIZE_UNKNOWN);
        } else if (u.status == UPLOAD_FILE_WRITE) {
          Update.write(u.buf, u.currentSize);
        } else if (u.status == UPLOAD_FILE_END) {
          Update.end(true);
        } else if (u.status == UPLOAD_FILE_ABORTED) {
          Update.abort();
        }
      });
  server.begin();
  zusatzSetup();
  profileLaden();
  shotsSetup();
  zeitplanSetup();
  matterLaden();
  benSetup();
  mqttSetup();
  waageSetup();
}

// Werte, die nur das Display kennt, regelmaessig selbst nachlesen; die
// Antworten gehen nicht ans Mainboard (Modus 0) und nicht ins Protokoll.
//   Register 0x03 (aktuelle Seite) alle 2 s: die Bridge erfaehrt Seitenwechsel
//     sonst nur, wenn das Mainboard sie schaltet, nach einem Neustart also gar nicht
//   Register 0x20 (Uhr, 7 Byte) alle 6 s: das Mainboard stellt sie im Betrieb
//     nur einmal, danach laeuft sie im Display weiter
static void displayNachlesen() {
  static uint32_t zuletzt = 0;
  static uint8_t takt = 0;
  if (millis() - zuletzt < 2000 || emulation == 1) return;
  zuletzt = millis();
  char b[32];
  strcpy(b, ++takt % 3 ? "d c6 a5 03 81 03 02" : "d c6 a5 03 81 20 07");
  befehl(b);
}

void webLoop() {
  dns.processNextRequest();
  server.handleClient();
  displayNachlesen();
  ablaufLoop();
  shotsLoop();
  zeitLoop();
  zeitplanLoop();
  improvLoop();
  matterLoop();
  benLoop();
  sichernLoop();
  eigeneSeitenLoop();
  verlaufLoop();
  bezugLoop();
  mqttLoop();
  if (!heimWlanAn) return;
  if (WiFi.status() == WL_CONNECTED) {
    heimWlanFehlschlaege = 0;
  } else if (heimWlanPause == 0 && heimWlanFehlschlaege >= 5) {
    WiFi.disconnect();  // Versuche einstellen, Funk bleibt auf dem Kanal des eigenen WLANs
    heimWlanPause = millis();
    Serial.println("# Heim-WLAN nicht erreichbar, neuer Versuch in 5 min");
  } else if (heimWlanPause && millis() - heimWlanPause > 300000) {
    heimWlanPause = 0;
    heimWlanFehlschlaege = 0;
    WiFi.reconnect();
  }
}

// USB-Befehl "n <ssid> <passwort>": Heim-WLAN speichern und verbinden.
// "n -" loescht es. Leerzeichen im Namen: "n Mein\ WLAN geheim".
void heimWlan(char *s) {
  while (*s == ' ') s++;
  if (*s == '?') {
    Serial.printf("# WLAN-Status %d, sichtbare Netze:\n", (int)WiFi.status());
    WiFi.disconnect();  // laufende Verbindungsversuche stoeren den Scan
    delay(100);
    // "n ?6" nur Kanal 6, passiv und lange (findet auch Netze, die auf Proben nicht antworten)
    int kanal = atoi(s + 1);
    int n = kanal ? WiFi.scanNetworks(false, true, true, 1500, kanal) : WiFi.scanNetworks(false, true, false, 400);
    Serial.printf("#   (%d Netze)\n", n);
    for (int i = 0; i < n; i++)
      Serial.printf("#   %-32s %4d dBm  Kanal %d  %s\n", WiFi.SSID(i).length() ? WiFi.SSID(i).c_str() : "(versteckt)", WiFi.RSSI(i),
                    WiFi.channel(i), WiFi.BSSIDstr(i).c_str());
    WiFi.scanDelete();
    return;
  }
  Preferences pref;
  pref.begin("netz", false);
  if (*s == '-') {
    pref.clear();
    pref.end();
    heimWlanAn = false;
    WiFi.disconnect();
    Serial.println("# Heim-WLAN geloescht");
    return;
  }
  String ssid, pass;
  bool imNamen = true;
  for (char *c = s; *c; c++) {
    if (imNamen && *c == '\\' && c[1] == ' ') {
      ssid += ' ';
      c++;
    } else if (imNamen && *c == ' ') {
      imNamen = false;
    } else {
      (imNamen ? ssid : pass) += *c;
    }
  }
  pref.end();
  Serial.printf("# Heim-WLAN %s gespeichert, verbinde ...\n", ssid.c_str());
  heimWlanSpeichern(ssid, pass);
}
