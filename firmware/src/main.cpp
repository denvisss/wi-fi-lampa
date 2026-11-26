#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <LittleFS.h>
#include "config.h"
#include "engine.h"
#include "mapping.h"
#include "net.h"
#include "store.h"
#include "sunrise.h"
#include "touch.h"
#include "web.h"

static void bootAnimation() {
  for (int step = 0; step <= 40; step++) {
    float f = step / 40.0f;
    for (uint8_t x = 0; x < 16; x++) {
      for (uint8_t y = 0; y < 16; y++) {
        float k = f * 1.6f - y / 16.0f * 0.8f;
        k = min(1.0f, max(0.0f, k)) * (0.35f + 0.65f * f);
        engine::leds[logicalToPhysical(x, y)] = CHSV((uint8_t)(x * 16), 200, (uint8_t)(k * 255));
      }
    }
    FastLED.show();
    delay(18);
  }
  FastLED.clear(true);
  delay(100);
}

static void factoryResetGesture() {
  pinMode(cfg::PIN_TOUCH, INPUT);
  if (digitalRead(cfg::PIN_TOUCH) != HIGH) return;
  Serial.println("[lampa] touch held at boot, waiting 3s to reset Wi-Fi");
  uint32_t start = millis();
  while (millis() - start < 3000) {
    if (digitalRead(cfg::PIN_TOUCH) != HIGH) return;
    delay(10);
  }
  Serial.println("[lampa] factory reset: Wi-Fi settings erased");
  LittleFS.remove("/config.json");
  settingsDefaults();
  settingsSaveNow();
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.printf("\n[lampa] fw %s, reset reason: %s\n", cfg::FW_VERSION,
                ESP.getResetReason().c_str());

  LittleFS.begin();
  if (!settingsLoad()) {
    // После uploadfs config.json стёрт — восстанавливаем всё из EEPROM
    if (settingsLoadEeprom()) {
      Serial.println("[store] settings restored from EEPROM");
      settingsSaveNow();
    } else {
      settingsDefaults();
      settingsSaveNow();
    }
  } else {
    settingsSaveEeprom();
  }
  factoryResetGesture();

  // Сеть поднимаем до матрицы: портал первой настройки должен быть доступен
  // даже если лента не подключена или питание проседает на яркой анимации
  net::begin();

  FastLED.addLeds<WS2812B, cfg::PIN_LED, GRB>(engine::leds, cfg::LED_COUNT);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, g.currentLimit);
  FastLED.setBrightness(64);  // загрузочная анимация не должна валить слабый БП
  bootAnimation();
  FastLED.setBrightness(255);

  engine::begin();
  web::begin();
  sunrise::begin();
  touch::begin();

  ArduinoOTA.setHostname(cfg::HOSTNAME);
  ArduinoOTA.begin();

  if (net::apMode()) {
    Serial.printf("[lampa] ready: connect to '%s', open http://192.168.4.1\n",
                  cfg::AP_SSID);
  } else {
    Serial.printf("[lampa] ready: ip=%s http://%s.local\n",
                  WiFi.localIP().toString().c_str(), cfg::HOSTNAME);
  }
}

void loop() {
  ArduinoOTA.handle();
  net::loop();
  web::loop();
  touch::loop();
  storeLoop();

  static uint32_t lastTick = 0;
  static uint32_t lastSlow = 0;
  uint32_t now = millis();
  if (now - lastTick >= 16) {
    float dt = (now - lastTick) / 1000.0f;
    lastTick = now;
    engine::tick(dt);
  }
  if (now - lastSlow >= 250) {
    lastSlow = now;
    sunrise::loopTick();
    engine::checkNewYear();
    // Ручное время переживает перезагрузку: раз в час фиксируем текущий epoch
    static uint32_t lastManualSave = 0;
    if (g.manualTime && net::timeValid() && now - lastManualSave > 3600000UL) {
      lastManualSave = now;
      g.manualEpoch = (uint32_t)time(nullptr);
      settingsSaveNow();
    }
  }
}
