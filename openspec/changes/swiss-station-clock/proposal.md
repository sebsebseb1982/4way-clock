## Why

L'ESP32 CYD (Cheap Yellow Display) avec LVGL fonctionne mais n'affiche qu'une démo générique. L'objectif est d'en faire une vraie horloge de gare suisse (Mondaine/SBB), synchronisée via NTP, qui exploite la configuration technique validée de l'application existante.

## What Changes

- Suppression de tout le fonctionnel existant (SD card, speaker, LED, LDR, boutons, touch)
- Remplacement par une horloge analogique style ferroviaire suisse (cadran + aiguilles) sur la moitié gauche de l'écran
- Ajout d'un panneau d'information droite : heure digitale, date, indicateur WiFi/NTP
- Connexion WiFi via `secrets.h` (SSID + password)
- Synchronisation de l'heure via NTP (`ntp.emi.u-bordeaux.fr`), timezone Europe/Paris (UTC+1/UTC+2 DST)
- LED RGB utilisée comme indicateur de statut WiFi/NTP (rouge = pas de WiFi, vert = connecté, bleu = NTP sync OK)
- Toute la configuration technique LVGL/TFT_eSPI/ST7789 est préservée à l'identique

## Capabilities

### New Capabilities

- `wifi-ntp-sync`: Connexion WiFi via secrets.h et synchronisation NTP périodique avec gestion timezone
- `clock-face`: Cadran analogique style SBB — cercle blanc, index horaires noirs, aiguilles heure/minute noires, trotteuse rouge avec disque rouge
- `info-panel`: Panneau droit avec heure digitale, date, et indicateur de statut WiFi/NTP
- `status-led`: LED RGB comme indicateur de statut (WiFi, NTP)

### Modified Capabilities

## Impact

- `src/main.cpp` : réécriture complète du fonctionnel, conservation de la couche TFT_eSPI/LVGL/backlight
- `platformio.ini` : ajout des dépendances WiFi et NTP (bibliothèques ESP32 Arduino SDK intégrées)
- `include/secrets.h` : nouveau fichier (hors contrôle de version) pour SSID et password
- Suppression des dépendances SD card, XPT2046, speaker dans platformio.ini
