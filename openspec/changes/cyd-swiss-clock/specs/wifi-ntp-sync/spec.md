## ADDED Requirements

### Requirement: WiFi connection at startup
The system SHALL connect to the WiFi network using the SSID and password defined in `include/secrets.h` (macros `WIFI_SSID` and `WIFI_PASSWORD`) during the `setup()` phase. Connection SHALL be retried up to 20 times (500 ms apart) before marking WiFi as unavailable.

#### Scenario: Successful connection
- **WHEN** the device boots and valid credentials are present in `secrets.h`
- **THEN** the device connects to the WiFi network within 10 seconds and proceeds to NTP sync

#### Scenario: Connection failure
- **WHEN** the WiFi network is unreachable or credentials are invalid after 20 retries
- **THEN** the device marks WiFi as unavailable, displays "--:--:--" in the digital area, and retries the full connection sequence every 30 seconds

### Requirement: NTP time synchronisation
The system SHALL synchronise time at startup using the SNTP protocol against the server `ntp.emi.u-bordeaux.fr`. The timezone SHALL be set to `CET-1CEST,M3.5.0,M10.5.0/3` (Europe/Paris). After initial sync, the system SHALL resynchronise every 3600 seconds.

#### Scenario: Successful NTP sync
- **WHEN** WiFi is connected and the NTP server is reachable
- **THEN** `localtime_r` returns a valid time within 15 seconds of boot and the clock display updates immediately

#### Scenario: NTP server unreachable
- **WHEN** the NTP server does not respond within 15 seconds of a sync attempt
- **THEN** the system retries after 30 seconds; the clock continues displaying the last known valid time (or "--:--:--" if never synced)

### Requirement: Credentials isolation
The file `include/secrets.h` SHALL define `WIFI_SSID` and `WIFI_PASSWORD` as string literals. A template `include/secrets.h.example` SHALL be provided. The real `secrets.h` SHALL be listed in `.gitignore`.

#### Scenario: Missing secrets file
- **WHEN** `secrets.h` is absent at compile time
- **THEN** the compiler stops with an error directing the user to copy `secrets.h.example`
