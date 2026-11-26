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

// Тёмный акцентный цвет поднимаем к видимому уровню, сохраняя оттенок
inline CRGB accentLift() {
  CRGB c = colorNow();
  const uint8_t m = max3(c);
  if (m < 110) {
    const uint8_t add = (uint8_t)((110 - m) / 2);
    c.r = qadd8(c.r, add);
    c.g = qadd8(c.g, add);
    c.b = qadd8(c.b, add);
  }
  return c;
}

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

class MatrixEffect : public EffectBase {
  struct Column {
    float y;
    float spd;
    uint8_t len;
    uint8_t seed;
  };
  Column cols[16];
  bool init = false;

 public:
  MatrixEffect() : EffectBase("matrix", "Матричный дождь", GRP_ATMO, false, true, true) {}
  void render(CRGB* fb, float dt, float) override {
    if (!init) {
      for (uint8_t x = 0; x < 16; x++) {
        cols[x] = {random8(0, 48) / 2.0f - 8.0f, 3.5f + random8(0, 60) / 10.0f,
                   (uint8_t)(4 + random8(0, 6)), random8()};
      }
      init = true;
    }
    fill_solid(fb, cfg::LED_COUNT, CRGB::Black);
    const float sf = 0.4f + g.speed / 128.0f;
    const uint8_t ib = (uint8_t)(190 + g.intensity / 3);
    for (uint8_t x = 0; x < 16; x++) {
      Column& c = cols[x];
      const uint8_t fl = (uint8_t)(215 + random8(0, 40));
      for (uint8_t k = 0; k < c.len; k++) {
        const int yi = (int)c.y + k;
        if (yi < 0 || yi >= 16) continue;
        uint8_t kk = k == 0 ? 255 : (uint8_t)(235 - k * 25);
        kk = scale8(kk, scale8(fl, ib));
        const CRGB col = k == 0 ? CRGB(185, 255, 200)
                                : CHSV((uint8_t)(96 + (c.seed & 7)), 255, 255);
        fb[XY(x, (uint8_t)yi)] = scaled(col, kk);
      }
      c.y -= c.spd * dt * sf;
      if (c.y < -(float)c.len - 2.0f) {
        c.y = 16.0f + random8(0, 10) / 2.0f;
        c.spd = 3.5f + random8(0, 60) / 10.0f;
        c.len = (uint8_t)(4 + random8(0, 6));
        c.seed = random8();
      }
    }
  }
};

class HelixEffect : public EffectBase {
 public:
  HelixEffect() : EffectBase("helix", "Спираль", GRP_ATMO, true, true) {}
  void render(CRGB* fb, float, float t) override {
    const float sp = 0.4f + g.speed / 110.0f;
    const int off = (int)(t * 14 * sp);
    CRGB c1 = colorNow();
    c1.r = qadd8(c1.r, 70);
    c1.g = qadd8(c1.g, 70);
    c1.b = qadd8(c1.b, 70);
    const CRGB c2 = CRGB(255, 242, 214);
    const CRGB c1dim = scaled(c1, 80);
    const CRGB c2dim = scaled(c2, 80);
    for (uint8_t x = 0; x < 16; x++) {
      for (uint8_t y = 0; y < 16; y++) {
        const uint8_t v = (uint8_t)((x + y + off) & 15);
        CRGB col = CRGB(5, 4, 3);
        if (v == 0) col = c1;
        else if (v == 8) col = c2;
        // соседние диоды — мягкое свечение нити
        else if (v == 1 || v == 15) col = c1dim;
        else if (v == 7 || v == 9) col = c2dim;
        fb[XY(x, y)] = col;
      }
    }
  }
};

class RingsEffect : public EffectBase {
 public:
  RingsEffect() : EffectBase("rings", "Кольца", GRP_ATMO, true, true) {}
  void render(CRGB* fb, float, float t) override {
    const float sp = 0.4f + g.speed / 110.0f;
    const CRGB base = accentLift();
    const int ph = (int)(t * 66 * sp);
    for (uint8_t x = 0; x < 16; x++) {
      const uint8_t wob = (uint8_t)((x * 5) & 7);
      for (uint8_t y = 0; y < 16; y++) {
        const uint8_t d = (uint8_t)(abs((int)y * 2 - 15) * 9);
        const uint8_t v = sin8((uint8_t)(d * 2 - ph + wob));
        CRGB col;
        if (v > 205) col = CRGB(255, 240, 215);  // гребень кольца
        else col = scaled(base, qadd8(10, v));
        fb[XY(x, y)] = col;
      }
    }
  }
};

class BeaconEffect : public EffectBase {
 public:
  BeaconEffect() : EffectBase("beacon", "Маяк", GRP_ATMO, false, true, true) {}
  void render(CRGB* fb, float, float t) override {
    const float sp = 0.3f + g.speed / 100.0f;
    const float beam = fmodf(t * 2.4f * sp, 16.0f);
    const uint8_t ib = (uint8_t)(200 + g.intensity / 3);
    for (uint8_t x = 0; x < 16; x++) {
      float d = fabsf((float)x - beam);
      d = min(d, 16.0f - d);
      uint8_t k = d < 1.0f ? 255 : d < 2.2f ? 130 : d < 3.4f ? 44 : 10;
      k = scale8(k, ib);
      for (uint8_t y = 0; y < 16; y++) {
        const uint8_t ky = scale8(k, (uint8_t)(246 - y * 5));
        fb[XY(x, y)] = scaled(CRGB(255, 248, 230), ky);
      }
    }
  }
};

class LightningEffect : public EffectBase {
  float localT = 0;
  float nextAt = 1.2f;
  float flashAt = -10.0f;
  uint8_t bolt[16];

  void strike() {
    flashAt = localT;
    uint8_t x = random8(2, 14);
    for (uint8_t y = 0; y < 16; y++) {
      bolt[y] = x;
      x = (uint8_t)constrain((int)x + (int8_t)random8(3) - 1, 0, 15);
    }
  }

 public:
  LightningEffect() : EffectBase("lightning", "Молния", GRP_ATMO, false, true, true) {}
  void enter() override {
    localT = 0;
    nextAt = 1.2f;
    flashAt = -10.0f;
  }
  void render(CRGB* fb, float dt, float) override {
    localT += dt;
    const float iv = max(1.6f, 7.0f - g.speed / 55.0f);
    if (localT > nextAt) {
      strike();
      nextAt = localT + iv + random8(0, 30) / 10.0f;
    }
    const float e = localT - flashAt;
    float kf = e >= 0.0f ? expf(-e * 6.0f) : 0.0f;
    if (e >= 0.10f) kf = max(kf, expf(-(e - 0.10f) * 8.0f) * 0.85f);
    if (e >= 0.0f) kf = max(kf, 0.12f * expf(-e * 2.2f));  // послесвечение
    const uint8_t k = (uint8_t)(min(1.0f, kf) * min(255.0f, 190.0f + g.intensity / 2));
    const uint8_t amb = (uint8_t)(k / 6);
    for (uint8_t y = 0; y < 16; y++) {
      const uint8_t bx = bolt[y];
      for (uint8_t x = 0; x < 16; x++) {
        if (x == bx && k > 8) {
          const uint8_t v = qadd8(amb, k);
          fb[XY(x, y)] = CRGB(v, v, qadd8(v, 20));  // ядро молнии — белое
        } else if (k > 150 && (x == (uint8_t)((bx + 1) & 15) || x == (uint8_t)((bx + 15) & 15))) {
          const uint8_t v = qadd8(amb, k / 3);
          fb[XY(x, y)] = CRGB(v, v, v);
        } else {
          fb[XY(x, y)] = CRGB(amb / 8, amb / 6, amb);  // голубое небо
        }
      }
    }
  }
};

class HypnoEffect : public EffectBase {
 public:
  HypnoEffect() : EffectBase("hypno", "Гипноз", GRP_ATMO, false, true, true) {}
  void render(CRGB* fb, float, float t) override {
    const float sp = 0.4f + g.speed / 120.0f;
    const int spin = (int)(t * 46 * sp);
    const uint8_t ib = (uint8_t)min(255.0f, 205.0f + g.intensity / 2);
    for (uint8_t x = 0; x < 16; x++) {
      for (uint8_t y = 0; y < 16; y++) {
        const uint8_t d = (uint8_t)(abs((int)y * 2 - 15) * 8);
        const uint8_t v = sin8((uint8_t)(x * 32 + d + spin));
        const uint8_t k = (uint8_t)((uint16_t)v * ib / 255);
        const uint8_t hue = (uint8_t)(150 + d / 2 + spin / 2);
        fb[XY(x, y)] = v > 128 ? CHSV(hue, 230, k) : CHSV(hue, 230, 12);
      }
    }
  }
};

class HeartbeatEffect : public EffectBase {
 public:
  HeartbeatEffect() : EffectBase("heartbeat", "Сердцебиение", GRP_ATMO, false, true, true) {}
  void render(CRGB* fb, float, float t) override {
    const float period = 1.35f - (g.speed / 255.0f) * 0.55f;
    const float p = fmodf(t, period) / period;
    float beat = p < 0.16f ? 1.0f - p / 0.16f : 0.0f;
    if (p > 0.22f && p < 0.40f) beat = max(beat, (1.0f - (p - 0.22f) / 0.18f) * 0.8f);
    const uint8_t peak = (uint8_t)(min(255.0f, (160.0f + g.intensity * 0.75f) * beat));
    const CRGB core = CRGB(255, 42, 54);
    for (uint8_t y = 0; y < 16; y++) {
      const uint8_t w = (uint8_t)(255 - abs((int)y * 2 - 15) * 10);
      const uint8_t k = qadd8(scale8(w, peak), 6);
      for (uint8_t x = 0; x < 16; x++) {
        fb[XY(x, y)] = scaled(core, k);
      }
    }
  }
};

class BubblesEffect : public EffectBase {
  struct Bubble {
    float x;
    float y;
    float vy;
    uint8_t ph;
    uint8_t big;
  };
  Bubble bs[30];
  bool init = false;

 public:
  BubblesEffect() : EffectBase("bubbles", "Пузырьки", GRP_ATMO, true, true, true) {}
  void render(CRGB* fb, float dt, float t) override {
    if (!init) {
      for (uint8_t i = 0; i < 30; i++) {
        bs[i] = {random8(0, 160) / 10.0f, random8(0, 160) / 10.0f,
                 1.2f + random8(0, 30) / 10.0f, random8(), (uint8_t)(random8(4) == 0)};
      }
      init = true;
    }
    fill_solid(fb, cfg::LED_COUNT, CRGB(1, 2, 6));
    const float sp = 0.4f + g.speed / 120.0f;
    CRGB base = colorNow();
    base.r = qadd8(base.r, 140);
    base.g = qadd8(base.g, 140);
    base.b = qadd8(base.b, 140);
    const uint8_t ib = (uint8_t)(210 + g.intensity / 3);
    for (uint8_t i = 0; i < 30; i++) {
      Bubble& b = bs[i];
      b.y += b.vy * dt * sp;
      b.x += sinf(t * 1.7f + b.ph) * dt * 0.9f;
      if (b.y > 16.6f) {
        b.y = -1.0f;
        b.x = random8(0, 160) / 10.0f;
        b.vy = 1.2f + random8(0, 30) / 10.0f;
        b.big = (uint8_t)(random8(4) == 0);
      }
      while (b.x < 0) b.x += 16.0f;
      while (b.x >= 16.0f) b.x -= 16.0f;
      const int xi = (int)b.x;
      const int yi = (int)b.y;
      if (yi < 0 || yi >= 16) continue;
      const uint8_t pulse = qadd8(205, sin8((uint8_t)((int)(t * 55) + b.ph)) / 5);
      const uint8_t k = scale8(pulse, ib);
      fb[XY((uint8_t)xi, (uint8_t)yi)] = scaled(base, k);
      if (b.big) {
        const CRGB dim = scaled(base, k / 2);
        if (xi + 1 < 16) fb[XY((uint8_t)(xi + 1), (uint8_t)yi)] = dim;
        if (yi + 1 < 16) fb[XY((uint8_t)xi, (uint8_t)(yi + 1))] = dim;
        if (xi + 1 < 16 && yi + 1 < 16)
          fb[XY((uint8_t)(xi + 1), (uint8_t)(yi + 1))] = scaled(base, k / 3);
        if (yi > 0) fb[XY((uint8_t)xi, (uint8_t)(yi - 1))] = scaled(CRGB(235, 245, 255), k);
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
    fill_solid(fb, cfg::LED_COUNT, CRGB(5, 3, 1));
    const float sp = g.speed / 128.0f;
    // Восемь нитей через всю матрицу: лампочки в шахматном порядке,
    // у каждой свой цвет, плюс мягкое свечение на соседних диодах нити.
    for (uint8_t r = 0; r < 8; r++) {
      const uint8_t y0 = (uint8_t)(1 + r * 2);
      for (uint8_t j = 0; j < 8; j++) {
        const uint8_t x = (uint8_t)((j * 2 + r) & 15);
        const uint8_t y = (uint8_t)(y0 - ((j + r) & 1));  // лёгкий провис нити
        uint8_t h = (uint8_t)(x * 197 + y * 89 + r * 61);
        h ^= (uint8_t)(h >> 4);
        h = (uint8_t)(h * 11);
        const CRGB bulb = GARLAND_PALETTE[(h >> 1) & 7];
        const uint8_t ph = (uint8_t)(x * 61 + y * 23);
        const uint8_t v = sin8((uint8_t)(int)(t * 40 * sp + ph));
        uint8_t k = qadd8(135, v / 2);
        if (sin8((uint8_t)((int)(t * 90) + ph * 3)) > 246) k = 255;
        fb[XY(x, y)] = scaled(bulb, k);
        const CRGB glow = scaled(bulb, 90);
        fb[XY((uint8_t)((x + 15) & 15), y)] += glow;
        fb[XY((uint8_t)((x + 1) & 15), y)] += glow;
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

class ConfettiEffect : public EffectBase {
  struct Piece {
    float x;
    float y;
    float vy;
    uint8_t hue;
    uint8_t ph;
  };
  Piece ps[44];
  bool init = false;

 public:
  ConfettiEffect() : EffectBase("confetti", "Конфетти", GRP_NY, false, true, true) {}
  void render(CRGB* fb, float dt, float t) override {
    if (!init) {
      for (uint8_t i = 0; i < 44; i++) {
        ps[i] = {random8(0, 160) / 10.0f, random8(0, 160) / 10.0f,
                 1.6f + random8(0, 26) / 8.0f, random8(), random8()};
      }
      init = true;
    }
    fill_solid(fb, cfg::LED_COUNT, CRGB(2, 1, 3));
    const float sp = 0.4f + g.speed / 128.0f;
    const uint8_t ib = (uint8_t)(200 + g.intensity / 3);
    for (uint8_t i = 0; i < 44; i++) {
      Piece& p = ps[i];
      p.y -= p.vy * dt * sp;
      p.x += sinf(t * 2.1f + p.ph) * dt * 1.4f;
      if (p.y < -1.0f) {
        p.y = 16.4f;
        p.x = random8(0, 160) / 10.0f;
        p.hue = random8();
      }
      while (p.x < 0) p.x += 16.0f;
      while (p.x >= 16.0f) p.x -= 16.0f;
      const int xi = (int)p.x;
      const int yi = (int)p.y;
      if (yi < 0 || yi >= 16) continue;
      const uint8_t tw = sin8((uint8_t)((int)(t * 100) + p.ph));
      const uint8_t k = scale8(ib, (uint8_t)(165 + tw / 3));
      const CRGB col = scaled(CHSV(p.hue, 215, 255), k);
      fb[XY((uint8_t)xi, (uint8_t)yi)] = col;
      if (yi + 1 < 16) fb[XY((uint8_t)xi, (uint8_t)(yi + 1))] = scaled(col, 140);
    }
  }
};

class SnakeEffect : public EffectBase {
  uint8_t body[192];
  uint8_t food = 0;
  uint8_t len = 0;
  uint8_t dir = 0;  // 0 вправо, 1 вниз, 2 влево, 3 вверх
  float acc = 0;
  bool init = false;

  static uint8_t shifted(uint8_t cell, uint8_t d) {
    uint8_t x = cell & 15, y = cell >> 4;
    switch (d) {
      case 0: x = (x + 1) & 15; break;
      case 1: y = (y + 1) & 15; break;
      case 2: x = (x + 15) & 15; break;
      default: y = (y + 15) & 15; break;
    }
    return (uint8_t)(y * 16 + x);
  }
  bool inBody(uint8_t cell) const {
    for (uint8_t i = 0; i < len; i++) {
      if (body[i] == cell) return true;
    }
    return false;
  }
  void placeFood() {
    for (uint8_t tries = 0; tries < 60; tries++) {
      const uint8_t c = random8();
      if (!inBody(c)) {
        food = c;
        return;
      }
    }
    food = random8();
  }
  void reset() {
    len = 5;
    for (uint8_t i = 0; i < len; i++) body[i] = (uint8_t)(8 * 16 + 6 - i);
    dir = 0;
    placeFood();
    acc = 0;
  }
  void moveStep() {
    const uint8_t head = body[0];
    const uint8_t fx = food & 15, fy = food >> 4;
    int8_t best = -1;
    uint16_t bestScore = 0xFFFF;
    for (uint8_t d = 0; d < 4; d++) {
      const uint8_t c = shifted(head, d);
      if (inBody(c)) continue;
      const uint8_t cx = c & 15, cy = c >> 4;
      uint16_t score =
          (uint16_t)ringDist(cx, fx) * 3 + (uint16_t)(cy > fy ? cy - fy : fy - cy) * 3;
      if (d == ((dir + 2) & 3)) score += 8;  // не разворачиваться в шею
      score += random8(0, 3);
      if (score < bestScore) {
        bestScore = score;
        best = (int8_t)d;
      }
    }
    if (best < 0) {
      reset();
      return;
    }
    dir = (uint8_t)best;
    const uint8_t next = shifted(head, dir);
    const bool grow = next == food && len < 192;
    if (grow) len++;
    memmove(body + 1, body, len - 1);
    body[0] = next;
    if (grow) {
      placeFood();
      if (len >= 190) reset();
    }
  }

 public:
  SnakeEffect() : EffectBase("snake", "Змейка", GRP_NY, false, true, true) {}
  void enter() override { init = false; }
  void render(CRGB* fb, float dt, float t) override {
    if (!init) {
      reset();
      init = true;
    }
    const float rate = 3.0f + g.speed / 24.0f;
    acc += dt * rate;
    while (acc >= 1.0f) {
      moveStep();
      acc -= 1.0f;
    }
    fill_solid(fb, cfg::LED_COUNT, CRGB(2, 3, 8));
    const uint8_t pulse = qadd8(180, sin8((uint8_t)((int)(t * 130))) / 4);
    fb[food] = scaled(CRGB(255, 44, 50), pulse);
    const uint8_t ib = (uint8_t)min(255.0f, 200.0f + g.intensity / 5);
    for (int16_t i = (int16_t)len - 1; i > 0; i--) {
      const uint8_t k =
          scale8(ib, (uint8_t)(170 + (uint16_t)(len - 1 - i) * 85 / (len > 1 ? len - 1 : 1)));
      const uint8_t hue = (uint8_t)(i * 9 + (int)(t * 40));
      fb[body[i]] = scaled(CHSV(hue, 200, 255), k);
    }
    fb[body[0]] = scaled(CRGB(240, 255, 245), ib);
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
MatrixEffect iMatrix;
HelixEffect iHelix;
RingsEffect iRings;
BeaconEffect iBeacon;
LightningEffect iLightning;
HypnoEffect iHypno;
HeartbeatEffect iHeartbeat;
BubblesEffect iBubbles;
FireEffect iFire;
SnowEffect iSnow;
GarlandEffect iGarland;
SpiralEffect iSpiral;
CandyEffect iCandy;
ConfettiEffect iConfetti;
SnakeEffect iSnake;
FireworksEffect iFireworks;
ClockEffect iClock;
NyCountdownEffect iNyCountdown;
FireworksEffect iNyShow("nyshow", "Новогоднее шоу", true);

EffectBase* REGISTRY[] = {&iSolid,   &iCandle,  &iBreathing,  &iAurora,    &iOcean,
                          &iStars,   &iRainbow, &iPlasma,     &iMatrix,    &iHelix,
                          &iRings,   &iBeacon,  &iLightning,  &iHypno,     &iHeartbeat,
                          &iBubbles, &iFire,    &iSnow,       &iGarland,   &iSpiral,
                          &iCandy,   &iConfetti, &iSnake,     &iFireworks, &iClock,
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
