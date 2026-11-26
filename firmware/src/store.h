#pragma once
#include <Arduino.h>
#include "config.h"

struct AlarmCfg {
  bool enabled = false;
  uint8_t days = 0;           // bit0 = Пн ... bit6 = Вс
  uint8_t hour = 7;
  uint8_t minute = 0;
  uint16_t durationMin = 15;  // 10..25 минут
  uint8_t maxBrightness = 200;
  uint8_t snoozeMin = 9;
  uint16_t afterOffMin = 0;   // 0 = остаться включённой
  uint16_t lastFiredYday = 0xFFFF;
};

struct ClockCfg {
  uint8_t colorMode = 1;  // 0 = выбранный цвет, 1 = радуга
  uint8_t speed = 80;     // px/сек
  bool showDate = true;
  bool hour12 = false;
};

struct MappingCfg {
  bool serpentine = true;
  bool reverse = false;
  bool flipY = false;
  uint8_t offset = 0;  // сдвиг по кругу 0..15
};

struct Settings {
  bool power = true;
  uint8_t brightness = cfg::DEFAULT_BRIGHTNESS;
  char effect[16] = "aurora";
  char color[8] = "#FFB066";
  uint8_t speed = 128;
  uint8_t intensity = 128;
  uint16_t currentLimit = cfg::DEFAULT_CURRENT_LIMIT_MA;

  ClockCfg clock;
  AlarmCfg alarms[cfg::MAX_ALARMS];
  bool playlistEnabled = false;
  uint16_t playlistIntervalMin = 10;

  MappingCfg mapping;
  char tz[48] = "Europe/Moscow";  // IANA ("Europe/Moscow") или POSIX ("MSK-3")

  bool manualTime = false;    // время выставлено вручную, SNTP не перезаписывает
  uint32_t manualEpoch = 0;

  char wifiSsid[33] = "";
  char wifiPass[65] = "";
};

extern Settings g;

bool settingsLoad();
void settingsDefaults();
void settingsSaveNow();
void settingsSaveSoon();
void storeLoop();

// Полный дубль настроек в EEPROM: переживает uploadfs и форматирование LittleFS
void settingsSaveEeprom();
bool settingsLoadEeprom();
