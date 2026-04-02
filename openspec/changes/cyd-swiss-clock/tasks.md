## 1. Project Scaffolding

- [x] 1.1 Créer le projet PlatformIO : `pio init --board esp32dev` dans le dossier `cyd-swiss-clock/`
- [x] 1.2 Configurer `platformio.ini` : board `esp32dev`, framework `arduino`, lib_deps (`bodmer/TFT_eSPI`, `lvgl/lvgl@^8.4.0`), build_flags pour inclure `lv_conf.h` et `User_Setup.h`
- [x] 1.3 Copier et adapter `User_Setup.h` pour le CYD (ILI9341 320×240, pins SPI corrects, rotation paysage)
- [x] 1.4 Copier `lv_conf.h` depuis les exemples LVGL et activer : `LV_COLOR_DEPTH 16`, `LV_FONT_MONTSERRAT_28`, `LV_USE_CANVAS`, `LV_USE_LINE`, `LV_USE_ARC`, `LV_USE_LABEL`
- [x] 1.5 Créer `include/secrets.h.example` avec `#define WIFI_SSID "your_ssid"` et `#define WIFI_PASSWORD "your_password"`
- [x] 1.6 Créer `include/secrets.h` vide (non commité) et ajouter `secrets.h` à `.gitignore`
- [x] 1.7 Ajouter dans `include/secrets.h` une garde de compilation

## 2. WiFi & NTP (wifi_ntp.cpp / wifi_ntp.h)

- [x] 2.1 Implémenter `wifiConnect()` : connexion WiFi avec 20 tentatives × 500 ms, retour `bool`
- [x] 2.2 Implémenter `ntpInit()` : appel `configTzTime(...)` + attente sync (max 15 s)
- [x] 2.3 Implémenter `getLocalTime(struct tm *out)` : wrapper `localtime_r` thread-safe, retourne `false` si jamais synchronisé
- [x] 2.4 Implémenter la logique de retry : si WiFi ou NTP échoue, planifier une nouvelle tentative toutes les 30 s via un flag et un timer dans `loop()`
- [ ] 2.5 Tester la connexion WiFi et valider que `getLocalTime` retourne l'heure correcte (Europe/Paris, heure été/hiver)

## 3. Cadran de l'horloge (clock_face.cpp / clock_face.h)

- [ ] 3.1 Créer la fonction `clockFaceCreate(lv_obj_t *parent)` qui retourne un objet LVGL représentant le cadran
- [ ] 3.2 Dessiner le cercle du cadran (blanc, ⌀ 210 px) centré en haut de l'écran (x=160, y=105)
- [ ] 3.3 Dessiner les 60 graduations de minutes (petits traits) et les 12 graduations d'heures (grands traits gras)
- [ ] 3.4 Dessiner le hub central (petit cercle noir au centre du cadran)
- [ ] 3.5 Vérifier que le fond global LVGL est noir (via `lv_obj_set_style_bg_color` sur le screen)

## 4. Aiguilles et animation (clock_hands.cpp / clock_hands.h)

- [ ] 4.1 Implémenter `clockHandsCreate(lv_obj_t *parent)` : créer les 3 objets `lv_line` (heures, minutes, secondes) avec les styles appropriés (épaisseur, couleur noire)
- [ ] 4.2 Implémenter `clockHandsUpdate(const struct tm *t)` : calculer les angles (° flottant) de chaque aiguille depuis les champs `tm_hour`, `tm_min`, `tm_sec`, mettre à jour les points de `lv_line`
- [ ] 4.3 Implémenter la logique d'aiguille des secondes SBB : vitesse 360°/58.5 s, arrêt de 1.5 s à 12h, reprise synchronisée sur l'heure NTP
- [ ] 4.4 Créer le disque rouge (`lv_obj` ellipse 16×16 px, couleur rouge #E8192C) et le solidariser à l'aiguille des secondes
- [ ] 4.5 Valider que les aiguilles sont dans le bon ordre de superposition (secondes au-dessus)

## 5. Affichage numérique (intégré dans main.cpp ou digital_display.cpp)

- [ ] 5.1 Créer un `lv_label` LVGL avec la police `lv_font_montserrat_28`, couleur blanche, aligné horizontalement en bas de l'écran (y ≈ 215)
- [ ] 5.2 Implémenter `digitalDisplayUpdate(const struct tm *t)` : formater en `HH:MM:SS` via `snprintf` et appeler `lv_label_set_text`
- [ ] 5.3 Afficher `--:--:--` si `getLocalTime` retourne `false`
- [ ] 5.4 Vérifier l'absence de chevauchement entre le label numérique et le cadran analogique

## 6. Boucle principale (main.cpp)

- [ ] 6.1 Dans `setup()` : init TFT_eSPI, init LVGL avec buffer partiel (1/10 écran), attacher le flush callback `lv_disp_drv`, appeler `wifiConnect()` puis `ntpInit()`
- [ ] 6.2 Dans `setup()` : créer les objets LVGL (cadran, aiguilles, label numérique) via les fonctions des modules
- [ ] 6.3 Dans `loop()` : appeler `lv_tick_inc(millis_delta)` et `lv_timer_handler()` toutes les ≤ 10 ms
- [ ] 6.4 Dans `loop()` : appeler `clockHandsUpdate` et `digitalDisplayUpdate` avec le temps local courant toutes les ~100 ms
- [ ] 6.5 Dans `loop()` : implémenter le retry WiFi/NTP toutes les 30 s si la connexion a échoué

## 7. Validation finale

- [ ] 7.1 Compiler le projet avec `pio run` sans erreur ni warning bloquant
- [ ] 7.2 Flasher sur le CYD et vérifier l'affichage du cadran sur fond noir
- [ ] 7.3 Vérifier que l'heure analogique et numérique correspond à l'heure locale exacte (Europe/Paris)
- [ ] 7.4 Vérifier le comportement SBB de l'aiguille des secondes (pause à 12h)
- [ ] 7.5 Tester la robustesse : démarrer sans WiFi, vérifier l'affichage de `--:--:--` et la reprise automatique
