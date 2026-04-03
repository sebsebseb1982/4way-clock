## Why

Le CYD (Cheap Yellow Display) n'affiche qu'une seule application fixe. En ajoutant un accéléromètre MPU6050, l'orientation physique de l'appareil devient un sélecteur naturel de mode : chaque bord posé sur la table active une fonctionnalité différente, sans toucher d'interface.

## What Changes

- Ajout du driver MPU6050 (I2C, SDA=22, SCL=27) pour détecter l'orientation de l'appareil
- Implémentation d'un système de sélection de mode basé sur l'orientation (4 modes, debounce 100ms)
- **BREAKING** : refactoring de `main.cpp` en modules séparés (`src/`) pour chaque mode d'affichage
- Réactivation du touchscreen XPT2046 Bitbang (supprimé dans le change `swiss-station-clock`) pour tous les modes, avec remapping des coordonnées selon la rotation active
- Ajout d'un mode **Countdown** (compte à rebours réglable via boutons touch) avec état background persistant
- Ajout de deux modes stub **Mode C** et **Mode D** (contenu défini ultérieurement)
- Le changement de mode entraîne un changement de rotation TFT (`setRotation`) et une reconstruction de l'UI LVGL

## Capabilities

### New Capabilities

- `orientation-detection` : lecture MPU6050, détection de l'axe dominant, debounce 100ms, état de mode global
- `mode-switching` : changement de rotation TFT, destruction/reconstruction de l'UI LVGL, gestion de la rotation active
- `touch-driver` : réactivation XPT2046 Bitbang avec remapping des coordonnées touch selon la rotation active
- `countdown-mode` : mode compte à rebours avec state machine background, boutons touch +/-H/M/S, démarrage/reset

### Modified Capabilities

## Impact

- `src/main.cpp` : réduit à l'orchestration (loop, init, mode switch) ; code métier déplacé en modules
- Nouveaux fichiers `src/mpu.cpp`, `src/mpu.h`, `src/touch_mgr.cpp`, `src/touch_mgr.h`, `src/ui_clock.cpp`, `src/ui_clock.h`, `src/ui_countdown.cpp`, `src/ui_countdown.h`, `src/ui_stub.cpp`, `src/ui_stub.h`
- `platformio.ini` : ajout bibliothèque MPU6050 (ex: `electroniccats/MPU6050` ou implémentation I2C directe)
- `include/secrets.h` : inchangé
