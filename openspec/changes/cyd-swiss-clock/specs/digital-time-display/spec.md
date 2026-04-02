## ADDED Requirements

### Requirement: Digital time display in Helvetica-style font
The system SHALL display the current time as a text string in `HH:MM:SS` format below the analogue clock face using LVGL. The font SHALL be `lv_font_montserrat_28` (28 px, geometric sans-serif, included in LVGL) or a custom bitmap font generated from TeX Gyre Heros (a Helvetica-equivalent free font). The text SHALL be white on black background, horizontally centred.

#### Scenario: Time displayed after NTP sync
- **WHEN** the time is successfully synchronised from NTP
- **THEN** the digital area shows the current local time in HH:MM:SS format, updated every second

#### Scenario: Time unavailable
- **WHEN** NTP has never successfully synchronised
- **THEN** the digital area displays `--:--:--` in the same font and position

### Requirement: Digital display positioning
The digital time label SHALL be positioned below the analogue clock face with a minimum vertical gap of 8 px. It SHALL be fully visible within the 320×240 display bounds.

#### Scenario: No overlap with clock face
- **WHEN** the clock face and digital label are both rendered
- **THEN** the digital label does not overlap the cadran circle or any clock hand

### Requirement: Seconds update cadence
The digital display SHALL update every second, synchronised with the system clock (`localtime_r`), not with a fixed 1000 ms timer, to avoid drift.

#### Scenario: Display stays aligned to wall clock
- **WHEN** the system has been running for 1 hour
- **THEN** the displayed seconds match the NTP-corrected local time (within ±1 s)
