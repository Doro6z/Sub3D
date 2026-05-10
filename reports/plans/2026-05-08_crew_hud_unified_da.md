# Crew HUD · Unified Design Architecture

Date: 2026-05-08 (round 3)
Workspace: `C:\Dev\Sub3D`
Status: **DA validated** — 4/4 scope questions answered. Implementation gate.
Authority-max plan: `reports/plans/2026-04-10_first_playable_strategic_analysis.md`
Supersedes: `reports/research/2026-05-08_crew_hud_design_analysis.md` §7 (eight-concept space replaced by six-layer architecture)
References:
- `reports/ui-pillars.md` (governance)
- `reports/ui-references/` (gallery, S01/S02 implementations)
- GDD board referenced 2026-05-08 (`Sub3D_GDD_Board (2).html` — local download)
- Existing helm widgets: `Source/Sub3D/Submarine/Helm/`

---

## 0. Why this document exists

After 3 rounds of mockups, the user committed to a six-layer architecture with strict information ownership. This file is the binding contract between concepts and implementation. Concepts that don't fit a layer are dropped; data that doesn't fit a layer doesn't ship.

---

## 1. The six layers

| # | Layer | Owner | Default visibility | Trigger to elevate |
|---|---|---|---|---|
| L1 | **Personal HUD** (ambient) | Crew character | Always-on, faint | Threshold-based degradation |
| L2 | **Statut Personnage** | Crew character | Aperçu always-on bottom-right · open on `C` key | Click aperçu or press `C` |
| L3 | **Helmet HUD** | Worn helmet item | Only when helmet equipped | Equip wearable |
| L4 | **Terminals @ Stations** | Submarine props | Only when at a station | Approach + activate station |
| L5 | **Context layer** | World focus target | Always — crosshair morphs on look-at | Auto on look-at |
| L6 | **Progressive reveal** | Crafted tools | Only when matching tool held/equipped | Item-driven |

### 1.1 Information ownership matrix

> A given data point belongs to **exactly one** layer. No data is duplicated across layers.

| Data | Layer | Notes |
|---|---|---|
| HP, Fatigue, XP, pulse, posture | L2 | Aperçu = imprecise. Open Statut for precise + per-limb. |
| Active afflictions, wounds | L2 | Hover member in detail view → tooltip. |
| Stamina | L1 | Audio breath + degraded movement only. **No bar.** |
| Wet, cold, ambient feedback | L1 | Camera FX (drops, breath frost). No icon. |
| HP critical alert | L1 | Pulse + late-stage screen feedback. Not a bar. |
| Skills tree, attribute axes (5), unlocked statuts | L2 | New tab `Skills` (standby content). |
| Crew roster, basic state | Future L4 (Bridge terminal) or L2 tab `Notes` | TBD per gameplay scope. |
| Crew detailed health | L6 (Medical scanner item) | Default = no info. |
| Held tool, hotbar | L1 | Bottom hotbar 6 slots + Held. |
| Compartment label | L4 (Bridge / Hull terminal) | Not on personal HUD. |
| Depth, ambient pressure | L3 (helmet) **or** L4 (Sensor terminal) | Personal HUD never shows depth without helmet. |
| Suit resistance / depth nominal | L3 only | Helmet readout. |
| Heading, speed, tilt | L4 (Sensor terminal) | Helm station hosts it. |
| Hull integrity, breach map | L4 (Integrity terminal) | Volumetric view, popup-broadcasts alerts to crew. |
| Sonar contacts | L4 (Sonar terminal) | Plus hint sound on Personal HUD if dramatic. |
| Through-wall vision | L6 (Frequency analyzer / Hull tomograph) | Default = none. |
| Interaction prompt (key + verb) | L5 | Always present at crosshair when an interactable is in scope. |
| Item type + name | L5 | Yellow label `#f4d03f` when looking at interactable. **No owner.** |
| Active interaction list (multiple actions) | L5 + main key F | One default key. Detail accessible only via L4 station or L2 carnet. |

---

## 2. Scope decisions (validated 2026-05-08)

| # | Question | Decision |
|---|---|---|
| Q1 | Statut Personnage form | **Hybride : carnet qui sort.** Slide + rotate animation depuis bottom-right. Pages internes papier. Animation diégétique, contenu carnet. |
| Q2 | Statut key | **C** (character). Mnemonic, free in `IMC_OnFoot`. |
| Q3 | L5 Context reveal | **Auto sur look-at.** Crosshair morphs → context icon. Label `type / nom` appears with ~150ms delay. No focus key needed. |
| Q4 | Palette | **Garder muted + ajouter `#f4d03f` (jaune GDD) pour labels contextuels uniquement.** No saturated GDD cyan in main HUD. |

---

## 3. Architecture details per layer

### 3.1 L1 — Personal HUD (ambient)

**Purpose:** minimum vital ambient signal + camera FX. Player feels state without reading numbers.

**Components:**

- Crosshair center (existing).
- Aperçu Statut bottom-right (~160×96), see L2.
- Hotbar 6 slots + Held bottom-center (existing pattern).
- Heart pulse icon (cardiac rhythm = stamina + HP coupling).
- Posture peek (small silhouette in aperçu, mirrors current pose).
- Camera FX: breath frost, water droplets, wet vignette, FOV breath when winded — all driven by gameplay state, not by HUD widgets.

**Forbidden in L1:**

- Bars beyond what's already on the aperçu.
- Numbers (depth, pressure, etc.) — these belong to L3/L4.
- Crew roster.
- Submarine state.

**Rule:** L1 should remain readable in your peripheral vision. Anything that demands center-fovea attention belongs to L2/L4.

### 3.2 L2 — Statut Personnage (the heart of round 3)

**Form factor:** hybride carnet qui sort. Mouvement type slide-out + tilt depuis bottom-right; intérieur = pages papier avec onglets latéraux.

**Aperçu (always-on, ~160×96):**

- Bordure du carnet visible (semble peeking depuis l'écran).
- Mini stick figure mirroring posture.
- HP imprecise indicator: 5 segments hatch-style (no number).
- Heart icon pulsing.

**Click aperçu ou touche `C` :**

- Animation slide-out + rotate (300–400 ms ease-out).
- Carnet ouvre à ~520×340 ancré bottom-right.
- Une page principale + onglets latéraux gauches.

**Onglets latéraux (mini-menu, gauche du carnet) :**

| Onglet | Contenu | Statut |
|---|---|---|
| `Status` | Stick figure + bars + afflictions par membre | Active par défaut |
| `Skills` | 5 axes attributs (Métabolisme, Kinésie, Neurologie, Physiologie, Technicité) + 5 statuts/rôles unlockables (Capitaine, Mécanicien, Plongeur EVA, Armurier, Artisan) | **Standby content** |
| `Notes` | Carnet libre · GDD §social tableau collectif (placeholder) | Standby |
| `Medical` | Locked icon — apparaît seulement si Medical scanner crafté (L6) | Locked default |
| `+1 slot` | Réservé pour modules futurs | — |

**Page Status (principale) :**

- Stick figure / dessin trait au centre, posture-aware.
- Membres survolables :
  - Hover head/torso/L-arm/R-arm/L-leg/R-leg → tooltip avec afflictions sur ce membre (bruise, cut, broken, frostbite, …).
  - Sinon : icônes simples sur les membres concernés.
- Sous le perso : 3 barres exactes : `HP`, `Fatigue`, `XP`. Pas de stamina, pas d'O₂.
- Heart pulse animé en bas du panel.
- Ligne d'état texte : `Standing · 72 bpm` — informationnel, pas warning.

**Animation de fermeture :** symétrique de l'ouverture. Esc ou re-press `C` ferme.

**Pas de pause :** le Statut ouvert n'arrête pas le jeu. Coop-safe. (P7 du pillars.)

### 3.3 L3 — Helmet HUD (worn-item layer)

**Trigger:** seulement quand un casque (Helmet item) est worn.

**Style:** strict engrave-on-glass (pas le visor v1 plein écran). Top strip + bottom strip étroits autour de la vue.

**Données :**

| Field | Source | Format |
|---|---|---|
| Depth | World position Z | `−1 240 m` |
| Ambient pressure | Computed from depth + medium | `128 kPa` |
| Suit resistance / depth nominal | Worn helmet/suit data | `nom: −2 000 m / max: −2 400 m` (warning quand on s'approche) |
| Compass / heading | Camera forward in submarine local | `047°` (optional FP) |

**Forbidden L3:**

- HP / vitales (L2).
- Hull / sonar (L4).
- Inventory (L1).

**Out of scope FP:** thermal vision, sonar overlay, tactical HUD modes — ce sont L6 via items futurs.

### 3.4 L4 — Terminals @ Stations (re-cadrage des helm widgets)

**Distinction stricte :**

- **Station** = prop physique 3D (chaise du capitaine, console torpilles, etc.).
- **Terminal** = écran/UI qui vit *à* une station.

Une station héberge un ou plusieurs terminaux selon sa fonction.

**Terminaux catalogue (FP + post-FP) :**

| Terminal | Données | Stations qui l'embarquent |
|---|---|---|
| `Terminal Helm Controls` | Telegraph, yoke, dive, kill switch | Helm |
| `Terminal Sensor` | Depth · speed · tilt · heading | Helm, Sonar (futur) |
| `Terminal Hull Integrity` | Vue volumétrique 3D du sub · breach markers · alerts → popup crew | Helm, Mécanique (futur) |
| `Terminal Navigation` | Carte holographique GDD · POI · trajet | Helm |
| `Terminal Sonar` | Contacts + active ping | Sonar station (futur) |
| `Terminal Manifold` | Power · pumps · ballast manuel | Mécanique station (futur) |
| `Terminal Weapons` | Tourelles · torpilles · contre-mesures | Armurier station (futur) |
| `Terminal Crew Roster` | Position crew · état basique · contracts/salaires (cf. GDD §social) | Bridge station (futur) |

**Mapping vers code existant :**

```
HelmCockpitWidget (existant)
  → devient compositeur "Station Helm"
  → embarque 3 terminaux sur le FP :
       - Terminal Helm Controls (les 4 instruments existants)
       - Terminal Sensor (NEW asset)
       - Terminal Hull Integrity (NEW asset)
```

Les 4 instruments existants (`HelmThrottleTelegraphWidget`, `HelmRudderYokeWidget`, `HelmDiveBoardWidget`, `HelmKillSwitchWidget`) sont **conservés sans modification** — ils sont PIE-validés (memory `project_helm_cockpit_redesign_2026_04_18.md`). Ils deviennent fragments du `Terminal Helm Controls`.

**Activation pattern :**

- Joueur s'approche d'une station → L5 contextual prompt apparaît (`E · Sit`).
- Joueur active → caméra se cale en place + terminaux apparaissent overlay diégétique sur la console.
- L1 reste minimal (le casque/Statut sont consultables).
- Esc / move → désactive la station, retourne au monde.

**GDD principle alignment:** "L'objet existe dans l'espace, son contenu se gère en menu" (humanisation rule). Les terminaux sont les écrans physiques posés sur les consoles 3D, pas des overlays libres.

### 3.5 L5 — Context layer

**Comportement progressif :**

```
État 0 — rien dans le viseur
  └─> crosshair neutre seul

État 1 — interactable détecté (auto)
  └─> crosshair MORPHS en icône contextuelle
       ✋ loot · 👆 use · 🚪 door · 🔧 repair · 👁️ read · 💬 talk
  └─> avec ~150ms delay :
      label `TYPE · nom` apparaît à côté
       ┌────────────────────┐
       │ HATCH              │  <- type, jaune #f4d03f
       │ ENG-02 → ENG-03    │  <- nom, blanc
       └────────────────────┘

État 2 — ne charge pas la scène davantage
  └─> action principale = F (convention)
  └─> détail (multiple actions) = via Statut L2 ou Terminal L4
```

**Contraintes :**

- Pas d'owner (`unmanned` / `Captain` etc.) — pas pertinent pour le focus.
- Pas de liste d'actions multiples (≠ B-Radial qui est dropped).
- Pas de connector lines aux corners (≠ B-Bloom v1).
- Hysteresis ~200 ms : si tu détournes un instant et reviens, pas de pop in/out.

**Couleur stricte :** label = `#f4d03f` (jaune GDD). Icône = `--gold` ou `--teal` selon le type d'action (à fixer dans le pillars). Pas de saturé cyan dans le HUD principal.

### 3.6 L6 — Progressive reveal (crafted tools)

**Rule :** aucune information privilégiée n'est par défaut. Chaque "vision augmentée" se débloque via un outil crafté.

**Outils proposés (placeholders, noms à finaliser) :**

| Outil | Layer affectée | Ce qu'il débloque |
|---|---|---|
| `Medical scanner` | L2 onglet Medical | Vue détaillée santé crew, afflictions, vitales précises |
| `Frequency analyzer` | L1 ambient + L4 popup | Détection brèches/anomalies à travers la coque (zones, pas wireframe) |
| `Hull tomograph` | L4 (mobile) | Vue volumétrique sub depuis n'importe où, pas seulement Terminal Hull Integrity |
| `Bio-detector` | L1 ambient | Contacts biologiques sans active ping |
| `Crew tracker` | L2 onglet Notes | Position crew temps réel |

**Implementation note :** L6 est principle-only pour FP. Aucun outil L6 ne ship en FP. Les hooks dans L1/L2/L4 sont en place mais non activés.

---

## 4. Concept survivants / drops / pivots

| ID | Statut | Devient |
|---|---|---|
| **C03 Wrist Slate** | Pivot | Mouvement diégétique → L2 Statut Personnage |
| **C04 Contextual Bloom** | Pivot | Label `type/nom` jaune → L5. Drop cards + lignes + owner |
| **W-Notebook** | Pivot | Carnet pages → L2 Statut Personnage |
| **W-StationTerminal** | Pivot | Distinction station/terminal → L4 entier |
| **W-Sketchbook** | Drop / fold | Aesthetic possible pour onglet `Notes` du L2, pas concept séparé |
| **W-Visor** | Drop | Pas d'engrave plein écran. L3 reste strict top/bottom strip |
| **W-Goggles** | Drop | Pas pertinent dans le contexte |
| **W-Palm** | Drop | Design rejeté |
| **B-Radial** | Drop | Remplacé par L5 + main key F + L2 carnet pour détails |
| **B-ThroughWall** | Drop | Remplacé par L6 (outils craftés) |
| **G-CornerPips** | Standby | Possiblement utile pour alertes critiques L1. Décidé plus tard |
| **G-VitalRing** | Drop | HP visible sur L2 aperçu, redondant |
| **G-VignetteSingle** | Drop | Effet ressenti (camera FX L1) > vignette dictionary |

---

## 5. Color & icon vocabulary update

Palette canonique inchangée (cf. `ui-pillars.md` V4) avec un **ajout** :

| Token | Hex | Usage |
|---|---|---|
| `--ctx-yellow` | `#f4d03f` | **NEW** — label `type/nom` sur L5 contextual focus uniquement |

Pas de saturated cyan, pas de saturated red, pas d'orange GDD adopté dans le HUD principal. Le jaune GDD est utilisé strictement pour signaler "ceci est une donnée contextuelle au focus".

**Icon plate update (à faire après build) :**

- Ajouter au plate les icônes des outils L6 placeholder (Medical scanner, Frequency analyzer, Hull tomograph, Bio-detector, Crew tracker).
- Ajouter les icônes terminaux L4 (Sensor, Integrity, Navigation, Sonar, Manifold, Weapons, Crew Roster).

---

## 6. Build plan (post-DA-validation)

### Phase A — Mockups HTML (this round)

1. **S01 Unified HUD** (HTML) — démontre L1 ambient + L2 Statut hybride carnet + L5 context auto. Le cœur de la session.
2. **S02 Station Helm composite** (HTML) — démontre L4 station-with-multiple-terminals. Conserve les 4 instruments helm existants en preview, ajoute Sensor + Hull Integrity placeholders.
3. **Index update** — pointe S01 + S02 comme ship targets, marque les concepts précédents comme superseded.
4. **Pillars update** — section "Six-layer architecture" + decision log entry.

### Phase B — UMG implementation (next session)

1. `WBP_PersonalHUD` — L1 ambient.
2. `WBP_StatutCarnet` — L2 hybride. Animation slide+rotate via UMG transitions.
3. `WBP_ContextLabel` — L5. Bind to interaction trace from `SubInteractionComponent`.
4. `WBP_HelmetHUD` — L3. Visibility-toggle when helmet equipped.
5. `WBP_TerminalSensor`, `WBP_TerminalHullIntegrity` — L4 new terminaux.
6. `WBP_StationHelm` — compositeur, embarque les 3 terminaux + les 4 instruments existants.

Editor-assigned conformément à P10 / V2 du pillars.

### Phase C — Data tables (post-FP scope)

1. `DT_ItemUIRequest` — P6 du pillars · format de déclaration UI per item.
2. `DT_TerminalCatalog` — table des terminaux disponibles avec leurs assets.
3. `DT_ContextIcon` — mapping interaction type → icon + verb + key.

---

## 7. Open standby items (pour des sessions futures)

| Sujet | Quand le rouvrir |
|---|---|
| Contenu de l'onglet `Skills` (5 axes attributs · 5 statuts unlockables) | Quand le système XP gameplay est arrêté |
| Contenu de l'onglet `Notes` | Quand le système tableau collectif GDD est défini |
| Outils L6 réels (noms, balance, craft) | Post-FP |
| Carte holographique de navigation (GDD §social) | Post-helm, priorité `project_post_helm_roadmap_2026_04_28.md` |
| Crew roster terminal | Quand contracts/salaires arrivent |
| G-CornerPips réintégration éventuelle | Si les 6 couches laissent un gap visible en alertes critiques |

---

## 8. Pillars que cette DA oblige à réviser

`reports/ui-pillars.md` doit être mis à jour pour refléter :

- **P3 Zone semantic split** → simplifier à `aperçu Statut bottom-right · context auto-revealing crosshair · helmet conditional · stations on-prop`. Drop la mention "left = extra-diegetic".
- **P2 Three bars maximum** → confirmer : HP / Fatigue / XP exactement, pas Stamina (déjà en place).
- **P4 Posture always visible** → confirmer : posture est sur l'aperçu Statut, mirroré.
- **P5 Stress vs calm escalation** → étendre : escalation passe par camera FX et sur le Statut, pas via vignettes plein écran.
- **P9 Stamina rhythm** → étendre : rhythm via heart pulse + audio breath + degraded movement, no overlay bar même en overflow critique sauf cas extrême.
- **NEW Pillar P13** : Information ownership matrix (cf. §1.1 ci-dessus).
- **NEW Pillar P14** : Six-layer architecture (cf. §1 ci-dessus).
- **NEW Pillar P15** : Station ≠ Terminal distinction (cf. §3.4).

Decision log : ajouter ligne 2026-05-08 avec référence à cette DA.
