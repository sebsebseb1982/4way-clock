## Why

Le projet CYD (Cheap Yellow Display / ESP32-2432S028) dispose d'un écran TFT 2.8" intégré mais n'a pas encore d'application. L'objectif est d'en faire une horloge de gare suisse esthétique, autonome et synchronisée par réseau, valorisant le matériel disponible.

## What Changes

- Création d'un projet PlatformIO complet ciblant le CYD (ESP32-2432S028)
- Affichage d'une horloge de gare suisse animée (aiguilles, cadran, point rouge) sur fond noir via LVGL
- Affichage de l'heure numérique en-dessous du cadran en police Helvetica
- Synchronisation de l'heure via NTP (`ntp.emi.u-bordeaux.fr`)
- Connexion WiFi configurable via `secrets.h` (SSID + mot de passe)

## Capabilities

### New Capabilities

- `wifi-ntp-sync`: Connexion WiFi et synchronisation de l'heure depuis le serveur NTP ; gestion du fuseau horaire Europe/Paris
- `swiss-clock-display`: Rendu de l'horloge de gare suisse (cadran, aiguilles, point rouge) animé en temps réel via LVGL sur fond noir
- `digital-time-display`: Affichage de l'heure en chiffres (HH:MM:SS) sous le cadran analogique en police Helvetica via LVGL

### Modified Capabilities

## Impact

- **Matériel cible** : ESP32-2432S028 (CYD) — TFT ILI9341 2.8" 320×240, rétroéclairage, pas de carte SD requise
- **Dépendances** : PlatformIO, LVGL (v8.x), TFT_eSPI ou lv_drivers pour le driver d'écran, bibliothèque Arduino WiFi + NTPClient ou sntp intégré ESP-IDF
- **Fichiers créés** : `platformio.ini`, `src/main.cpp`, `src/secrets.h` (template), `src/clock_ui.cpp/h`, `lv_conf.h`
- **Réseau** : accès WiFi requis au démarrage ; l'horloge fonctionne en mode dégradé (heure invalide affichée) si NTP échoue
