#pragma once
// Easter Egg: zehnmal schnell auf den Titel einer eigenen Seite (z. B. "Bruehkurve") tippen -> Doom auf dem Display.
//
// Erkennen: Die Bridge liest auf der Startseite das Touch-Register 0x05
// (TP_Flag, TP_Status, X, Y) und zaehlt Tipper rechts neben der Menuetaste,
// also auf dem Schriftzug. Am Display selbst aendert das nichts.
//
// Anzeigen: Eine eigene Seite (Standard 299, aus tools/seitenbau, per
// SD-Karte installiert) traegt zwei Zeichenflaechen (Basic Graphics 0x21):
//   VP 0x0800  Befehl 0x000F Bitmap, 96x62 Pixel RGB565 bei (112,89)
//   VP 0x1FC0  Befehl 0x0010 doppelt gross auf (64,58) (optional)
// Nutzbar sind nur VPs bis 0x1FFF: ab 0x2000 liegt der Empfangspuffer des
// Displays, ueber 0x3FFF wird gespiegelt. 96x62 Pixel ab 0x0806 enden bei 0x1F46.
// Doom (doomgeneric, GPL-2.0, src/doom) rechnet in 320x200 mit Palette; jedes
// zweite Pixel geht als RGB565 ins Bitmap, und zwar nur die Worte, die sich
// seit dem letzten Bild geaendert haben. Bei 115200 Baud sind das wenige
// Bilder pro Sekunde, mit "Turbo" (Display kurzzeitig auf 921600 Baud) mehr.
//
// Waehrenddessen beantwortet die Bridge das Mainboard selbst (wie Modus 1)
// und reicht nichts ans Display weiter; danach zeigt sie wieder die Seite,
// die das Mainboard zuletzt wollte. Ein Alarm der Maschine beendet Doom sofort.
//
// Steuerung per Touch (320x240):
//   oben (y<40):  links Menue/ESC (3 s halten = zurueck zur Maschine), Mitte Enter, rechts Benutzen
//   sonst:        links drehen, rechts drehen, Mitte oben vor, Mitte Feuer, unten zurueck
// oder in der Weboberflaeche mit der Tastatur.
// Ohne WAD-Datei (Weboberflaeche -> Diagnose -> Doom) laeuft das Doom-Feuer.

#include <LittleFS.h>
#include "src/doom/doomgeneric.h"
#include "src/doom/doomkeys.h"

extern "C" {
struct DoomFarbe {
  uint32_t b : 8, g : 8, r : 8, a : 8;
};
extern DoomFarbe colors[256];  // i_video.c (CMAP256)
}

static const uint16_t DOOM_VP_BILD = 0x0800, DOOM_VP_ZOOM = 0x1FC0;
static const int DOOM_B = 96, DOOM_H = 62, DOOM_X = 112, DOOM_Y = 89;
static const char *DOOM_WAD = "/littlefs/doom.wad";

struct DoomEinst {
  uint16_t seite = 299;
  bool zoom = true, turbo = false;
};
static DoomEinst doomEinst;
static volatile bool doomAktiv = false;
static bool doomEngineLaeuft = false, doomBeendet = false, doomFsOk = false;
static TaskHandle_t doomTask = nullptr;
static QueueHandle_t doomTasten = nullptr;  // uint16_t: (gedrueckt << 8) | taste
static uint16_t *doomBild = nullptr, *doomGesendet = nullptr;
static bool doomVoll = true;
static int32_t doomRueckSeite = -1;
static uint8_t doomModusVorher = 0;
static char doomStatus[64] = "bereit";
static uint32_t doomBilder = 0, doomLetzterTouch = 0;

// Touch-Zustand aus den Antworten auf Register 0x05
static volatile uint8_t tpStatus = 0;
static volatile uint16_t tpX = 0, tpY = 0;
static volatile uint32_t tpZeit = 0;
static volatile bool tpNeu = false;

static void doomLaden() {
  Preferences p;
  p.begin("doom", true);
  doomEinst.seite = p.getUShort("seite", 299);
  doomEinst.zoom = p.getBool("zoom", true);
  doomEinst.turbo = p.getBool("turbo", false);
  p.end();
}
static void doomSpeichern() {
  Preferences p;
  p.begin("doom", false);
  p.putUShort("seite", doomEinst.seite);
  p.putBool("zoom", doomEinst.zoom);
  p.putBool("turbo", doomEinst.turbo);
  p.end();
}

static bool doomFs() {
  if (!doomFsOk) doomFsOk = LittleFS.begin(true, "/littlefs", 5, "spiffs");
  return doomFsOk;
}
static bool doomWadDa() { return doomFs() && LittleFS.exists("/doom.wad"); }

// ── Rahmen direkt ans Display (nur waehrend Doom, dann schreibt sonst niemand) ──
static void doomSchreiben(const uint8_t *nutz, size_t n) {
  uint8_t r[260];
  r[0] = KOPF0;
  r[1] = KOPF1;
  r[2] = n;
  memcpy(r + 3, nutz, n);
  uartB.write(r, n + 3);
}
static void doomVp(uint16_t vp, const uint16_t *w, size_t anz) {
  uint8_t b[3 + 2 * 126];
  b[0] = 0x82;
  b[1] = vp >> 8;
  b[2] = vp;
  for (size_t i = 0; i < anz; i++) {
    b[3 + 2 * i] = w[i] >> 8;
    b[4 + 2 * i] = w[i];
  }
  doomSchreiben(b, 3 + 2 * anz);
}
static void doomRegister(uint8_t reg, uint8_t wert) {
  uint8_t b[] = {0x80, reg, wert};
  doomSchreiben(b, sizeof b);
}
static void doomSeiteZeigen(uint16_t s) {
  uint8_t b[] = {0x80, 0x03, (uint8_t)(s >> 8), (uint8_t)s};
  doomSchreiben(b, sizeof b);
}
static void doomTouchAbfragen() {
  static const uint8_t f[] = {0x81, 0x05, 0x06};
  doomSchreiben(f, sizeof f);
}

// ── Antworten vom Display (aus rahmenVomDisplay, loop()) ──
static void doomAntwort(const uint8_t *r, size_t n) {
  if (n < 13 || r[3] != 0x81 || r[4] != 0x05) return;
  if (r[6] != 0x5A) return;  // keine neuen Koordinaten
  tpStatus = r[7];
  tpX = (r[8] << 8) | r[9];
  tpY = (r[10] << 8) | r[11];
  tpZeit = millis();
  tpNeu = true;
  if (doomAktiv) return;  // die Doom-Task loescht selbst, sonst mischen sich zwei Schreiber
  uint8_t loeschen[] = {KOPF0, KOPF1, 0x03, 0x80, 0x05, 0x00};  // Flag zuruecksetzen, keine Antwort
  uartB.write(loeschen, sizeof loeschen);
}

// ── Tasten ──
static void doomTaste(uint8_t taste, bool gedrueckt) {
  if (!doomTasten || !taste) return;
  uint16_t t = (gedrueckt ? 0x100 : 0) | taste;
  xQueueSend(doomTasten, &t, 0);
}

static uint8_t doomZone(uint16_t x, uint16_t y) {
  if (y < 40) return x < 107 ? KEY_ESCAPE : x > 213 ? KEY_USE : KEY_ENTER;
  if (x < 107) return KEY_LEFTARROW;
  if (x > 213) return KEY_RIGHTARROW;
  if (y < 140) return KEY_UPARROW;
  if (y < 200) return KEY_FIRE;
  return KEY_DOWNARROW;
}

static void doomPausieren(const char *grund);
static String queryWert(const String &q, const char *k);  // web.h

static void doomTouchAuswerten() {
  static uint8_t gehalten = 0;
  static uint32_t seit = 0;
  bool neu = tpNeu;
  tpNeu = false;
  if (neu) doomRegister(0x05, 0x00);
  bool unten = neu && (tpStatus == 0x01 || tpStatus == 0x03);
  if (!neu && gehalten && millis() - tpZeit > 400) unten = false;  // Loslassen verpasst
  else if (!neu) return;
  uint8_t zone = unten ? doomZone(tpX, tpY) : 0;
  if (zone != gehalten) {
    if (gehalten) doomTaste(gehalten, false);
    if (zone) doomTaste(zone, true);
    gehalten = zone;
    seit = millis();
  }
  if (unten) doomLetzterTouch = millis();
  if (gehalten == KEY_ESCAPE && millis() - seit > 3000) {
    doomTaste(KEY_ESCAPE, false);
    gehalten = 0;
    doomPausieren("ESC gehalten");
  }
}

// ── Bild senden: nur geaenderte Worte, zwischendurch Touch abfragen ──
static void doomBildSenden() {
  const int N = DOOM_B * DOOM_H;
  uint32_t abfrage = 0;
  int i = 0;
  while (i < N && doomAktiv) {
    if (millis() - abfrage > 50) {
      abfrage = millis();
      doomTouchAbfragen();
      doomTouchAuswerten();
    }
    if (!doomVoll && doomBild[i] == doomGesendet[i]) {
      i++;
      continue;
    }
    int j = i, gleich = 0;
    while (j < N && j - i < 126) {
      if (!doomVoll && doomBild[j] == doomGesendet[j]) {
        if (++gleich > 3) break;
      } else
        gleich = 0;
      j++;
    }
    j -= gleich;
    doomVp(DOOM_VP_BILD + 6 + i, doomBild + i, j - i);
    memcpy(doomGesendet + i, doomBild + i, 2 * (j - i));
    i = j;
  }
  doomVoll = false;
  doomBilder++;
  doomTouchAuswerten();
}

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

// ── doomgeneric-Schnittstelle (laeuft in der Doom-Task) ──
extern "C" {
void *DG_Psram(unsigned int n) {  // src/doom: grosse Tabellen (Konstruktoren beim Start)
  void *p = heap_caps_calloc(1, n, MALLOC_CAP_SPIRAM);
  return p ? p : calloc(1, n);
}
void DG_Init() {}
void DG_SetWindowTitle(const char *) {}
uint32_t DG_GetTicksMs() { return millis(); }
void DG_SleepMs(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms ? ms : 1)); }
int DG_GetKey(int *gedrueckt, unsigned char *taste) {
  uint16_t t;
  if (!doomTasten || !xQueueReceive(doomTasten, &t, 0)) return 0;
  *gedrueckt = t >> 8;
  *taste = t & 0xFF;
  return 1;
}
void DG_DrawFrame() {
  if (!doomAktiv) return;
  const uint8_t *q = (const uint8_t *)DG_ScreenBuffer;
  for (int y = 0; y < DOOM_H; y++)
    for (int x = 0; x < DOOM_B; x++) {
      const DoomFarbe &c = colors[q[(y * DOOMGENERIC_RESY / DOOM_H) * DOOMGENERIC_RESX + x * DOOMGENERIC_RESX / DOOM_B]];
      doomBild[y * DOOM_B + x] = rgb565(c.r, c.g, c.b);
    }
  doomBildSenden();
  vTaskDelay(1);  // Leerlauf-Task auf Kern 0 nicht aushungern
}
void DG_Beenden(int code) {  // Doom-Menue "Quit" oder Fehler: Task ruht, Neustart der Bridge noetig
  snprintf(doomStatus, sizeof doomStatus, code ? "Doom-Fehler %d (Bridge neu starten)" : "beendet (Bridge neu starten)", code);
  doomBeendet = true;
  doomPausieren(nullptr);
  for (;;) vTaskSuspend(NULL);  // Stack liegt im PSRAM: nicht loeschen, nur schlafen
}
}

// ── Doom-Feuer (ohne WAD) ──
static void doomFeuer() {
  static uint8_t *f = nullptr;
  static uint16_t pal[37];
  if (!f) {
    f = (uint8_t *)ps_malloc(DOOM_B * DOOM_H);
    static const uint8_t p[37][3] = {{7, 7, 7}, {31, 7, 7}, {47, 15, 7}, {71, 15, 7}, {87, 23, 7}, {103, 31, 7}, {119, 31, 7}, {143, 39, 7}, {159, 47, 7}, {175, 63, 7}, {191, 71, 7}, {199, 71, 7}, {223, 79, 7}, {223, 87, 7}, {223, 87, 7}, {215, 95, 7}, {215, 95, 7}, {215, 103, 15}, {207, 111, 15}, {207, 119, 15}, {207, 127, 15}, {207, 135, 23}, {199, 135, 23}, {199, 143, 23}, {199, 151, 31}, {191, 159, 31}, {191, 159, 31}, {191, 167, 39}, {191, 167, 39}, {191, 175, 47}, {183, 175, 47}, {183, 183, 47}, {183, 183, 55}, {207, 207, 111}, {223, 223, 159}, {239, 239, 199}, {255, 255, 255}};
    for (int i = 0; i < 37; i++) pal[i] = rgb565(p[i][0], p[i][1], p[i][2]);
    memset(f, 0, DOOM_B * DOOM_H);
    memset(f + (DOOM_H - 1) * DOOM_B, 36, DOOM_B);
  }
  for (int x = 0; x < DOOM_B; x++)
    for (int y = 1; y < DOOM_H; y++) {
      int q = y * DOOM_B + x, r = esp_random() & 3;
      int ziel = q - DOOM_B - r + 1;
      if (ziel >= 0) f[ziel] = f[q] ? f[q] - (r & 1) : 0;
    }
  for (int i = 0; i < DOOM_B * DOOM_H; i++) doomBild[i] = pal[f[i]];
  doomBildSenden();
  if (tpNeu || millis() - doomLetzterTouch > 60000) {
    doomTouchAuswerten();
    if (millis() - doomLetzterTouch > 60000) doomPausieren("Zeit");
  }
  vTaskDelay(pdMS_TO_TICKS(20));
}

static void doomTaskFn(void *) {
  freopen("/dev/null", "w", stdout);  // Doom erzaehlt viel; USB bleibt der Bridge
  for (;;) {
    if (!doomAktiv) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      continue;
    }
    if (!doomEngineLaeuft && doomWadDa()) {
      static char a0[] = "doom", a1[] = "-iwad", a2[] = "/littlefs/doom.wad";
      static char *argv[] = {a0, a1, a2, nullptr};
      doomEngineLaeuft = true;
      strlcpy(doomStatus, "laeuft", sizeof doomStatus);
      doomgeneric_Create(3, argv);
    }
    if (doomEngineLaeuft) doomgeneric_Tick();
    else doomFeuer();
    if (millis() - doomLetzterTouch > 180000) doomPausieren("3 min ohne Touch");
  }
}

// ── Starten / Pausieren ──
static void doomDisplayVorbereiten() {
  if (doomEinst.turbo) {  // R1 = 0x10 (921600), 0xA5 = uebernehmen ohne zu speichern
    doomRegister(0x11, 0x10);
    doomRegister(0x1D, 0xA5);
    uartB.flush();
    delay(600);
    uartB.updateBaudRate(921600);
    delay(200);
  }
  doomSeiteZeigen(doomEinst.seite);
  uint16_t kopf[] = {0x000F, 1, DOOM_X, DOOM_Y, DOOM_B, DOOM_H};
  doomVp(DOOM_VP_BILD, kopf, 6);
  if (doomEinst.zoom) {  // Bitmap (112,89)-(207,150) doppelt gross nach (64,58)
    uint16_t z[] = {0x0010, 1, 64, 58, DOOM_X, DOOM_Y, DOOM_X + DOOM_B - 1, DOOM_Y + DOOM_H - 1};
    doomVp(DOOM_VP_ZOOM, z, 8);
  } else {
    uint16_t z[] = {0x0010, 0};
    doomVp(DOOM_VP_ZOOM, z, 2);
  }
  doomVoll = true;
}

static bool doomStarten(const char *ausloeser) {
  if (doomAktiv || doomBeendet) return false;
  if (!doomBild) {
    doomBild = (uint16_t *)ps_malloc(2 * DOOM_B * DOOM_H);
    doomGesendet = (uint16_t *)ps_malloc(2 * DOOM_B * DOOM_H);
    doomTasten = xQueueCreate(16, sizeof(uint16_t));
    if (!doomBild || !doomGesendet) {
      strlcpy(doomStatus, "kein PSRAM", sizeof doomStatus);
      return false;
    }
  }
  ereignis("- Doom: %s", ausloeser);
  doomModusVorher = emulation;
  doomRueckSeite = leitung.seite;
  emulation = 1;  // Mainboard bekommt Antworten von der Bridge (nicht gespeichert)
  doomLetzterTouch = millis();
  delay(30);  // laufende Rahmen ausklingen lassen
  doomDisplayVorbereiten();
  doomAktiv = true;
  if (!doomTask) xTaskCreatePinnedToCoreWithCaps(doomTaskFn, "doom", 32768, nullptr, 1, &doomTask, 0, MALLOC_CAP_SPIRAM);  // Stack im PSRAM
  else xTaskNotifyGive(doomTask);
  if (!doomEngineLaeuft) strlcpy(doomStatus, doomWadDa() ? "startet" : "Feuer (keine WAD-Datei)", sizeof doomStatus);
  return true;
}

static void doomPausieren(const char *grund) {
  if (!doomAktiv) return;
  doomAktiv = false;
  delay(20);
  if (doomEinst.turbo) {
    doomRegister(0x11, 0x07);
    doomRegister(0x1D, 0xA5);
    uartB.flush();
    delay(600);
    uartB.updateBaudRate(115200);
    delay(200);
  }
  if (doomRueckSeite >= 0) doomSeiteZeigen(doomRueckSeite);
  emulation = doomModusVorher;
  if (grund) {
    ereignis("- Doom pausiert: %s", grund);
    if (!doomBeendet) strlcpy(doomStatus, doomEngineLaeuft ? "pausiert" : "bereit", sizeof doomStatus);
  }
}

// aus rahmenVomMainboard: waehrend Doom nichts ans Display, nur die Wunschseite merken
static bool doomVomMainboard(const uint8_t *r, size_t n) {
  if (!doomAktiv) return false;
  if (n >= 7 && r[3] == 0x80 && r[4] == 0x03) doomRueckSeite = (r[5] << 8) | r[6];
  return true;
}

// ── Easter Egg erkennen (loop) ──
static void doomLoop() {
  if (doomAktiv) {
    if (aktuellerAlarm() || (doomRueckSeite >= 0 && alarmText(doomRueckSeite % 100))) doomPausieren("Alarm der Maschine");
    return;
  }
  static uint32_t zuletzt = 0, letzterTipp = 0;
  static uint8_t tipps = 0;
  static bool warUnten = false;
  // Nur auf eigenen Seiten (96-99 je Sprache): auf der Startseite liegt das
  // Logo beim Display 2.2 im Touch-Bereich der Menuetaste.
  int s = leitung.seite % 100;
  if (emulation == 1 || doomBeendet || s < 96 || leitung.seite == doomEinst.seite) return;
  if (tpNeu) {
    tpNeu = false;
    bool unten = tpStatus == 0x01 || tpStatus == 0x03;
    if (unten && !warUnten) {
      bool logo = tpX <= 140 && tpY <= 30;  // Titel oben links
      if (!logo || millis() - letzterTipp > 1500) tipps = 0;
      if (logo) {
        letzterTipp = millis();
        if (++tipps >= 10) {
          tipps = 0;
          doomStarten("10x Titel");
        }
      }
    }
    warUnten = unten;
  }
  if (millis() - zuletzt < 80) return;
  zuletzt = millis();
  uint8_t f[] = {KOPF0, KOPF1, 0x03, 0x81, 0x05, 0x06};
  eigeneAnfrageMerken(f, sizeof f);  // Antwort geht nicht ans Mainboard
  uartB.write(f, sizeof f);
}

// ── Weboberflaeche ──
static File doomUpload;
static void doomWadHochladen() {  // /api/doom_wad, Datei-Teile
  HTTPUpload &u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    if (!doomFs()) return;
    doomUpload = LittleFS.open("/doom.wad.neu", "w");
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (doomUpload) doomUpload.write(u.buf, u.currentSize);
  } else if (u.status == UPLOAD_FILE_END && doomUpload) {
    doomUpload.close();
    File f = LittleFS.open("/doom.wad.neu", "r");
    char kopf[4] = {};
    f.read((uint8_t *)kopf, 4);
    f.close();
    if (!memcmp(kopf, "IWAD", 4)) {
      LittleFS.remove("/doom.wad");
      LittleFS.rename("/doom.wad.neu", "/doom.wad");
    } else
      LittleFS.remove("/doom.wad.neu");
  }
}

static String doomJson() {
  String j = "{\"aktiv\":";
  j += doomAktiv ? "true" : "false";
  j += ",\"status\":";
  jsonText(j, doomStatus);
  j += ",\"wad\":";
  bool wad = doomWadDa();
  j += wad ? "true" : "false";
  if (wad) {
    File f = LittleFS.open("/doom.wad", "r");
    j += ",\"wad_bytes\":" + String(f ? f.size() : 0);
    f.close();
  }
  j += ",\"frei\":" + String(doomFsOk ? (uint32_t)(LittleFS.totalBytes() - LittleFS.usedBytes()) : 0);
  j += ",\"bilder\":" + String(doomBilder);
  j += ",\"seite\":" + String(doomEinst.seite);
  j += ",\"zoom\":";
  j += doomEinst.zoom ? "true" : "false";
  j += ",\"turbo\":";
  j += doomEinst.turbo ? "true" : "false";
  j += '}';
  return j;
}

// POST start | stop | taste <code> <0|1> | seite=<n>&zoom=0|1&turbo=0|1 | wad_loeschen
static bool doomAktion(const String &a) {
  if (a == "start") return doomStarten("Web") || doomAktiv;
  if (a == "stop") {
    doomPausieren("Web");
    return true;
  }
  if (a == "wad_loeschen") {
    if (doomFs()) LittleFS.remove("/doom.wad");
    return true;
  }
  if (a.startsWith("taste ")) {
    int t = 0, g = 0;
    sscanf(a.c_str() + 6, "%d %d", &t, &g);
    doomLetzterTouch = millis();
    doomTaste(t, g);
    return true;
  }
  if (a.indexOf('=') > 0) {
    String s = queryWert(a, "seite"), z = queryWert(a, "zoom"), t = queryWert(a, "turbo");
    if (s.length() && s.toInt() >= 0 && s.toInt() < 300) doomEinst.seite = s.toInt();
    if (z.length()) doomEinst.zoom = z == "1";
    if (t.length()) doomEinst.turbo = t == "1";
    doomSpeichern();
    return true;
  }
  return false;
}
