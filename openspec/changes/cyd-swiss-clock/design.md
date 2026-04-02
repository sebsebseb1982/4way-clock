## Context

Le CYD (ESP32-2432S028) est un module ESP32 avec écran TFT ILI9341 2.8" (320×240) intégré, rétroéclairage LED, et connectivité WiFi/Bluetooth. Il est ciblé via PlatformIO avec le framework Arduino.

L'application doit afficher une réplique de l'horloge de gare suisse (design Hans Hilfiker, 1944) en temps réel, synchronisée par NTP, sans aucune interaction utilisateur requise après le démarrage.

## Goals / Non-Goals

**Goals:**
- Projet PlatformIO complet, compilable et flashable sur CYD sans modification autre que `secrets.h`
- Rendu LVGL de l'horloge : cadran blanc sur fond noir, aiguilles noires, point/disque rouge sur l'aiguille des secondes
- Animation fluide : aiguille des minutes avance par petits pas, aiguille des secondes fait une révolution en ~58,5 s puis attend à 12h avant de repartir (comportement SBB authentique)
- Heure numérique HH:MM:SS en Helvetica (ou équivalent embarqué) sous le cadran
- Synchronisation NTP au démarrage + resynchronisation périodique (toutes les heures)
- Configuration WiFi isolée dans `include/secrets.h`

**Non-Goals:**
- Interface de configuration (pas de portail captif, pas d'OTA)
- Gestion du tactile (le CYD a un écran résistif mais il n'est pas utilisé ici)
- Support multi-fuseau ou sélection de serveur NTP depuis l'UI
- Fonctionnement hors-ligne prolongé (si NTP échoue au boot, l'heure est invalide)

## Decisions

### D1 — Framework UI : LVGL v8 (pas v9)
**Choix** : LVGL 8.4.x via la bibliothèque PlatformIO `lvgl/lvgl@^8.4.0`.  
**Raison** : v8 est stable, bien documentée pour ESP32/Arduino, compatible avec les drivers TFT_eSPI disponibles. v9 est encore jeune et peu de bindings Arduino existent.  
**Alternative écartée** : TFT_eSPI seul (plus simple mais animation frame-by-frame plus complexe à maintenir).

### D2 — Driver d'écran : TFT_eSPI
**Choix** : `bodmer/TFT_eSPI` comme backend LVGL (`lv_disp_drv`).  
**Raison** : Support natif ILI9341 sur ESP32, configurations CYD largement documentées dans la communauté. Le fichier `User_Setup.h` est préconfiguré pour le CYD (pins SPI, résolution 320×240).  
**Alternative écartée** : `Adafruit_ILI9341` (plus lent, moins d'optimisations DMA).

### D3 — NTP : sntp ESP-IDF via `configTzTime`
**Choix** : API `esp_sntp` / `configTzTime` fournie par l'ESP-IDF intégré dans le framework Arduino ESP32.  
**Raison** : Pas de bibliothèque externe, gestion native du fuseau horaire POSIX (`CET-1CEST,M3.5.0,M10.5.0/3`), `localtime_r` thread-safe.  
**Alternative écartée** : `arduino-NTPClient` (nécessite une boucle manuelle, pas de gestion du DST).

### D4 — Police : Helvetica embarquée en bitmap LVGL
**Choix** : Utiliser `lv_font_montserrat_28` (inclus dans LVGL) pour l'affichage numérique ; optionnellement générer une police Helvetica custom avec `lv_font_conv` si la ressemblance est critique.  
**Raison** : Montserrat est géométrique et proche de Helvetica, disponible sans étape de build supplémentaire. Une police custom augmente la taille du firmware.  
**Alternative** : Police custom générée depuis une TTF Helvetica libre (TeX Gyre Heros) — documentée dans tasks.md comme étape optionnelle.

### D5 — Architecture du code
```
src/
  main.cpp          # setup() / loop() : init WiFi, NTP, LVGL, boucle de rendu
  clock_face.cpp/h  # Dessin du cadran LVGL (canvas + primitives)
  clock_hands.cpp/h # Animation des aiguilles (calcul angles, update positions)
  wifi_ntp.cpp/h    # Connexion WiFi + init NTP + getter heure locale
include/
  secrets.h         # WIFI_SSID, WIFI_PASSWORD (gitignored)
  lv_conf.h         # Configuration LVGL
```

### D6 — Rendu de l'horloge SBB
- Cadran : cercle blanc 210px de diamètre centré sur la moitié haute de l'écran (y_center ~100)
- Graduations : 60 petits traits, 12 grands traits (heures)
- Aiguilles : dessinées via `lv_canvas` ou `lv_line` + rotation par matrice
- Aiguille des secondes : animation en 58,5 s (pas de 360°/58,5 ≈ 6,15°/s), arrêt à 12h pendant ~1,5 s, puis repartie au signal NTP — simulée par timer LVGL
- Disque rouge : `lv_obj` ellipse rouge (16×16 px) centré sur le pivot des secondes

## Risks / Trade-offs

- **Taille mémoire LVGL + TFT_eSPI** → Utiliser un seul framebuffer partiel (1/10e de l'écran) pour économiser la SRAM. Risque d'artefacts visuels si le flush est trop lent. Mitigation : activer DMA SPI dans TFT_eSPI.
- **Précision de l'animation SBB** → Le comportement d'attente à 12h est une approximation logicielle (timer LVGL). Il ne sera pas synchronisé à la milliseconde avec une vraie horloge SBB. Mitigation : acceptable pour un usage décoratif.
- **Disponibilité NTP** → Le serveur `ntp.emi.u-bordeaux.fr` est un serveur universitaire, pas garanti 24/7. Mitigation : en cas d'échec, afficher "--:--:--" et réessayer toutes les 30 s.
- **Police Helvetica** → Aucune fonte Helvetica authentique n'est libre de droits. Mitigation : Montserrat ou TeX Gyre Heros (licence libre) sont visuellement très proches.
- **Orientation écran CYD** → Le CYD peut être câblé en portrait ou paysage selon le variant. Mitigation : forcer `TFT_ROTATION 1` (paysage 320×240) dans `User_Setup.h`.

## Open Questions

- Faut-il afficher la date sous l'heure numérique ? (supposé non pour l'instant)
- Luminosité du rétroéclairage : fixe à 100 % ou dimmer après inactivité ? (supposé fixe)
