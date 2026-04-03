## ADDED Requirements

### Requirement: TFT rotation change on mode switch
When the active mode changes, the system SHALL call `tft.setRotation(n)` with the rotation value corresponding to the new mode, then update the LVGL display driver resolution accordingly.

Rotation mapping:
- MODE_CLOCK      → rotation 1 (landscape 320×240)
- MODE_COUNTDOWN  → rotation 3 (landscape 320×240, USB up)
- MODE_C          → rotation 0 (portrait 240×320)
- MODE_D          → rotation 2 (portrait 240×320, inverted)

#### Scenario: Mode switch updates TFT rotation
- **WHEN** the mode switches from any mode to MODE_C
- **THEN** `tft.setRotation(0)` SHALL be called and the LVGL driver SHALL be updated to hor_res=240, ver_res=320

#### Scenario: Mode switch to landscape mode
- **WHEN** the mode switches to MODE_CLOCK or MODE_COUNTDOWN
- **THEN** `tft.setRotation(1 or 3)` SHALL be called and the LVGL driver SHALL be updated to hor_res=320, ver_res=240

### Requirement: LVGL UI rebuild on mode switch
After rotation change, the system SHALL call `lv_obj_clean(lv_scr_act())` to destroy all current LVGL widgets, then SHALL call the `ui_XXX_build()` function corresponding to the new mode.

#### Scenario: UI destroyed before rebuild
- **WHEN** a mode switch occurs
- **THEN** all LVGL objects on the active screen SHALL be destroyed before the new UI is built

#### Scenario: New mode UI built after destroy
- **WHEN** the UI has been cleared following a mode switch
- **THEN** the new mode's build function SHALL be called and the display SHALL show the new mode UI within one LVGL render cycle

### Requirement: Canvas buffer preserved across mode switches
The clock canvas buffer (200×200×2 bytes, heap-allocated at boot) SHALL NOT be freed or reallocated during mode switches. It SHALL remain allocated for the lifetime of the application.

#### Scenario: Canvas buffer survives mode switch
- **WHEN** the mode switches away from MODE_CLOCK and back to MODE_CLOCK
- **THEN** the canvas buffer pointer SHALL remain valid and the clock UI SHALL render correctly
