## 1. Nettoyage et configuration du projet

- [ ] 1.1 Supprimer les dépendances SD, XPT2046, SdFat et Adafruit SPIFlash de `platformio.ini`
- [ ] 1.2 Vider `src/main.cpp` de tout le code fonctionnel (boutons, SD, speaker, LDR, touch) en conservant la couche LVGL/TFT_eSPI/backlight
- [ ] 1.3 Créer `include/secrets.h` avec `WIFI_SSID` et `WIFI_PASSWORD` en placeholder
- [ ] 1.4 Ajouter `include/secrets.h` au `.gitignore`

## 2. WiFi et NTP

- [ ] 2.1 Ajouter les includes `WiFi.h`, `time.h`, `esp_sntp.h` dans `main.cpp`
- [ ] 2.2 Implémenter `initWifi()` : connexion non-bloquante avec timeout 30s, retour `bool`
- [ ] 2.3 Implémenter `initNTP()` : appel `configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "ntp.emi.u-bordeaux.fr")`
- [ ] 2.4 Implémenter `isNtpSynced()` : retourne `sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED`
- [ ] 2.5 Appeler `initWifi()` puis `initNTP()` dans `setup()`

## 3. LED RGB statut (non-bloquant)

- [ ] 3.1 Conserver l'initialisation PWM des LED RGB (pins 4/16/17, canaux 0/1/2) depuis le code existant
- [ ] 3.2 Implémenter `setLedColor(bool red, bool green, bool blue)` (actif LOW, common-anode)
- [ ] 3.3 Implémenter `updateStatusLed()` : rouge=no WiFi, vert=WiFi/no NTP, bleu=NTP OK — appelé toutes les 500ms via `millis()` dans `loop()`

## 4. Cadran analogique — fond et index

- [ ] 4.1 Créer un `lv_obj_t` blanc 240×240 à x=0, y=0 comme fond du cadran
- [ ] 4.2 Calculer et créer les 12 index horaires (rectangles noirs 12×4px) par trigonométrie sur le pourtour
- [ ] 4.3 Calculer et créer les 48 index minutes (rectangles noirs 6×2px) par trigonométrie
- [ ] 4.4 Appliquer les couleurs via `lv_style_t` avec `lv_style_set_bg_opa(..., LV_OPA_COVER)` pour chaque index

## 5. Canvas aiguilles

- [ ] 5.1 Allouer le buffer du canvas (240×240×2 bytes pour RGB565) en `static`
- [ ] 5.2 Créer le `lv_canvas_t` 240×240 superposé sur le fond blanc, à z-order supérieur
- [ ] 5.3 Implémenter `drawHands(int h, int m, int s)` : effacement canvas (fill blanc), dessin aiguille heure (noir 6px, 60px), minute (noir 4px, 90px), secondes (rouge 2px, 100px + disque rouge Ø10px à l'extrémité)
- [ ] 5.4 Vérifier que `lv_canvas_draw_line()` et `lv_canvas_fill_bg()` fonctionnent avec `lv_color_make()`

## 6. Panneau d'information droit

- [ ] 6.1 Créer un `lv_obj_t` noir 80×240 positionné à x=240, y=0
- [ ] 6.2 Créer le label heure digitale (blanc, police la plus grande disponible) aligné en haut du panneau
- [ ] 6.3 Créer le label date (blanc, police medium) sous le label heure
- [ ] 6.4 Créer le label statut WiFi/NTP (blanc, petite police) sous la date
- [ ] 6.5 Appliquer les couleurs texte et fond via `lv_style_t` + `LV_OPA_COVER`

## 7. Boucle principale et mise à jour

- [ ] 7.1 Dans `loop()`, implémenter la détection de changement de seconde via `localtime()` comparée à la valeur précédente
- [ ] 7.2 Sur changement de seconde : appeler `drawHands()` + mettre à jour les 3 labels du panneau droit
- [ ] 7.3 Sur changement de seconde : afficher "--:--:--" si `!isNtpSynced()`
- [ ] 7.4 Appeler `updateStatusLed()` toutes les 500ms via `millis()` (non-bloquant)
- [ ] 7.5 S'assurer que `lv_task_handler()` et `lv_tick_inc()` sont maintenus (code existant)

## 8. Validation

- [ ] 8.1 Compiler sans erreur avec `platformio run`
- [ ] 8.2 Vérifier l'affichage correct du cadran et des aiguilles sur le matériel
- [ ] 8.3 Vérifier la synchronisation NTP et l'affichage de l'heure locale correcte (Europe/Paris + DST)
- [ ] 8.4 Vérifier la LED RGB dans les 3 états (no WiFi, WiFi, NTP synced)
