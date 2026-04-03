## ADDED Requirements

### Requirement: Analog clock face layout
The system SHALL display a Swiss station-style (Mondaine/SBB) analog clock face occupying a 240×240 pixel square on the left side of the screen (x=0 to x=239). The background SHALL be white.

#### Scenario: Clock face positioned correctly
- **WHEN** the application starts
- **THEN** a white 240×240 square SHALL appear on the left half of the 320×240 display

### Requirement: Hour markers on clock face
The clock face SHALL display 12 large rectangular hour markers (black, 12×4px) and 48 small minute markers (black, 6×2px) arranged around the perimeter of the clock circle at their correct angular positions.

#### Scenario: Hour and minute markers rendered at init
- **WHEN** the clock face is initialized
- **THEN** 60 markers SHALL be visible around the clock perimeter (12 thick for hours, 48 thin for minutes)

### Requirement: Hour and minute hands
The system SHALL draw hour and minute hands as black lines centered at the clock pivot (120, 120). Hour hand: 6px wide, 60px long. Minute hand: 4px wide, 90px long. Hands SHALL be redrawn every second.

#### Scenario: Hands point to correct time
- **WHEN** the current time is 10:10:00
- **THEN** the hour hand SHALL point roughly at 10 and the minute hand at 2 (i.e., at 10-hour and 10-minute positions)

#### Scenario: Hands updated each second
- **WHEN** one second elapses
- **THEN** the canvas SHALL be cleared and all hands redrawn at the updated time positions

### Requirement: Seconds hand with red disc
The seconds hand SHALL be a red line (2px wide, 100px long from pivot). A filled red circle of diameter 10px SHALL be drawn at the tip of the seconds hand. The hand SHALL update every second (simple tick, no sweep animation).

#### Scenario: Seconds hand visible and red
- **WHEN** the clock is displaying a valid time
- **THEN** a red line with a red disc at its tip SHALL be visible, pointing at the current second position

#### Scenario: Seconds hand moves each second
- **WHEN** the second value changes
- **THEN** the seconds hand SHALL jump to the new second position (6° per second)

### Requirement: Canvas rendering approach
Hands SHALL be drawn using an `lv_canvas_t` of size 240×240 superimposed on the white clock face background. Colors SHALL be applied using `lv_color_make()` in draw descriptors. The canvas SHALL be cleared to white before each redraw.

#### Scenario: Canvas cleared before redraw
- **WHEN** the canvas is redrawn each second
- **THEN** the previous hand positions SHALL be erased (canvas filled with white) before drawing new positions
