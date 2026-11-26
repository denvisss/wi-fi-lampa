#include "web.h"
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <Updater.h>
#include <WebSocketsServer.h>
#include <time.h>
#include "config.h"
#include "effects.h"
#include "engine.h"
#include "mapping.h"
#include "net.h"
#include "ota.h"
#include "store.h"
#include "sunrise.h"

static ESP8266WebServer server(80);
static WebSocketsServer ws(cfg::WS_PORT);

static bool stateDirty = false;
static uint32_t lastStatePush = 0;
static uint32_t lastPreview = 0;
static uint32_t lastSunrisePush = 0;
static uint32_t restartAt = 0;
static bool otaOk = false;
static uint8_t pvFps = 6;
static bool pv[WEBSOCKETS_SERVER_CLIENT_MAX];
static uint8_t pvFrame[cfg::LED_COUNT * 3];

void notifyState() { stateDirty = true; }

// ---------------------------------------------------------------------------
// JSON

static int daysToNy() {
  time_t now = time(nullptr);
  if (now < 1700000000) return -1;
  tm tt, tgt = {};
  localtime_r(&now, &tt);
  tgt.tm_year = tt.tm_year + 1;
  tgt.tm_mon = 0;
  tgt.tm_mday = 1;
  return (int)((mktime(&tgt) - now) / 86400);
}

static void fillAlarms(JsonArray arr) {
  for (uint8_t i = 0; i < cfg::MAX_ALARMS; i++) {
    const AlarmCfg& a = g.alarms[i];
    JsonObject o = arr.add<JsonObject>();
    o["enabled"] = a.enabled;
    o["days"] = a.days;
    o["hour"] = a.hour;
    o["minute"] = a.minute;
    o["durationMin"] = a.durationMin;
    o["maxBrightness"] = a.maxBrightness;
    o["snoozeMin"] = a.snoozeMin;
    o["afterOffMin"] = a.afterOffMin;
  }
}

static String buildState() {
  JsonDocument d;
  d["power"] = g.power;
  d["brightness"] = g.brightness;
  d["effect"] = g.effect;
  d["color"] = g.color;
  d["speed"] = g.speed;
  d["intensity"] = g.intensity;
  d["currentLimit"] = g.currentLimit;
  d["tz"] = g.tz;
  d["manualTime"] = g.manualTime;

  JsonObject c = d["clock"].to<JsonObject>();
  c["colorMode"] = g.clock.colorMode;
  c["speed"] = g.clock.speed;
  c["showDate"] = g.clock.showDate;
  c["hour12"] = g.clock.hour12;

  JsonObject m = d["mapping"].to<JsonObject>();
  m["serpentine"] = g.mapping.serpentine;
  m["reverse"] = g.mapping.reverse;
  m["flipY"] = g.mapping.flipY;
  m["offset"] = g.mapping.offset;

  JsonObject pl = d["playlist"].to<JsonObject>();
  pl["enabled"] = g.playlistEnabled;
  pl["intervalMin"] = g.playlistIntervalMin;

  fillAlarms(d["alarms"].to<JsonArray>());

  JsonObject dev = d["device"].to<JsonObject>();
  dev["fw"] = cfg::FW_VERSION;
  dev["ip"] = net::ip();
  dev["rssi"] = net::rssi();
  dev["uptime"] = (uint32_t)(millis() / 1000);
  dev["heap"] = (uint32_t)ESP.getFreeHeap();
  dev["mac"] = WiFi.macAddress();
  dev["time"] = net::isoTime();
  dev["ap"] = net::apMode();

  JsonObject sr = d["sunrise"].to<JsonObject>();
  sr["active"] = sunrise::active();
  sr["snoozing"] = sunrise::snoozing();
  sr["progress"] = sunrise::progress();
  sr["alarmId"] = sunrise::alarmId();

  JsonObject ny = d["ny"].to<JsonObject>();
  ny["days"] = daysToNy();

  String s;
  serializeJson(d, s);
  return s;
}

static String buildEffects() {
  JsonDocument d;
  JsonArray arr = d.to<JsonArray>();
  for (int i = 0; i < effectsCount(); i++) {
    EffectBase* e = effectAt(i);
    JsonObject o = arr.add<JsonObject>();
    o["id"] = e->id;
    o["name"] = e->name;
    o["group"] = (e->group == GRP_ATMO) ? "atmo" : (e->group == GRP_NY ? "ny" : "time");
    o["color"] = e->useColor;
    o["speed"] = e->useSpeed;
    o["intensity"] = e->useIntensity;
  }
  String s;
  serializeJson(d, s);
  return s;
}

static String wrapStateMessage() {
  String data = buildState();
  String msg = "{\"type\":\"state\",\"data\":";
  msg += data;
  msg += "}";
  return msg;
}

// ---------------------------------------------------------------------------
// HTTP helpers

static void sendJson(int code, const String& body) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
  server.send(code, "application/json", body);
}

static String mimeFor(const String& path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".js")) return "text/javascript";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".svg")) return "image/svg+xml";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".ico")) return "image/x-icon";
  if (path.endsWith(".woff2")) return "font/woff2";
  return "text/plain";
}

static bool serveFile(String path) {
  if (path.indexOf("..") >= 0) return false;
  if (path == "/") path = "/index.html";
  String gz = path + ".gz";
  bool acceptsGzip =
      server.hasHeader("Accept-Encoding") && server.header("Accept-Encoding").indexOf("gzip") >= 0;
  bool useGz = acceptsGzip && LittleFS.exists(gz);
  String target = useGz ? gz : path;
  if (!LittleFS.exists(target)) return false;
  File f = LittleFS.open(target, "r");
  if (!f) return false;
  if (useGz) server.sendHeader("Content-Encoding", "gzip");
  if (path == "/index.html" || path == "/setup.html") {
    server.sendHeader("Cache-Control", "no-cache");
  } else {
    server.sendHeader("Cache-Control", "max-age=86400");
  }
  server.streamFile(f, mimeFor(path));
  f.close();
  return true;
}

// ---------------------------------------------------------------------------
// Handlers

static void handleStateGet() { sendJson(200, buildState()); }

static void applyPatch(JsonDocument& d) {
  if (d["power"].is<bool>()) engine::setPower(d["power"].as<bool>());
  if (d["brightness"].is<int>())
    engine::setBrightness((uint8_t)constrain(d["brightness"].as<int>(), 0, 255));
  if (d["effect"].is<const char*>()) engine::setEffect(d["effect"].as<const char*>());
  if (d["color"].is<const char*>())
    strlcpy(g.color, d["color"].as<const char*>(), sizeof(g.color));
  if (d["speed"].is<int>()) g.speed = (uint8_t)constrain(d["speed"].as<int>(), 0, 255);
  if (d["intensity"].is<int>()) g.intensity = (uint8_t)constrain(d["intensity"].as<int>(), 0, 255);
  if (d["currentLimit"].is<int>()) engine::setCurrentLimit((uint16_t)d["currentLimit"].as<int>());
  if (d["tz"].is<const char*>()) {
    strlcpy(g.tz, d["tz"].as<const char*>(), sizeof(g.tz));
    net::applyTz();
  }

  JsonObject c = d["clock"].as<JsonObject>();
  if (!c.isNull()) {
    if (c["colorMode"].is<int>()) g.clock.colorMode = (uint8_t)constrain(c["colorMode"].as<int>(), 0, 2);
    if (c["speed"].is<int>()) g.clock.speed = (uint8_t)constrain(c["speed"].as<int>(), 10, 150);
    if (c["showDate"].is<bool>()) g.clock.showDate = c["showDate"].as<bool>();
    if (c["hour12"].is<bool>()) g.clock.hour12 = c["hour12"].as<bool>();
  }

  JsonObject m = d["mapping"].as<JsonObject>();
  if (!m.isNull()) {
    if (m["serpentine"].is<bool>()) g.mapping.serpentine = m["serpentine"].as<bool>();
    if (m["reverse"].is<bool>()) g.mapping.reverse = m["reverse"].as<bool>();
    if (m["flipY"].is<bool>()) g.mapping.flipY = m["flipY"].as<bool>();
    if (m["offset"].is<int>()) g.mapping.offset = (uint8_t)constrain(m["offset"].as<int>(), 0, 15);
  }

  JsonObject p = d["playlist"].as<JsonObject>();
  if (!p.isNull()) {
    if (p["enabled"].is<bool>()) g.playlistEnabled = p["enabled"].as<bool>();
    if (p["intervalMin"].is<int>())
      g.playlistIntervalMin = (uint16_t)constrain(p["intervalMin"].as<int>(), 1, 120);
  }
}

static void handleStatePost() {
  JsonDocument d;
  DeserializationError err = deserializeJson(d, server.arg("plain"));
  if (err) {
    sendJson(400, "{\"error\":\"bad json\"}");
    return;
  }
  const char* tz = d["tz"] | (const char*)nullptr;
  if (tz && !net::tzValid(tz)) {
    sendJson(400, "{\"error\":\"unknown timezone\"}");
    return;
  }
  applyPatch(d);
  settingsSaveSoon();
  sendJson(200, buildState());
  notifyState();
}

static void handleEffectsGet() { sendJson(200, buildEffects()); }

static void handleAlarmsPut() {
  JsonDocument d;
  DeserializationError err = deserializeJson(d, server.arg("plain"));
  if (err) {
    sendJson(400, "{\"error\":\"bad json\"}");
    return;
  }
  JsonArray arr = d["alarms"].as<JsonArray>();
  if (arr.isNull()) {
    sendJson(400, "{\"error\":\"alarms required\"}");
    return;
  }
  AlarmCfg old[cfg::MAX_ALARMS];
  memcpy(old, g.alarms, sizeof(old));
  for (uint8_t i = 0; i < cfg::MAX_ALARMS; i++) g.alarms[i] = AlarmCfg();
  uint8_t i = 0;
  for (JsonVariant v : arr) {
    if (i >= cfg::MAX_ALARMS) break;
    JsonObject a = v.as<JsonObject>();
    if (a.isNull()) continue;
    AlarmCfg& ac = g.alarms[i];
    ac.enabled = a["enabled"] | false;
    ac.days = (uint8_t)(a["days"] | 0) & 0x7F;
    ac.hour = (uint8_t)constrain((int)(a["hour"] | 7), 0, 23);
    ac.minute = (uint8_t)constrain((int)(a["minute"] | 0), 0, 59);
    ac.durationMin = (uint16_t)constrain((int)(a["durationMin"] | 15), 1, 40);
    ac.maxBrightness = (uint8_t)constrain((int)(a["maxBrightness"] | 200), 5, 255);
    ac.snoozeMin = (uint8_t)constrain((int)(a["snoozeMin"] | 9), 0, 30);
    ac.afterOffMin = (uint16_t)constrain((int)(a["afterOffMin"] | 0), 0, 180);
    ac.lastFiredYday = old[i].lastFiredYday;
    i++;
  }
  settingsSaveNow();
  sendJson(200, buildState());
  notifyState();
}

static void handleAction() {
  JsonDocument d;
  DeserializationError err = deserializeJson(d, server.arg("plain"));
  if (err) {
    sendJson(400, "{\"error\":\"bad json\"}");
    return;
  }
  const char* action = d["action"] | "";
  if (strcmp(action, "sunrise_test") == 0) {
    sunrise::test();
  } else if (strcmp(action, "sunrise_stop") == 0) {
    sunrise::stop();
  } else if (strcmp(action, "sunrise_snooze") == 0) {
    sunrise::snooze();
  } else if (strcmp(action, "calibrate") == 0) {
    engine::calibrate(d["pattern"] | "grid", d["ms"] | 20000);
  } else if (strcmp(action, "nyshow") == 0) {
    engine::startNyShow();
  } else if (strcmp(action, "settime") == 0) {
    uint32_t epoch = d["epoch"] | 0;
    if (epoch < 1700000000) {
      sendJson(400, "{\"error\":\"bad epoch\"}");
      return;
    }
    net::setManualTime(epoch);
    settingsSaveNow();
  } else if (strcmp(action, "timens") == 0) {
    net::resumeAutoTime();
    settingsSaveNow();
  } else if (strcmp(action, "reboot") == 0) {
    sendJson(200, "{\"ok\":true}");
    restartAt = millis() + 600;
    return;
  } else if (strcmp(action, "factory") == 0) {
    LittleFS.remove("/config.json");
    sendJson(200, "{\"ok\":true}");
    restartAt = millis() + 600;
    return;
  } else {
    sendJson(400, "{\"error\":\"unknown action\"}");
    return;
  }
  sendJson(200, buildState());
  notifyState();
}

static void handleWifiPost() {
  JsonDocument d;
  DeserializationError err = deserializeJson(d, server.arg("plain"));
  if (err) {
    sendJson(400, "{\"error\":\"bad json\"}");
    return;
  }
  const char* ssid = d["ssid"] | "";
  const char* pass = d["pass"] | "";
  if (!ssid[0]) {
    sendJson(400, "{\"error\":\"ssid required\"}");
    return;
  }
  net::saveWifi(ssid, pass);
  sendJson(200, "{\"ok\":true}");
  restartAt = millis() + 1000;
}

static void handleScan() {
  sendJson(200, net::scanJson());
}

struct TzCtx {
  bool started;
  uint16_t count;
  String chunk;
};

static void tzJsonVisitor(const char* name, size_t len, void* ctxp) {
  TzCtx& c = *(TzCtx*)ctxp;
  if (!c.started) {
    c.chunk = "[";
    c.started = true;
  }
  if (c.count) c.chunk += ",";
  c.chunk += "\"";
  for (size_t i = 0; i < len; i++) c.chunk += name[i];
  c.chunk += "\"";
  c.count++;
  if (c.chunk.length() > 512) {
    server.sendContent(c.chunk);
    c.chunk = "";
  }
}

static void handleTimezones() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  TzCtx ctx{false, 0, ""};
  ctx.chunk.reserve(640);
  net::tzVisit(tzJsonVisitor, &ctx);
  if (!ctx.started) ctx.chunk = "[";
  ctx.chunk += "]";
  server.sendContent(ctx.chunk);
  server.sendContent("");
}

static void handleUpdateCheck() {
  if (ota::busy()) {
    sendJson(409, "{\"error\":\"busy\"}");
    return;
  }
  bool ok = ota::checkNow();
  const ota::Release& r = ota::release();
  JsonDocument d;
  d["current"] = cfg::FW_VERSION;
  if (ok) {
    d["latest"] = r.tag;
    d["available"] = r.available;
    d["url"] = r.url;
    d["size"] = r.size;
    d["notes"] = r.notes;
  } else {
    d["error"] = r.error;
  }
  String s;
  serializeJson(d, s);
  sendJson(200, s);
}

static void handleUpdateInstall() {
  if (ota::busy()) {
    sendJson(409, "{\"error\":\"busy\"}");
    return;
  }
  if (!ota::release().checked && !ota::checkNow()) {
    sendJson(502, "{\"error\":\"check failed\"}");
    return;
  }
  if (ota::release().url.length() == 0) {
    sendJson(502, "{\"error\":\"no firmware asset\"}");
    return;
  }
  if (!ota::release().available) {
    sendJson(409, "{\"error\":\"already latest\"}");
    return;
  }
  if (!ota::startInstall()) {
    sendJson(409, "{\"error\":\"busy\"}");
    return;
  }
  notifyState();
  sendJson(200, "{\"ok\":true}");
}

static void handleOtaUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    otaOk = false;
    uint32_t maxSketch = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    Update.begin(maxSketch, U_FLASH);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.isRunning()) Update.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    Update.end(true);
    otaOk = !Update.hasError();
  }
}

static void handleOtaDone() {
  if (otaOk) {
    sendJson(200, "{\"ok\":true}");
    restartAt = millis() + 1200;
  } else {
    sendJson(500, "{\"error\":\"update failed\"}");
  }
}

static void handleNotFound() {
  if (server.method() == HTTP_OPTIONS) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
    server.send(204, "text/plain", "");
    return;
  }
  String path = server.uri();
  if (serveFile(path)) return;
  if (path.indexOf("/api/") == 0) {
    sendJson(404, "{\"error\":\"not found\"}");
    return;
  }
  if (net::apMode()) {
    server.sendHeader("Location", "/setup.html");
    server.send(302, "text/plain", "");
    return;
  }
  if (path.indexOf('.') < 0 && serveFile("/index.html")) return;
  sendJson(404, "{\"error\":\"not found\"}");
}

// ---------------------------------------------------------------------------
// WebSocket

static void wsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
  if (num >= WEBSOCKETS_SERVER_CLIENT_MAX) return;
  switch (type) {
    case WStype_CONNECTED: {
      pv[num] = false;
      String hello = wrapStateMessage();
      ws.sendTXT(num, hello);
      break;
    }
    case WStype_DISCONNECTED:
      pv[num] = false;
      break;
    case WStype_TEXT: {
      JsonDocument d;
      if (deserializeJson(d, payload, len)) break;
      const char* t = d["type"] | "";
      if (strcmp(t, "preview") == 0) {
        pv[num] = d["on"] | false;
        uint8_t fps = d["fps"] | 6;
        pvFps = (uint8_t)constrain((int)fps, 1, 15);
      } else if (strcmp(t, "ping") == 0) {
        ws.sendTXT(num, "{\"type\":\"pong\"}");
      }
      break;
    }
    default:
      break;
  }
}

// ---------------------------------------------------------------------------

void web::begin() {
  memset(pv, 0, sizeof(pv));
  ota::begin();
  server.on("/", HTTP_GET, []() {
    if (net::apMode()) {
      server.sendHeader("Location", "/setup.html");
      server.send(302, "text/plain", "");
    } else {
      serveFile("/index.html");
    }
  });
  server.on("/api/state", HTTP_GET, handleStateGet);
  server.on("/api/state", HTTP_POST, handleStatePost);
  server.on("/api/effects", HTTP_GET, handleEffectsGet);
  server.on("/api/alarms", HTTP_PUT, handleAlarmsPut);
  server.on("/api/action", HTTP_POST, handleAction);
  server.on("/api/wifi", HTTP_POST, handleWifiPost);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/timezones", HTTP_GET, handleTimezones);
  server.on("/api/update/check", HTTP_GET, handleUpdateCheck);
  server.on("/api/update/install", HTTP_POST, handleUpdateInstall);
  server.on("/api/ota", HTTP_POST, handleOtaDone, handleOtaUpload);
  server.onNotFound(handleNotFound);
  server.begin();
  ws.begin();
  ws.onEvent(wsEvent);
  lastStatePush = millis();
}

void web::loop() {
  server.handleClient();
  ws.loop();
  ota::pump();

  if (restartAt && (int32_t)(millis() - restartAt) >= 0) {
    ESP.restart();
  }

  uint32_t now = millis();

  static const char* lastOtaPhase = "idle";
  static uint8_t lastOtaPct = 0;
  static uint32_t lastOtaPush = 0;
  uint8_t otaPct = ota::progress();
  const char* otaPhase = ota::phaseName();
  bool phaseNew = strcmp(otaPhase, lastOtaPhase) != 0;
  if (phaseNew || (otaPct != lastOtaPct && now - lastOtaPush > 400)) {
    lastOtaPush = now;
    lastOtaPct = otaPct;
    lastOtaPhase = otaPhase;
    char buf[240];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"ota\",\"phase\":\"%s\",\"progress\":%u,\"error\":\"%s\"}",
             otaPhase, (unsigned)otaPct, ota::lastError().c_str());
    ws.broadcastTXT(buf);
  }

  if (stateDirty && now - lastStatePush > 150) {
    stateDirty = false;
    lastStatePush = now;
    String msg = wrapStateMessage();
    ws.broadcastTXT(msg);
  } else if (now - lastStatePush > cfg::STATE_PUSH_MS) {
    stateDirty = false;
    lastStatePush = now;
    String msg = wrapStateMessage();
    ws.broadcastTXT(msg);
  }

  if (sunrise::active() && now - lastSunrisePush > 700) {
    lastSunrisePush = now;
    char buf[140];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"sunrise\",\"active\":true,\"snoozing\":%s,\"progress\":%.3f}",
             sunrise::snoozing() ? "true" : "false", sunrise::progress());
    ws.broadcastTXT(buf);
  }

  bool any = false;
  for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
    if (pv[i]) {
      any = true;
      break;
    }
  }
  if (any && now - lastPreview >= (uint32_t)(1000 / pvFps)) {
    lastPreview = now;
    engine::previewCopy(pvFrame);
    for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
      if (pv[i]) ws.sendBIN(i, pvFrame, sizeof(pvFrame));
    }
  }
}
