#include "touch.h"
#include "config.h"
#include "engine.h"
#include "store.h"
#include "sunrise.h"

namespace touch {

static bool raw = false;
static bool stable = false;
static uint32_t lastChange = 0;
static uint32_t pressAt = 0;
static bool hold = false;
static int8_t rampDir = 1;
static uint32_t lastRamp = 0;
static uint8_t taps = 0;
static uint32_t lastTapAt = 0;
static bool pendingSingle = false;

static void doSingle() {
  if (sunrise::active()) {
    sunrise::snooze();
  } else if (!g.power) {
    engine::setPower(true);
  } else {
    engine::nextEffect();
  }
  notifyState();
}

static void doDouble() {
  if (sunrise::active()) {
    sunrise::snooze();
  } else {
    engine::togglePower();
  }
  notifyState();
}

static void onRelease(uint32_t held) {
  if (hold) {
    hold = false;
    settingsSaveSoon();
    notifyState();
    return;
  }
  if (held >= 120 && held < 600) {
    taps++;
    lastTapAt = millis();
    if (taps >= 2) {
      taps = 0;
      pendingSingle = false;
      doDouble();
    } else {
      pendingSingle = true;
    }
  }
}

void begin() {
  pinMode(cfg::PIN_TOUCH, INPUT);
  stable = raw = (digitalRead(cfg::PIN_TOUCH) == HIGH);
  lastChange = millis();
}

void loop() {
  bool r = (digitalRead(cfg::PIN_TOUCH) == HIGH);
  if (r != raw) {
    raw = r;
    lastChange = millis();
  }
  if (millis() - lastChange > 18 && stable != raw) {
    stable = raw;
    if (stable) {
      pressAt = millis();
      hold = false;
    } else {
      onRelease(millis() - pressAt);
    }
  }

  if (stable && !hold && (millis() - pressAt > 600)) {
    hold = true;
    if (sunrise::active()) {
      sunrise::stop();
      hold = false;
      pendingSingle = false;
      taps = 0;
    } else {
      rampDir = 1;
      lastRamp = millis();
      pendingSingle = false;
      taps = 0;
    }
  }

  if (stable && hold && (millis() - lastRamp > 28)) {
    lastRamp = millis();
    int v = (int)g.brightness + rampDir * 3;
    if (v >= 255) {
      v = 255;
      rampDir = -1;
    }
    if (v <= 2) {
      v = 2;
      rampDir = 1;
    }
    g.brightness = (uint8_t)v;
    notifyState();
  }

  if (pendingSingle && (millis() - lastTapAt > 380)) {
    pendingSingle = false;
    taps = 0;
    doSingle();
  }
}

}  // namespace touch
