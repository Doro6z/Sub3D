# Crew HUD — Design Research and Analysis

Date: 2026-05-08
Workspace: `C:\Dev\Sub3D`
Status: research and analysis · feeds `reports/ui-pillars.md` and the concept gallery in `reports/ui-references/`
Authority-max plan: `reports/plans/2026-04-10_first_playable_strategic_analysis.md`

---

## 0. Method and scope

### 0.1 What this document is

A structured analysis of HUD/UI design for the Sub3D crew layer, before any UMG asset is created. It feeds three downstream artifacts:

- `reports/ui-pillars.md` — the persistent design pillars (governance doc).
- `reports/ui-references/crew_hud_icon_plate.html` — the icon vocabulary.
- `reports/ui-references/concept_*.html` — eight concept mockups (gallery).

### 0.2 What it is not

- Not an implementation plan. No `WBP_*` assets are described.
- Not a settled design. It enumerates options; pillars in the next doc commit to a subset.
- Not a copy of any reference game. Patterns are inspiration only — Sub3D ships its own combinations.

### 0.3 Methodology

1. List Sub3D's own constraints (setting, scope, multi-crew, item-driven contract).
2. Inspect 10 reference games for HUD patterns. For each, record: zones used, diegesis level, persistence rule, attention cost.
3. Build a taxonomy of information types Sub3D needs to surface.
4. Map information types to attention budget (constant, glance, on-demand, alert).
5. Articulate the item-driven UI rule: who creates a panel? who removes it?
6. Derive a design space matrix and pick eight distinct concepts that span it.

### 0.4 References inspected

External games (inspection only — no copy of code, assets, names, values, or behaviors):

- Barotrauma (XML content already inspected in `reports/research/2026-05-05_barotrauma_design_inspiration.md`).
- DayZ (recent versions).
- Escape from Tarkov.
- Subnautica (XML inspection in `reports/research/2026-05-05_games_to_analyze_for_inspiration.md`).
- Death Stranding (PC).
- Dead Space remake.
- Resident Evil 4 remake.
- Alien Isolation.
- S.T.A.L.K.E.R. (Anomaly / GAMMA).
- Hardspace: Shipbreaker.

Sub3D internal sources:

- `CLAUDE.md` (project overview, scope, constraints).
- `reports/plans/2026-04-10_first_playable_strategic_analysis.md` (FP scope).
- Memory `project_abyssal_setting_2026_05_07.md` (no sun, no surface, all artificial light).
- Memory `feedback_collaboration_ui_method.md` (mockup-before-code).
- Memory `project_post_helm_roadmap_2026_04_28.md` (post-helm gameplay loop priorities).

---

## 1. Sub3D-specific constraints

### 1.1 Hard constraints (cannot be designed away)

| Constraint | Source | Consequence for HUD |
|---|---|---|
| Abyssal setting · 100% deep, no sun | `project_abyssal_setting_2026_05_07.md` | Background contrast is dominated by artificial light pools. Light-on-dark is the only viable HUD contrast direction. White-on-bright will not happen. |
| Submarine = mobile authoritative frame | CLAUDE.md, `2026-04-21_local_grid_space_authority_architecture.md` | Position-based UI elements must specify *which frame* they live in (sub-local for stations, world for outside) — see §6. |
| 4–16 player coop | CLAUDE.md | Crew roster + status of others is a real surface, not a single-player afterthought. Network state visibility matters. |
| First Playable scope, generator paused | `project_pipelines_status_2026_04_18.md` | UI must not couple to runtime-procgen features. Panels target the handmade Craniata. |
| No runtime auto-spawn UMG | CLAUDE.md "Architecture constraints" | Every persistent panel is editor-assigned with explicit names and stable layout. Component-level runtime spawns are allowed only for **gameplay state materialization** (water planes, doors, hull boundaries). UI always editor-assigned. |
| Posture as state axis (Standing, Crouched, Prone, Swimming) | `CrewLocomotionTypes.h` | HUD must surface posture explicitly. It is not implicit from camera. |

### 1.2 Soft constraints (preferences)

| Preference | Source | Consequence |
|---|---|---|
| Iteration over perfection | `feedback_iteration_pace.md` | Concepts ship coherent first pass; tunables are `EditAnywhere`. |
| Avoid over-engineering | `feedback_avoid_surengineering.md` | Don't reinvent UE UMG idioms. Prefer simpler concepts that ship. |
| Less neon, more grounded | User feedback 2026-05-08 (this conversation) | Industrial palette over Tron palette. Muted teal + warm amber over saturated cyan. |
| Item-driven UI assignment | User clarification 2026-05-08 (this conversation) | Panels are owned by **items / context**, not by **roles**. Hold a hull-checker → hull panel appears. Holster it → it disappears. The "Captain" abstraction is rejected as a UI driver. |
| Statuses on bottom-right | User feedback 2026-05-08 | Right-bottom is now the **clean status zone**. Top-left is for extra-diegetic equipment-driven panels. |

### 1.3 Non-constraints (resolved deliberately as open)

- Aspect ratio: assume 16:9 baseline; ultrawide is not a FP target but should not be made impossible.
- VR: not a target. Layouts can use screen edges.
- Localization: layouts must reserve text expansion (FR ≈ +20% over EN).

---

## 2. Reference analysis

### 2.1 Per-game pattern matrix

For each reference, the relevant slot. "Diegesis" is the in-fiction realism of the UI element: D = diegetic (in the world), C = contextual diegetic (visible in-world but framed as info), X = extra-diegetic (overlay, fictional UI device).

| Game | Vitals zone | Inventory zone | Interaction prompt | Crew/peer info | Distinctive trait |
|---|---|---|---|---|---|
| Barotrauma | Bottom-left circular icons (X) | Bottom-center hotbar + tab inventory (X) | Center crosshair tooltip (C) | Crew list right (X) with tasks/status. | Item-driven panels: pick up wrench, screwdriver UI changes; sit at sonar, sonar overlay appears. **Closest reference for Sub3D's item-driven contract.** |
| DayZ | Bottom-right pictograms with state colors (X) | F-key inventory full screen | Center-bottom contextual prompt | None (no party UI) | Pictograms degrade, no numbers. Info hides between numerical exact and ambient gestalt. |
| Escape from Tarkov | Body silhouette in inventory only (X) | Tab inventory · grid (X) | Minimal in-world | None | Body silhouette with limb damage is **the** medical UI. No always-on health bar in raid. |
| Subnautica | Wrist PDA (D) for inventory and depth meter on Seamoth/Cyclops (D) | PDA (D) | Center subtitle prompts | None | Almost everything diegetic on wrist or in vehicle. **Most diegetic of all references.** |
| Death Stranding | Cargo weight glyph at top (X) | Hold L1 to flip out backpack (semi-D) | None — gestural input | Tab to social map (X) | UI fades when irrelevant. **Best at progressive disclosure.** |
| Dead Space remake | Spine bar (D) + RIG light segments (D) | Pause-time inventory hologram (D-ish) | Hold X for kinesis (no UI) | None | UI **on the body**. Player-readable from third person. **Most committed to diegetic.** |
| Resident Evil 4 remake | Bottom-left HP bar + ammo bottom-right (X) | Briefcase (semi-D in pause) | Center bottom prompt | None | Crisp X UI without irony. **Most readable of all.** |
| Alien Isolation | Almost no HUD; motion tracker held in hand (D) | Inventory wheel (X) | Subtle prompts | None | UI is the alien — your tools are visible-when-held. **Best minimal.** |
| S.T.A.L.K.E.R. | Top-left bars + corners with statuses (X) | Tab inventory grid (X) | Center | Faction relations (X) | Dense, technical. **Worst for new player legibility.** |
| Hardspace: Shipbreaker | Top-left fuel/O₂ bars (X) + tool circle bottom-right (X) | Wheel | Center | None | Tool wheel is **item-driven** like Barotrauma. Rest is restrained. |

### 2.2 Patterns extracted

**P1 — Item-driven panel ownership.** Barotrauma, Hardspace, Subnautica, Alien Isolation: the visible UI changes when you pick up a tool. This is Sub3D's chosen contract.

**P2 — Body silhouette as detailed status.** Tarkov, Dead Space, DayZ all have a body silhouette layer beneath the always-on bars. Limb-level damage is the standard.

**P3 — Progressive disclosure.** Death Stranding, Alien Isolation, Hardspace: information hides when irrelevant, surfaces on demand or on threshold. Reduces noise.

**P4 — Diegetic floor + extra-diegetic ceiling.** Subnautica, Dead Space: maximally diegetic. RE4 remake, Tarkov: maximally extra-diegetic. The split lets each game pick its lane and not waver.

**P5 — Gestalt over numbers.** DayZ pictograms, Death Stranding glyphs: prefer "I'm cold and hungry" gestalt to "Temp 36.4 / Calories 1840". For Sub3D's sentinel-style concepts this is a serious option.

**P6 — Contextual prompt at crosshair.** Universal — every game centers interaction prompts. Sub3D should do the same. This is the only HUD location the player is *always* looking at.

**P7 — Hotbar + held tool separation.** Barotrauma, RE4, Hardspace: the held tool gets its own visual treatment, separate from the hotbar grid. Sub3D's v2 already does this.

**P8 — Pause-time deep UI.** RE4, Subnautica, Tarkov: detailed inventory/medical happens in pause-time, not in active play. For Sub3D's coop this becomes "non-pausing detail screen", but the principle (don't fight detail UI in active play) holds.

### 2.3 Anti-patterns flagged

**A1 — Always-on five bars.** STALKER, MMOs. Reads as "stat sheet on top of the world". Visually noisy, looks like a spreadsheet. Sub3D commits to **three bars maximum** as a hard constraint (HP, Fatigue, XP).

**A2 — Stamina as separate bar.** Most action games put stamina next to HP. User decision 2026-05-08: stamina is **not** a bar. It is a heart-rhythm + on-threshold overflow. This kills the spreadsheet feel.

**A3 — Numeric status spam.** Tarkov shows hydration, calories, body temp at all times. For Sub3D's FP, those are detail-screen only, not always-on.

**A4 — Role-bound UI.** "Captain UI vs Mechanic UI" branching at the role level. Rejected by user 2026-05-08. UI is item-bound, not role-bound. A captain holding a wrench gets the wrench's UI, not a captain UI. **This is the most important correction in this conversation.**

**A5 — Spawn UMG widgets at runtime.** Forbidden by `CLAUDE.md`. All persistent panels are editor-assigned.

**A6 — Decorative CRT scanlines.** Tempting but cliché. They cost legibility for "feel". Out unless they earn their cost.

---

## 3. Information taxonomy

### 3.1 The eight information families

Sub3D's HUD must surface eight families of information. Each has different visibility rules.

| # | Family | Examples | Visibility rule |
|---|---|---|---|
| 1 | **Self vitals** | HP, Fatigue, XP, Stamina | Always at low intensity. Bars allowed for HP/Fatigue/XP. Stamina is rhythm, not bar (overflow on critical only). |
| 2 | **Self body** | Per-limb damage, bleeding, hydration, calories, body temp, skill levels | On-demand only. Click status panel → detail overlay. |
| 3 | **Self posture** | Standing, Crouched, Prone, Swimming | Always visible — embedded in the status panel via stick figure. |
| 4 | **Self environment** | Compartment, depth, water immersion, ambient pressure, oxygen, audio volume | Surface only when item or condition makes them relevant. Held depth-gauge → depth surfaces. Swimming → immersion surfaces. Otherwise hidden. |
| 5 | **Self equipment / context** | Held tool, helmet attachments, modular gadgets | Item-driven panel left side. Vanishes when nothing relevant. |
| 6 | **Crew · others** | Roster, role tag, status dot, location | Always visible at low intensity (compact list). Detail on hover/hold. |
| 7 | **Submarine state** | Hull integrity, power, sonar, helm, ballast | **Item-driven** — appears only via a hull-checker, sonar pinger, helm console, etc. Not a default panel. |
| 8 | **Alerts / events** | Breach, fire, low O₂, intruder, hull stress | Always — but suppressed when irrelevant, escalated when active. Bottom-right zone. |

### 3.2 Frequency-vs-criticality matrix

```
         CRITICAL ↑
                  │  Alerts (fam 8)
                  │  Vitals critical (fam 1)
                  │
     low-freq     │     high-freq
   high-criticality│   high-criticality
                  │
        ──────────┼──────────  → FREQUENCY
                  │
   low-freq       │     high-freq
   low-criticality│   low-criticality
                  │
                  │  Posture (fam 3)
                  │  Hotbar (fam 5)
                  │  Crew roster (fam 6)
         ─────────┴─────────
```

The HUD's job is to put the **top-left quadrant** (rare but critical: alerts) where it interrupts attention reliably, and to put the **top-right quadrant** (frequent and critical: vitals) where the eye lands during stress. Bottom-right (frequent, low-criticality) gets calm zones. Bottom-left (rare, low-criticality) is on-demand.

### 3.3 Sub3D mapping

| Family | Quadrant | Default zone | Trigger to elevate |
|---|---|---|---|
| Vitals | Top-right of matrix | Bottom-right panel, low intensity | HP < 30 → red border pulse |
| Body detail | Bottom-left of matrix | Hidden | Click status panel |
| Posture | Bottom-right of matrix | Bottom-right (in status panel) | Always low intensity |
| Environment | Bottom-left of matrix | Hidden | Item-driven |
| Equipment context | Bottom-right of matrix | Top-left, item-driven | Hold an item |
| Crew | Bottom-right of matrix | Mid-left compact list | Hover row |
| Submarine state | Bottom-left of matrix | Hidden | Item-driven |
| Alerts | Top-left of matrix | Bottom-right above status | Active event raises it |

---

## 4. Attention budget

### 4.1 The crosshair is the only constant gaze location

The player looks at center-screen >90% of the time during active play. Every other UI zone is **periphery**. Periphery information must:

- Survive blur — recognizable without sharp focus.
- Not animate distractingly except on threshold.
- Be reachable by saccade in <300 ms.

### 4.2 Saccade cost ladder

Glance distance from crosshair, in screen-degrees of saccade:

| Cost | Zone | Use for |
|---|---|---|
| ~5° | Just below crosshair | Contextual interaction prompt (universal P6) |
| ~12° | Bottom corners | Vitals, inventory hotbar |
| ~20° | Top corners | Lower-priority info: equipment-driven panels, crew |
| ~30° | Edges (mid-side) | Alerts that can flash |
| Off-screen | Pause-time / detail overlay | Body detail, full inventory, station UIs |

### 4.3 Attention budget per minute

A coarse model. During typical FP play, the player gives roughly:

- 90% of attention to the world (movement, looking, aiming, listening).
- 5% to the contextual prompt (just below crosshair).
- 3% to vitals (bottom-right glance).
- 1% to crew/equipment (top-left glance).
- 1% to alerts (escalation only).

If a UI design pushes any single zone over its budget, that design is wrong. **Bars that demand checking every 3 seconds are a budget violation.**

### 4.4 Stress vs calm

In calm moments (cruising, exploring), HUD should fade. In stress moments (combat, breach, low O₂), HUD should escalate. The transition is the *single most important UX feature* of the system.

Implementation rules:

- Calm: vitals at 60% opacity. Equipment panel at 70%. Crew at 60%. Alert zone empty.
- Stress: relevant panel pulses to 100%. Other panels drop to 40%. Crosshair de-emphasized.
- Recovery: lerp back to calm over 2–3 seconds after threshold clears.

---

## 5. Item-driven UI · principles

This is the core contract chosen by the user 2026-05-08, replacing the role-driven model from v2.

### 5.1 The contract

> A persistent UI panel exists on screen if and only if an item or contextual condition currently in scope explicitly requests it.

When the requester goes out of scope (item holstered, condition cleared), the panel fades out within 300 ms.

### 5.2 What can request a panel

In ascending order of permanence:

1. **Held tool** (hand slot) — explicit, ephemeral. E.g., wrench held → repair panel.
2. **Equipped wearable** (helmet, suit) — explicit, semi-persistent. E.g., depth helmet → depth gauge.
3. **Worn modular** (gauge, strap-on monitor) — explicit, persistent unless removed.
4. **Worldspace context** (standing at a station, near a breach, in a flooded compartment) — implicit, location-driven.
5. **Game state** (low O₂ alarm, breach alert) — implicit, event-driven.

A given panel can be requested by multiple sources. Conflict resolution: the most specific source wins. Held tool > worn modular > worldspace context. Game-state alerts always coexist with whatever is up.

### 5.3 What an item declares

For UI purposes, every item that requests UI must declare:

```
struct FItemUIRequest {
  EItemUIPanel PanelId;        // which panel to show
  EItemUIPriority Priority;    // resolves conflicts
  float FadeInSeconds;
  float FadeOutSeconds;
  TArray<EAlertCategory> SuppressAlerts;  // optional: rebreather suppresses low-O2 alarm
}
```

This is data, not code. A new item ships with a row, not a class.

### 5.4 What this kills

- "Captain HUD vs Mechanic HUD" branching. Gone — the captain holding a wrench gets the wrench panel.
- Role-gated overlays. A medic kit shows the medic panel regardless of who holds it.
- "Equip class to see UI" anti-pattern. Items teach UI directly.

### 5.5 What this enables (post-FP)

- Modular helmet upgrades. Buy a thermal-vision module → world thermal overlay activates while helmet worn.
- Faction-specific tooling. A faction's hull-checker has a custom panel skin without role plumbing.
- Player progression as item progression. Better tools = more legible UI, no skill-tree-to-UI plumbing.

### 5.6 Failure modes to design against

- **Panel flicker during quick-swap.** Holster → equip in 200 ms must not flash a fade-out/fade-in. Solution: 200 ms hysteresis before fade-out triggers.
- **Information loss on holster.** If a hull-checker showed "ENG-02 at 62%" and the player holsters it, where does that info go? Answer: into a "last seen" memory accessible via the detail overlay. Not on default HUD.
- **Stack of panels.** With four worn modulars, the left side becomes a stack. Solution: vertical stack order = priority order; max 3 visible; oldest collapses to a row of icons.

---

## 6. Diegesis spectrum

### 6.1 The four positions

| Position | Description | Sub3D fit |
|---|---|---|
| **Pure diegetic** (D) | UI is in the world. Watch on wrist, gauge on suit, panel on station. | Excellent for immersion. Risky for FP — requires animated wrist mesh, suit overlays, IK constraints. |
| **Contextual diegetic** (C) | UI overlays are aligned to in-world objects but rendered by HUD. E.g., interaction prompt floats on the door. | Sweet spot for many panels. Cheap to ship, immersive enough. |
| **Extra-diegetic with diegetic justification** (X+) | Overlay panels framed as "information from a worn device" — a heads-up display from your helmet. Visual style references the device. | Very strong for Sub3D. The helmet becomes the storyteller. |
| **Pure extra-diegetic** (X) | Overlay panels with no in-world justification. Fast, cheap, traditional. | Always available as fallback. Acceptable for vital-critical alerts. |

### 6.2 Rule of thumb

- Self vitals → X+ (helmet readout). Always-on means always-needed.
- Posture → X+ (helmet readout) bottom-right.
- Body detail → X (overlay) on demand. Diegetic doesn't pay off for a click-to-open screen.
- Equipment-driven panels → X+ (the held device's HUD). The wrench's UI looks like a worn tool's display.
- Crew roster → X+ (helmet) or pure X depending on aesthetic concept.
- Alerts → X (always extra-diegetic so they reach the player).

### 6.3 Concept differentiation by diegesis

Each of the eight concepts in the gallery picks a position on this spectrum:

- C01 Sentinel: maximally minimal X.
- C02 Industrial Bridge: X+ helmet readout.
- C03 Wrist Slate: D wrist device.
- C04 Contextual Bloom: C — UI radiates from in-world targets.
- C05 Item Cards: X+ item-bound panels.
- C06 Corner Quartet: X with strong zoning.
- C07 Edge Glow: X with maximum suppression — almost ambient.
- C08 Analog Gauges: X+ with steam-gauge fiction.

---

## 7. The design space

Eight concept slots cannot cover every axis. The chosen axes are:

- **Density** (how much info on screen): minimal · medium · dense.
- **Diegesis** (D / C / X+ / X).
- **Persistence** (always-on / on-demand / hybrid).
- **Aesthetic** (industrial · clinical · steam-gauge · ambient).

The eight concepts are placed:

| ID | Density | Diegesis | Persistence | Aesthetic |
|---|---|---|---|---|
| C01 Sentinel | minimal | X | on-demand | ambient |
| C02 Industrial Bridge | medium | X+ | hybrid | industrial |
| C03 Wrist Slate | medium | D | on-demand | clinical |
| C04 Contextual Bloom | medium | C | hybrid | industrial |
| C05 Item Cards | medium | X+ | hybrid (item-driven) | industrial |
| C06 Corner Quartet | dense | X | always-on | industrial |
| C07 Edge Glow | minimal | X | hybrid | ambient |
| C08 Analog Gauges | medium | X+ | always-on | steam-gauge |

The matrix has unused cells (e.g., dense + D, dense + ambient). Those would be valid concepts but were judged less productive for FP — diegetic dense is too costly, ambient dense contradicts itself.

---

## 8. Open questions for the design phase

These are decisions that should be resolved before pillars commit.

| # | Question | Default if undecided |
|---|---|---|
| Q1 | Does the helmet exist as a per-character mesh in FP? | No — assume worn implicitly. Helmet HUD is a fiction the UI uses. |
| Q2 | Does Sub3D ever pause in solo? | No — coop precludes pause. Detail overlays don't pause game. |
| Q3 | How loud are alerts? Audio + visual + haptic? | Audio + visual flashing border. Haptic later. |
| Q4 | Is sonar a held item or a station? | Both. Held pinger = local short-range. Station sonar = global long-range. |
| Q5 | Does the player carry an explicit "status checker" item to see vitals? | No — vitals are intrinsic to "you wear a suit". Status checker would be redundant. |
| Q6 | Body silhouette in detail overlay: 6 zones or 12? | 6 for FP. 12 post-FP if depth justifies. |
| Q7 | Crew roster max size visible? | 5. Above 5, group by compartment. |
| Q8 | Inventory base slots: 4 or 6? | 6 visible (1–6 keys), backpack adds 4–8 more. |
| Q9 | Stamina overflow bar threshold | 25% remaining. Below = bar appears, fades when ≥30%. |
| Q10 | Detail overlay closes on movement? | No — close on Esc/click outside. Movement allowed (coop). |

---

## 9. Out-of-scope for FP (post-FP backlog)

- Animated wrist mesh + IK for diegetic concept.
- Helmet thermal/sonar overlays (contextual diegetic post-FP).
- Pixel-snapped icons for art-direction polish.
- Haptic feedback layer (controller).
- Localization expansion checks beyond FR.
- Accessibility: colorblind palette, scaling, screen-reader (post-FP, but pillars must not preclude).

---

## 10. References for the design phase

When a concept makes a pattern choice, it cites this matrix.

- **Item-driven contract** → §5 (this doc), Barotrauma · Hardspace pattern (P1).
- **Body silhouette in overlay** → §3, Tarkov · Dead Space (P2).
- **Progressive disclosure** → §4.4, Death Stranding · Alien Isolation (P3).
- **Stress vs calm transition** → §4.4, Hardspace Shipbreaker (own observation).
- **No five always-on bars** → §2.3 A1, anti-pattern.
- **Stamina as rhythm not bar** → §2.3 A2, user decision 2026-05-08.
- **Three-bar cap** → §2.3 A1, user decision 2026-05-08.
- **Role-driven UI rejected** → §2.3 A4, user decision 2026-05-08.

---

## 11. Conclusion

Sub3D's HUD design space is constrained enough to be tractable. The largest uncommitted choice is the diegesis spectrum — that choice generates the eight distinct concepts in the gallery. The smallest committed-and-final choice is the item-driven contract, which is the spine that all concepts share.

The pillars document commits to a subset; the gallery concretizes options for the user to compare. After concept selection, one concept becomes the "ship target" and the icon plate becomes its vocabulary.
