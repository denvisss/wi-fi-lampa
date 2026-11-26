#pragma once
#include <Arduino.h>

namespace net {
void begin();
void loop();
bool connected();
bool apMode();
String ip();
int8_t rssi();
bool timeValid();
bool localTime(tm& out);
String isoTime();
String scanJson();
void saveWifi(const char* ssid, const char* pass);
void applyTz();

// Часовые пояса: IANA-имя из встроенной таблицы или POSIX-строка как есть
bool tzValid(const char* tz);
bool tzToPosix(const char* tz, char* out, size_t n);
size_t tzCount();
typedef void (*TzVisitor)(const char* name, size_t len, void* ctx);
void tzVisit(TzVisitor fn, void* ctx);

// Ручное время: SNTP останавливается, пока пользователь не вернёт автосинхронизацию
void setManualTime(uint32_t epoch);
void resumeAutoTime();
}  // namespace net
