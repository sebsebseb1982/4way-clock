## Context

L'application actuelle (`swiss-station-clock`) affiche une unique horloge de gare sur le CYD (ESP32 + ST7789 320×240, LVGL v8). Le nom du projet `4way-clock` anticipe depuis le départ 4 modes d'affichage. L'ajout d'un accéléromètre MPU6050 (I2C, SDA=22, SCL=27) permet de détecter l'orientation physique de l'appareil et de sélectionner le mode correspondant.

L'état actuel du code (`main.cpp`, ~420 lignes) est monolithique. Ce changement introduit une architecture modulaire par fichiers séparés dans `src/`.

## Goals / Non-Goals

**Goals:**
- Détecter les 4 orientations cardinales du CYD via MPU6050 (debounce 100ms)
- Switcher le mode d'affichage (rotation TFT + rebuild UI LVGL) selon l'orientation
- Réactiver le touchscreen XPT2046 avec remapping des coordonnées pour tous les modes
- Implémenter le mode Countdown (compte à rebours fonctionnel avec boutons touch)
- Implémenter les modes C et D comme stubs affichant un message de placeholder
- Découper le code en modules `src/`

**Non-Goals:**
- Définir le contenu des modes C et D
- Animation de transition entre les modes
- Gestion de l'inclinaison (pitch/roll) — uniquement l'orientation plane
- Persistance de la configuration entre reboots (pas de NVS/EEPROM)

## Decisions

### D1 — Lecture MPU6050 sans bibliothèque externe

**Choix :** Lecture directe I2C via `Wire.h` (ESP32 Arduino SDK) — pas de lib externe.

**Rationale :** Le MPU6050 ne nécessite que 6 registres d'accéléromètre (0x3B–0x40) + wake-up (0x6B). Une lib externe ajouterait des dépendances inutiles. `Wire.beginTransmission / requestFrom` suffit.

**Alternative écartée :** `electroniccats/MPU6050` — surcharge et risque de conflit avec les canaux I2C existants.

### D2 — Bus I2C dédié pour le MPU6050

**Choix :** `Wire.begin(22, 27)` — bus I2C n°1 (SDA=22, SCL=27). L'ESP32 a deux bus I2C hardware. Le bus 0 (SDA=21, SCL=22) est libre (le CYD actuel n'utilise pas l'I2C), mais les pins 22/27 sont explicitement spécifiés par l'utilisateur.

**Rationale :** Isoler le MPU sur son propre bus évite toute collision si un futur périphérique I2C est ajouté sur bus 0.

### D3 — Détection d'orientation : seuil sur axe dominant

**Choix :**
```
RAW = lire Ax, Ay (accéléromètre 16-bit, échelle ±2g → diviseur 16384)
axe_dominant = max(|Ax|, |Ay|)
Si axe_dominant < 0.7g → AMBIGUOUS (pas de changement)
Sinon :
  Ax > +0.7g → MODE_CLOCK      (bord droit posé)
  Ax < -0.7g → MODE_COUNTDOWN  (bord gauche posé)
  Ay > +0.7g → MODE_C          (bord haut posé)
  Ay < -0.7g → MODE_D          (bord bas posé)
```

**Debounce :** Si `orientation_candidate` est stable >= 100ms → switch effectif.

**Rationale :** Le seuil à 0.7g donne ±45° de tolérance angulaire (cos(45°)≈0.7). La zone AMBIGUOUS évite les faux positifs quand le CYD est à plat.

### D4 — Rotation TFT + rebuild UI LVGL

**Choix :** Séquence de switch de mode :
```cpp
tft.setRotation(n);
disp_drv.hor_res = (n % 2 == 0) ? 240 : 320;
disp_drv.ver_res = (n % 2 == 0) ? 320 : 240;
lv_disp_drv_update(lv_disp_get_default(), &disp_drv);
lv_obj_clean(lv_scr_act());
ui_XXX_build();
```

**Mapping rotation TFT :**
```
MODE_CLOCK      → setRotation(1) : paysage 320×240  (USB bas)
MODE_COUNTDOWN  → setRotation(3) : paysage 320×240  (USB haut)
MODE_C          → setRotation(0) : portrait 240×320 (USB droite)
MODE_D          → setRotation(2) : portrait 240×320 (USB gauche)
```

**Alternative écartée :** LVGL sw_rotate — trop lent, nécessite un buffer double-taille.

### D5 — Touchscreen : remapping coordonnées

**Choix :** Après `getTouch()`, transformer (x, y) selon la rotation active :
```
rotation=0 : (x, y) → (x, y)          portrait normal
rotation=1 : (x, y) → (y, 240-x)      paysage
rotation=2 : (x, y) → (240-x, 320-y)  portrait inversé
rotation=3 : (x, y) → (320-y, x)      paysage inversé
```

La calibration SPIFFS sera relancée si nécessaire (flag `RERUN_CALIBRATE`).

### D6 — Countdown : state machine background

**Choix :** La variable `countdown_remaining_ms` (uint32_t) est décrémentée dans la loop() via `millis()` delta, indépendamment du mode affiché.

```
États :
  IDLE    → configuration (boutons +/-)
  RUNNING → décompte actif (delta millis)
  DONE    → 00:00:00 atteint (LED rouge clignotante)
```

Le mode DONE persiste jusqu'à appui sur "Réinitialiser".

### D7 — Architecture fichiers

```
src/
  main.cpp          orchestration, setup(), loop(), mode switch
  mpu.cpp / .h      init Wire, lecture Ax/Ay, détection orientation, debounce
  touch_mgr.cpp/.h  XPT2046 init, getTouch() + remapping par rotation
  ui_clock.cpp/.h   build + update UI horloge (code extrait de main.cpp actuel)
  ui_countdown.cpp/.h  build + update + state machine countdown
  ui_stub.cpp/.h    build UI placeholder pour modes C et D
```

## Risks / Trade-offs

**[Wire + SPIFFS simultanés]** → SPIFFS pour calibration touch utilise le même task que Wire. Initialiser SPIFFS avant Wire. **Mitigation :** ordre d'init fixé dans setup().

**[lv_obj_clean au switch de mode]** → Détruire tous les objets LVGL libère la mémoire heap correctement si les objets sont enfants de `lv_scr_act()`. Le canvas_buf du clock est alloué en malloc statique — ne pas le libérer entre les modes pour éviter la fragmentation. **Mitigation :** malloc une seule fois au boot, garder le pointeur global.

**[Countdown background en mode horloge]** → La loop décrémente le countdown même si l'UI clock est affichée. Si `countdown_remaining_ms` atteint 0 en dehors du mode countdown, la LED passe rouge mais l'UI ne le montre pas immédiatement. **Mitigation :** acceptable — l'utilisateur tourne le CYD pour voir le countdown.

**[Calibration touch par rotation]** → La calibration SPIFFS est stockée pour une seule orientation. Le remapping mathématique D5 est une approximation. **Mitigation :** suffisant pour des boutons de taille généreuse (min 60×60px).

## Open Questions

- Aucune — toutes les décisions ont été prises en explore mode.
