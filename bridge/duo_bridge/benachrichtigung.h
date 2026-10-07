#pragma once
// Meldungen aufs Handy und Bedienung von unterwegs ohne Tunnel:
//   ntfy     Push-Meldungen ueber ntfy.sh (oder eigenen Server): App "ntfy"
//            installieren, ein geheimes Thema abonnieren, dieselbe Adresse
//            hier eintragen, z. B. https://ntfy.sh/duo-7f3k2q9x
//   Telegram eigener Bot (bei @BotFather anlegen, Token eintragen). Befehle:
//            /an /aus /status /hilfe. Nur Chats, deren Nummer eingetragen ist,
//            duerfen schalten; unbekannte Chats bekommen ihre Nummer genannt.
// Beides laeuft in einer eigenen Task (TLS dauert), Befehle gehen ueber eine
// Warteschlange zurueck in loop().
// Meldungen: bereit, Shot fertig, Ruecksspuelen faellig, Alarm, Ein/Aus.

#include <WiFiClientSecure.h>

struct Benachrichtigung {
  String ntfyUrl, tgToken, tgChats;
  bool bereit = true, shot = true, wartung = true, alarm = true;
};
static Benachrichtigung ben;

static void benLaden() {
  Preferences p;
  p.begin("melden", true);
  ben.ntfyUrl = p.getString("ntfy", "");
  ben.tgToken = p.getString("tg_token", "");
  ben.tgChats = p.getString("tg_chats", "");
  ben.bereit = p.getBool("bereit", true);
  ben.shot = p.getBool("shot", true);
  ben.wartung = p.getBool("wartung", true);
  ben.alarm = p.getBool("alarm", true);
  p.end();
}
static void benSpeichern() {
  Preferences p;
  p.begin("melden", false);
  p.putString("ntfy", ben.ntfyUrl);
  p.putString("tg_token", ben.tgToken);
  p.putString("tg_chats", ben.tgChats);
  p.putBool("bereit", ben.bereit);
  p.putBool("shot", ben.shot);
  p.putBool("wartung", ben.wartung);
  p.putBool("alarm", ben.alarm);
  p.end();
}

struct Meldung {
  char titel[48];
  char text[160];
  char chat[24];  // leer = an alle (ntfy + alle Telegram-Chats)
};
static QueueHandle_t meldungen = nullptr;
static QueueHandle_t tgBefehle = nullptr;  // char[64]: "an" | "aus" | "status <chat>"
static char benStatus[64] = "aus";

static void melden(const char *titel, const char *fmt, ...) {
  if (!meldungen) return;
  Meldung m = {};
  strlcpy(m.titel, titel, sizeof m.titel);
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(m.text, sizeof m.text, fmt, ap);
  va_end(ap);
  xQueueSend(meldungen, &m, 0);
}
static void meldenAn(const char *chat, const char *text) {
  if (!meldungen) return;
  Meldung m = {};
  strlcpy(m.text, text, sizeof m.text);
  strlcpy(m.chat, chat, sizeof m.chat);
  xQueueSend(meldungen, &m, 0);
}

static String jsonFeld(const String &j, const char *name, int ab = 0) {  // einfacher Wert hinter "name":
  int i = j.indexOf(String("\"") + name + "\":", ab);
  if (i < 0) return "";
  i += strlen(name) + 3;
  if (j[i] == '"') {
    int e = j.indexOf('"', i + 1);
    return j.substring(i + 1, e);
  }
  int e = i;
  while (e < (int)j.length() && (isdigit(j[e]) || j[e] == '-')) e++;
  return j.substring(i, e);
}

static bool tgErlaubt(const String &chat) { return ("," + ben.tgChats + ",").indexOf("," + chat + ",") >= 0; }

static void benTask(void *) {
  long tgOffset = 0;
  uint32_t tgZuletzt = 0;
  for (;;) {
    bool netz = WiFi.status() == WL_CONNECTED || ETH.linkUp();
    Meldung m;
    while (netz && xQueueReceive(meldungen, &m, 0)) {
      WiFiClientSecure tls;
      tls.setInsecure();
      if (ben.ntfyUrl.length() && !m.chat[0]) {
        // http:// ohne TLS: braucht ~40 KB weniger RAM (klassischer ESP32); ntfy.sh nimmt beides an
        WiFiClient klar;
        HTTPClient h;
        bool ok = ben.ntfyUrl.startsWith("http://") ? h.begin(klar, ben.ntfyUrl) : h.begin(tls, ben.ntfyUrl);
        if (ok) {
          h.addHeader("Title", m.titel);
          h.addHeader("Tags", "coffee");
          int c = h.POST((uint8_t *)m.text, strlen(m.text));
          snprintf(benStatus, sizeof benStatus, "ntfy: %s", c == 200 ? "gesendet" : "Fehler");
          h.end();
        }
      }
      if (ben.tgToken.length()) {
        String chats = m.chat[0] ? String(m.chat) : ben.tgChats;
        int a = 0;
        while (a < (int)chats.length()) {
          int e = chats.indexOf(',', a);
          if (e < 0) e = chats.length();
          String chat = chats.substring(a, e);
          chat.trim();
          a = e + 1;
          if (!chat.length()) continue;
          HTTPClient h;
          if (h.begin(tls, "https://api.telegram.org/bot" + ben.tgToken + "/sendMessage")) {
            h.addHeader("Content-Type", "application/json");
            String body = "{\"chat_id\":" + chat + ",\"text\":";
            jsonText(body, m.titel[0] ? String(m.titel) + "\n" + m.text : String(m.text));
            body += "}";
            h.POST(body);
            h.end();
          }
        }
      }
    }
    // Telegram abfragen (alle 3 s, ohne lange Verbindung)
    if (netz && ben.tgToken.length() && millis() - tgZuletzt > 3000) {
      tgZuletzt = millis();
      WiFiClientSecure tls;
      tls.setInsecure();
      HTTPClient h;
      if (h.begin(tls, "https://api.telegram.org/bot" + ben.tgToken + "/getUpdates?timeout=0&offset=" + String(tgOffset))) {
        int c = h.GET();
        if (c == 200) {
          String j = h.getString();
          strlcpy(benStatus, "Telegram: verbunden", sizeof benStatus);
          int pos = 0;
          while ((pos = j.indexOf("\"update_id\":", pos)) >= 0) {
            long id = jsonFeld(j, "update_id", pos).toInt();
            int chatPos = j.indexOf("\"chat\":", pos);
            String chat = chatPos >= 0 ? jsonFeld(j, "id", chatPos) : "";
            int tPos = j.indexOf("\"text\":", pos);
            int naechste = j.indexOf("\"update_id\":", pos + 12);
            String text = tPos >= 0 && (naechste < 0 || tPos < naechste) ? jsonFeld(j, "text", tPos - 1) : "";
            text.toLowerCase();
            tgOffset = id + 1;
            pos += 12;
            if (!chat.length()) continue;
            if (!tgErlaubt(chat)) {
              meldenAn(chat.c_str(), ("Diese Bridge kennt diesen Chat noch nicht. In der Weboberfläche unter Einstellungen → Benachrichtigungen die Nummer " + chat + " eintragen.").c_str());
              continue;
            }
            char b[64];
            if (text.startsWith("/an") || text == "an") strlcpy(b, "an", sizeof b);
            else if (text.startsWith("/aus") || text == "aus") strlcpy(b, "aus", sizeof b);
            else if (text.startsWith("/status") || text == "status") snprintf(b, sizeof b, "status %s", chat.c_str());
            else {
              meldenAn(chat.c_str(), "Befehle: /an (einschalten), /aus (Standby), /status");
              continue;
            }
            xQueueSend(tgBefehle, b, 0);
          }
        } else if (c == 401) strlcpy(benStatus, "Telegram: Token ungültig", sizeof benStatus);
        h.end();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

static String kurzStatus() {
  char b[160];
  float k = tempKaffee(), d = tempService();
  snprintf(b, sizeof b, "%s%s · Kaffee %s °C · Dampf %s °C · %lu Bezüge", maschinenStatus().c_str(),
           maschineBereit() ? " (bereit)" : "", isnan(k) ? "–" : String((int)k).c_str(), isnan(d) ? "–" : String((int)d).c_str(),
           (unsigned long)bezuegeMaschine());
  return b;
}

// in loop(): Telegram-Befehle ausfuehren, Ereignisse erkennen und melden
static void benLoop() {
  char b[64];
  while (tgBefehle && xQueueReceive(tgBefehle, b, 0)) {
    if (!strcmp(b, "an") || !strcmp(b, "aus")) {
      bool an = !strcmp(b, "an");
      ereignis("- Telegram: %s", b);
      if (an != maschineAn()) maschineSchalten(an);
      melden("", an ? "Wird eingeschaltet. Ich melde mich, wenn sie bereit ist." : "Geht in Standby.");
    } else if (!strncmp(b, "status ", 7)) {
      meldenAn(b + 7, kurzStatus().c_str());
    }
  }
  static uint32_t zuletzt = 0;
  if (millis() - zuletzt < 2000) return;
  zuletzt = millis();
  static bool warBereit = false, warAlarm = false, warFaellig = false;
  static uint32_t letzterShot = 0;
  bool bereit = maschineBereit();
  if (ben.bereit && bereit && !warBereit) melden("Espresso bereit", "Kaffeekessel %d °C", (int)tempKaffee());
  warBereit = bereit;
  const char *alarm = aktuellerAlarm();
  if (ben.alarm && alarm && !warAlarm) melden("Bezzera Duo: Alarm", "%s", alarm);
  warAlarm = alarm != nullptr;
  if (shotNr != letzterShot) {
    if (letzterShot && ben.shot && shotLogN) {
      const ShotEintrag &e = shotLog[0];
      melden("Shot fertig", "%.0f s%s%s, max. %.1f bar", e.dauer, isnan(e.gewicht) ? "" : ", ",
             isnan(e.gewicht) ? "" : (String(e.gewicht, 1) + " g").c_str(), e.druckMax);
    }
    letzterShot = shotNr;
  }
  bool faellig = zaehler.rueckspuelenAlle && zaehler.seitRueckspuelen >= zaehler.rueckspuelenAlle;
  if (ben.wartung && faellig && !warFaellig) melden("Rückspülen fällig", "%lu Bezüge seit dem letzten Rückspülen", (unsigned long)zaehler.seitRueckspuelen);
  warFaellig = faellig;
}

static void benSetup() {
  benLaden();
  meldungen = xQueueCreate(8, sizeof(Meldung));
  tgBefehle = xQueueCreate(4, 64);
}

static bool benTaskLaeuft = false;
static void benStarten() {  // erst, wenn ntfy oder Telegram eingerichtet ist (RAM, siehe zeitplan.h)
  if (benTaskLaeuft || (!ben.ntfyUrl.length() && !ben.tgToken.length())) return;
  benTaskLaeuft = true;
  xTaskCreatePinnedToCore(benTask, "melden", 12288, nullptr, 1, nullptr, 0);
}

static String benJson() {
  String j = "{\"ntfy\":";
  jsonText(j, ben.ntfyUrl);
  j += ",\"tg_token_gesetzt\":";
  j += ben.tgToken.length() ? "true" : "false";
  j += ",\"tg_chats\":";
  jsonText(j, ben.tgChats);
  j += ",\"bereit\":";
  j += ben.bereit ? "true" : "false";
  j += ",\"shot\":";
  j += ben.shot ? "true" : "false";
  j += ",\"wartung\":";
  j += ben.wartung ? "true" : "false";
  j += ",\"alarm\":";
  j += ben.alarm ? "true" : "false";
  j += ",\"status\":";
  jsonText(j, benStatus);
  j += '}';
  return j;
}

static void benFeld(const String &k, const String &v) {
  if (k == "ntfy") ben.ntfyUrl = v;
  else if (k == "tg_token") { if (v.length()) ben.tgToken = v == "-" ? "" : v; }
  else if (k == "tg_chats") ben.tgChats = v;
  else if (k == "bereit") ben.bereit = v == "1";
  else if (k == "shot") ben.shot = v == "1";
  else if (k == "wartung") ben.wartung = v == "1";
  else if (k == "alarm") ben.alarm = v == "1";
}
