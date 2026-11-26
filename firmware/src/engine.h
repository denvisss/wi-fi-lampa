#pragma once
#include <FastLED.h>
#include "config.h"

namespace engine {

extern CRGB leds[cfg::LED_COUNT];

void begin();
void tick(float dt);

void setPower(bool on);
void togglePower();
bool powerOn();
void setBrightness(uint8_t v);
void setCurrentLimit(uint16_t ma);

bool setEffect(const char* id, bool instant = false);
void nextEffect();
const char* currentId();

void calibrate(const char* pattern, uint16_t ms);
bool calibrating();

void startNyShow();
bool nyShowActive();
void checkNewYear();

void previewCopy(uint8_t* rgbOut);

}  // namespace engine
