#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include "config.h"
#include "store.h"

uint16_t logicalToPhysical(uint8_t x, uint8_t y);
CRGB parseColorHex(const char* s);

inline uint16_t XY(uint8_t x, uint8_t y) {
  return (uint16_t)y * cfg::MATRIX_W + x;
}

inline uint8_t ringDist(uint8_t a, uint8_t b) {
  int d = abs((int)a - (int)b);
  return (uint8_t)min(d, cfg::MATRIX_W - d);
}
