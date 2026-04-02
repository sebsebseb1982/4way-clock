#include "wifi_ntp.h"
#include "secrets.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>

// Forward-declare the Arduino ESP32 built-in (avoids header pollution).
extern "C" bool getLocalTime(struct tm *info, uint32_t ms);

// ---- Constants ----------------------------------------------------------------
static const char *NTP_SERVER   = "ntp.emi.u-bordeaux.fr";
static const char *TZ_POSIX     = "CET-1CEST,M3.5.0,M10.5.0/3"; // Europe/Paris
static const int   WIFI_RETRIES = 20;
static const int   WIFI_RETRY_DELAY_MS = 500;
static const int   NTP_TIMEOUT_S = 15;

// ---- Internal state -----------------------------------------------------------
static bool s_timeSynced = false;

// ---- Public API ---------------------------------------------------------------

bool wifiConnect() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < WIFI_RETRIES) {
        delay(WIFI_RETRY_DELAY_MS);
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.println("[WiFi] Connection failed after retries.");
    return false;
}

bool ntpInit() {
    configTzTime(TZ_POSIX, NTP_SERVER);

    // Wait up to NTP_TIMEOUT_S seconds for the first sync
    struct tm timeinfo;
    const unsigned long deadline = millis() + (unsigned long)NTP_TIMEOUT_S * 1000UL;
    while (millis() < deadline) {
        if (getLocalTime(&timeinfo, 0)) {  // Arduino built-in, 0 ms = non-blocking
            s_timeSynced = true;
            Serial.printf("[NTP] Synced: %02d:%02d:%02d\n",
                          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            return true;
        }
        delay(500);
    }

    Serial.println("[NTP] Sync timeout.");
    return false;
}

bool getCurrentTime(struct tm *out) {
    if (out == nullptr) return false;
    time_t now = time(nullptr);
    localtime_r(&now, out);
    // Guard: year < 2020 means clock is not yet synchronised
    return (out->tm_year + 1900 >= 2020);
}

bool isTimeSynced() {
    return s_timeSynced;
}
