#include "net.h"
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <sntp.h>
#include <time.h>
#include "config.h"
#include "store.h"
#include "tzdata.h"

namespace net {

static bool ap = false;
static bool staStarted = false;
static bool ntpStarted = false;
static bool timeoutLogged = false;
static uint32_t connectDeadline = 0;
static uint32_t apStopAt = 0;
static uint32_t lastNtp = 0;
static uint32_t lastApCheck = 0;
static DNSServer dns;

static void startAP() {
  Serial.println("[net] starting setup AP");
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  delay(40);
  // STA нужен даже в режиме настройки: без него не работает скан сетей в портале
  WiFi.mode(WIFI_AP_STA);
  delay(40);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  bool ok = WiFi.softAP(cfg::AP_SSID, nullptr, 1, 0, 4);
  if (!ok || WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
    Serial.println("[net] softAP failed, retrying");
    WiFi.mode(WIFI_OFF);
    delay(150);
    WiFi.mode(WIFI_AP_STA);
    delay(40);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                      IPAddress(255, 255, 255, 0));
    ok = WiFi.softAP(cfg::AP_SSID, nullptr, 1, 0, 4);
  }
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  dns.stop();
  dns.start(53, "*", WiFi.softAPIP());
  MDNS.begin(cfg::HOSTNAME);
  MDNS.addService("http", "tcp", 80);
  ap = true;
  apStopAt = 0;
  lastApCheck = millis();
  Serial.printf("[net] AP '%s' %s ip=%s mac=%s\n", cfg::AP_SSID, ok ? "up" : "FAILED",
                WiFi.softAPIP().toString().c_str(), WiFi.softAPmacAddress().c_str());
}

static void stopAP() {
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  MDNS.end();
  MDNS.begin(cfg::HOSTNAME);
  MDNS.addService("http", "tcp", 80);
  ap = false;
  apStopAt = 0;
  Serial.println("[net] AP stopped, station online");
}

static bool tzFromTable(const char* name, char* out, size_t n) {
  size_t len = strlen(name);
  const char* p = TZ_TABLE;
  while (pgm_read_byte(p)) {
    const char* lineEnd = p;
    while (pgm_read_byte(lineEnd) && pgm_read_byte(lineEnd) != '\n') lineEnd++;
    const char* tab = nullptr;
    for (const char* q = p; q < lineEnd; q++) {
      if (pgm_read_byte(q) == '\t') {
        tab = q;
        break;
      }
    }
    if (tab && (size_t)(tab - p) == len) {
      bool same = true;
      for (size_t i = 0; i < len; i++) {
        if ((char)pgm_read_byte(p + i) != name[i]) {
          same = false;
          break;
        }
      }
      if (same) {
        size_t vlen = lineEnd - tab - 1;
        if (vlen >= n) vlen = n - 1;
        for (size_t i = 0; i < vlen; i++) out[i] = (char)pgm_read_byte(tab + 1 + i);
        out[vlen] = 0;
        return true;
      }
    }
    if (!pgm_read_byte(lineEnd)) break;
    p = lineEnd + 1;
  }
  return false;
}

bool tzToPosix(const char* tz, char* out, size_t n) {
  if (!tz || !tz[0] || strlen(tz) >= 47) return false;
  if (strcmp(tz, "UTC") == 0) {
    strlcpy(out, "UTC0", n);
    return true;
  }
  // POSIX-строка без слэша: MSK-3, CET-1CEST,M3.5.0,M10.5.0/3 и т.п.
  if (!strchr(tz, '/')) {
    strlcpy(out, tz, n);
    return true;
  }
  return tzFromTable(tz, out, n);
}

bool tzValid(const char* tz) {
  char buf[64];
  return tzToPosix(tz, buf, sizeof buf);
}

static size_t tzLines() {
  static size_t cached = 0;
  if (cached) return cached;
  size_t n = 0;
  for (const char* p = TZ_TABLE; pgm_read_byte(p); p++) {
    if (pgm_read_byte(p) == '\n') n++;
  }
  cached = n;
  return n;
}

size_t tzCount() { return tzLines(); }

void tzVisit(TzVisitor fn, void* ctx) {
  char buf[48];
  const char* p = TZ_TABLE;
  while (pgm_read_byte(p)) {
    const char* tab = p;
    while (pgm_read_byte(tab) && pgm_read_byte(tab) != '\t') tab++;
    if (!pgm_read_byte(tab)) break;
    size_t len = (size_t)(tab - p);
    if (len >= sizeof buf) len = sizeof(buf) - 1;
    for (size_t i = 0; i < len; i++) buf[i] = (char)pgm_read_byte(p + i);
    fn(buf, len, ctx);
    const char* lineEnd = tab;
    while (pgm_read_byte(lineEnd) && pgm_read_byte(lineEnd) != '\n') lineEnd++;
    if (!pgm_read_byte(lineEnd)) break;
    p = lineEnd + 1;
  }
}

static void startNtp() {
  char posix[64];
  if (!tzToPosix(g.tz, posix, sizeof posix)) strlcpy(posix, "UTC0", sizeof posix);
  configTzTime(posix, "pool.ntp.org", "time.nist.gov", "ntp1.stratum1.ru");
  lastNtp = millis();
  ntpStarted = true;
}

static void applyEpoch(uint32_t epoch) {
  timeval tv;
  tv.tv_sec = (time_t)epoch;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}

void setManualTime(uint32_t epoch) {
  g.manualTime = true;
  g.manualEpoch = epoch;
  sntp_stop();
  ntpStarted = true;  // loop не должен сам поднимать SNTP поверх ручного времени
  applyEpoch(epoch);
  Serial.printf("[net] manual time set: %lu\n", (unsigned long)epoch);
}

void resumeAutoTime() {
  g.manualTime = false;
  ntpStarted = false;
  lastNtp = 0;
  Serial.println("[net] auto time (NTP) resumed");
}

void begin() {
  WiFi.persistent(false);
  // Ручное время живёт без сети: ставим его сразу, SNTP даже не стартуем
  if (g.manualTime && g.manualEpoch > 1700000000) {
    sntp_stop();
    applyEpoch(g.manualEpoch);
  }
  // Точка доступа поднимается на каждом старте: портал настройки доступен,
  // даже если сохранённая сеть недоступна или ещё не настроена
  startAP();
  if (g.wifiSsid[0] == '\0') {
    Serial.println("[net] no saved Wi-Fi, waiting in setup mode");
    return;
  }
  WiFi.hostname(cfg::HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(g.wifiSsid, g.wifiPass);
  connectDeadline = millis() + cfg::WIFI_CONNECT_TIMEOUT_MS;
  timeoutLogged = false;
  staStarted = true;
  Serial.printf("[net] STA connect to '%s', timeout %us\n", g.wifiSsid,
                (unsigned)(cfg::WIFI_CONNECT_TIMEOUT_MS / 1000));
}

void loop() {
  uint32_t now = millis();

  if (ap) {
    dns.processNextRequest();
    MDNS.update();
    // Если софт-точка не поднялась (редкий сбой RF), повторяем попытку
    if (now - lastApCheck > 5000) {
      lastApCheck = now;
      if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
        Serial.println("[net] AP lost, restarting");
        startAP();
      }
    }
  }

  if (staStarted) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!ntpStarted) {
        MDNS.begin(cfg::HOSTNAME);
        MDNS.addService("http", "tcp", 80);
        if (g.manualTime) {
          ntpStarted = true;
          applyEpoch(g.manualEpoch);
        } else {
          startNtp();
        }
        Serial.printf("[net] connected ip=%s\n", WiFi.localIP().toString().c_str());
      } else if (!g.manualTime && now - lastNtp > cfg::NTP_RESYNC_MS) {
        startNtp();
      }
      if (ap && apStopAt == 0) {
        apStopAt = now + cfg::AP_GRACE_MS;
        Serial.println("[net] online, setup AP shuts down shortly");
      }
      MDNS.update();
    } else if (connectDeadline && (int32_t)(now - connectDeadline) >= 0 && !timeoutLogged) {
      timeoutLogged = true;
      connectDeadline = 0;
      Serial.println("[net] Wi-Fi timeout, setup AP stays up");
    }
  }

  if (ap && apStopAt && (int32_t)(now - apStopAt) >= 0) {
    if (WiFi.status() == WL_CONNECTED) {
      stopAP();
    } else {
      apStopAt = 0;
    }
  }
}

bool connected() { return WiFi.status() == WL_CONNECTED; }
bool apMode() { return ap && !connected(); }
String ip() { return connected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString(); }
int8_t rssi() { return connected() ? (int8_t)WiFi.RSSI() : 0; }

bool timeValid() { return time(nullptr) > 1700000000; }

bool localTime(tm& out) {
  time_t now = time(nullptr);
  if (now < 1700000000) return false;
  localtime_r(&now, &out);
  return true;
}

String isoTime() {
  time_t now = time(nullptr);
  if (now < 1700000000) return "";
  tm tt;
  localtime_r(&now, &tt);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tt);
  return String(buf);
}

String scanJson() {
  int n = WiFi.scanNetworks(false, false);
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n && i < 20; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["secure"] = (WiFi.encryptionType(i) != ENC_TYPE_NONE);
  }
  WiFi.scanDelete();
  String s;
  serializeJson(doc, s);
  return s;
}

void saveWifi(const char* ssid, const char* pass) {
  strlcpy(g.wifiSsid, ssid ? ssid : "", sizeof(g.wifiSsid));
  strlcpy(g.wifiPass, pass ? pass : "", sizeof(g.wifiPass));
  settingsSaveNow();
}

void applyTz() {
  char posix[64];
  if (!tzToPosix(g.tz, posix, sizeof posix)) strlcpy(posix, "UTC0", sizeof posix);
  setenv("TZ", posix, 1);
  tzset();
  if (!g.manualTime && ntpStarted) startNtp();
}

}  // namespace net
