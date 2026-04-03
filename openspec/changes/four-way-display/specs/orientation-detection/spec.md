## ADDED Requirements

### Requirement: MPU6050 initialization
The system SHALL initialize the MPU6050 on I2C bus 1 (SDA=22, SCL=27, address 0x68) at boot. The accelerometer SHALL be configured in ±2g range. The device SHALL be woken from sleep mode (register 0x6B = 0x00).

#### Scenario: Successful MPU init
- **WHEN** the device boots and the MPU6050 is connected on SDA=22, SCL=27
- **THEN** the MPU6050 SHALL respond on address 0x68 and return valid accelerometer data within 100ms

#### Scenario: MPU not found
- **WHEN** the MPU6050 is not connected or not responding
- **THEN** the system SHALL log an error on Serial and default to MODE_CLOCK without crashing

### Requirement: Orientation detection from accelerometer
The system SHALL read raw Ax and Ay values from the MPU6050 every 50ms. The dominant axis SHALL be computed as max(|Ax|, |Ay|). If the dominant axis magnitude is below 0.7g, the orientation SHALL be considered AMBIGUOUS and no mode change SHALL occur.

#### Scenario: Clear orientation — right edge down (clock)
- **WHEN** Ax > +0.7g and Ax is the dominant axis
- **THEN** the detected orientation SHALL be MODE_CLOCK

#### Scenario: Clear orientation — left edge down (countdown)
- **WHEN** Ax < -0.7g and Ax is the dominant axis
- **THEN** the detected orientation SHALL be MODE_COUNTDOWN

#### Scenario: Clear orientation — top edge down (mode C)
- **WHEN** Ay > +0.7g and Ay is the dominant axis
- **THEN** the detected orientation SHALL be MODE_C

#### Scenario: Clear orientation — bottom edge down (mode D)
- **WHEN** Ay < -0.7g and Ay is the dominant axis
- **THEN** the detected orientation SHALL be MODE_D

#### Scenario: Flat / ambiguous orientation
- **WHEN** the dominant axis magnitude is below 0.7g
- **THEN** no mode change SHALL occur and the current mode SHALL be maintained

### Requirement: Orientation debounce 100ms
The system SHALL switch to a new mode only if the detected orientation remains stable for at least 100ms continuously. Any change in detected orientation during the debounce window SHALL reset the timer.

#### Scenario: Stable orientation triggers mode switch
- **WHEN** the same orientation is detected for 100ms without interruption
- **THEN** the system SHALL switch to the corresponding mode

#### Scenario: Unstable orientation is ignored
- **WHEN** the detected orientation changes before 100ms has elapsed
- **THEN** the debounce timer SHALL reset and no mode switch SHALL occur
