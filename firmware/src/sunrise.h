#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

namespace sunrise {
void begin();
void loopTick();
bool render(CRGB* fb, float dt);
bool active();
bool snoozing();
float progress();
void test();
void stop();
void snooze();
int8_t alarmId();
}  // namespace sunrise
