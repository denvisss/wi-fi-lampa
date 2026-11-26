#include "effects.h"
#include <math.h>
#include <time.h>
#include "font.h"
#include "mapping.h"
#include "store.h"

namespace {

inline CRGB colorNow() { return parseColorHex(g.color); }
inline CRGB scaled(const CRGB& c, uint8_t k) {
  CRGB r = c;
  r.nscale8_video(k);
  return r;
}
inline uint8_t max3(const CRGB& c) { return max(c.r, max(c.g, c.b)); }

const char* const WD_RU[7] = {"\xC2\xD1", "\xCF\xCD", "\xC2\xD2", "\xD1\xD0",
                              "\xD7\xD2", "\xCF\xD2", "\xD1\xC1"};
const char NY_MSG[] = "\xD1 \xCD\xCE\xC2\xDB\xCC \xC3\xCE\xC4\xCE\xCC! ";

// ---------------------------------------------------------------------------

class SolidEffect : public EffectBase {
 public:
  SolidEffect() : EffectBase("solid", "Сплошной цвет", GRP_ATMO, true, false, false) {}
  void render(CRGB* fb, float, float) override { fill_solid(fb, cfg::LED_COUNT, colorNow()); }
};

class BreathingEffect : public EffectBase {
 public:
  BreathingEffect() : EffectBase("breathing", "Дыхание", GRP_ATMO, true, true) {}
  void render(CRGB* fb, float, float t) override {
    float ph = sinf(t * (0.35f + g.speed / 300.0f)) * 0.5f + 0.5f;
    uint8_t k = (uint8_t)(20 + 235 * ph * ph);
    CRGB c = scaled(colorNow(), k);
    fill_solid(fb, cfg::LED_COUNT, c);
  }
};

class CandleEffect : public EffectBase {
  uint8_t fl[16];
  bool init = false;

 public:
  CandleEffect() : EffectBase("candle", "Свеча", GRP_ATMO, false, false, true) {}
  void render(CRGB* fb, float, float t) override {
    if (!init) {
      for (uint8_t i = 0; i < 16; i++) fl[i] = 190;
      init = true;
    }
    for (uint8_t x = 0; x < 16; x++) {
      uint8_t target = 150 + (uint8_t)(100 * (g.intensity / 255.0f)) - random8(0, 45);
      fl[x] = (uint8_t)(((uint16_t)fl[x] * 7 + target) / 8);
      for (uint8_t y = 0; y < 16; y++) {
        uint8_t v = sin8((uint8_t)((int)(t * 90) + x * 21 + y * 13));
        uint8_t k = (uint8_t)((uint16_t)fl[x] * (200 + v / 3) / 255);
        fb[XY(x, y)] = scaled(CRGB(255, 138, 50), k);
      }
    }
    if (random8() < 6) fb[XY(random8(16), random8(3, 13))] = CRGB(255, 210, 120);
  }
};

class AuroraEffect : public EffectBase {
 public:
  AuroraEffect() : EffectBase("aurora", "Северное сияние", GRP_ATMO, false, true, true) {}
  void render(CRGB* fb, float, float t) override {
    float sp = 0.6f + g.speed / 128.0f;
    float inten = 0.45f + g.intensity / 255.0f * 0.75f;
    for (uint8_t x = 0; x < 16; x++) {
      float fx = x * 0.55f;
      for (uint8_t y = 0; y < 16; y++) {
        uint8_t w1 = sin8((uint8_t)(int)(fx * 43 + t * 22 * sp + y * 6));
        uint8_t w2 = sin8((uint8_t)(int)(fx * 24 - t * 14 * sp + y * 19));
        uint8_t v = (w1 / 2 + w2 / 2);
        v = scale8(v, 245 - y * 11);
        uint8_t hue = 96 + (uint8_t)(y * 4 + sin8((uint8_t)(int)(fx * 17 + t * 9)) / 10);
        uint8_t k = (uint8_t)((float)v * inten);
        fb[XY(x, y)] = CHSV(hue, 220, k);
      }
    }
  }
};

class OceanEffect : public EffectBase {
 public:
  OceanEffect() : EffectBase("ocean", "Океан", GRP_ATMO, false, true, true) {}
  void render(CRGB* fb, float, float t) override {
    float sp = 0.5f + g.speed / 100.0f;
    for (uint8_t x = 0; x < 16; x++) {
      for (uint8_t y = 0; y < 16; y++) {
        uint8_t v = sin8((uint8_t)(int)(x * 18 + t * 20 * sp + sin8((uint8_t)(int)(y * 26 + t * 16)) / 3));
        uint8_t k = (uint8_t)(30 + (uint16_t)v * (140 + g.intensity / 3) / 255);
        k = scale8(k, 235 - y * 8);
        CRGB c = CHSV((uint8_t)(146 + y * 2), 230, k);
        if (v > 232) c = CRGB(160, 225, 255);
        fb[XY(x, y)] = c;
      }
    }
  }
};

class StarfieldEffect : public EffectBase {
  struct Star {
    uint8_t x, y, hue;
    uint16_t phase;
    uint8_t step;
  };
  Star stars[42];
  bool init = false;
  float shootX = -10, shootY = 0;
  bool shooting = false;

 public:
  StarfieldEffect() : EffectBase("starfield", "Звёздное небо", GRP_ATMO, false, true, true) {}
  void initStars() {
    for (uint8_t i = 0; i < 42; i++) {
      stars[i].x = random8(16);
      stars[i].y = random8(16);
      stars[i].phase = random16();
      stars[i].step = random8(2, 16);
      stars[i].hue = random8(150, 220);
    }
    init = true;
  }
  void render(CRGB* fb, float dt, float) override {
    if (!init) initStars();
    fill_solid(fb, cfg::LED_COUNT, CRGB(0, 0, 2));
    for (uint8_t i = 0; i < 42; i++) {
      Star& s = stars[i];
      s.phase += (uint16_t)(dt * s.step * 60);
      uint8_t b = sin8(s.phase >> 3);
      if (b > 30) fb[XY(s.x, s.y)] += scaled(CHSV(s.hue, 120, 255), b);
    }
    if (!shooting && random8() < 2) {
      shooting = true;
      shootX = random8(16);
      shootY = 15;
    }
    if (shooting) {
      int bx = (int)shootX, by = (int)shootY;
      shootX -= dt * 14;
      shootY -= dt * 12;
      for (uint8_t k = 0; k < 4; k++) {
        int x = bx + k, y = by + k;
        if (y >= 0 && y < 16) fb[XY(x & 15, y)] += scaled(CRGB(200, 220, 255), 220 - k * 55);
      }
      if (shootY < 0 || shootX < -4) shooting = false;
    }
  }
};

class RainbowEffect : public EffectBase {
 public:
  RainbowEffect() : EffectBase("rainbow", "Радуга", GRP_ATMO, false, true) {}
  void render(CRGB* fb, float, float t) override {
    for (uint8_t x = 0; x < 16; x++) {
      for (uint8_t y = 0; y < 16; y++) {
        uint8_t hue = (uint8_t)(x * 16 + y * 5 + (int)(t * 30 * (g.speed / 128.0f)));
        fb[XY(x, y)] = CHSV(hue, 245, 235);
      }
    }
  }
};

class PlasmaEffect : public EffectBase {
 public:
  PlasmaEffect() : EffectBase("plasma", "Плазма", GRP_ATMO, false, true, true) {}
  void render(CRGB* fb, float, float t) override {
    float sp = 0.5f + g.speed / 128.0f;
    for (uint8_t x = 0; x < 16; x++) {
      for (uint8_t y = 0; y < 16; y++) {
        uint8_t v = sin8((uint8_t)(int)(x * 20 + t * 15 * sp));
        v = qadd8(v, sin8((uint8_t)(int)(y * 22 - t * 11 * sp)));
        v = qadd8(v, sin8((uint8_t)(int)((x + y) * 11 + t * 7 * sp)));
        uint8_t hue = (uint8_t)(v + t * 6);
        uint8_t k = (uint8_t)(90 + (uint16_t)v * (100 + g.intensity / 2) / 255);
        fb[XY(x, y)] = CHSV(hue, 235, k);
      }
    }
  }
};

class FireEffect : public EffectBase {
  uint8_t heat[256];
  bool init = false;
  float acc = 0;
  CRGB lut[256];

  void buildLut() {
    const uint8_t pal[9][3] = {{0, 0, 0},     {40, 4, 0},    {92, 10, 0},  {140, 26, 0},
                               {190, 52, 0},  {225, 92, 0},  {250, 140, 12}, {255, 195, 60},
                               {255, 240, 175}};
    for (int i = 0; i < 256; i++) {
      float f = i / 255.0f * 8.0f;
      int a = (int)f;
      int b = min(8, a + 1);
      float m = f - a;
      lut[i] = CRGB(pal[a][0] + (pal[b][0] - pal[a][0]) * m,
                    pal[a][1] + (pal[b][1] - pal[a][1]) * m,
                    pal[a][2] + (pal[b][2] - pal[a][2]) * m);
    }
  }
  void step() {
    for (int y = 15; y >= 1; y--) {
      for (int x = 0; x < 16; x++) {
        uint16_t sum = (uint16_t)heat[(y - 1) * 16 + ((x + 15) & 15)] * 2 +
                       heat[(y - 1) * 16 + x] * 4 +
                       heat[(y - 1) * 16 + ((x + 1) & 15)] * 2;
        heat[y * 16 + x] = (uint8_t)min(255, sum / 8 * 24 / 25);
      }
    }
    uint8_t seed = 90 + g.intensity / 3;
    for (int x = 0; x < 16; x++) {
      heat[x] = scale8(heat[x], 170);
      if (random8() < 70 + g.intensity / 5) heat[x] = qadd8(heat[x], random8(seed / 2, seed));
    }
  }

 public:
  FireEffect() : EffectBase("fire", "Камин", GRP_NY, false, true, true) {}
  void initFire() {
    buildLut();
    for (int i = 0; i < 256; i++) heat[i] = 0;
    init = true;
  }
  void render(CRGB* fb, float dt, float) override {
    if (!init) initFire();
    acc += dt * (30 + g.speed);
    int n = 0;
    while (acc >= 1.0f && n < 4) {
      step();
      acc -= 1.0f;
      n++;
    }
    for (int i = 0; i < 256; i++) fb[i] = lut[heat[i]];
  }
};

class SnowEffect : public EffectBase {
  struct Flake {
    float x, y;
    float vy;
    uint8_t ph;
    uint8_t size;
  };
  Flake flakes[56];
  bool init = false;

 public:
  SnowEffect() : EffectBase("snowfall", "Снегопад", GRP_NY, false, true, true) {}
  void render(CRGB* fb, float dt, float t) override {
    if (!init) {
      for (uint8_t i = 0; i < 56; i++) {
        flakes[i].x = random8(16);
        flakes[i].y = random8(16);
        flakes[i].vy = 1.2f + random8(0, 25) / 10.0f;
        flakes[i].ph = random8();
        flakes[i].size = random8(2) ? 0 : 1;
      }
      init = true;
    }
    fill_solid(fb, cfg::LED_COUNT, CRGB(0, 0, 4));
    uint8_t bright = 170 + g.intensity / 4;
    float sp = 0.5f + g.speed / 128.0f;
    for (uint8_t i = 0; i < 56; i++) {
      Flake& f = flakes[i];
      f.y -= f.vy * dt * sp * 1.6f;
      f.x += sinf(t * 0.7f + f.ph) * dt * 1.4f;
      if (f.y < 0) {
        f.y = 16.2f;
        f.x = random8(16);
      }
      while (f.x < 0) f.x += 16;
      while (f.x >= 16) f.x -= 16;
      uint8_t tw = 190 + sin8((uint8_t)((int)(t * 40) + f.ph)) / 6;
      CRGB c = scaled(CRGB(220, 235, 255), min(bright, tw));
      int xi = (int)f.x;
      int yi = (int)f.y;
      if (yi >= 0 && yi < 16) {
        fb[XY(xi, yi)] = c;
        if (f.size) fb[XY((xi + 1) & 15, min(15, yi))] += scaled(c, 120);
      }
    }
  }
};

static const CRGB GARLAND_PALETTE[8] = {
    CRGB(255, 44, 44),   CRGB(255, 146, 24), CRGB(255, 214, 64), CRGB(52, 205, 84),
    CRGB(48, 128, 255),  CRGB(176, 92, 255), CRGB(72, 224, 255), CRGB(255, 240, 214),
};

class GarlandEffect : public EffectBase {
 public:
  GarlandEffect() : EffectBase("garland", "Гирлянда", GRP_NY, false, true) {}
  void render(CRGB* fb, float, float t) override {
    fill_solid(fb, cfg::LED_COUNT, CRGB(3, 2, 0));
    const float sp = g.speed / 128.0f;
    const uint8_t ringY[3] = {3, 8, 13};
    for (uint8_t r = 0; r < 3; r++) {
      uint8_t y = ringY[r];
      for (uint8_t x = 0; x < 16; x++) {
        if (((x + r * 2) & 3) != 0) continue;
        // у каждой лампочки свой цвет, как в настоящей гирлянде
        uint8_t h = (uint8_t)(x * 197 + r * 91);
        h ^= (uint8_t)(h >> 4);
        h = (uint8_t)(h * 11);
        CRGB bulb = GARLAND_PALETTE[(h >> 1) & 7];
        uint8_t v = sin8((uint8_t)(int)(t * 45 * sp + x * 61 + r * 30));
        uint8_t k = 90 + v / 2;
        if (sin8((uint8_t)((int)(t * 90) + x * 33)) > 246) k = 255;
        fb[XY(x, y)] = scaled(bulb, k);
      }
    }
  }
};

class SpiralEffect : public EffectBase {
 public:
  SpiralEffect() : EffectBase("spiral", "Ёлочная спираль", GRP_NY, true, true) {}
  void render(CRGB* fb, float, float t) override {
    float sp = 0.4f + g.speed / 100.0f;
    for (uint8_t x = 0; x < 16; x++) {
      for (uint8_t y = 0; y < 16; y++) {
        uint8_t v = sin8((uint8_t)(x * 32 + y * 26 + (int)(t * 55 * sp)));
        if (v > 205) {
          fb[XY(x, y)] = scaled(colorNow(), (uint8_t)min(255, (v - 205) * 6 + 60));
        } else {
          fb[XY(x, y)] = scaled(CRGB(255, 150, 40), 12);
        }
      }
    }
  }
};

class CandyEffect : public EffectBase {
 public:
  CandyEffect() : EffectBase("candy", "Карамель", GRP_NY, false, true) {}
  void render(CRGB* fb, float, float t) override {
    float sp = 0.5f + g.speed / 100.0f;
    for (uint8_t x = 0; x < 16; x++) {
      for (uint8_t y = 0; y < 16; y++) {
        uint8_t v = sin8((uint8_t)(int)(x * 16 + y * 22 - t * 60 * sp));
        fb[XY(x, y)] = (v > 128) ? CRGB(235, 235, 240) : CRGB(210, 25, 40);
      }
    }
  }
};

class FireworksEffect : public EffectBase {
 protected:
  struct Part {
    float x, y, vx, vy, life;
    uint8_t hue;
    bool active;
  };
  static Part parts[110];
  static CRGB trail[cfg::LED_COUNT];
  static float spawnAcc;
  static float overlayScroll;
  bool withText;
  float localT = 0;  // время жизни эффекта: шоу стартует с первых секунд

  void fade() {
    for (int i = 0; i < cfg::LED_COUNT; i++) trail[i].nscale8_video(185);
  }
  void burst(uint8_t cx, uint8_t cy) {
    uint8_t hue = random8();
    int count = 14 + g.intensity / 14;
    for (int i = 0; i < 110 && count > 0; i++) {
      if (parts[i].active) continue;
      float a = random8() / 255.0f * 6.28318f;
      float sp = 1.5f + random8(0, 100) / 40.0f;
      parts[i] = {cx + 0.0f, (float)cy, cosf(a) * sp, sinf(a) * sp, 1.6f, (uint8_t)(hue + random8(20) - 10), true};
      count--;
    }
  }

 public:
  FireworksEffect(const char* id, const char* name, bool text)
      : EffectBase(id, name, GRP_NY, false, true, true), withText(text) {}
  FireworksEffect() : FireworksEffect("fireworks", "Салют", false) {}
  void enter() override {
    memset(parts, 0, sizeof(parts));
    memset(trail, 0, sizeof(trail));
    spawnAcc = 0.9f;
    localT = 0;
    overlayScroll = 0;
  }
  void render(CRGB* fb, float dt, float t) override {
    fade();
    localT += dt;
    // Шоу идёт в спокойном темпе: залпы реже, искры медленнее
    float sdt = withText ? dt * 0.55f : dt;
    spawnAcc += sdt;
    float interval = withText ? max(1.0f, 2.6f - g.intensity / 220.0f)
                              : max(0.35f, 1.5f - g.intensity / 300.0f);
    if (spawnAcc > interval) {
      spawnAcc = 0;
      burst(random8(16), random8(6, 14));
    }
    for (int i = 0; i < 110; i++) {
      Part& p = parts[i];
      if (!p.active) continue;
      p.vy -= sdt * 5.0f;
      p.vx *= withText ? 0.992f : 0.985f;
      p.x += p.vx * sdt * 6.0f;
      p.y += p.vy * sdt * 6.0f;
      p.life -= sdt * 0.55f;
      while (p.x < 0) p.x += 16;
      while (p.x >= 16) p.x -= 16;
      if (p.life <= 0 || p.y < 0 || p.y > 16.5f) {
        p.active = false;
        continue;
      }
      uint8_t k = (uint8_t)(min(1.0f, p.life) * 255);
      CRGB c = CHSV(p.hue, 220, k);
      int xi = ((int)p.x) & 15;
      int yi = (int)p.y;
      if (yi >= 0 && yi < 16) trail[XY(xi, yi)] += c;
    }
    memcpy(fb, trail, sizeof(trail));
    if (withText) {
      // Поздравление — одна медленная полная прокрутка (18 с), затем пауза на салют
      float cyc = fmodf(localT, 26.0f);
      if (cyc < 18.0f) {
        overlayScroll += dt * 10.0f;
        font5x7::drawScrollText(fb, NY_MSG, overlayScroll, CRGB(255, 205, 90), false, localT, 2, true);
      } else {
        overlayScroll = 0;
      }
    }
  }
};
FireworksEffect::Part FireworksEffect::parts[110];
CRGB FireworksEffect::trail[cfg::LED_COUNT];
float FireworksEffect::spawnAcc = 0;
float FireworksEffect::overlayScroll = 0;

class ClockEffect : public EffectBase {
  char buf[48] = "--:--";
  uint8_t lastSec = 255;
  float scroll = 0;

  void rebuild(const tm& tt) {
    int h = tt.tm_hour;
    if (g.clock.hour12) {
      h = h % 12;
      if (h == 0) h = 12;
    }
    char colon = (tt.tm_sec % 2) ? ' ' : ':';
    if (g.clock.showDate) {
      const char* wd = WD_RU[tt.tm_wday];
      snprintf(buf, sizeof(buf), "%02d%c%02d %s %02d.%02d ", h, colon, tt.tm_min, wd,
               tt.tm_mday, tt.tm_mon + 1);
    } else {
      snprintf(buf, sizeof(buf), "%02d%c%02d ", h, colon, tt.tm_min);
    }
  }

 public:
  ClockEffect() : EffectBase("clock", "Часы", GRP_TIME, true, true) {}
  void render(CRGB* fb, float dt, float t) override {
    fill_solid(fb, cfg::LED_COUNT, CRGB::Black);
    time_t now = time(nullptr);
    tm tt;
    localtime_r(&now, &tt);
    if (now < 1700000000) {
      font5x7::drawScrollText(fb, "--:-- ", scroll, colorNow(), false, t, 2, true);
      return;
    }
    if (tt.tm_sec != lastSec) {
      lastSec = tt.tm_sec;
      rebuild(tt);
    }
    // Крупный кегль — скорость ниже, чтобы строка читалась
    scroll += dt * (4.0f + g.clock.speed * 0.14f);
    font5x7::drawScrollText(fb, buf, scroll, colorNow(), g.clock.colorMode == 1, t, 2, true);
  }
};

class NyCountdownEffect : public EffectBase {
  char buf[48] = "  ";
  float scroll = 0;
  uint8_t lastMin = 255;

  void rebuild() {
    time_t now = time(nullptr);
    if (now < 1700000000) {
      snprintf(buf, sizeof(buf), "  ");
      return;
    }
    tm tt, tgt = {};
    localtime_r(&now, &tt);
    tgt.tm_year = tt.tm_year + 1;
    tgt.tm_mon = 0;
    tgt.tm_mday = 1;
    time_t target = mktime(&tgt);
    long delta = (long)(target - now);
    int days = delta / 86400;
    if (days > 0) {
      snprintf(buf, sizeof(buf), "\xC4\xCE \xCD\xC3 %d \xC4\xCD ", days);
    } else {
      int hh = (delta / 3600) % 24;
      int mm = (delta / 60) % 60;
      snprintf(buf, sizeof(buf), "\xC4\xCE \xCD\xC3 %02d:%02d ", hh, mm);
    }
  }

 public:
  NyCountdownEffect() : EffectBase("nycountdown", "До Нового года", GRP_TIME, true, true) {}
  void render(CRGB* fb, float dt, float t) override {
    fill_solid(fb, cfg::LED_COUNT, CRGB::Black);
    time_t now = time(nullptr);
    tm tt;
    localtime_r(&now, &tt);
    if (tt.tm_min != lastMin) {
      lastMin = tt.tm_min;
      rebuild();
    }
    scroll += dt * (4.0f + g.clock.speed * 0.14f);
    font5x7::drawScrollText(fb, buf, scroll, colorNow(), false, t, 2, true);
  }
};

// ---------------------------------------------------------------------------

SolidEffect iSolid;
BreathingEffect iBreathing;
CandleEffect iCandle;
AuroraEffect iAurora;
OceanEffect iOcean;
StarfieldEffect iStars;
RainbowEffect iRainbow;
PlasmaEffect iPlasma;
FireEffect iFire;
SnowEffect iSnow;
GarlandEffect iGarland;
SpiralEffect iSpiral;
CandyEffect iCandy;
FireworksEffect iFireworks;
ClockEffect iClock;
NyCountdownEffect iNyCountdown;
FireworksEffect iNyShow("nyshow", "Новогоднее шоу", true);

EffectBase* REGISTRY[] = {&iSolid,     &iCandle,   &iBreathing,  &iAurora, &iOcean,
                          &iStars,     &iRainbow,  &iPlasma,     &iFire,   &iSnow,
                          &iGarland,   &iSpiral,   &iCandy,      &iFireworks, &iClock,
                          &iNyCountdown, &iNyShow};

}  // namespace

int effectsCount() { return sizeof(REGISTRY) / sizeof(REGISTRY[0]); }

EffectBase* effectAt(int i) {
  if (i < 0 || i >= effectsCount()) return nullptr;
  return REGISTRY[i];
}

EffectBase* effectById(const char* id) {
  for (int i = 0; i < effectsCount(); i++) {
    if (strcmp(REGISTRY[i]->id, id) == 0) return REGISTRY[i];
  }
  return nullptr;
}
