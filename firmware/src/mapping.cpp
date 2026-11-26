#include "mapping.h"

uint16_t logicalToPhysical(uint8_t x, uint8_t y) {
  const MappingCfg& m = g.mapping;
  uint8_t yy = m.flipY ? (cfg::MATRIX_H - 1 - y) : y;
  uint8_t xx = (uint8_t)((x + m.offset) & (cfg::MATRIX_W - 1));
  if (m.reverse) xx = cfg::MATRIX_W - 1 - xx;
  if (m.serpentine && (yy & 1)) xx = cfg::MATRIX_W - 1 - xx;
  return (uint16_t)yy * cfg::MATRIX_W + xx;
}

CRGB parseColorHex(const char* s) {
  if (!s) return CRGB::White;
  while (*s == ' ') s++;
  if (*s == '#') s++;
  uint32_t v = strtoul(s, nullptr, 16);
  return CRGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}
