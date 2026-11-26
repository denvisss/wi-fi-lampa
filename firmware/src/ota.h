#pragma once
#include <Arduino.h>

namespace ota {

struct Release {
  bool checked = false;
  bool available = false;
  String tag;      // v1.1.0-pre1
  String url;      // browser_download_url последнего firmware.bin
  uint32_t size = 0;
  String notes;
  String error;
};

void begin();

// Синхронная проверка релизов на GitHub (несколько секунд)
bool checkNow();
const Release& release();

// Установка асинхронная: startInstall() -> pump() в каждом web::loop()
bool startInstall();
void pump();
bool busy();
uint8_t progress();
const char* phaseName();  // idle|connecting|downloading|finalizing|done|error
const String& lastError();

}  // namespace ota
