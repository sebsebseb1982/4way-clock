#pragma once
#include <time.h>
#include <stdbool.h>

/**
 * Connect to WiFi using credentials from secrets.h.
 * Retries up to 20 times (500 ms apart).
 * Returns true on success, false if unreachable.
 */
bool wifiConnect();

/**
 * Initialise SNTP with the university NTP server and Europe/Paris timezone.
 * Must be called after a successful wifiConnect().
 * Blocks up to 15 s waiting for the first sync.
 * Returns true if the clock is synchronised, false on timeout.
 */
bool ntpInit();

/**
 * Thread-safe wrapper around localtime_r.
 * Returns false if the time has never been synchronised (year < 2020).
 * Use this instead of the Arduino getLocalTime() to avoid blocking.
 */
bool getCurrentTime(struct tm *out);

/**
 * Returns true if WiFi + NTP are both active and healthy.
 */
bool isTimeSynced();
