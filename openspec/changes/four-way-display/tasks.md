## 1. Restructuration du projet

- [x] 1.1 Créer les fichiers vides `src/mpu.h`, `src/mpu.cpp`, `src/touch_mgr.h`, `src/touch_mgr.cpp`, `src/ui_clock.h`, `src/ui_clock.cpp`, `src/ui_countdown.h`, `src/ui_countdown.cpp`, `src/ui_stub.h`, `src/ui_stub.cpp`
- [x] 1.2 Ajouter `XPT2046_Bitbang` dans `lib_deps` de `platformio.ini`
- [x] 1.3 Extraire le code horloge de `main.cpp` vers `ui_clock.cpp` : `drawHand()`, `drawClockFace()`, `buildUI_clock()` (renommé), `updateInfoPanel()`
- [x] 1.4 Vider `main.cpp` de tout le code UI horloge et touch, en conservant uniquement : inclusions globales, variables LVGL/TFT, `setup()`, `loop()`

## 2. Module MPU6050 (`mpu.cpp`)

- [x] 2.1 Implémenter `mpu_init()` : `Wire.begin(22, 27)`, wake-up registre 0x6B=0x00, vérification présence adresse 0x68, log Serial
- [x] 2.2 Implémenter `mpu_read_accel(float &ax, float &ay)` : lecture 4 octets registres 0x3B–0x3D, conversion en g (÷16384.0f)
- [x] 2.3 Implémenter `mpu_get_orientation()` : retourne enum `{ORIENT_CLOCK, ORIENT_COUNTDOWN, ORIENT_C, ORIENT_D, ORIENT_AMBIGUOUS}` selon seuil 0.7g sur axe dominant
- [x] 2.4 Implémenter `mpu_update()` : appelé dans loop() toutes les 50ms, gère le debounce 100ms, retourne `true` si mode a changé
- [x] 2.5 Exposer `mpu_current_mode()` retournant le mode actif après debounce

## 3. Module Touch (`touch_mgr.cpp`)

- [x] 3.1 Implémenter `touch_init()` : `XPT2046_Bitbang` init + SPIFFS calibration (code extrait de l'original)
- [x] 3.2 Implémenter `touch_read(lv_indev_drv_t*, lv_indev_data_t*)` : lecture XPT2046 + remapping selon `g_active_rotation` (variable globale), validité bounds, state LVGL
- [x] 3.3 Enregistrer le driver touch LVGL dans `touch_init()` (appel unique au boot)
- [x] 3.4 Exposer `g_active_rotation` (uint8_t global, modifié lors du switch de mode) dans un header partagé

## 4. Système de switch de mode (`main.cpp`)

- [x] 4.1 Déclarer `typedef enum { MODE_CLOCK, MODE_COUNTDOWN, MODE_C, MODE_D } AppMode` dans un header partagé (`src/app_mode.h`)
- [x] 4.2 Implémenter `apply_mode(AppMode mode)` dans `main.cpp` : `tft.setRotation(n)`, mise à jour `disp_drv.hor_res/ver_res`, `lv_disp_drv_update()`, `lv_obj_clean()`, appel `ui_XXX_build()`
- [x] 4.3 Initialiser le mode par défaut `MODE_CLOCK` dans `setup()` après `initLVGL()`
- [x] 4.4 Dans `loop()`, appeler `mpu_update()` et si mode changé appeler `apply_mode()`

## 5. Module UI Clock (`ui_clock.cpp`)

- [x] 5.1 Déplacer `drawHand()`, `drawClockFace()` depuis `main.cpp` vers `ui_clock.cpp` sans modification
- [x] 5.2 Renommer `buildUI()` → `ui_clock_build()`, l'adapter pour fonctionner après `apply_mode()` (canvas_buf déjà alloué)
- [x] 5.3 Implémenter `ui_clock_update(struct tm *t, bool synced)` extrait de la logique loop() actuelle
- [x] 5.4 Appeler `ui_clock_update()` depuis `loop()` uniquement quand `MODE_CLOCK` est actif

## 6. Module UI Countdown (`ui_countdown.cpp`)

- [x] 6.1 Déclarer `typedef enum { CD_IDLE, CD_RUNNING, CD_DONE } CdState` et `uint32_t g_countdown_ms` (global partagé)
- [x] 6.2 Implémenter `ui_countdown_build()` : layout landscape 320×240 avec labels HH:MM:SS et 8 boutons touch (+H,-H,+M,-M,+S,-S, START/PAUSE, RESET)
- [x] 6.3 Implémenter callbacks boutons +H/M/S et -H/M/S avec gestion overflow (60s→+1min, 60min→+1h)
- [x] 6.4 Implémenter callback START/PAUSE (toggle CD_IDLE↔CD_RUNNING si remaining>0)
- [x] 6.5 Implémenter callback RESET (→ CD_IDLE, g_countdown_ms=0)
- [x] 6.6 Implémenter `ui_countdown_update()` : met à jour le label HH:MM:SS si MODE_COUNTDOWN actif
- [x] 6.7 Implémenter `countdown_tick()` dans `loop()` (toujours actif) : si CD_RUNNING, décrémenter `g_countdown_ms` via `millis()` delta ; si 0 → CD_DONE
- [x] 6.8 En état CD_DONE, faire clignoter la LED rouge (500ms via millis) dans `updateStatusLed()`

## 7. Module UI Stub (`ui_stub.cpp`)

- [x] 7.1 Implémenter `ui_stub_build(const char *label)` : fond noir, texte centré indiquant le nom du mode et "À définir"
- [x] 7.2 Appeler `ui_stub_build("Mode C")` pour MODE_C et `ui_stub_build("Mode D")` pour MODE_D dans `apply_mode()`

## 8. Intégration `setup()` et `loop()`

- [x] 8.1 Dans `setup()` : appeler `mpu_init()` avant `initLVGL()`, puis `touch_init()` après `initLVGL()`
- [x] 8.2 Dans `loop()` : appeler `countdown_tick()` à chaque itération (background)
- [x] 8.3 Dans `loop()` : appeler `mpu_update()` toutes les 50ms ; si mode changé → `apply_mode()`
- [x] 8.4 Dans `loop()` : router les appels `ui_clock_update()` / `ui_countdown_update()` selon le mode actif
- [x] 8.5 Adapter `updateStatusLed()` pour gérer CD_DONE (rouge clignotant prioritaire sur statut WiFi/NTP)

## 9. Validation

- [x] 9.1 Compiler sans erreur avec `platformio run`
- [ ] 9.2 Vérifier le switch MODE_CLOCK ↔ MODE_COUNTDOWN sur le matériel (rotation TFT + rebuild UI)
- [ ] 9.3 Vérifier les boutons touch du countdown dans les deux états du remapping (rotation 3)
- [ ] 9.4 Vérifier que le countdown continue en background lors d'un switch vers MODE_CLOCK
- [ ] 9.5 Vérifier que la LED passe rouge clignotant quand le countdown atteint 0

- [ ] 1.2 Ajouter `XPT2046_Bitbang` dans `lib_deps` de `platformio.ini`
- [ ] 1.3 Extraire le code horloge de `main.cpp` vers `ui_clock.cpp` : `drawHand()`, `drawClockFace()`, `buildUI_clock()` (renommé), `updateInfoPanel()`
- [ ] 1.4 Vider `main.cpp` de tout le code UI horloge et touch, en conservant uniquement : inclusions globales, variables LVGL/TFT, `setup()`, `loop()`

## 2. Module MPU6050 (`mpu.cpp`)

- [ ] 2.1 Implémenter `mpu_init()` : `Wire.begin(22, 27)`, wake-up registre 0x6B=0x00, vérification présence adresse 0x68, log Serial
- [ ] 2.2 Implémenter `mpu_read_accel(float &ax, float &ay)` : lecture 4 octets registres 0x3B–0x3D, conversion en g (÷16384.0f)
- [ ] 2.3 Implémenter `mpu_get_orientation()` : retourne enum `{ORIENT_CLOCK, ORIENT_COUNTDOWN, ORIENT_C, ORIENT_D, ORIENT_AMBIGUOUS}` selon seuil 0.7g sur axe dominant
- [ ] 2.4 Implémenter `mpu_update()` : appelé dans loop() toutes les 50ms, gère le debounce 100ms, retourne `true` si mode a changé
- [ ] 2.5 Exposer `mpu_current_mode()` retournant le mode actif après debounce

## 3. Module Touch (`touch_mgr.cpp`)

- [ ] 3.1 Implémenter `touch_init()` : `XPT2046_Bitbang` init + SPIFFS calibration (code extrait de l'original)
- [ ] 3.2 Implémenter `touch_read(lv_indev_drv_t*, lv_indev_data_t*)` : lecture XPT2046 + remapping selon `g_active_rotation` (variable globale), validité bounds, state LVGL
- [ ] 3.3 Enregistrer le driver touch LVGL dans `touch_init()` (appel unique au boot)
- [ ] 3.4 Exposer `g_active_rotation` (uint8_t global, modifié lors du switch de mode) dans un header partagé

## 4. Système de switch de mode (`main.cpp`)

- [ ] 4.1 Déclarer `typedef enum { MODE_CLOCK, MODE_COUNTDOWN, MODE_C, MODE_D } AppMode` dans un header partagé (`src/app_mode.h`)
- [ ] 4.2 Implémenter `apply_mode(AppMode mode)` dans `main.cpp` : `tft.setRotation(n)`, mise à jour `disp_drv.hor_res/ver_res`, `lv_disp_drv_update()`, `lv_obj_clean()`, appel `ui_XXX_build()`
- [ ] 4.3 Initialiser le mode par défaut `MODE_CLOCK` dans `setup()` après `initLVGL()`
- [ ] 4.4 Dans `loop()`, appeler `mpu_update()` et si mode changé appeler `apply_mode()`

## 5. Module UI Clock (`ui_clock.cpp`)

- [ ] 5.1 Déplacer `drawHand()`, `drawClockFace()` depuis `main.cpp` vers `ui_clock.cpp` sans modification
- [ ] 5.2 Renommer `buildUI()` → `ui_clock_build()`, l'adapter pour fonctionner après `apply_mode()` (canvas_buf déjà alloué)
- [ ] 5.3 Implémenter `ui_clock_update(struct tm *t, bool synced)` extrait de la logique loop() actuelle
- [ ] 5.4 Appeler `ui_clock_update()` depuis `loop()` uniquement quand `MODE_CLOCK` est actif

## 6. Module UI Countdown (`ui_countdown.cpp`)

- [ ] 6.1 Déclarer `typedef enum { CD_IDLE, CD_RUNNING, CD_DONE } CdState` et `uint32_t g_countdown_ms` (global partagé)
- [ ] 6.2 Implémenter `ui_countdown_build()` : layout landscape 320×240 avec labels HH:MM:SS et 8 boutons touch (+H,-H,+M,-M,+S,-S, START/PAUSE, RESET)
- [ ] 6.3 Implémenter callbacks boutons +H/M/S et -H/M/S avec gestion overflow (60s→+1min, 60min→+1h)
- [ ] 6.4 Implémenter callback START/PAUSE (toggle CD_IDLE↔CD_RUNNING si remaining>0)
- [ ] 6.5 Implémenter callback RESET (→ CD_IDLE, g_countdown_ms=0)
- [ ] 6.6 Implémenter `ui_countdown_update()` : met à jour le label HH:MM:SS si MODE_COUNTDOWN actif
- [ ] 6.7 Implémenter `countdown_tick()` dans `loop()` (toujours actif) : si CD_RUNNING, décrémenter `g_countdown_ms` via `millis()` delta ; si 0 → CD_DONE
- [ ] 6.8 En état CD_DONE, faire clignoter la LED rouge (500ms via millis) dans `updateStatusLed()`

## 7. Module UI Stub (`ui_stub.cpp`)

- [ ] 7.1 Implémenter `ui_stub_build(const char *label)` : fond noir, texte centré indiquant le nom du mode et "À définir"
- [ ] 7.2 Appeler `ui_stub_build("Mode C")` pour MODE_C et `ui_stub_build("Mode D")` pour MODE_D dans `apply_mode()`

## 8. Intégration `setup()` et `loop()`

- [ ] 8.1 Dans `setup()` : appeler `mpu_init()` avant `initLVGL()`, puis `touch_init()` après `initLVGL()`
- [ ] 8.2 Dans `loop()` : appeler `countdown_tick()` à chaque itération (background)
- [ ] 8.3 Dans `loop()` : appeler `mpu_update()` toutes les 50ms ; si mode changé → `apply_mode()`
- [ ] 8.4 Dans `loop()` : router les appels `ui_clock_update()` / `ui_countdown_update()` selon le mode actif
- [ ] 8.5 Adapter `updateStatusLed()` pour gérer CD_DONE (rouge clignotant prioritaire sur statut WiFi/NTP)

## 9. Validation

- [ ] 9.1 Compiler sans erreur avec `platformio run`
- [ ] 9.2 Vérifier le switch MODE_CLOCK ↔ MODE_COUNTDOWN sur le matériel (rotation TFT + rebuild UI)
- [ ] 9.3 Vérifier les boutons touch du countdown dans les deux états du remapping (rotation 3)
- [ ] 9.4 Vérifier que le countdown continue en background lors d'un switch vers MODE_CLOCK
- [ ] 9.5 Vérifier que la LED passe rouge clignotant quand le countdown atteint 0
