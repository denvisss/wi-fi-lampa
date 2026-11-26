#pragma once
#include <FastLED.h>
#include "config.h"

enum EffectGroup : uint8_t { GRP_ATMO, GRP_NY, GRP_TIME };

class EffectBase {
 public:
  const char* id;
  const char* name;
  EffectGroup group;
  bool useColor;
  bool useSpeed;
  bool useIntensity;

  EffectBase(const char* id, const char* name, EffectGroup group, bool color = false,
             bool speed = true, bool intensity = false)
      : id(id), name(name), group(group), useColor(color), useSpeed(speed), useIntensity(intensity) {}
  virtual ~EffectBase() {}
  virtual void enter() {}
  virtual void render(CRGB* fb, float dt, float t) = 0;
};

int effectsCount();
EffectBase* effectAt(int i);
EffectBase* effectById(const char* id);
