## ADDED Requirements

### Requirement: WiFi connection via secrets file
The system SHALL connect to a WiFi network using credentials defined in `include/secrets.h` (`WIFI_SSID` and `WIFI_PASSWORD` macros). The `secrets.h` file SHALL be excluded from version control via `.gitignore`.

#### Scenario: Successful WiFi connection at boot
- **WHEN** the device boots and valid credentials are present in `secrets.h`
- **THEN** the device SHALL connect to the WiFi network within 30 seconds

#### Scenario: WiFi unavailable at boot
- **WHEN** the device boots and WiFi is not available or credentials are wrong
- **THEN** the device SHALL continue booting without blocking and display "--:--:--" on the clock face

### Requirement: NTP time synchronization
The system SHALL synchronize time via NTP with the server `ntp.emi.u-bordeaux.fr`. Timezone SHALL be Europe/Paris with automatic DST handling using the POSIX string `"CET-1CEST,M3.5.0,M10.5.0/3"`.

#### Scenario: Successful NTP sync
- **WHEN** WiFi is connected
- **THEN** the system SHALL synchronize time via NTP and display the correct local time within 10 seconds of WiFi connection

#### Scenario: Time display before NTP sync
- **WHEN** NTP has not yet synchronized (epoch time or sync status not OK)
- **THEN** the clock SHALL display "--:--:--" for the digital time and a blank/zero state for the analog hands

#### Scenario: NTP resynchronization
- **WHEN** the device has been running for an extended period
- **THEN** the ESP32 SNTP stack SHALL automatically resynchronize without application intervention
