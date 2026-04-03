## ADDED Requirements

### Requirement: Countdown display layout
The countdown mode SHALL display in landscape (320×240, rotation 3). The layout SHALL include:
- A large digital time display (HH:MM:SS) centered in the upper area
- Six adjustment buttons: +H, +M, +S (top row) and -H, -M, -S (bottom row)
- A START/PAUSE button
- A RESET button

#### Scenario: Countdown UI built on mode entry
- **WHEN** orientation switches to MODE_COUNTDOWN
- **THEN** the countdown UI SHALL be fully built and visible with all buttons and the current remaining time displayed

### Requirement: Countdown time adjustment via touch buttons
In IDLE state, the user SHALL be able to adjust the countdown duration using touch buttons:
- +H / -H : increment/decrement hours (0–99)
- +M / -M : increment/decrement minutes (0–59), with overflow/underflow wrapping to hours
- +S / -S : increment/decrement seconds (0–59), with overflow/underflow wrapping to minutes

#### Scenario: Increment minutes
- **WHEN** the +M button is pressed in IDLE state
- **THEN** the minutes value SHALL increase by 1, wrapping to 0 and incrementing hours at 60

#### Scenario: Adjustment disabled while running
- **WHEN** the countdown is in RUNNING state
- **THEN** the +/- buttons SHALL have no effect

### Requirement: Countdown start and pause
The START/PAUSE button SHALL toggle between RUNNING and IDLE states. Pressing START from IDLE SHALL begin the countdown from the current remaining time. Pressing PAUSE from RUNNING SHALL freeze the remaining time.

#### Scenario: Start countdown
- **WHEN** remaining time > 0 and user presses START in IDLE state
- **THEN** the countdown SHALL enter RUNNING state and begin decrementing

#### Scenario: Pause countdown
- **WHEN** countdown is RUNNING and user presses PAUSE
- **THEN** the countdown SHALL enter IDLE state with remaining time frozen

#### Scenario: Start with zero time
- **WHEN** remaining time is 00:00:00 and user presses START
- **THEN** no state change SHALL occur (button press ignored)

### Requirement: Countdown background operation
The countdown decrement logic SHALL run in the main `loop()` regardless of the active display mode. The countdown SHALL continue running when the user switches to another mode.

#### Scenario: Countdown continues in clock mode
- **WHEN** countdown is RUNNING and the user rotates CYD to MODE_CLOCK
- **THEN** the countdown SHALL continue decrementing in the background

#### Scenario: Countdown display resumes correctly
- **WHEN** the user returns to MODE_COUNTDOWN while countdown is RUNNING
- **THEN** the display SHALL show the current (decremented) remaining time immediately

### Requirement: Countdown completion
When the remaining time reaches 00:00:00, the countdown SHALL enter DONE state. In DONE state the RGB LED SHALL blink red (500ms on/off). The display SHALL show 00:00:00 and the DONE indicator.

#### Scenario: Countdown reaches zero
- **WHEN** remaining time decrements to 00:00:00
- **THEN** the state SHALL change to DONE and the LED SHALL begin blinking red

#### Scenario: Reset from DONE state
- **WHEN** user presses RESET in DONE or IDLE state
- **THEN** remaining time SHALL be set to 00:00:00, state SHALL return to IDLE, and the LED SHALL return to normal status indication
