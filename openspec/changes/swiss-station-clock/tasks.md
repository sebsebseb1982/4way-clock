## 1. Nettoyage et configuration du projet

- [x] 1.1 Supprimer les dépendances SD, XPT2046, SdFat et Adafruit SPIFlash de `platformio.ini`
- [x] 1.2 Vider `src/main.cpp` de tout le code fonctionnel (boutons, SD, speaker, LDR, touch) en conservant la couche LVGL/TFT_eSPI/backlight
- [x] 1.3 Créer `include/secrets.h` avec `WIFI_SSID` et `WIFI_PASSWORD` en placeholder
- [x] 1.4 Ajouter `include/secrets.h` au `.gitignore`

## 2. WiFi et NTP

- [x] 2.1 Ajouter les includes `WiFi.h`, `time.h`, `esp_sntp.h` dans `main.cpp`
- [x] 2.2 Implémenter `initWifi()` : connexion non-bloquante avec timeout 30s, retour `bool`
- [x] 2.3 Implémenter `initNTP()` : appel `configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "ntp.emi.u-bordeaux.fr")`
- [x] 2.4 Implémenter `isNtpSynced()` : retourne `sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED`
- [x] 2.5 Appeler `initWifi()` puis `initNTP()` dans `setup()`

## 3. LED RGB statut (non-bloquant)

- [x] 3.1 Conserver l'initialisation PWM des LED RGB (pins 4/16/17, canaux 0/1/2) depuis le code existant
- [x] 3.2 Implémenter `setLedColor(bool red, bool green, bool blue)` (actif LOW, common-anode)
- [x] 3.3 Implémenter `updateStatusLed()` : rouge=no WiFi, vert=WiFi/no NTP, bleu=NTP OK — appelé toutes les 500ms via `millis()` dans `loop()`

## 4. Cadran analogique — fond et index

- [x] 4.1 Créer un `lv_obj_t` blanc 240×240 à x=0, y=0 comme fond du cadran
- [x] 4.2 Calculer et créer les 12 index horaires (rectangles noirs 12×4px) par trigonométrie sur le pourtour
- [x] 4.3 Calculer et créer les 48 index minutes (rectangles noirs 6×2px) par trigonométrie
- [x] 4.4 Appliquer les couleurs via `lv_style_t` avec `lv_style_set_bg_opa(..., LV_OPA_COVER)` pour chaque index

## 5. Canvas aiguilles

- [x] 5.1 Allouer le buffer du canvas (240×240×2 bytes pour RGB565) en `static`
- [x] 5.2 Créer le `lv_canvas_t` 240×240 superposé sur le fond blanc, à z-order supérieur
- [x] 5.3 Implémenter `drawHands(int h, int m, int s)` : effacement canvas (fill blanc), dessin aiguille heure (noir 6px, 60px), minute (noir 4px, 90px), secondes (rouge 2px, 100px + disque rouge Ø10px à l'extrémité)
- [x] 5.4 Vérifier que `lv_canvas_draw_line()` et `lv_canvas_fill_bg()` fonctionnent avec `lv_color_make()`

## 6. Panneau d'information droit

- [x] 6.1 Créer un `lv_obj_t` noir 80×240 positionné à x=240, y=0
- [x] 6.2 Créer le label heure digitale (blanc, police la plus grande disponible) aligné en haut du panneau
- [x] 6.3 Créer le label date (blanc, police medium) sous le label heure
- [x] 6.4 Créer le label statut WiFi/NTP (blanc, petite police) sous la date
- [x] 6.5 Appliquer les couleurs texte et fond via `lv_style_t` + `LV_OPA_COVER`

## 7. Boucle principale et mise à jour

- [x] 7.1 Dans `loop()`, implémenter la détection de changement de seconde via `localtime()` comparée à la valeur précédente
- [x] 7.2 Sur changement de seconde : appeler `drawHands()` + mettre à jour les 3 labels du panneau droit
- [x] 7.3 Sur changement de seconde : afficher "--:--:--" si `!isNtpSynced()`
- [x] 7.4 Appeler `updateStatusLed()` toutes les 500ms via `millis()` (non-bloquant)
- [x] 7.5 S'assurer que `lv_task_handler()` et `lv_tick_inc()` sont maintenus (code existant)

## 8. Validation

- [x] 8.1 Compiler sans erreur avec `platformio run`
- [ ] 8.2 Vérifier l'affichage correct du cadran et des aiguilles sur le matériel
- [ ] 8.3 Vérifier la synchronisation NTP et l'affichage de l'heure locale correcte (Europe/Paris + DST)
- [ ] 8.4 Vérifier la LED RGB dans les 3 états (no WiFi, WiFi, NTP synced)

