## Context

L'application existante est une démo CYD (Cheap Yellow Display, ESP32 + ST7789 320×240) avec LVGL v8.3, TFT_eSPI et XPT2046. Elle fonctionne — ce qui est rare avec cette stack — grâce à une configuration très précise (pins, driver ST7789, gestion des couleurs via styles LVGL avec `LV_OPA_COVER`). L'objectif est de réutiliser exactement cette base pour construire une horloge de gare suisse (Mondaine/SBB).

**Contraintes techniques figées :**
- ESP32 DevKit, ST7789, 320×240 paysage, SPI @27MHz
- LVGL v8.3, `LV_COLOR_DEPTH 16`, backlight pin 21
- `lv_color_make()` / `lv_color_hex()` + `lv_style_set_bg_opa(..., LV_OPA_COVER)` pour les couleurs
- `lv_task_handler()` + `lv_tick_inc()` dans la loop

## Goals / Non-Goals

**Goals:**
- Afficher une horloge analogique style SBB fidèle, cadran à gauche (Ø ~230px)
- Panneau droit (80px) : heure digitale HH:MM:SS, date JJ/MM/AAAA, icône WiFi/NTP
- Synchronisation NTP via `ntp.emi.u-bordeaux.fr`, timezone Europe/Paris avec DST
- LED RGB comme indicateur silencieux : rouge=no WiFi, vert=WiFi OK, bleu=NTP synced
- Préserver à 100% la configuration LVGL/TFT_eSPI

**Non-Goals:**
- Interface tactile
- Carte SD, speaker
- Trotteuse "sweep" ou comportement SBB fidèle (pause 1.5s)
- OTA, serveur web embarqué

## Decisions

### D1 — Approche rendu : Hybride Canvas LVGL + primitives

**Choix :** Fond et index horaires en primitives LVGL (cercle `lv_arc`, rectangles `lv_obj`), aiguilles dessinées sur un `lv_canvas_t` trigonométrique mis à jour chaque seconde.

**Pourquoi :** Les primitives LVGL utilisent le système de styles qui *fonctionne* dans cette app (couleurs, opacité). Le canvas offre la liberté de dessiner des lignes d'épaisseur variable pour les aiguilles avec `lv_canvas_draw_line()`. C'est le seul moyen dans LVGL v8 de faire des lignes pivotantes sans rotation d'objet native.

**Alternative écartée :** Tout en canvas → perd le système de styles LVGL éprouvé. Tout en primitives → LVGL v8 n'a pas de rotation d'objet.

### D2 — Layout écran

```
┌─────────────────────────────────────────┐
│                                         │
│   ┌──────────────────┐  ┌───────────┐  │
│   │                  │  │  14:32:07 │  │
│   │   Cadran LVGL    │  │  03/04/26 │  │
│   │   Ø ~230px       │  │  WiFi ✓   │  │
│   │   canvas 240×240 │  │  NTP ✓    │  │
│   └──────────────────┘  └───────────┘  │
│         x=0..239            x=240..319  │
└─────────────────────────────────────────┘
          240px                  80px
```

Le cadran occupe un carré 240×240 centré verticalement sur le côté gauche. Le panneau droit est un `lv_obj` de 80×240 avec labels LVGL.

### D3 — Dessin du cadran

**Fond :** `lv_obj_t` blanc 240×240, positionné en x=0.

**Index horaires :** 12 rectangles noirs (heures, 12×4px) et 48 tirets noirs (minutes, 6×2px) disposés par calcul trigonométrique sur le pourtour du cercle, créés une seule fois à l'initialisation.

**Aiguilles :** Canvas 240×240 superposé au fond, redessiné chaque seconde :
- Effacement du canvas (fond blanc)
- Aiguille heure : ligne noire épaisse (6px), longueur 60px depuis le centre
- Aiguille minute : ligne noire épaisse (4px), longueur 90px depuis le centre  
- Trotteuse : ligne rouge (2px), longueur 100px + disque rouge Ø10px à l'extrémité

**Calcul angle :** `angle = (valeur / max) * 2π - π/2` pour démarrer à 12h.

### D4 — WiFi + NTP

**Bibliothèques :** `WiFi.h` + `time.h` (ESP32 Arduino SDK, pas de lib externe).

**NTP :** `configTime(offset_sec, dst_sec, "ntp.emi.u-bordeaux.fr")` — Europe/Paris = UTC+3600, DST=3600.

**Resync :** Le SDK ESP32 gère la resynchronisation automatiquement via SNTP. Pas de logique custom.

**Secrets :** `include/secrets.h` avec `#define WIFI_SSID "..."` et `#define WIFI_PASSWORD "..."`. Ajouté au `.gitignore`.

### D5 — LED RGB comme indicateur de statut

LED commune-anode (actif LOW) sur pins 4/16/17, canaux PWM 0/1/2 déjà configurés dans l'existant.

| État | Couleur |
|------|---------|
| Connexion WiFi en cours | Rouge clignotant |
| WiFi connecté, NTP en attente | Vert fixe |
| NTP synchronisé | Bleu fixe |
| Perte WiFi | Rouge fixe |

### D6 — Mise à jour de l'affichage

Le `loop()` : 
1. `lv_task_handler()` + `lv_tick_inc()` (inchangé)
2. Lecture `time()` → `localtime()`
3. Si seconde a changé → redessiner canvas aiguilles + mettre à jour labels panneau droit
4. Vérification statut WiFi toutes les 10s → mise à jour LED + indicateur

## Risks / Trade-offs

**[Performance canvas]** → Le canvas 240×240 en RGB565 = 115Ko. Clear + redraw chaque seconde peut être lent. **Mitigation :** Redessiner uniquement sur changement de seconde (pas chaque loop). Si trop lent, réduire à une zone clipping.

**[Couleurs LVGL sur canvas]** → `lv_canvas_draw_line()` utilise `lv_draw_line_dsc_t` avec `color` directement, pas de système de styles. Tester avec `lv_color_make()`. **Mitigation :** Testé dans l'app existante via `lv_color_make(r,g,b)` → fonctionne.

**[DST automatique]** → `configTime` avec offsets fixes ne gère pas le changement d'heure automatiquement. **Mitigation :** Utiliser la string POSIX timezone `"CET-1CEST,M3.5.0,M10.5.0/3"` via `setenv("TZ", ...)` + `tzset()` — gère le DST automatiquement.

**[NTP indisponible au boot]** → Si pas de WiFi, `localtime()` retourne l'epoch (01/01/1970). **Mitigation :** Afficher "--:--:--" tant que NTP pas synced (vérifier via `sntp_get_sync_status()`).

## Open Questions

- Aucune — toutes les décisions ont été prises en explore mode.
