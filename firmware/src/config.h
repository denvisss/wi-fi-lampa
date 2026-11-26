#pragma once
#include <Arduino.h>

namespace cfg {

constexpr uint8_t PIN_LED = 2;    // D4 -> DIN матрицы
constexpr uint8_t PIN_TOUCH = 4;  // D2 -> OUT TTP223

constexpr uint16_t MATRIX_W = 16;
constexpr uint16_t MATRIX_H = 16;
constexpr uint16_t LED_COUNT = MATRIX_W * MATRIX_H;

constexpr uint16_t DEFAULT_CURRENT_LIMIT_MA = 9000;
constexpr uint16_t MIN_CURRENT_LIMIT_MA = 500;
constexpr uint16_t MAX_CURRENT_LIMIT_MA = 15000;  // 256 LED × 60 мА ≈ 15.4 А
constexpr uint16_t LED_MA_FULL = 60;

constexpr const char* FW_VERSION = "1.3.0";
constexpr const char* GH_REPO = "denvisss/wi-fi-lampa";
constexpr const char* GH_FW_ASSET = "firmware.bin";
constexpr const char* HOSTNAME = "lampa";
constexpr const char* AP_SSID = "Lampa-Setup";

constexpr uint8_t DEFAULT_BRIGHTNESS = 160;
constexpr const char* DEFAULT_EFFECT = "aurora";
constexpr const char* DEFAULT_COLOR = "#FFB066";
constexpr const char* DEFAULT_TZ = "Europe/Moscow";

constexpr uint8_t MAX_ALARMS = 4;
constexpr uint8_t WS_PORT = 81;

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 25000;
constexpr uint32_t AP_GRACE_MS = 15000;
constexpr uint32_t NTP_RESYNC_MS = 6UL * 60UL * 60UL * 1000UL;
constexpr uint32_t STATE_PUSH_MS = 15000;

constexpr float CROSSFADE_SEC = 0.8f;
constexpr uint32_t NY_SHOW_MS = 120000;

}  // namespace cfg

// Реализация в web.cpp — рассылает состояние всем клиентам
void notifyState();

