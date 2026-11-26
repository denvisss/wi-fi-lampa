#include "store.h"
#include <LittleFS.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

// Резервная копия настроек в EEPROM: uploadfs форматирует LittleFS и стирает config.json
static const uint32_t EEPROM_MAGIC = 0x4C414D50;  // "LAMP"
static const uint16_t EEPROM_LAYOUT = 2;          // менять при изменении struct Settings
struct EepromBlob {
  uint32_t magic;
  uint16_t layout;
  uint16_t size;
  Settings s;
};

Settings g;

static bool dirty = false;
static uint32_t dirtyAt = 0;

static void copyStr(char* dst, size_t n, const char* src) {
  if (!src) src = "";
  strlcpy(dst, src, n);
}

void settingsDefaults() {
  g = Settings();
}

bool settingsLoad() {
  File f = LittleFS.open("/config.json", "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  g.power = doc["power"] | true;
  g.brightness = doc["brightness"] | cfg::DEFAULT_BRIGHTNESS;
  copyStr(g.effect, sizeof g.effect, doc["effect"] | cfg::DEFAULT_EFFECT);
  copyStr(g.color, sizeof g.color, doc["color"] | cfg::DEFAULT_COLOR);
  g.speed = doc["speed"] | 128;
  g.intensity = doc["intensity"] | 128;
  g.currentLimit = doc["currentLimit"] | cfg::DEFAULT_CURRENT_LIMIT_MA;
  copyStr(g.tz, sizeof g.tz, doc["tz"] | cfg::DEFAULT_TZ);
  g.manualTime = doc["manualTime"] | false;
  g.manualEpoch = doc["manualEpoch"] | 0;
  copyStr(g.wifiSsid, sizeof g.wifiSsid, doc["ssid"] | "");
  copyStr(g.wifiPass, sizeof g.wifiPass, doc["pass"] | "");

  JsonObject c = doc["clock"];
  if (!c.isNull()) {
    g.clock.colorMode = c["colorMode"] | 1;
    g.clock.speed = c["speed"] | 80;
    g.clock.showDate = c["showDate"] | true;
    g.clock.hour12 = c["hour12"] | false;
  }

  JsonObject m = doc["mapping"];
  if (!m.isNull()) {
    g.mapping.serpentine = m["serpentine"] | true;
    g.mapping.reverse = m["reverse"] | false;
    g.mapping.flipY = m["flipY"] | false;
    g.mapping.offset = m["offset"] | 0;
  }

  JsonObject p = doc["playlist"];
  if (!p.isNull()) {
    g.playlistEnabled = p["enabled"] | false;
    g.playlistIntervalMin = p["intervalMin"] | 10;
  }

  JsonArray al = doc["alarms"];
  if (!al.isNull()) {
    uint8_t i = 0;
    for (JsonObject a : al) {
      if (i >= cfg::MAX_ALARMS) break;
      AlarmCfg& ac = g.alarms[i++];
      ac.enabled = a["enabled"] | false;
      ac.days = a["days"] | 0;
      ac.hour = a["hour"] | 7;
      ac.minute = a["minute"] | 0;
      ac.durationMin = a["durationMin"] | 15;
      ac.maxBrightness = a["maxBrightness"] | 200;
      ac.snoozeMin = a["snoozeMin"] | 9;
      ac.afterOffMin = a["afterOffMin"] | 0;
      uint32_t ly = a["lastFiredYday"] | 65535;
      ac.lastFiredYday = (uint16_t)ly;
    }
  }
  return true;
}

void settingsSaveNow() {
  JsonDocument doc;
  doc["power"] = g.power;
  doc["brightness"] = g.brightness;
  doc["effect"] = g.effect;
  doc["color"] = g.color;
  doc["speed"] = g.speed;
  doc["intensity"] = g.intensity;
  doc["currentLimit"] = g.currentLimit;
  doc["tz"] = g.tz;
  doc["manualTime"] = g.manualTime;
  doc["manualEpoch"] = g.manualEpoch;
  doc["ssid"] = g.wifiSsid;
  doc["pass"] = g.wifiPass;

  JsonObject c = doc["clock"].to<JsonObject>();
  c["colorMode"] = g.clock.colorMode;
  c["speed"] = g.clock.speed;
  c["showDate"] = g.clock.showDate;
  c["hour12"] = g.clock.hour12;

  JsonObject m = doc["mapping"].to<JsonObject>();
  m["serpentine"] = g.mapping.serpentine;
  m["reverse"] = g.mapping.reverse;
  m["flipY"] = g.mapping.flipY;
  m["offset"] = g.mapping.offset;

  JsonObject p = doc["playlist"].to<JsonObject>();
  p["enabled"] = g.playlistEnabled;
  p["intervalMin"] = g.playlistIntervalMin;

  JsonArray al = doc["alarms"].to<JsonArray>();
  for (uint8_t i = 0; i < cfg::MAX_ALARMS; i++) {
    JsonObject a = al.add<JsonObject>();
    const AlarmCfg& ac = g.alarms[i];
    a["enabled"] = ac.enabled;
    a["days"] = ac.days;
    a["hour"] = ac.hour;
    a["minute"] = ac.minute;
    a["durationMin"] = ac.durationMin;
    a["maxBrightness"] = ac.maxBrightness;
    a["snoozeMin"] = ac.snoozeMin;
    a["afterOffMin"] = ac.afterOffMin;
    a["lastFiredYday"] = ac.lastFiredYday;
  }

  File f = LittleFS.open("/config.json", "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
  settingsSaveEeprom();
}

void settingsSaveSoon() {
  dirty = true;
  dirtyAt = millis();
}

void storeLoop() {
  if (dirty && (millis() - dirtyAt > 2500)) {
    dirty = false;
    settingsSaveNow();
  }
}

void settingsSaveEeprom() {
  EepromBlob b;
  b.magic = EEPROM_MAGIC;
  b.layout = EEPROM_LAYOUT;
  b.size = (uint16_t)sizeof(Settings);
  b.s = g;
  EEPROM.begin(sizeof b);
  EEPROM.put(0, b);
  EEPROM.commit();
  EEPROM.end();
}

bool settingsLoadEeprom() {
  EepromBlob b;
  EEPROM.begin(sizeof b);
  EEPROM.get(0, b);
  EEPROM.end();
  if (b.magic != EEPROM_MAGIC || b.layout != EEPROM_LAYOUT || b.size != sizeof(Settings)) return false;
  g = b.s;
  g.effect[sizeof g.effect - 1] = 0;
  g.color[sizeof g.color - 1] = 0;
  g.tz[sizeof g.tz - 1] = 0;
  g.wifiSsid[sizeof g.wifiSsid - 1] = 0;
  g.wifiPass[sizeof g.wifiPass - 1] = 0;
  return true;
}
