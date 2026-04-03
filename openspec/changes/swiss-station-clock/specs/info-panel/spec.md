## ADDED Requirements

### Requirement: Info panel layout
The system SHALL display an information panel occupying the right 80×240 pixels of the screen (x=240 to x=319). The panel SHALL have a black background with white text.

#### Scenario: Panel visible at correct position
- **WHEN** the application starts
- **THEN** a black panel SHALL be visible on the right 80px of the 320×240 display

### Requirement: Digital time display
The panel SHALL display the current time in HH:MM:SS format using a white LVGL label, updated every second.

#### Scenario: Time displayed and updated
- **WHEN** one second elapses and NTP is synchronized
- **THEN** the digital time label SHALL show the updated time in HH:MM:SS format

#### Scenario: Time not synchronized
- **WHEN** NTP has not yet synchronized
- **THEN** the time label SHALL display "--:--:--"

### Requirement: Date display
The panel SHALL display the current date in DD/MM/YY format using a white LVGL label, updated once per second.

#### Scenario: Date displayed correctly
- **WHEN** NTP is synchronized
- **THEN** the date label SHALL show the current local date in DD/MM/YY format

### Requirement: WiFi and NTP status indicator
The panel SHALL display a text-based status indicator showing WiFi connection state and NTP sync state, updated every 10 seconds or on state change.

#### Scenario: WiFi not connected
- **WHEN** WiFi is not connected
- **THEN** the status indicator SHALL show "WiFi: X" (or equivalent)

#### Scenario: WiFi connected, NTP pending
- **WHEN** WiFi is connected but NTP is not yet synced
- **THEN** the status indicator SHALL show "NTP:..."

#### Scenario: Fully synchronized
- **WHEN** both WiFi and NTP are synchronized
- **THEN** the status indicator SHALL show "NTP: OK"
