#pragma once
// Matter (Apple Home, Google Home, Alexa): die Bridge erscheint als Steckdose
// "Espresso" (Ein/Standby) und als Temperatursensor (Kaffeekessel). Von
// unterwegs bedient man sie ueber den Home-Hub (HomePod, Apple TV, Nest, Echo),
// auch per Sprache ("Hey Siri, Espresso an").
//
// Einrichtung im Heimnetz ohne Bluetooth (Bluetooth bleibt fuer die Waage):
// Bridge im Heim-WLAN, Matter unter Einstellungen einschalten, Neustart, dann
// in der Home-App "Geraet hinzufuegen" und den Code aus der Weboberflaeche
// eingeben bzw. den QR-Code scannen. Matter startet erst, wenn das Heim-WLAN
// verbunden ist; ein Ausschalten wirkt nach dem Neustart.

#if !CONFIG_IDF_TARGET_ESP32S3
// Klassischer ESP32: zu wenig internes RAM fuer Matter neben Web, BLE und
// Bridge. Die Weboberflaeche zeigt dann "nicht verfuegbar".
static void matterLaden() {}
static void matterLoop() {}
static String matterJson() { return "{\"an\":false,\"gestartet\":false,\"eingerichtet\":false,\"code\":\"\",\"qr\":\"\",\"verfuegbar\":false}"; }
static bool matterAktion(const String &) { return false; }
#else
#include <Matter.h>

// Bluetooth beim Start nicht an Matter geben: die Waage braucht es
extern "C" bool bleInUse(void) { return false; }

static MatterOnOffPlugin matterSchalter;
static MatterTemperatureSensor matterTemp;
static bool matterAn = false, matterGestartet = false;
static volatile int8_t matterWunsch = -1;  // aus dem Matter-Task: 1 ein, 0 aus

static void matterLaden() {
  Preferences p;
  p.begin("matter", true);
  matterAn = p.getBool("an", false);
  p.end();
}
static void matterSpeichern(bool an) {
  Preferences p;
  p.begin("matter", false);
  p.putBool("an", an);
  p.end();
  matterAn = an;
}

static bool matterSchalterCb(bool an) {
  matterWunsch = an;  // ausfuehren in loop(), nicht im Matter-Task
  return true;
}

static void matterLoop() {
  if (!matterAn) return;
  if (!matterGestartet) {
    if (WiFi.status() != WL_CONNECTED) return;
    Matter.setBLECommissioningEnabled(false);  // nur im Netz einrichten
    Matter.setVendorName("larszu");
    Matter.setProductName("Bezzera Duo Bridge");
    matterSchalter.begin(maschineAn());
    matterSchalter.onChange(matterSchalterCb);
    matterTemp.begin(isnan(tempKaffee()) ? 20.0 : tempKaffee());
    Matter.begin();
    matterGestartet = true;
    ereignis("- Matter gestartet, Code %s", Matter.getManualPairingCode().c_str());
    return;
  }
  if (matterWunsch >= 0) {
    bool an = matterWunsch;
    matterWunsch = -1;
    if (an != maschineAn()) {
      ereignis("- Matter: %s", an ? "ein" : "aus");
      maschineSchalten(an);
    }
  }
  static uint32_t zuletzt = 0;
  if (millis() - zuletzt < 5000) return;
  zuletzt = millis();
  if (matterSchalter.getOnOff() != maschineAn()) matterSchalter.setOnOff(maschineAn());
  float t = tempKaffee();
  if (!isnan(t)) matterTemp.setTemperature(t);
}

static String matterJson() {
  String j = "{\"an\":";
  j += matterAn ? "true" : "false";
  j += ",\"gestartet\":";
  j += matterGestartet ? "true" : "false";
  j += ",\"eingerichtet\":";
  j += matterGestartet && Matter.isDeviceCommissioned() ? "true" : "false";
  j += ",\"code\":";
  jsonText(j, matterGestartet ? Matter.getManualPairingCode() : String(""));
  j += ",\"qr\":";
  jsonText(j, matterGestartet ? Matter.getOnboardingQRCodeUrl() : String(""));
  j += '}';
  return j;
}

// "an" | "aus" (wirkt nach Neustart) | "zuruecksetzen" (aus allen Home-Apps entfernen)
static bool matterAktion(const String &a) {
  if (a == "an" || a == "aus") {
    matterSpeichern(a == "an");
    return true;
  }
  if (a == "zuruecksetzen" && matterGestartet) {
    Matter.decommission();
    return true;
  }
  return false;
}
#endif
