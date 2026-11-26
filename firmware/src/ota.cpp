#include "ota.h"
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <Updater.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "net.h"

namespace ota {

enum Phase : uint8_t { P_IDLE, P_CONNECTING, P_DOWNLOADING, P_FINALIZING, P_DONE, P_FAILED };

static Phase phase = P_IDLE;
static Release rel;
static String errorText;
static uint8_t progressPct = 0;
static uint32_t doneAt = 0;
static WiFiClientSecure* tls = nullptr;
static HTTPClient* http = nullptr;
static uint32_t totalSize = 0;
static uint32_t doneSize = 0;
static uint8_t rxBuf[1024];

static void setPhase(Phase p) { phase = p; }

static void cleanup() {
  if (http) {
    http->end();
    delete http;
    http = nullptr;
  }
  if (tls) {
    delete tls;
    tls = nullptr;
  }
}

static void fail(const char* why) {
  errorText = why;
  if (Update.isRunning()) Update.end(false);  // abort: не применяем частичную прошивку
  cleanup();
  setPhase(P_FAILED);
  Serial.printf("[ota] failed: %s\n", why);
}

struct Version {
  int core[3];
  bool pre;
  String preTag;
};

static Version parseVer(const char* s) {
  Version v = {{0, 0, 0}, false, ""};
  if (*s == 'v' || *s == 'V') s++;
  for (int i = 0; i < 3; i++) {
    while (*s >= '0' && *s <= '9') {
      v.core[i] = v.core[i] * 10 + (*s - '0');
      s++;
    }
    if (*s == '.') {
      s++;
    } else {
      break;
    }
  }
  if (*s == '-') {
    v.pre = true;
    v.preTag = s + 1;
  }
  return v;
}

static bool verNewer(const char* latest, const char* current) {
  Version a = parseVer(latest);
  Version b = parseVer(current);
  for (int i = 0; i < 3; i++) {
    if (a.core[i] != b.core[i]) return a.core[i] > b.core[i];
  }
  if (a.pre != b.pre) return !a.pre;  // стабильный релиз новее предрелиза
  return a.pre && a.preTag != b.preTag;
}

static void startDownload() {
  if (!net::connected() || rel.url.length() == 0) {
    fail("нет данных о релизе");
    return;
  }
  if (Update.isRunning()) {
    fail("занято");
    return;
  }
  tls = new WiFiClientSecure();
  if (!tls) {
    fail("нет памяти");
    return;
  }
  tls->setInsecure();  // без корневых сертификатов: экономим RAM
  tls->setBufferSizes(1024, 1024);
  http = new HTTPClient();
  if (!http) {
    fail("нет памяти");
    return;
  }
  http->setTimeout(15000);
  http->setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http->setReuse(false);
  if (!http->begin(*tls, rel.url)) {
    fail("http begin");
    return;
  }
  http->addHeader("User-Agent", "lampa-fw");
  int code = http->GET();
  if (code != HTTP_CODE_OK) {
    fail("http код");
    return;
  }
  int len = http->getSize();
  if (len <= 0) len = (int)rel.size;
  if (len <= 0) {
    fail("нет размера");
    return;
  }
  if (!Update.begin((size_t)len, U_FLASH)) {
    fail("flash begin");
    return;
  }
  totalSize = (uint32_t)len;
  doneSize = 0;
  progressPct = 0;
  setPhase(P_DOWNLOADING);
  Serial.printf("[ota] downloading %lu bytes\n", (unsigned long)totalSize);
}

static void readChunk() {
  if (!http || !http->getStreamPtr()) {
    fail("поток");
    return;
  }
  uint32_t t0 = millis();
  while (millis() - t0 < 25) {
    int n = http->getStreamPtr()->readBytes(rxBuf, sizeof(rxBuf));
    if (n <= 0) {
      fail("обрыв загрузки");
      return;
    }
    if (Update.write(rxBuf, n) != (size_t)n) {
      fail("ошибка записи");
      return;
    }
    doneSize += n;
    uint32_t pct = (uint64_t)doneSize * 100 / totalSize;
    progressPct = (uint8_t)(pct > 99 ? 99 : pct);
    if (doneSize >= totalSize) {
      setPhase(P_FINALIZING);
      return;
    }
  }
}

static void finishInstall() {
  if (!Update.end(true) || Update.hasError()) {
    fail("проверка прошивки");
    return;
  }
  cleanup();
  progressPct = 100;
  doneAt = millis();
  setPhase(P_DONE);
  Serial.println("[ota] firmware installed, rebooting");
}

void begin() {
  phase = P_IDLE;
  rel = Release();
}

bool checkNow() {
  rel = Release();
  rel.checked = true;
  if (!net::connected()) {
    rel.error = "нет сети";
    return false;
  }
  Serial.printf("[ota] check start, heap=%u\n", ESP.getFreeHeap());
  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(1024, 1024);
  HTTPClient http;
  http.setTimeout(10000);
  http.setReuse(false);
  String url = String("https://api.github.com/repos/") + cfg::GH_REPO + "/releases?per_page=5";
  if (!http.begin(client, url)) {
    rel.error = "https";
    return false;
  }
  http.addHeader("User-Agent", "lampa-fw");
  http.addHeader("Accept", "application/vnd.github+json");
  int code = http.GET();
  Serial.printf("[ota] check GET code=%d, heap=%u\n", code, ESP.getFreeHeap());
  if (code != HTTP_CODE_OK) {
    rel.error = String("GitHub: код ") + code;
    http.end();
    return false;
  }
  JsonDocument filter;
  JsonObject f = filter[0].to<JsonObject>();
  f["draft"] = true;
  f["tag_name"] = true;
  f["body"] = true;
  f["assets"][0]["name"] = true;
  f["assets"][0]["browser_download_url"] = true;
  f["assets"][0]["size"] = true;
  JsonDocument doc;
  DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    rel.error = "ответ GitHub";
    return false;
  }
  for (JsonObject r : doc.as<JsonArray>()) {
    if (r["draft"] | false) continue;
    const char* tag = r["tag_name"] | "";
    if (!tag[0]) continue;
    rel.tag = tag;
    rel.notes = (const char*)(r["body"] | "");
    if (rel.notes.length() > 500) rel.notes = rel.notes.substring(0, 500);
    for (JsonObject a : r["assets"].as<JsonArray>()) {
      const char* name = a["name"] | "";
      if (strcmp(name, cfg::GH_FW_ASSET) == 0) {
        rel.url = (const char*)(a["browser_download_url"] | "");
        rel.size = a["size"] | 0;
      }
    }
    break;
  }
  if (rel.tag.length() == 0) {
    rel.error = "релизов нет";
    return false;
  }
  rel.available = rel.url.length() > 0 && verNewer(rel.tag.c_str(), cfg::FW_VERSION);
  return true;
}

const Release& release() { return rel; }

bool startInstall() {
  if (busy()) return false;
  if (phase == P_DONE) return false;
  errorText = "";
  progressPct = 0;
  setPhase(P_CONNECTING);
  return true;
}

void pump() {
  switch (phase) {
    case P_CONNECTING:
      startDownload();
      break;
    case P_DOWNLOADING:
      readChunk();
      break;
    case P_FINALIZING:
      finishInstall();
      break;
    case P_DONE:
      if (doneAt && millis() - doneAt > 1200) ESP.restart();
      break;
    default:
      break;
  }
}

bool busy() { return phase == P_CONNECTING || phase == P_DOWNLOADING || phase == P_FINALIZING; }

uint8_t progress() { return progressPct; }

const char* phaseName() {
  switch (phase) {
    case P_CONNECTING:
      return "connecting";
    case P_DOWNLOADING:
      return "downloading";
    case P_FINALIZING:
      return "finalizing";
    case P_DONE:
      return "done";
    case P_FAILED:
      return "error";
    default:
      return "idle";
  }
}

const String& lastError() { return errorText; }

}  // namespace ota
