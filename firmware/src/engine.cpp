#include "engine.h"
#include <math.h>
#include "effects.h"
#include "mapping.h"
#include "store.h"
#include "sunrise.h"

namespace engine {

CRGB leds[cfg::LED_COUNT];

static CRGB fbCur[cfg::LED_COUNT];
static CRGB fbOld[cfg::LED_COUNT];
static CRGB fbFinal[cfg::LED_COUNT];
static uint8_t gLUT[256];

static EffectBase* cur = nullptr;
static bool fading = false;
static float fadeT = 0;
static float tGlobal = 0;

static bool calibActive = false;
static uint32_t calibUntil = 0;
static char calibPattern[12] = "grid";

static bool nyActive = false;
static uint32_t nyUntil = 0;
static char prevNyEffect[16] = "";

static uint32_t lastRotate = 0;
static uint8_t plIndex = 0;

static const char* PLAYLIST[] = {"candle", "aurora",   "ocean", "starfield", "rainbow",
                                 "plasma", "fire",     "snowfall", "garland", "spiral",
                                 "candy",  "fireworks", "breathing"};
static const uint8_t PLAYLIST_N = sizeof(PLAYLIST) / sizeof(PLAYLIST[0]);

static void buildGamma() {
  for (int i = 0; i < 256; i++) {
    float v = powf(i / 255.0f, 2.35f) * 255.0f + 0.5f;
    gLUT[i] = (uint8_t)min(255.0f, v);
  }
}

void begin() {
  buildGamma();
  fill_solid(fbCur, cfg::LED_COUNT, CRGB::Black);
  const char* startId = g.effect;
  if (!effectById(startId)) startId = cfg::DEFAULT_EFFECT;
  cur = effectById(startId);
  if (!cur) cur = effectAt(0);
  strlcpy(g.effect, cur->id, sizeof(g.effect));
  lastRotate = millis();
}

void setPower(bool on) {
  if (g.power == on) return;
  g.power = on;
  if (!on && sunrise::active()) {
    sunrise::stop();
  }
  settingsSaveSoon();
  notifyState();
}

void togglePower() { setPower(!g.power); }
bool powerOn() { return g.power; }

void setBrightness(uint8_t v) {
  g.brightness = v;
  settingsSaveSoon();
  notifyState();
}

void setCurrentLimit(uint16_t ma) {
  ma = constrain(ma, cfg::MIN_CURRENT_LIMIT_MA, cfg::MAX_CURRENT_LIMIT_MA);
  if (g.currentLimit == ma) return;
  g.currentLimit = ma;
  FastLED.setMaxPowerInVoltsAndMilliamps(5, ma);
  settingsSaveSoon();
  notifyState();
}

bool setEffect(const char* id, bool instant) {
  EffectBase* e = effectById(id);
  if (!e) return false;
  if (cur == e) return true;
  memcpy(fbOld, fbCur, sizeof(fbCur));
  cur = e;
  cur->enter();
  strlcpy(g.effect, e->id, sizeof(g.effect));
  fading = !instant;
  fadeT = 0;
  lastRotate = millis();
  settingsSaveSoon();
  notifyState();
  return true;
}

void nextEffect() {
  int n = effectsCount();
  int idx = 0;
  for (int i = 0; i < n; i++) {
    if (effectAt(i) == cur) {
      idx = i;
      break;
    }
  }
  idx = (idx + 1) % n;
  setEffect(effectAt(idx)->id);
}

const char* currentId() { return cur ? cur->id : ""; }

void calibrate(const char* pattern, uint16_t ms) {
  if (!pattern || strcmp(pattern, "off") == 0) {
    calibActive = false;
    return;
  }
  strlcpy(calibPattern, pattern, sizeof(calibPattern));
  calibActive = true;
  calibUntil = millis() + ms;
  if (!g.power) g.power = true;
  notifyState();
}

bool calibrating() { return calibActive; }

void startNyShow() {
  if (nyActive) return;
  if (!g.power || sunrise::active()) return;
  strlcpy(prevNyEffect, g.effect, sizeof(prevNyEffect));
  if (strcmp(prevNyEffect, "nyshow") == 0) return;
  nyActive = true;
  nyUntil = millis() + cfg::NY_SHOW_MS;
  setEffect("nyshow");
}

bool nyShowActive() { return nyActive; }

void checkNewYear() {
  time_t now = time(nullptr);
  if (now < 1700000000) return;
  tm tt;
  localtime_r(&now, &tt);
  if (tt.tm_mon == 0 && tt.tm_mday == 1 && tt.tm_hour == 0 && tt.tm_min < 3) {
    static int lastYear = -32000;
    if (lastYear != tt.tm_year) {
      lastYear = tt.tm_year;
      startNyShow();
    }
  }
}

static void drawCalibration() {
  if (strcmp(calibPattern, "chase") == 0) {
    float posBase = millis() / 1000.0f * 9.0f;
    for (uint8_t x = 0; x < 16; x++) {
      for (uint8_t y = 0; y < 16; y++) {
        float pos = fmodf(posBase + y * 1.7f, 16.0f);
        float d = fabsf((float)x - pos);
        d = min(d, 16.0f - d);
        if (d < 1.3f) {
          fbCur[XY(x, y)] = CRGB(255, 230, 190);
        } else {
          fbCur[XY(x, y)] = CRGB(0, 0, 30);
        }
      }
    }
  } else {
    for (uint8_t x = 0; x < 16; x++) {
      for (uint8_t y = 0; y < 16; y++) {
        fbCur[XY(x, y)] = CHSV((uint8_t)(x * 16), 255, (uint8_t)(18 + y * 14));
      }
    }
  }
}

static void outputFrame() {
  uint32_t sum = 0;
  for (uint16_t i = 0; i < cfg::LED_COUNT; i++) {
    sum += max(fbCur[i].r, max(fbCur[i].g, fbCur[i].b));
  }
  float k = g.brightness / 255.0f;
  float estmA = sum * ((float)cfg::LED_MA_FULL / 255.0f) * k;
  if (estmA > g.currentLimit) {
    k *= (float)g.currentLimit / estmA;
  }
  uint16_t scale = (uint16_t)(k * 255.0f + 0.5f);
  for (uint8_t y = 0; y < 16; y++) {
    for (uint8_t x = 0; x < 16; x++) {
      uint16_t li = XY(x, y);
      CRGB c = fbCur[li];
      c.r = gLUT[(c.r * scale) >> 8];
      c.g = gLUT[(c.g * scale) >> 8];
      c.b = gLUT[(c.b * scale) >> 8];
      fbFinal[li] = c;
      leds[logicalToPhysical(x, y)] = c;
    }
  }
  FastLED.show();
}

void tick(float dt) {
  if (dt > 0.1f) dt = 0.1f;
  tGlobal += dt;

  if (nyActive && (int32_t)(millis() - nyUntil) >= 0) {
    nyActive = false;
    if (prevNyEffect[0]) setEffect(prevNyEffect);
  }

  if (calibActive) {
    if ((int32_t)(millis() - calibUntil) >= 0) {
      calibActive = false;
    } else {
      drawCalibration();
      outputFrame();
      return;
    }
  }

  bool sr = sunrise::render(fbCur, dt);
  if (!sr) {
    if (!g.power) {
      fill_solid(fbCur, cfg::LED_COUNT, CRGB::Black);
    } else {
      if (cur) cur->render(fbCur, dt, tGlobal);
      if (fading) {
        fadeT += dt;
        uint8_t k = (uint8_t)(min(1.0f, fadeT / cfg::CROSSFADE_SEC) * 255.0f);
        for (uint16_t i = 0; i < cfg::LED_COUNT; i++) {
          fbCur[i].r = fbOld[i].r + ((int)fbCur[i].r - fbOld[i].r) * k / 255;
          fbCur[i].g = fbOld[i].g + ((int)fbCur[i].g - fbOld[i].g) * k / 255;
          fbCur[i].b = fbOld[i].b + ((int)fbCur[i].b - fbOld[i].b) * k / 255;
        }
        if (fadeT >= cfg::CROSSFADE_SEC) fading = false;
      }
    }
  }

  if (g.playlistEnabled && g.power && !sunrise::active() && !nyActive &&
      millis() - lastRotate > (uint32_t)g.playlistIntervalMin * 60000UL) {
    lastRotate = millis();
    plIndex = (plIndex + 1) % PLAYLIST_N;
    setEffect(PLAYLIST[plIndex]);
  }

  outputFrame();
}

void previewCopy(uint8_t* rgbOut) {
  for (uint16_t i = 0; i < cfg::LED_COUNT; i++) {
    rgbOut[i * 3 + 0] = fbFinal[i].r;
    rgbOut[i * 3 + 1] = fbFinal[i].g;
    rgbOut[i * 3 + 2] = fbFinal[i].b;
  }
}

}  // namespace engine
