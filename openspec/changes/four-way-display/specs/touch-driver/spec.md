## ADDED Requirements

### Requirement: XPT2046 touchscreen reactivation
The XPT2046 Bitbang touchscreen (MOSI=32, MISO=39, CLK=25, CS=33) SHALL be initialized at boot with SPIFFS calibration loading. It SHALL be active in all four display modes. The touch driver SHALL be registered with LVGL.

#### Scenario: Touch initialized at boot
- **WHEN** the device boots
- **THEN** the XPT2046 SHALL be initialized and calibration data SHALL be loaded from SPIFFS

#### Scenario: Touch active in all modes
- **WHEN** any mode is active (clock, countdown, C, D)
- **THEN** touch events SHALL be processed by LVGL and delivered to widgets in the active UI

### Requirement: Touch coordinate remapping by rotation
Raw touch coordinates from XPT2046 SHALL be remapped to match the current TFT rotation before being passed to LVGL, using the following transforms:

| Rotation | Transform |
|----------|-----------|
| 0 (portrait) | (x, y) → (x, y) |
| 1 (landscape) | (x, y) → (y, 240-x) |
| 2 (portrait inv.) | (x, y) → (240-x, 320-y) |
| 3 (landscape inv.) | (x, y) → (320-y, x) |

#### Scenario: Touch remapped in landscape mode (rotation 1)
- **WHEN** the TFT rotation is 1 and the user touches a point
- **THEN** the LVGL touch coordinates SHALL be (raw_y, 240 - raw_x)

#### Scenario: Touch remapped in portrait inverted mode (rotation 2)
- **WHEN** the TFT rotation is 2 and the user touches a point
- **THEN** the LVGL touch coordinates SHALL be (240 - raw_x, 320 - raw_y)

### Requirement: Touch coordinate validity check
Only touch coordinates within the display bounds for the active rotation SHALL be passed to LVGL as pressed. Out-of-bounds coordinates SHALL be reported as LV_INDEV_STATE_REL.

#### Scenario: Valid touch in landscape
- **WHEN** rotation is 1 and remapped coordinates are within [0,319]×[0,239]
- **THEN** LVGL state SHALL be LV_INDEV_STATE_PR

#### Scenario: Invalid touch coordinates
- **WHEN** remapped coordinates fall outside display bounds
- **THEN** LVGL state SHALL be LV_INDEV_STATE_REL
