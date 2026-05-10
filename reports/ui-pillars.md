# Sub3D — UI Design Pillars

Référence persistante pour toute session UI. Ces piliers ne changent pas sans décision explicite.

Last update: 2026-05-08 — added crew HUD pillars P1–P12 derived from `reports/research/2026-05-08_crew_hud_design_analysis.md` and user feedback this date.

---

## Méthode de collaboration UI

1. **Vérifier `reports/ui-references/`** avant de commencer. Si un fichier de référence visuelle existe pour le widget cible, le lire.
2. **Phase A avant Phase B** — proposer un mockup ASCII ou HTML structuré, attendre validation, PUIS écrire le C++/Slate/UMG.
3. **Respecter ces piliers** — ils overrident les préférences par défaut d'UMG ou les patterns génériques.

---

## Piliers visuels (toutes UI)

### V1 · Lisibilité instrumentale

Chaque widget d'instrument (telegraph, yoke, dive, kill, gauges) est lisible en un coup d'œil depuis la position cockpit. Pas de texte décoratif, indicateurs visuels positionnels.

### V2 · Pas d'auto-spawn UI

Widgets editor-assigned. Noms stables. Pas de `CreateWidget` dans des chemins de tick ou de `BeginPlay` conditionnels. Les panels item-driven sont **visibility-toggled**, pas spawn-toggled — ils vivent dans l'arbre éditeur, le runtime les masque/affiche.

### V3 · Debug = toggleable, pas permanent

Overlays debug via `USub3DDebugSettings`. Jamais hardcodés dans les widgets de production.

### V4 · Light-on-dark only
Setting = abysse. Tous les fonds sont sombres. Texte clair toujours.

Palette canonique :

| Token | Hex | Usage |
|---|---|---|
| `--bg-deep` | `#0a0d12` | viewport background |
| `--bg-mid` | `#11151b` | secondary surface |
| `--panel` | `#181c23` | primary panel surface |
| `--border` | `#2a313b` | panel border |
| `--border-hi` | `#3a4452` | hover border |
| `--text` | `#d8d4ca` | primary text (warm off-white) |
| `--text-dim` | `#8d9095` | secondary text |
| `--text-faint` | `#5e636a` | tertiary text |
| `--teal` | `#6db4a3` | clean / healthy / OK accent |
| `--amber` | `#c89868` | warning / CRT / held tool secondary |
| `--gold` | `#d4b876` | held tool primary highlight |
| `--leaf` | `#87a86b` | progress / XP / positive |
| `--crimson` | `#b8554a` | critical / damage / alert |

Forbidden : `#00ffff` pure cyan, `#ff0000` pure red, `#ffffff` pure white. They overshoot.

---

## Piliers HUD crew (2026-05-08)

### P1 · Item-driven panel ownership

> Persistent UI panels exist on screen if and only if an item, wearable, worldspace condition, or game event explicitly requests them.

- Resolution : Held tool > Worn modular > Worldspace context > Game event.
- Fade out 300 ms après que le requester sort de scope, avec 200 ms d'hystérésis sur quick-swap.
- Roles ne drivent jamais l'UI. Un capitaine qui tient une clé voit l'UI clé.
- Nouvel item = nouvelle row dans `FItemUIRequest` data table, pas une UClass.

**Why** : le model role-driven branchait l'UI par job et cassait dès qu'un joueur ramassait l'outil de quelqu'un d'autre. Item-driven scale linéairement avec le catalog d'items.

### P2 · Three bars maximum

> Health, Fatigue, Experience sont les seuls bar-rendered values du HUD par défaut.

- Stamina = rythme cardiaque, pas barre. Overflow bar uniquement < 25%.
- Hydration, calories, body temp, oxygen, pressure, hull integrity = jamais barre par défaut. Detail overlay ou item-driven.

**Why** : les HUD à 5 barres se lisent comme une feuille de calcul ; l'œil arrête de scanner.

### P3 · Zone semantic split

| Zone | Content | Permanence |
|---|---|---|
| Top-left | Equipment-driven panel (item teaches HUD) | Item-bound |
| Mid-left | Crew roster (compact) | Always-on, low intensity |
| Center | Crosshair + interaction prompt + dynamic context icon | Always-on |
| Mid-right | Contextual interaction menu (keys for current target) | Context-bound |
| Bottom-right | Status panel (posture stick + 3 bars + heart rhythm) | Always-on, low intensity |
| Bottom-center | Hotbar (6 slots) + held tool, optional bag, stamina overflow bar | Always-on |
| Bottom-right above status | Active alerts | Suppressed when none |

**Why** : gauche = stuff the player learns *about* (extra-diegetic), droite = stuff the player acts *on* (diegetic + contextual).

### P4 · Posture is always visible

Le status panel montre un stick figure qui reflète la posture (Standing / Crouched / Prone / Swimming). Click → detail overlay (corps + per-limb + secondary stats).

**Why** : posture est gameplay-relevant (clearance, vitesse, traversal). Cacher la pousse à la deviner par les coups de capsule.

### P5 · Stress vs calm escalation

| State | Vitals | Equipment | Crew | Alerts |
|---|---|---|---|---|
| Calm | 60% opacity | 70% | 60% | empty |
| Stress | relevant → 100% pulse | dim → 40% | dim → 40% | escalation |
| Recovery | lerp 2–3 s | lerp | lerp | clear |

Transitions 300–500 ms ease-out, sauf alerte qui demande snap immédiat.

**Why** : œil drawn vers ce qui matter maintenant. Static max-opacity HUD entraîne l'œil à tout ignorer.

### P6 · Item-driven contract is data, not code

Une nouvelle item qui veut UI ship une row dans une data table, pas une UClass. La row nomme : panel id, priority, fade in/out timings, optional alert suppression list.

**Why** : ship velocity. 20 outils ne doit pas demander 20 widget classes.

### P7 · Detail overlays do not pause

Coop précluds pause. Detail overlay (body, inventory, station UI) = layer translucide dismissible Esc/click-outside. Le monde tourne dessous.

- Critical alerts piercent l'overlay (banner top semi-transparent).
- L'overlay ne capture pas la souris totalement — head movement / hot keys marchent.

**Why** : 4–16 player coop précluds pause.

### P8 · Light-on-dark only

Voir V4 ci-dessus pour la palette canonique. Aucune UI n'assume un fond clair.

**Why** : `project_abyssal_setting_2026_05_07.md` — pas de soleil, pas de surface, lumière artificielle.

### P9 · Stamina is rhythm, not bar

| Stamina | Heart period | Visible |
|---|---|---|
| 100–60% | 1.0 s steady | icon only |
| 60–25% | 0.7 s winded | icon + label "winded" |
| 25–0% | 0.4 s gasping | icon + label + overflow bar above inventory + audio |

Recovery : bar fade quand ≥ 30%.

**Why** : decision user 2026-05-08 — gestalt feedback (P5 in research doc) plus lisible pour ephemeral state.

### P10 · Editor-assigned, no runtime spawn

Voir V2.

### P11 · Icon vocabulary is finite and curated

Tous les icons viennent de `reports/ui-references/crew_hud_icon_plate.html`. New icons added there first.

- Style : linear, 24×24 viewBox, 1.4–1.6 stroke, round caps + joins. Single color, inherit from CSS.
- Cute, cartoon, 3D-rendered = out.

### P12 · Localization-aware spacing

Chaque panel reserve +20% horizontal text expansion pour FR. Truncation is the failure mode, not wrap (sauf body-text).

---

## Piliers gameplay-UI (specific systems)

### G1 · Helm cockpit widgets

4 instruments séparés : telegraph, yoke, dive, kill switch. Aucun ne dépend de l'autre. Tous editor-assigned. Voir `Source/Sub3D/Submarine/Helm/`.

### G2 · Sub3D Debug Panel (editor-only)

Slate dockable panel exposant tous les `USub3DDebugSettings` toggles. Module `Sub3DDebugPanel`. Pas de runtime impact.

---

## Références visuelles

`reports/ui-references/` — fichiers numérotés par concept :

| Type | Naming |
|---|---|
| Reference album | `[date]_[name]_reference.md` (texte) ou `.png`/`.jpg` |
| Concept mockup | `concept_[id]_[name].html` |
| Icon plate | `crew_hud_icon_plate.html` |
| Mockup ASCII validé | `[date]_[widget-name]_mockup.md` |

Concepts actuels (2026-05-08) :

- `concept_C01_sentinel.html` — minimal/ambient
- `concept_C02_industrial_bridge.html` — refined v2 baseline
- `concept_C03_wrist_slate.html` — diegetic
- `concept_C04_contextual_bloom.html` — UI radiates from target
- `concept_C05_item_cards.html` — UI grows from items
- `concept_C06_corner_quartet.html` — zoned semantic corners
- `concept_C07_edge_glow.html` — ambient feedback only
- `concept_C08_analog_gauges.html` — steam-gauge

Index : `index.html` dans `reports/ui-references/`.

---

## Widgets existants (état 2026-05-08)

| Widget | Fichier | Statut |
|---|---|---|
| UHelmCockpitWidget | `Source/Sub3D/Submarine/Helm/SubHelmCockpitWidget.*` | Actif, PIE-validé |
| UTelegraphWidget | `Source/Sub3D/Submarine/Helm/` | Actif |
| UYokeWidget | `Source/Sub3D/Submarine/Helm/` | Actif |
| UDiveControlWidget | `Source/Sub3D/Submarine/Helm/` | Actif |
| UKillSwitchWidget | `Source/Sub3D/Submarine/Helm/` | Actif |
| UCrewAnimDebugWidget | `Source/Sub3D/Submarine/CrewAnimDebugWidget.*` | Legacy tuner — to be migrated to editor-assigned WBP per debug plan 2026-05-08 |

---

## Six-layer architecture (added 2026-05-08 round 3)

Authoritative reference : `reports/plans/2026-05-08_crew_hud_unified_da.md`.

| # | Layer | Owner | Default visibility |
|---|---|---|---|
| L1 | Personal HUD ambient | Crew character | Always-on, faint |
| L2 | Statut Personnage | Crew character | Aperçu always-on bottom-right · open `C` |
| L3 | Helmet HUD | Worn helmet | Only when helmet equipped |
| L4 | Terminals @ Stations | Submarine props | Only at station |
| L5 | Context layer | World focus target | Auto on look-at |
| L6 | Progressive reveal | Crafted tools | Item-driven |

Each data point belongs to **exactly one** layer. No duplication. See DA §1.1 for the information ownership matrix.

### P13 · Information ownership matrix

> Every data point shown on screen belongs to exactly one layer. No data is duplicated across layers.

- HP / Fatigue / XP / pulse / posture / afflictions → L2.
- Stamina, ambient feedback (wet/cold), held tool, crosshair → L1.
- Depth / pressure / suit resistance → L3 (helmet) only.
- Sub state (heading, hull, sonar, navigation) → L4 (terminals at stations) only.
- Item type + name on focused interactable → L5 only (`#f4d03f` label).
- Through-wall / detailed crew vitals / etc → L6 (crafted tool) only.

**Why** : the previous draft duplicated depth on personal HUD AND helmet AND helm terminal. Player got confused on what was canonical. Single-owner rule kills duplication.

### P14 · Station ≠ Terminal

> A *station* is a physical 3D prop in the submarine. A *terminal* is a UI screen that lives at a station. A station hosts one or more terminals.

- `HelmCockpitWidget` is the **Station Helm compositor**, not a single terminal.
- The 4 helm instruments (`Telegraph`, `Yoke`, `DiveBoard`, `KillSwitch`) are **fragments of `Terminal Helm Controls`**.
- New terminals (`Terminal Sensor`, `Terminal Hull Integrity`, `Terminal Navigation`, etc.) are added to the Helm station alongside the existing instruments.

**Why** : aligns with GDD §humanisation rule "l'objet existe dans l'espace, son contenu se gère en menu". Stations are 3D props; terminals are their screens. Refactor scope is minimal — existing instruments are preserved (PIE-validated, memory `project_helm_cockpit_redesign_2026_04_18.md`).

### P15 · Context auto-reveal · no actions in HUD

> When the camera reticle hits an interactable, the crosshair morphs into a context icon (~150 ms delay) and a `TYPE · name` label appears in `--ctx-yellow` (`#f4d03f`).

- No action list appears (no radial wheel, no card cluster).
- No owner display (`unmanned`, `Captain`, etc. removed).
- Main action defaults to **F** by convention.
- Multi-action contexts route to L2 carnet (Notes / Status) or L4 terminal — never to a HUD-side menu.

**Why** : keeps the screen calm. The player can act on the default verb instantly; complex actions go to the carnet or station, where there's room.

### P16 · Color addition

Add to canonical palette : `--ctx-yellow #f4d03f` for L5 contextual labels only. Other GDD colors (saturated cyan, orange) are **not** adopted in the HUD.

---

## Decision log

| Date | Pillar | Change | Reason |
|---|---|---|---|
| 2026-04-25 | initial | V1–V3 + helm widgets | Helm cockpit redesign |
| 2026-05-08 round 1 | P1–P12 | Crew HUD pillars added | Crew HUD design phase |
| 2026-05-08 round 3 | P13–P16 | Six-layer architecture committed · Station ≠ Terminal · context auto-reveal · `--ctx-yellow` added | DA `reports/plans/2026-05-08_crew_hud_unified_da.md` validated by user |

---

## Pillar test for new HUD work

Before any new crew HUD asset is reviewed, confirm :

- [ ] Item, wearable, condition, or event requests this panel (P1).
- [ ] No bars beyond HP/Fatigue/XP (P2).
- [ ] Lives in the correct zone (P3).
- [ ] Posture stick figure preserved if bottom-right (P4).
- [ ] Calm and stress states defined (P5).
- [ ] If new item-driven panel : data row added (P6).
- [ ] Coop-safe, doesn't pause (P7).
- [ ] Palette respects P8 / V4.
- [ ] Stamina, if present, follows P9.
- [ ] Editor-assigned WBP, not runtime-spawned (P10 / V2).
- [ ] Icons from the plate, or added there first (P11).
- [ ] FR +20% reserved (P12).

If any box is unchecked, the work is not ready for review.
