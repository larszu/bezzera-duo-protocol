#pragma once
// Home Assistant ueber MQTT (esp-mqtt aus dem ESP32-Core, keine Bibliothek
// noetig). Die Bridge meldet sich per MQTT-Discovery an; in Home Assistant
// erscheint das Geraet "Bezzera Duo" mit Temperaturen, Druck (sobald das
// Druckwort bekannt ist), Zustand, Alarm, Ein/Aus-Schalter, Waage, Bezuegen,
// Zielgewicht und Tara.
//
// Themen (<id> = letzte 6 Stellen der MAC):
//   duo/<id>/zustand      JSON mit allen Werten, alle 5 s und bei Aenderung
//   duo/<id>/verfuegbar   online/offline (Last Will)
//   duo/<id>/ereignis     bezug_start, ziel_erreicht, bezug_fertig
//   duo/<id>/set/maschine ON/OFF    duo/<id>/set/ziel <g>
//   duo/<id>/set/tara     beliebig  duo/<id>/set/aktion an|aus|tara|abbruch|stopp

#include <mqtt_client.h>

static esp_mqtt_client_handle_t mqtt = nullptr;
static volatile bool mqttVerbunden = false, mqttNeu = false;
static String mqttId, mqttBasis;

struct MqttBefehl {
  char thema[24];
  char daten[40];
};
static QueueHandle_t mqttBefehle = nullptr;

static void mqttSenden(const String &thema, const String &daten, bool behalten = false) {
  if (mqtt && mqttVerbunden) esp_mqtt_client_enqueue(mqtt, thema.c_str(), daten.c_str(), daten.length(), 0, behalten, true);
}

static void mqttEreignis(const char *art) { mqttSenden(mqttBasis + "/ereignis", art); }

// Laeuft in der MQTT-Task: Befehle in die Warteschlange fuer loop(),
// Waagenwerte direkt (waageMelden ist threadsicher).
static void mqttHandler(void *, esp_event_base_t, int32_t id, void *daten) {
  auto e = (esp_mqtt_event_handle_t)daten;
  if (id == MQTT_EVENT_CONNECTED) {
    mqttVerbunden = true;
    mqttNeu = true;
    esp_mqtt_client_subscribe(e->client, (mqttBasis + "/set/#").c_str(), 0);
    if (cfg.waageArt == 3 && cfg.waageTopic.length()) esp_mqtt_client_subscribe(e->client, cfg.waageTopic.c_str(), 0);
  } else if (id == MQTT_EVENT_DISCONNECTED) {
    mqttVerbunden = false;
  } else if (id == MQTT_EVENT_DATA && e->topic_len && e->data_len < 64) {
    String thema(e->topic, e->topic_len);
    char d[64];
    memcpy(d, e->data, e->data_len);
    d[e->data_len] = 0;
    if (cfg.waageArt == 3 && thema == cfg.waageTopic) {
      waageMelden(waageZahl(d));
    } else if (thema.startsWith(mqttBasis + "/set/")) {
      MqttBefehl b;
      strlcpy(b.thema, thema.c_str() + mqttBasis.length() + 5, sizeof b.thema);
      strlcpy(b.daten, d, sizeof b.daten);
      xQueueSend(mqttBefehle, &b, 0);
    }
  }
}

static void mqttStoppen() {
  if (!mqtt) return;
  esp_mqtt_client_stop(mqtt);
  esp_mqtt_client_destroy(mqtt);
  mqtt = nullptr;
  mqttVerbunden = false;
}

static void mqttStarten() {
  mqttStoppen();
  if (!cfg.mqttUri.length()) return;
  static String uri, lwt;  // esp-mqtt behaelt die Zeiger
  uri = cfg.mqttUri.indexOf("://") < 0 ? "mqtt://" + cfg.mqttUri : cfg.mqttUri;
  lwt = mqttBasis + "/verfuegbar";
  esp_mqtt_client_config_t c = {};
  c.broker.address.uri = uri.c_str();
  if (cfg.mqttUser.length()) c.credentials.username = cfg.mqttUser.c_str();
  if (cfg.mqttPass.length()) c.credentials.authentication.password = cfg.mqttPass.c_str();
  c.credentials.client_id = mqttId.c_str();
  c.session.last_will.topic = lwt.c_str();
  c.session.last_will.msg = "offline";
  c.session.last_will.retain = 1;
  c.session.keepalive = 30;
  c.network.reconnect_timeout_ms = 10000;
  mqtt = esp_mqtt_client_init(&c);
  if (!mqtt) return;
  esp_mqtt_client_register_event(mqtt, MQTT_EVENT_ANY, mqttHandler, nullptr);
  esp_mqtt_client_start(mqtt);
}

static String mqttStatus() {
  if (!cfg.mqttUri.length()) return "aus";
  return mqttVerbunden ? "verbunden" : "nicht verbunden";
}

// Discovery: ein Konfigurationsthema je Entitaet
static void haEntitaet(const char *art, const char *obj, const char *name, const String &extra, bool aktiv = true) {
  String thema = cfg.haPrefix + "/" + art + "/duo_" + mqttId + "/" + obj + "/config";
  if (!aktiv) {  // leere Nachricht loescht die Entitaet
    mqttSenden(thema, "", true);
    return;
  }
  String j = "{\"~\":\"" + mqttBasis + "\",\"name\":\"" + name + "\",\"uniq_id\":\"duo_" + mqttId + "_" + obj +
             "\",\"avty_t\":\"~/verfuegbar\",\"stat_t\":\"~/zustand\"," + extra +
             ",\"dev\":{\"ids\":[\"duo_" + mqttId +
             "\"],\"name\":\"Bezzera Duo\",\"mf\":\"Bezzera\",\"mdl\":\"Duo DE (duo_bridge)\"}}";
  mqttSenden(thema, j, true);
}

static void haDiscovery() {
  const String T = "\"dev_cla\":\"temperature\",\"unit_of_meas\":\"°C\",\"stat_cla\":\"measurement\",\"val_tpl\":\"{{ value_json.";
  haEntitaet("sensor", "kaffee", "Kaffeekessel", T + "kaffee }}\"");
  haEntitaet("sensor", "service", "Servicekessel", T + "service }}\"");
  const String P = "\"dev_cla\":\"pressure\",\"unit_of_meas\":\"bar\",\"stat_cla\":\"measurement\",\"val_tpl\":\"{{ value_json.";
  haEntitaet("sensor", "druck_pumpe", "Pumpendruck", P + "druck_pumpe }}\"", cfg.druckPWort >= 0);
  haEntitaet("sensor", "druck_kessel", "Druck Servicekessel", P + "druck_kessel }}\"", cfg.druckKWort >= 0);
  haEntitaet("sensor", "status", "Zustand", "\"ic\":\"mdi:coffee-maker\",\"val_tpl\":\"{{ value_json.status }}\"");
  haEntitaet("binary_sensor", "alarm", "Alarm",
             "\"dev_cla\":\"problem\",\"val_tpl\":\"{{ value_json.alarm }}\",\"json_attr_t\":\"~/zustand\",\"json_attr_tpl\":\"{{ {'text': value_json.alarm_text} | tojson }}\"");
  haEntitaet("switch", "maschine", "Maschine", "\"ic\":\"mdi:power\",\"cmd_t\":\"~/set/maschine\",\"val_tpl\":\"{{ value_json.an }}\"");
  haEntitaet("sensor", "gewicht", "Gewicht", "\"dev_cla\":\"weight\",\"unit_of_meas\":\"g\",\"val_tpl\":\"{{ value_json.gewicht }}\"", cfg.waageArt);
  haEntitaet("sensor", "durchfluss", "Durchfluss", "\"unit_of_meas\":\"g/s\",\"ic\":\"mdi:water-outline\",\"val_tpl\":\"{{ value_json.durchfluss }}\"", cfg.waageArt);
  haEntitaet("binary_sensor", "bezug", "Bezug läuft", "\"dev_cla\":\"running\",\"val_tpl\":\"{{ value_json.bezug }}\"", cfg.waageArt);
  haEntitaet("sensor", "letzter_g", "Letzter Bezug", "\"dev_cla\":\"weight\",\"unit_of_meas\":\"g\",\"val_tpl\":\"{{ value_json.letzter_g }}\"", cfg.waageArt);
  haEntitaet("sensor", "letzter_s", "Letzter Bezug Dauer", "\"dev_cla\":\"duration\",\"unit_of_meas\":\"s\",\"val_tpl\":\"{{ value_json.letzter_s }}\"", cfg.waageArt);
  haEntitaet("sensor", "bezuege", "Bezüge", "\"stat_cla\":\"total_increasing\",\"ic\":\"mdi:counter\",\"val_tpl\":\"{{ value_json.bezuege }}\"", cfg.waageArt);
  haEntitaet("number", "ziel", "Zielgewicht", "\"cmd_t\":\"~/set/ziel\",\"min\":5,\"max\":100,\"step\":0.5,\"unit_of_meas\":\"g\",\"ic\":\"mdi:scale\",\"val_tpl\":\"{{ value_json.ziel }}\"", cfg.waageArt);
  haEntitaet("button", "tara", "Tara", "\"cmd_t\":\"~/set/tara\",\"ic\":\"mdi:scale-balance\"", cfg.waageArt);
  haEntitaet("button", "stopp", "Bezug stoppen", "\"cmd_t\":\"~/set/aktion\",\"payload_press\":\"stopp\",\"ic\":\"mdi:stop\"", cfg.stoppPin >= 0);
  haEntitaet("binary_sensor", "bereit", "Bereit", "\"ic\":\"mdi:coffee\",\"val_tpl\":\"{{ value_json.bereit }}\"");
  haEntitaet("sensor", "bezuege_maschine", "Bezüge Maschine", "\"stat_cla\":\"total_increasing\",\"ic\":\"mdi:counter\",\"val_tpl\":\"{{ value_json.bezuege_maschine }}\"");
  haEntitaet("sensor", "waage", "Waage", "\"ent_cat\":\"diagnostic\",\"val_tpl\":\"{{ value_json.waage }}\"");
  mqttSenden(mqttBasis + "/verfuegbar", "online", true);
}

static String haZustand() {
  String j = "{\"kaffee\":";
  jsonZahl(j, tempKaffee(), 0);
  j += ",\"service\":";
  jsonZahl(j, tempService(), 0);
  j += ",\"druck_pumpe\":";
  jsonZahl(j, druckPumpe(), 1);
  j += ",\"druck_kessel\":";
  jsonZahl(j, druckKessel(), 2);
  j += ",\"status\":";
  jsonText(j, maschinenStatus());
  const char *a = aktuellerAlarm();
  j += ",\"alarm\":\"";
  j += a ? "ON" : "OFF";
  j += "\",\"alarm_text\":";
  jsonText(j, a ? a : "");
  j += ",\"an\":\"";
  j += maschineAn() ? "ON" : "OFF";
  j += "\",\"gewicht\":";
  jsonZahl(j, millis() - waageMs < 3000 ? (bezugZustand == BZ_LAEUFT ? bezugG : waageRoh - (cfg.waageArt >= 2 ? waageNull : 0)) : NAN, 1);
  j += ",\"durchfluss\":";
  jsonZahl(j, fluss, 1);
  j += ",\"bezug\":\"";
  j += bezugZustand == BZ_LAEUFT ? "ON" : "OFF";
  j += "\",\"letzter_g\":";
  const BezugInfo *l = bezugListeN ? &bezugListe[(bezugListeN - 1) % 10] : nullptr;
  jsonZahl(j, l ? l->g : NAN, 1);
  j += ",\"letzter_s\":";
  jsonZahl(j, l ? l->dauer : NAN, 1);
  j += ",\"bezuege\":";
  j += bezuegeGesamt;
  j += ",\"ziel\":";
  jsonZahl(j, cfg.ziel, 1);
  j += ",\"waage\":";
  jsonText(j, waageStatusText());
  j += ",\"bereit\":\"";
  j += maschineBereit() ? "ON" : "OFF";
  j += "\",\"bezuege_maschine\":";
  if (bezuegeMaschine()) j += bezuegeMaschine();
  else j += "null";
  j += ",\"shot_laeuft\":";
  j += shotLaeuft ? "true" : "false";
  j += ",\"seit_rueckspuelen\":";
  j += zaehler.seitRueckspuelen;
  j += ",\"rueckspuelen_alle\":";
  j += zaehler.rueckspuelenAlle;
  j += '}';
  return j;
}

static void mqttSetup() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char b[8];
  snprintf(b, sizeof b, "%02x%02x%02x", mac[3], mac[4], mac[5]);
  mqttId = b;
  mqttBasis = "duo/" + mqttId;
  mqttBefehle = xQueueCreate(8, sizeof(MqttBefehl));
}

static void mqttLoop() {
  static bool netzDa = false;
  static uint32_t zuletzt = 0;
  static String gesendet;
  // erst starten, wenn Ethernet oder Heim-WLAN eine Adresse hat
  bool netz = ETH.linkUp() && ETH.localIP() != IPAddress(0, 0, 0, 0) || WiFi.status() == WL_CONNECTED;
  if (netz && !netzDa && cfg.mqttUri.length()) mqttStarten();
  netzDa = netz;

  MqttBefehl b;
  while (mqttBefehle && xQueueReceive(mqttBefehle, &b, 0)) {
    String t = b.thema, d = b.daten;
    d.trim();
    if (t == "maschine") aktion(d.equalsIgnoreCase("ON") ? "an" : "aus");
    else if (t == "ziel") aktion("ziel " + d);
    else if (t == "tara") aktion("tara");
    else if (t == "aktion") aktion(d);
    ereignis("- MQTT %s %s", b.thema, b.daten);
    zuletzt = 0;  // Zustand gleich melden
  }
  if (!mqttVerbunden) return;
  if (mqttNeu) {
    mqttNeu = false;
    haDiscovery();
  }
  // alle 5 s, waehrend eines Bezugs alle 0,5 s, sonst bei Aenderung (hoechstens 1/s)
  uint32_t takt = bezugZustand == BZ_LAEUFT ? 500 : 5000;
  if (millis() - zuletzt < min<uint32_t>(takt, 1000)) return;
  String z = haZustand();
  if (z == gesendet && millis() - zuletzt < takt) return;
  zuletzt = millis();
  gesendet = z;
  mqttSenden(mqttBasis + "/zustand", z);
}

// Status fuer die Weboberflaeche (/api/zusatz)
static String zusatzJson() {
  String j;
  j.reserve(1600);
  j += "{\"maschine\":{\"status\":";
  jsonText(j, maschinenStatus());
  j += ",\"an\":";
  j += maschineAn() ? "true" : "false";
  j += "},\"werte\":";
  j += haZustand();
  j += ",\"waage\":{\"status\":";
  jsonText(j, waageStatusText());
  j += ",\"alter_ms\":";
  j += waageNr ? (long)(millis() - waageMs) : -1L;
  j += "},\"bezug\":{\"laeuft\":";
  j += bezugZustand == BZ_LAEUFT ? "true" : "false";
  j += ",\"nr\":";
  j += bezugNr;
  j += ",\"t\":";
  jsonZahl(j, bezugZustand == BZ_LAEUFT ? (millis() - bezugStartMs) / 1000.0f : NAN, 1);
  j += ",\"g\":";
  jsonZahl(j, bezugG, 1);
  j += ",\"stopp_gesendet\":";
  j += stoppGesendet ? "true" : "false";
  j += ",\"stopp_aktiv\":";
  j += stoppAktiv ? "true" : "false";
  j += ",\"liste\":[";
  for (uint32_t i = 0; i < min<uint32_t>(bezugListeN, 10); i++) {
    const BezugInfo &b = bezugListe[(bezugListeN - 1 - i) % 10];
    if (i) j += ',';
    j += "{\"alter_s\":";
    j += (millis() - b.endeMs) / 1000;
    j += ",\"g\":";
    jsonZahl(j, b.g, 1);
    j += ",\"ziel\":";
    jsonZahl(j, b.ziel, 1);
    j += ",\"dauer\":";
    jsonZahl(j, b.dauer, 1);
    j += ",\"gestoppt\":";
    j += b.gestoppt ? "true" : "false";
    j += '}';
  }
  j += "]},\"mqtt\":";
  jsonText(j, mqttStatus());
  j += ",\"mqtt_basis\":";
  jsonText(j, mqttBasis);
  j += ",\"psram\":";
  j += psramFound() ? "true" : "false";
  j += ",\"cfg\":{\"mqtt_uri\":";
  jsonText(j, cfg.mqttUri);
  j += ",\"mqtt_user\":";
  jsonText(j, cfg.mqttUser);
  j += ",\"mqtt_pass_gesetzt\":";
  j += cfg.mqttPass.length() ? "true" : "false";
  j += ",\"ha_prefix\":";
  jsonText(j, cfg.haPrefix);
  j += ",\"waage_art\":";
  j += cfg.waageArt;
  j += ",\"waage_url\":";
  jsonText(j, cfg.waageUrl);
  j += ",\"waage_topic\":";
  jsonText(j, cfg.waageTopic);
  j += ",\"waage_ble\":";
  jsonText(j, cfg.waageBle);
  j += ",\"ziel\":";
  jsonZahl(j, cfg.ziel, 1);
  j += ",\"vorlauf\":";
  jsonZahl(j, cfg.vorlauf, 1);
  j += ",\"lernen\":";
  j += cfg.lernen ? "true" : "false";
  j += ",\"stopp_pin\":";
  j += cfg.stoppPin;
  j += ",\"stopp_high\":";
  j += cfg.stoppHigh ? "true" : "false";
  j += ",\"stopp_puls\":";
  j += cfg.stoppPulsMs;
  j += ",\"druck_p_wort\":";
  j += cfg.druckPWort;
  j += ",\"druck_p_teil\":";
  j += cfg.druckPTeiler;
  j += ",\"druck_k_wort\":";
  j += cfg.druckKWort;
  j += ",\"druck_k_teil\":";
  j += cfg.druckKTeiler;
  j += "}}";
  return j;
}
