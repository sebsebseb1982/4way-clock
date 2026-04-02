## ADDED Requirements

### Requirement: Swiss railway clock face
The system SHALL render a Swiss railway clock face (SBB/CFF design) using LVGL on the TFT display. The cadran SHALL be a white circle (diameter ≥ 200 px) centred horizontally and positioned in the upper portion of the 320×240 screen. The background SHALL be black. The clock face SHALL include 60 minute-graduation marks and 12 hour-graduation marks in black, and a hub circle at the centre.

#### Scenario: Clock face rendered at boot
- **WHEN** LVGL is initialised and the NTP time is available
- **THEN** the full clock face (cadran, graduations, aiguilles, red disc) is drawn within 500 ms of LVGL init

#### Scenario: Display on black background
- **WHEN** the application starts
- **THEN** the screen background is fully black (RGB 0,0,0) with no other background colour visible

### Requirement: Animated clock hands
The system SHALL animate three clock hands: hours (black, thick), minutes (black, medium), seconds (black, thin). Hand positions SHALL be derived from the current local time obtained via `localtime_r`.

#### Scenario: Hour and minute hands update
- **WHEN** the current time changes
- **THEN** the hour hand points smoothly between hour marks (proportional to elapsed minutes) and the minute hand advances continuously

#### Scenario: Seconds hand SBB behaviour
- **WHEN** the seconds hand reaches the 12 o'clock position (after ~58.5 s of travel)
- **THEN** the hand pauses at 12 o'clock for approximately 1.5 s before restarting its sweep at the NTP-corrected time

### Requirement: Red disc on seconds hand
The system SHALL display a filled red circle (diameter ≈ 16 px) at a fixed offset from the pivot on the seconds hand, as per the authentic SBB clock design.

#### Scenario: Red disc visible and animated
- **WHEN** the clock is running
- **THEN** the red disc rotates with the seconds hand and remains consistently visible against the cadran

### Requirement: LVGL tick and handler loop
The system SHALL call `lv_tick_inc()` and `lv_timer_handler()` at regular intervals (≤ 10 ms) to ensure smooth animation. This SHALL be implemented in the `loop()` function.

#### Scenario: Smooth animation
- **WHEN** the device is running normally
- **THEN** all animations update at ≥ 20 frames per second with no visible stuttering
