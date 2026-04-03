## ADDED Requirements

### Requirement: RGB LED status indication
The RGB LED (common-anode, pins 4/16/17, PWM channels 0/1/2) SHALL indicate the current WiFi/NTP state as follows:
- No WiFi: red fixed
- WiFi connecting: red blinking (500ms on/off)
- WiFi connected, NTP pending: green fixed
- NTP synchronized: blue fixed

#### Scenario: Red LED when WiFi not connected
- **WHEN** the device has no WiFi connection
- **THEN** the red LED SHALL be ON and green and blue LEDs SHALL be OFF

#### Scenario: Green LED when WiFi connected, NTP pending
- **WHEN** WiFi is connected but NTP synchronization has not completed
- **THEN** the green LED SHALL be ON and red and blue LEDs SHALL be OFF

#### Scenario: Blue LED when NTP synchronized
- **WHEN** NTP synchronization is confirmed via `sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED`
- **THEN** the blue LED SHALL be ON and red and green LEDs SHALL be OFF

### Requirement: LED update frequency
The LED status SHALL be re-evaluated and updated in the main loop at most every 500ms (non-blocking, using `millis()` comparison).

#### Scenario: LED updated without blocking
- **WHEN** 500ms have elapsed since last LED update
- **THEN** the LED state SHALL be recalculated and applied without using `delay()`
