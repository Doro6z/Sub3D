# Mood board prompts — M01 to M06

6 images mood key pour locker l'aesthetic Sub3D avant de générer les items individuels. À lancer dans Meshy text-to-image (ou ChatGPT-Image / Stable Diffusion si Meshy ne supporte pas le text-to-image direct sur ton compte). Résolution 1024×1024 minimum, 4K si possible.

Chaque prompt référence la palette du color sheet canonique (`C:/ACC/Projects/Sub3D/Image & Concept/ChatGPT Image 5 mai 2026, 10_40_11.png`) et le style "hand-painted PBR low-poly stylized" validé.

**Output target** : `reports/asset-library/moodboard/M0X_*.png`

---

## M01 — Compartment ambiant (couloir crew)

**Concept** : couloir entre compartiments, ambiance habitée mais industrielle, tungstène chaud + filaments LED rouge alarme distante (off pour cette image).

```
Submarine interior corridor between compartments, hand-painted PBR style low-poly stylized game-ready, brushed steel #4A4E52 walls with weathered paint patches and visible rivets, ribbed metal ceiling, rubber black flooring with grip pattern, exposed pipework copper and brass #A57212 along upper edge, single warm tungsten 2700K ceiling lamp casting amber light, secondary cool fill from distant compartment doorway, painted stencil markings rouge minium #80363F on pipes, slight haze, atmospheric depth, NOT photoreal, NOT cartoon, hand-painted texture feel with edge wear, UE5 game-ready aesthetic, palette aligned with Sub3D canonical color sheet, Surcouf 1929 French submarine inspired layout, no characters, 3/4 perspective view down corridor, lived-in but maintained, 4K quality
```

---

## M02 — Helm cockpit ambient

**Concept** : poste de pilotage allumé, hublot abyssal noir, instruments analogiques + écrans CRT phosphore vert.

```
Submarine helm cockpit interior, hand-painted PBR low-poly stylized game-ready, brushed steel #4A4E52 console panel with bakelite amber knobs and CRT phosphor green displays, glowing dials with warm tungsten backlighting, large round porthole window showing abyssal pitch black ocean with faint distant cyan bioluminescent specks, leather captain's chair charcoal, mounted brass periscope handles, exposed copper pipework along wall, ambient warm 2700K tungsten lighting + secondary CRT phosphor green glow on pilot seat area, NOT photoreal, NOT cartoon, painterly texture work, UE5 game-ready, palette aligned with Sub3D canonical color sheet, Surcouf 1929 inspired analog instruments, 3/4 perspective from pilot seat, no characters, 4K
```

---

## M03 — Crew quarters off-duty

**Concept** : couchettes, ambiance détendue, photos punaisées, lampe basse tungstène, bouquin posé.

```
Submarine crew quarters interior, hand-painted PBR low-poly stylized game-ready, three stacked bunks with charcoal wool blankets and weathered cream pillows, wood paneling with hand-painted texture #6B4F32, pinned-up family photographs and postcards on small bulletin board, low-mounted bedside reading lamp casting warm tungsten 2700K pool of light, open paperback book on lower bunk, hanging coat on hook, small personal locker with stenciled name, brass-rimmed circular wall clock, ambient warm light + dim cool fill from far doorway, NOT photoreal, NOT cartoon, hand-painted painterly textures with edge wear and lived-in feel, UE5 game-ready, palette aligned with Sub3D canonical color sheet but warmer than corridor, Surcouf 1929 French naval feel, no characters, 3/4 perspective from doorway, 4K
```

---

## M04 — Breach moment

**Concept** : panneau cassé, eau jaillissante haute pression, alarme rouge, vapeur, urgence.

```
Submarine compartment hull breach moment, hand-painted PBR low-poly stylized game-ready, ruptured steel panel with twisted metal edges revealing pitch black ocean beyond, high-pressure water jet spraying inward at 45 degrees, white mist and water spray particles in air, red LED #C8332D alarm light strobing on ceiling casting strong red key light, distant ambient warm tungsten now overpowered by red emergency lighting, water already pooling on rubber floor with small wave, scattered debris (clipboard, mug, papers) being pushed by water, dramatic chiaroscuro lighting with red dominant + cyan rim from breach, NOT photoreal, NOT cartoon, painterly texture work with motion blur on water spray, UE5 game-ready aesthetic, palette aligned with Sub3D canonical color sheet but red-shifted alarm state, Surcouf 1929 inspired hull, no characters, dynamic 3/4 perspective showing both breach and compartment, urgent atmospheric, 4K
```

---

## M05 — Abyss exterior

**Concept** : sub silhouette, projecteurs traversant le noir absolu, créature cyan distante.

```
Submarine exterior view in deep abyssal ocean, hand-painted PBR low-poly stylized game-ready, weathered steel hull #4A4E52 with rust patina patches, rouge minium #80363F painted markings, three powerful tungsten searchlights casting long yellow cones into pitch black water revealing floating organic particulate, faint distant cyan bioluminescent silhouette of a Stalker creature in background fog, water depth visible by particulate density, no surface visible, no sun, no sky, only artificial light from sub, ambient pressure feel, dramatic side profile of submarine cigar shape with sail, NOT photoreal, NOT cartoon, painterly water atmospherics, UE5 game-ready aesthetic, palette aligned with Sub3D canonical color sheet with deep ocean color shift, Surcouf 1929 inspired silhouette, no human characters, side perspective, 4K
```

---

## M06 — Item still life on workshop bench

**Concept** : référence pour tous les item card prompts. Bench engineer, outils posés, lighting reference.

```
Engineering workshop still life on workbench inside submarine, hand-painted PBR low-poly stylized game-ready, scarred wood workbench surface with grease stains, arranged items: pipe wrench with rouge minium paint patches and rust patina, brass valve handwheel partially polished, coil of copper wire, oil-stained rag, small notebook with technical sketches, brass oil can, single warm tungsten 2700K work lamp clamped to bench casting strong directional light from upper-left, deep shadows on right, secondary cool fill from distant CRT phosphor green glow, palette dominated by acier brossé #4A4E52 + cuivre/laiton #A57212 + acier oxydé orange tones, NOT photoreal, NOT cartoon, painterly textures with hand-painted feel, UE5 game-ready aesthetic, palette aligned with Sub3D canonical color sheet, Surcouf 1929 era tools, no characters, 3/4 overhead view, used as material/lighting reference for all subsequent item prompts, 4K
```

---

## Usage downstream

- Une fois M01-M06 générées et sauvegardées dans `reports/asset-library/moodboard/`, elles deviennent les **references** pour tous les prompts d'items.
- Pattern d'amorce d'un prompt item : `"... in the style of Sub3D mood reference M0X, hand-painted PBR low-poly stylized..."` puis description spécifique de l'item.
- Si Meshy supporte image-reference + text fusionné : passer `M0X.png` en input + prompt texte.
- Si Meshy ne permet pas image-reference en text-to-image : refonder le vocabulaire stylistique dans chaque prompt item (palette hex, "hand-painted PBR low-poly stylized", éclairage tungstène 2700K, etc.) — pattern déjà appliqué dans `assets-data.js`.

## Itération

Si une mood image converge mal :

1. Garder le prompt et le run plusieurs fois (variabilité du modèle).
2. Si toujours mauvais, ajouter des qualifiers négatifs : `--no chrome, --no glossy plastic, --no cartoon eyes, --no anime, --no Borderlands stylization`.
3. Si style hand-painted ne ressort pas : insister `painterly brush strokes visible in textures, NOT smooth photographic textures, hand-painted PBR similar to League of Legends or Overwatch material work but more grounded`.
4. Si proportions sub se cassent : ajouter `industrial scale, 1.85m door clearance for human reference, NO oversized rooms, claustrophobic interior space`.

## Validation visuelle

Avant de passer aux items individuels, valider que les 6 mood images :

- [ ] Respectent la palette du color sheet (acier brossé, jaune sécurité, rouge minium, cuivre/laiton dominent)
- [ ] Sont stylized hand-painted, pas photoréal
- [ ] Lumière tungstène 2700K dominante (pas blanc 6500K cinéma)
- [ ] M05 montre bien la bioluminescence cyan exclusive aux créatures
- [ ] Cohérence inter-mood (les 6 ressemblent au même jeu)

Si OK → proceed au batch tools (5 items via prompts spécifiques).
Si pas OK → re-prompt jusqu'à convergence.
