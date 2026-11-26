#include "sunrise.h"
#include <math.h>
#include <time.h>
#include "mapping.h"
#include "store.h"

namespace sunrise {

enum State : uint8_t { IDLE, ACTIVE, SNOOZE };

static State st = IDLE;
static int8_t curAlarm = -1;
static uint32_t t0 = 0, durMs = 0, snoozeEnd = 0, offAt = 0;
static bool isTest = false;
static bool prevPower = true;
static uint8_t prevBright = 160;
static char prevEffect[16] = "";
static uint8_t lastMin = 255;

static CRGB kelvinRamp(float p) {
  const float stops[3] = {0.0f, 0.55f, 1.0f};
  const CRGB cols[3] = {CRGB(255, 112, 8), CRGB(255, 168, 86), CRGB(255, 210, 165)};
  if (p <= stops[0]) return cols[0];
  if (p >= stops[2]) return cols[2];
  int seg = (p < stops[1]) ? 0 : 1;
  float m = (p - stops[seg]) / (stops[seg + 1] - stops[seg]);
  return CRGB(cols[seg].r + (cols[seg + 1].r - cols[seg].r) * m,
              cols[seg].g + (cols[seg + 1].g - cols[seg].g) * m,
              cols[seg].b + (cols[seg + 1].b - cols[seg].b) * m);
}

static void startAlarm(int8_t idx) {
  curAlarm = idx;
  st = ACTIVE;
  isTest = false;
  t0 = millis();
  durMs = (uint32_t)g.alarms[idx].durationMin * 60000UL;
  offAt = 0;
  notifyState();
}

static void finish() {
  if (isTest) {
    g.power = prevPower;
    g.brightness = prevBright;
    strlcpy(g.effect, prevEffect, sizeof(g.effect));
  } else {
    AlarmCfg& a = g.alarms[curAlarm < 0 ? 0 : curAlarm];
    g.power = true;
    g.brightness = a.maxBrightness;
    strlcpy(g.effect, "solid", sizeof(g.effect));
    strlcpy(g.color, "#FFD9B0", sizeof(g.color));
    if (a.afterOffMin > 0) offAt = millis() + (uint32_t)a.afterOffMin * 60000UL;
  }
  st = IDLE;
  curAlarm = -1;
  isTest = false;
  settingsSaveSoon();
  notifyState();
}

void begin() {
  st = IDLE;
  lastMin = 255;
}

void loopTick() {
  time_t now = time(nullptr);
  if (now < 1700000000) return;
  tm tt;
  localtime_r(&now, &tt);

  if (offAt && (int32_t)(millis() - offAt) >= 0) {
    offAt = 0;
    g.power = false;
    settingsSaveSoon();
    notifyState();
  }

  if (st == SNOOZE && (int32_t)(millis() - snoozeEnd) >= 0) {
    st = ACTIVE;
    t0 = millis();
    durMs = 3UL * 60000UL;
    notifyState();
    return;
  }

  if (st == IDLE && tt.tm_min != lastMin) {
    lastMin = tt.tm_min;
    for (uint8_t i = 0; i < cfg::MAX_ALARMS; i++) {
      AlarmCfg& a = g.alarms[i];
      if (!a.enabled) continue;
      uint8_t bit = (tt.tm_wday == 0) ? 6 : (uint8_t)(tt.tm_wday - 1);
      if (!(a.days & (1 << bit))) continue;
      if (a.hour != tt.tm_hour || a.minute != tt.tm_min) continue;
      uint16_t dayId = (uint16_t)((tt.tm_year % 4) * 400 + tt.tm_yday);
      if (a.lastFiredYday == dayId) continue;
      a.lastFiredYday = dayId;
      settingsSaveSoon();
      startAlarm((int8_t)i);
      break;
    }
  }
}

bool render(CRGB* fb, float dt) {
  if (st == IDLE) return false;
  uint32_t now = millis();

  if (st == SNOOZE) {
    float breath = 0.9f + 0.1f * sinf(now / 1000.0f * 0.9f);
    uint8_t k = (uint8_t)(30 * breath);
    CRGB c = CRGB(255, 150, 70);
    c.nscale8_video(k);
    fill_solid(fb, cfg::LED_COUNT, c);
    return true;
  }

  float p = (float)(now - t0) / (float)durMs;
  if (p >= 1.0f) {
    finish();
    return false;
  }

  float ease = p * p * (3.0f - 2.0f * p);
  uint8_t maxB = isTest ? 220 : g.alarms[curAlarm < 0 ? 0 : curAlarm].maxBrightness;
  float breath = 1.0f + 0.035f * sinf(now / 1000.0f * 1.1f);
  CRGB warm = kelvinRamp(p);
  float front = p * 20.0f;

  for (uint8_t x = 0; x < 16; x++) {
    for (uint8_t y = 0; y < 16; y++) {
      float dy = front - y;
      float m = (dy >= 0) ? min(1.0f, 0.55f + dy * 0.25f) : max(0.0f, 0.55f + dy * 0.045f);
      float b = (maxB / 255.0f) * ease * breath * m;
      CRGB c = warm;
      c.nscale8_video((uint8_t)(min(1.0f, b) * 255));
      fb[XY(x, y)] = c;
    }
  }
  return true;
}

bool active() { return st != IDLE; }
bool snoozing() { return st == SNOOZE; }
int8_t alarmId() { return curAlarm; }

float progress() {
  if (st != ACTIVE) return 0;
  float p = (float)(millis() - t0) / (float)durMs;
  return p > 1.0f ? 1.0f : p;
}

void test() {
  if (st != IDLE) return;
  prevPower = g.power;
  prevBright = g.brightness;
  strlcpy(prevEffect, g.effect, sizeof(prevEffect));
  curAlarm = -1;
  isTest = true;
  st = ACTIVE;
  t0 = millis();
  durMs = 45000UL;
  notifyState();
}

void stop() {
  if (st == IDLE) return;
  if (isTest) {
    g.power = prevPower;
    g.brightness = prevBright;
    strlcpy(g.effect, prevEffect, sizeof(g.effect));
  } else {
    g.power = false;
  }
  st = IDLE;
  curAlarm = -1;
  isTest = false;
  offAt = 0;
  settingsSaveSoon();
  notifyState();
}

void snooze() {
  if (st == IDLE || isTest || curAlarm < 0) return;
  AlarmCfg& a = g.alarms[curAlarm];
  if (a.snoozeMin == 0) return;
  st = SNOOZE;
  snoozeEnd = millis() + (uint32_t)a.snoozeMin * 60000UL;
  notifyState();
}

}  // namespace sunrise
