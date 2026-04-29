---
title: Eau intérieure Sub3D — Matrice d'évaluation des approches
date: 2026-04-29
status: open / in research
related:
  - reports/research/2026-04-29_interior_water_architectural_reframing.md
  - memory/project_flood_containment_decision_2026_04_24.md
  - memory/project_water_material_post_fp.md
---

# Eau intérieure Sub3D — Matrice d'évaluation des approches

## 0. Statut et mode d'emploi

Document de R&D. **Aucune recommandation finale.** Pour chaque axe identifié dans `2026-04-29_interior_water_architectural_reframing.md`, ce doc liste les candidats et les évalue selon une grille multi-critères.

L'évaluation est qualitative (★ à ★★★★★) et **comparative à l'intérieur d'un même axe**. Une note ★★★★★ en "coût GPU steady" ne se compare pas à un autre axe.

Lecture recommandée : ouvrir le reframing en parallèle pour le contexte.

---

## 1. Légende des critères

Les 12 critères transverses du reframing sont rappelés ici, en abréviation pour les tableaux.

| Code | Critère | Sens (★ = mauvais, ★★★★★ = excellent) |
|---|---|---|
| **GPU-S** | Coût GPU steady-state | ★ = lourd permanent ; ★★★★★ = quasi-gratuit |
| **GPU-T** | Coût GPU transitoire (event) | ★ = pic violent ; ★★★★★ = pas de pic |
| **VIS-S** | Fidélité visuelle eau au repos | ★ = inerte/fake ; ★★★★★ = photo |
| **VIS-D** | Fidélité visuelle dynamique | ★ = aucune réaction ; ★★★★★ = vivant |
| **IMPL** | Complexité implémentation initiale | ★ = semaines/risqué ; ★★★★★ = quelques heures |
| **AUTH** | Complexité authoring récurrent | ★ = re-bake si géo change ; ★★★★★ = aucun authoring |
| **REP** | Robustesse repère mobile (sub bouge) | ★ = jitter/world-space casse ; ★★★★★ = sub-attaché par construction |
| **BRSQ** | Robustesse mouvements brusques | ★ = casse ; ★★★★★ = magnifie l'event |
| **SIM** | Compatibilité avec `USubFloodComponent` | ★ = nécessite refactor sim ; ★★★★★ = lit la donnée native |
| **MAINT** | Maintenabilité long-terme | ★ = module à entretenir indéfiniment ; ★★★★★ = stock UE |
| **ROLL** | Rollback-ability si l'approche échoue | ★ = couplé partout ; ★★★★★ = isolé, retire en 1 commit |
| **AMB** | Capacité ambiance "game over imminent" | ★ = inerte ; ★★★★★ = viscéral |

---

## 2. Axe 1 — Représentation du volume d'eau

> Quelle géométrie/structure représente le volume d'eau d'un compartiment ?

### 2.1 Tableau d'évaluation

| Approche | GPU-S | GPU-T | VIS-S | VIS-D | IMPL | AUTH | REP | BRSQ | SIM | MAINT | ROLL | AMB |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Plan plat unique (statu quo) | ★★★★★ | ★★★★★ | ★ | ★ | ★★★★★ | ★★★★ | ★★★ | ★★ | ★★★★★ | ★★★★ | ★★★★★ | ★ |
| Cap mesh plat per-room (silhouette XY) | ★★★★★ | ★★★★★ | ★★ | ★ | ★★★★ | ★★ | ★★★★★ | ★★ | ★★★★★ | ★★★★ | ★★★★ | ★★ |
| Volume mesh fermé suivant la coque + clip world-Z | ★★★★ | ★★★★★ | ★★★★ | ★★★ | ★★★ | ★★ | ★★★★★ | ★★★★ | ★★★★★ | ★★★ | ★★★ | ★★★★ |
| Box translucide englobante + clip + MDF sample | ★★★★ | ★★★★★ | ★★★ | ★★ | ★★★ | ★★★★ | ★★★★ | ★★★ | ★★★★★ | ★★★ | ★★★ | ★★★ |
| Heightfield 2D Niagara local par compartiment | ★★ | ★★★ | ★★★★ | ★★★★★ | ★★ | ★★★★★ | ★★★★ | ★★★★ | ★★★★ | ★★ | ★★ | ★★★★ |
| Heightfield 2D compute custom | ★★★ | ★★★ | ★★★★ | ★★★★★ | ★ | ★★★★★ | ★★★★ | ★★★★ | ★★★★ | ★ | ★★ | ★★★★ |
| Heightfield baked (Houdini) + playback paramétrique | ★★★★★ | ★★★★★ | ★★★★ | ★★★ | ★★ | ★ | ★★★★ | ★★★ | ★★★★ | ★★ | ★★ | ★★★ |
| Render Target heightmap peint à la main | ★★★★ | ★★★ | ★★★ | ★★★★ | ★★ | ★★★★★ | ★★★★ | ★★★ | ★★★★ | ★★ | ★★★ | ★★★ |
| FLIP/SPH 3D local par compartiment | ★ | ★ | ★★★★★ | ★★★★★ | ★ | ★★★★★ | ★★★ | ★★★★★ | ★★★ | ★ | ★★ | ★★★★★ |

### 2.2 Notes par approche

**Plan plat unique (statu quo).** L'option zéro. Visuel inerte, masking shader inévitable, l'origine des bugs actuels. Conservé dans la matrice pour calibrage.

**Cap mesh plat per-room.** Mesh plat dont la silhouette XY = section du compartiment au niveau du pont. La géométrie porte la silhouette → zéro masking shader. **Limite connue** (memory `project_flood_containment_decision_2026_04_24.md`) : la section XY varie avec Z dans une coque organique courbe. Le cap dimensionné à un Z donné déborde aux autres Z. Acceptable si la coque est quasi-tube ; rejeté pour Craniata.

**Volume mesh fermé + clip world-Z.** R&D approche 7.2. Mesh 3D fermé qui suit l'intérieur de la coque ; clip horizontal en world Z dans le shader (`if WorldPos.z > WaterSurfaceWorldZ discard`). La silhouette à n'importe quel niveau d'eau est la coupe horizontale du mesh — naturellement courbe. Pitch/roll du sub gérés gratuitement par le clip world-space. Authoring Blender plus exigeant (volume fermé, booleans avec piliers).

**Box translucide englobante + clip + MDF sample.** Variante simplifiée : box low-poly comme volume, clip au top par world-Z, masquage latéral par sample du Mesh Distance Field local de la coque. Authoring trivial (box auto-générée), mais dépend de la qualité du MDF — qui peut être insuffisante pour cloisons fines.

**Heightfield 2D Niagara local par compartiment.** UE 5.6+ Niagara Fluids 2D Shallow Water. Une grille 32×32 ou 64×32 par compartiment, alimentée par `WaterHeightCm` comme conditions aux bords. Pas d'authoring de mesh nécessaire. Coûteux en GPU, fragile à apprendre, la dynamique émerge naturellement (ondulations à l'impact, slosh), mais cellule de simulation à entretenir comme module.

**Heightfield 2D compute custom.** Mêmes propriétés que Niagara mais en compute shader maison. Pattern académique standard (papiers Solarflare, Müller). Plus de contrôle artistique, mais module à maintenir.

**Heightfield baked + playback.** Pré-simulé en Houdini sous accélérations latérales variables, encodé en flowmap + heightmap animées. Lookup à runtime selon l'accélération courante. Ratio fidélité/coût excellent steady, mais peu réactif aux événements (breach soudain). Sea of Thieves utilise des variantes pour rivières.

**Render Target heightmap peint à la main.** `UCanvasRenderTarget2D` 64×64 par compartiment, ping-pong. Sources (breach), drains (pompe), forces (accélération sub) écrits en clear+draw chaque tick ; un material applique diffusion. Le shader d'eau lit la RT comme heightmap. Reconstruction artisanale d'un Niagara Fluids 2D, mais contrôle total.

**FLIP/SPH 3D local par compartiment.** Niagara Pool of Water template. Spectaculaire pour un breach (gerbes au point d'entrée), trop coûteux pour le steady state. Stratégie hybride viable : FLIP juste à l'embouchure d'un breach les 2 premières secondes, puis fade vers une représentation steady plus légère (axe 5).

### 2.3 Couplage avec les autres axes

- Choix d'un **plan/cap/volume mesh** → axe 3 (dynamique) repose sur slosh paramétrique ou Gerstner ; pas d'ondulations émergentes.
- Choix d'un **heightfield** (Niagara/compute/RT) → axe 3 obtient les ondulations gratuitement ; axe 4 peut connecter topologiquement.
- Choix d'un **mesh suivant la coque** → axe 4 doit gérer le raccord aux portes (heightfield ne le requiert pas).

---

## 3. Axe 2 — Repère

> Dans quel repère la géométrie/le clip s'exprime-t-il ?

### 3.1 Tableau d'évaluation

| Approche | GPU-S | VIS-S | VIS-D | IMPL | REP | BRSQ | MAINT | ROLL | AMB |
|---|---|---|---|---|---|---|---|---|---|
| Tout en repère local sub (clip local Z) | ★★★★★ | ★★★ | ★★ | ★★★★★ | ★★★★★ | ★★ | ★★★★★ | ★★★★ | ★★ |
| Tout en repère monde (mesh non parenté) | ★★★★ | ★★★ | ★★★ | ★★★ | ★ | ★★ | ★★★ | ★★★ | ★★ |
| Hybride : mesh parenté local, clip world-Z | ★★★★★ | ★★★★ | ★★★★ | ★★★★ | ★★★★★ | ★★★★ | ★★★★ | ★★★★ | ★★★★ |
| Hybride avec yaw-only (statu quo) | ★★★★★ | ★★ | ★★ | ★★★★★ | ★★★★★ | ★★ | ★★★★★ | ★★★★★ | ★ |

### 3.2 Notes par approche

**Tout en repère local sub.** Mesh parenté au sub, clip dans le repère local. Surface d'eau toujours parallèle au plancher du sub. **Faux quand le sub pitche/roule** (l'eau ne reste pas gravity-aligned). Acceptable seulement si le sub a un pitch/roll très limité au gameplay.

**Tout en repère monde.** Mesh non parenté, position et clip en world. **Sujet au jitter** quand le sub bouge (l'écart numérique entre la position simulée du sub et la position du mesh diverge tick à tick). Casse aussi le découplage simulation/présentation utilisé partout dans Sub3D.

**Hybride mesh local + clip world-Z.** Le mesh est parenté au sub (zero jitter, suit le sub partout) ; le clip dans le shader prend la valeur world Z de la surface d'eau (`GetWaterSurfaceWorldLocation()`). Quand le sub pitche, le mesh tilte avec lui mais le clip horizontal en world reste horizontal → la portion visible adopte la forme correcte. Pitch/roll gérés par construction, pas de jitter.

**Hybride yaw-only (statu quo).** Le code actuel parente le mesh au sub mais ne prend que le Yaw du sub (`PlaneMeshComponent->SetWorldRotation(FRotator(0.f, SubYaw, 0.f))` dans `FloodWaterPlaneComponent.cpp:166`). Surface gravity-aligned correctement, mais ignore Pitch et Roll → la surface "flotte" dans le compartiment quand le sub pitche, sans réagir. Visuellement faux pour l'angle. Conservé en réf.

### 3.3 Couplage

- L'**hybride local+world** est compatible avec toute représentation. Recommandé par défaut sauf raison contraire.
- Un **heightfield Niagara** vit naturellement en sub-local (la grille suit le sub), mais sa surface peut être tiltée en world via une normale gravity-aligned dans le shader.

---

## 4. Axe 3 — Dynamique de surface

> Comment la surface bouge-t-elle ?

### 4.1 Tableau d'évaluation

| Approche | GPU-S | VIS-S | VIS-D | IMPL | BRSQ | AMB |
|---|---|---|---|---|---|---|
| Statique | ★★★★★ | ★ | ★ | ★★★★★ | ★ | ★ |
| Sine simple uniforme | ★★★★★ | ★★ | ★★ | ★★★★★ | ★ | ★★ |
| Gerstner waves vertex shader (faible amplitude) | ★★★★ | ★★★ | ★★★ | ★★★★ | ★★ | ★★★ |
| Slosh paramétrique 1D (offset Z) | ★★★★★ | ★★★ | ★★★★ | ★★★★ | ★★★★★ | ★★★★ |
| Slosh paramétrique 2D (offset Z + tilt) | ★★★★★ | ★★★ | ★★★★ | ★★★★ | ★★★★★ | ★★★★ |
| Heightfield simulé (Niagara/compute) — ondes émergentes | ★★ | ★★★★ | ★★★★★ | ★★ | ★★★★ | ★★★★★ |
| Gerstner FFT | ★★ | ★★★★★ | ★★★★ | ★ | ★★ | ★★★ |
| Heightfield baked + flowmap | ★★★★★ | ★★★★ | ★★★ | ★★ | ★★ | ★★★ |

### 4.2 Notes par approche

**Statique.** Aucun mouvement. Inerte à 100 %. Calibrage de la matrice.

**Sine simple uniforme.** `Z += sin(Time * f) * a`. Coûte rien, fait minimalement vivre. Ne réagit à rien.

**Gerstner waves vertex shader.** Pattern standard d'eau marine, amplitude réduite à 1-2 cm pour intérieur (compartiments ~200 cm). Donne une sensation d'eau vivante au repos. N'incorpore pas l'inertie du sub, juste le temps.

**Slosh paramétrique 1D.** Un état `(z_offset, z_velocity)` par compartiment, intégré comme ressort amorti excité par l'accélération du sub. Moins cher du tableau, ~80 lignes de C++. La lame "balance" en Z quand le sub accélère/freine. Forte sensation de masse pour très peu de coût. Implémenté côté CPU, poussé en MID/MPC.

**Slosh paramétrique 2D.** Variante avec aussi un `tilt` 2D (offset Z + inclinaison du plan de coupe). Reflète à la fois pitch et roll induits par l'accélération latérale. Mêmes propriétés que 1D, juste un peu plus d'état.

**Heightfield simulé.** Surface vivante par construction : ondes circulaires à l'impact, slosh émergent quand on excite les bords, ripples permanents subtils. Le plus expressif, le plus coûteux. Les autres axes (4, 5) en bénéficient gratuitement.

**Gerstner FFT.** Synthèse de Phillips spectrum. Conçu pour grandes étendues marines, overkill pour un compartiment 2 m × 2 m × 2 m. Coût et complexité injustifiés ici.

**Heightfield baked + flowmap.** Pré-simulé Houdini, encodé en textures. Steady fidèle, peu réactif aux événements ponctuels. Bon pour de l'ambiance "eau qui ondule", pas pour des transitions abruptes.

### 4.3 Couplage

- Slosh paramétrique = **CPU-side**, n'impose rien à la représentation. Compatible plan/cap/volume/heightfield. Probablement le pari le plus universel.
- Heightfield simulé inclut la dynamique dans l'axe 1. On ne combine pas heightfield + slosh paramétrique (redondant).

---

## 5. Axe 4 — Continuité visuelle aux portes

> Quand deux compartiments adjacents sont connectés par une porte ouverte, comment le rendu réagit-il ?

### 5.1 Tableau d'évaluation

| Approche | VIS-S | VIS-D | IMPL | SIM | MAINT | AMB |
|---|---|---|---|---|---|---|
| Aucune continuité (chaque compartiment ferme à la porte) | ★ | ★ | ★★★★★ | ★★★★★ | ★★★★★ | ★ |
| Stencil portal (porte ouverte écrit stencil partagé) | ★★★ | ★★ | ★★ | ★★★★ | ★★★ | ★★★ |
| VFX explicite (cascade Niagara à l'ouverture) | ★★★★ | ★★★★★ | ★★★ | ★★★★ | ★★★ | ★★★★★ |
| Heightfield connecté topologiquement | ★★★★★ | ★★★★ | ★★ | ★★★ | ★★ | ★★★★ |
| Raccord stylisé par seuil étendu (mesh d'eau dépasse de quelques cm dans la porte) | ★★★ | ★★ | ★★★★ | ★★★★ | ★★★★ | ★★ |

### 5.2 Notes par approche

**Aucune continuité.** Chaque compartiment a sa lame indépendante, fermée géométriquement par sa propre silhouette. Si la porte est ouverte et l'eau pleine des deux côtés, on voit deux lames indépendantes qui ne se rejoignent pas — visuellement c'est un raccord brutal, ressemble à un bug.

**Stencil portal.** Inspiré du portal rendering. Quand la porte est ouverte, elle écrit un stencil partagé entre les deux compartiments adjacents qui autorise la lame voisine à rendre dans son champ de vision. Quand la porte est fermée, pas de stencil partagé → cloisonnement strict. Lit `FFloodEdgeState.bClosed` côté client. Élégant mais fragile à mettre au point ; la pipeline custom depth + stencil avec translucides demande des workarounds.

**VFX explicite.** Quand la porte s'ouvre et qu'il y a un gradient de niveau d'eau entre les deux compartiments, déclencher une cascade Niagara au seuil. La continuité n'est pas géométrique — elle est narrée par un effet. Très bon pour l'ambiance ; le joueur voit littéralement l'eau "passer" la porte. Coût d'implémentation modéré (un Niagara system + un trigger sur changement d'état porte).

**Heightfield connecté.** Si l'axe 1 a choisi heightfield, on peut connecter la grille des deux compartiments à la position de la porte, conditionnellement à `bClosed`. La surface d'eau devient topologiquement continue. Solution la plus puissante mais qui présuppose heightfield (pas un choix indépendant).

**Raccord stylisé.** Le mesh d'eau d'un compartiment dépasse de 5-10 cm dans la porte ; un mesh de seuil de porte masque le raccord. Solution low-tech qui marche acceptablement quand les deux niveaux d'eau sont proches, casse quand ils divergent fortement.

### 5.3 Couplage

- Si **VFX explicite** (cette approche) couvre les portes, la même infra peut couvrir les brèches (axe 5).
- **Heightfield connecté** est inaccessible si l'axe 1 n'a pas choisi heightfield.

---

## 6. Axe 5 — Brèches

> Comment le visuel d'une brèche se déclenche-t-il et perdure-t-il ?

### 6.1 Tableau d'évaluation

| Approche | GPU-T | VIS-D | IMPL | SIM | MAINT | AMB |
|---|---|---|---|---|---|---|
| Source ponctuelle Niagara (jet) | ★★★ | ★★★★★ | ★★★★ | ★★★★★ | ★★★★ | ★★★★★ |
| Déformation locale du heightfield (impulse) | ★★★ | ★★★★★ | ★★ | ★★★★ | ★★ | ★★★★ |
| Burst FLIP/SPH 2 sec puis fade | ★ | ★★★★★ | ★ | ★★★★ | ★ | ★★★★★ |
| VFX-only sans modif représentation | ★★★★ | ★★★★ | ★★★★ | ★★★★★ | ★★★★ | ★★★★ |
| Mix : jet Niagara + montée de la lame existante | ★★★★ | ★★★★★ | ★★★★ | ★★★★★ | ★★★★ | ★★★★★ |

### 6.2 Notes par approche

**Source ponctuelle Niagara.** Le moment où `USubFloodComponent::CreateBreach` est appelé déclenche un Niagara system positionné à `BreachLocalCenter`, émettant un jet d'eau radial dans le compartiment. Indépendant de la représentation principale (axe 1) ; lit la sim pour son intensité (`InflowRateLitersPerSec`).

**Déformation locale du heightfield.** Si la représentation principale est un heightfield, on peut écrire une impulsion à la position de la brèche → ondulation circulaire émergente. Visuellement crédible, mais inopérant si l'axe 1 n'est pas heightfield.

**Burst FLIP/SPH.** Niagara FLIP 3D les premières secondes du breach, fade vers la représentation steady ensuite. Spectaculaire mais coûteux ; à réserver à des moments scénarisés.

**VFX-only.** Niagara seul, aucune modification de la lame d'eau principale. La sim continue à monter le niveau ; le VFX vit sa vie. Découplage maximal, le plus simple à implémenter.

**Mix jet + montée.** Le jet Niagara couvre la transition perceptive ; la lame d'eau monte parallèlement via la sim normale. Bon compromis entre impact visuel ponctuel et économie de complexité.

### 6.3 Couplage

- Toutes les approches dépendent de l'existence d'un mécanisme de **trigger gameplay→VFX** au moment du breach. Mécanisme à concevoir, n'existe pas pour Craniata aujourd'hui.
- Cohérent avec l'axe 4 si la même infra VFX gère portes et brèches.

---

## 7. Axe 6 — Vue sous l'eau

> Que voit le joueur quand sa caméra est immergée ?

### 7.1 Tableau d'évaluation

| Approche | GPU-S | VIS-S | VIS-D | IMPL | MAINT | ROLL | AMB |
|---|---|---|---|---|---|---|---|
| Post-process volume confiné par compartiment | ★★★★ | ★★★★ | ★★★ | ★★★★ | ★★★★ | ★★★★★ | ★★★★ |
| Post-process fullscreen + masking stencil | ★★★ | ★★★★ | ★★★★ | ★★★ | ★★★ | ★★★ | ★★★★ |
| Substrate slab épaisseur perçue | ★★★ | ★★★★★ | ★★★★ | ★★★ | ★★★ | ★★★ | ★★★★ |
| Volumetric fog densifié dans compartiment | ★★ | ★★★★ | ★★★ | ★★★★ | ★★★★ | ★★★★ | ★★★★ |
| Decals caustique projetés depuis waterline | ★★★★ | ★★★★ | ★★★ | ★★★ | ★★★ | ★★★★ | ★★★★ |
| Audio-only (LowPass + reverb humide) | ★★★★★ | ★ | ★★★ | ★★★★★ | ★★★★★ | ★★★★★ | ★★★★ |

### 7.2 Notes par approche

**Post-process volume confiné.** `APostProcessVolume` enfant du sub par compartiment, `bUnbound=false`, top clippé à `WaterHeightCm`. Quand la caméra y entre, blend underwater (tint, vignette, chromatic aberration). Pattern standard UE pour underwater confiné. Coût quasi nul, retour ambiance très élevé.

**Post-process fullscreen + stencil.** Material post-process plein écran qui détecte si la caméra est immergée via un stencil ID écrit par la lame d'eau, applique tint+distortion. Plus puissant mais plus complexe.

**Substrate slab.** Le matériau d'eau lui-même (Substrate slab) ajoute de l'absorption et du scattering. Vu de l'extérieur de l'eau on perçoit l'épaisseur ; vu de l'intérieur, la transmission donne un teint. UE 5.7 compatible. Cohérent avec une représentation volume mesh.

**Volumetric fog densifié.** `VolumetricCloud`/`Exponential Height Fog` confiné par un volume, densité montant avec `WaterLevelNormalized`. L'eau "monte" par densification volumétrique plus que par hauteur géométrique. Hack mais lit très bien.

**Decals caustique.** Light Function ou Decal projeté depuis "au-dessus de la waterline" avec texture caustique animée. Masqué au compartiment par stencil ID. Surface ajout réaliste sur les murs/sols immergés.

**Audio-only.** Aucun effet visuel ; juste un Sound Mix LowPass + reverb humide quand la caméra passe la waterline. Iron Lung principle — le son porte l'ambiance. À combiner avec n'importe quelle option visuelle pour amplifier l'effet.

### 7.3 Couplage

- L'axe 6 est **largement orthogonal** aux cinq autres. Il peut être prototypé indépendamment.
- Tout combo réussi inclura **plusieurs** options de cet axe (typiquement post-process volume + audio + caustique).

---

## 8. Combinaisons cohérentes (combos)

Certains choix par axe se composent mieux que d'autres. Quelques combos qui forment des systèmes cohérents :

### 8.1 Combo "minimum viable"

- Axe 1 : Cap mesh plat per-room (accepte la limite section-Z) **OU** Volume mesh + clip world-Z
- Axe 2 : Hybride mesh local + clip world-Z
- Axe 3 : Slosh paramétrique 1D
- Axe 4 : Aucune continuité (assumé, FP isolé)
- Axe 5 : VFX-only ponctuel (Niagara jet sur breach trigger)
- Axe 6 : Post-process volume confiné + audio LowPass

Profil : implémentation rapide, ambiance correcte, fragile sur la continuité aux portes (mais Craniata FP n'a pas de flow inter-compartiment de toute façon).

### 8.2 Combo "ambiance maximale"

- Axe 1 : Heightfield 2D Niagara local
- Axe 2 : Sub-local (heightfield grille)
- Axe 3 : Émergent (inclus dans heightfield)
- Axe 4 : Heightfield connecté
- Axe 5 : Déformation locale heightfield + jet Niagara
- Axe 6 : Post-process volume + Substrate slab + caustique decals + audio

Profil : implémentation longue, visuel le plus expressif, dépendance forte à Niagara Fluids (roadmap Epic).

### 8.3 Combo "low-effort, low-risk"

- Axe 1 : Plan plat per-room (cap mesh) — accepte la limite courbe Z
- Axe 2 : Hybride yaw-only (statu quo)
- Axe 3 : Statique ou sine
- Axe 4 : Aucune continuité
- Axe 5 : VFX-only
- Axe 6 : Audio-only + post-process volume

Profil : prototype en 2-3 jours, zéro nouvelle techno, atmosphère portée à 80% par l'audio et le PP. Moins viscéral en visu pure, mais shippable.

### 8.4 Combo "Iron Lung"

- Axe 1-3 : N'importe quel rendu minimal (plan plat suffit)
- Axe 4-5 : Aucune continuité, aucun VFX
- Axe 6 : Audio massif + visual minimal + obscurité partielle

Profil : pari design fort sur l'audio. Si le sound design est excellent, plus saisissant que n'importe quel rendu réaliste. Pari risqué.

---

## 9. Antipatterns à éviter

Documenté pour ne pas y retomber.

| Antipattern | Raison |
|---|---|
| Demander au shader de raymarcher la coque | Causa le bug actuel. Topologie n'est pas un problème shader. |
| Plan unique > footprint compartiment puis masquer | Force le shader à clipper, jamais fiable. |
| OBB en HLSL avec params poussés depuis C++ | Approximation grossière de la silhouette ; bugs de transcription quasi-inévitables (voir Row2/Row3). |
| Mesh non parenté au sub en world space | Jitter inéluctable dès que le sub bouge. |
| Cap mesh dimensionné à un Z dans une coque qui varie en Z | Déborde aux autres Z (memory `project_flood_containment_decision_2026_04_24.md`). |
| Coupler la simulation au rendu | `USubFloodComponent` pousse la donnée ; le rendu lit. Pas l'inverse. |
| Réutiliser `UFloodWaterVisualsComponent` legacy sur Craniata | Composant tagué Proto02, pas adapté à Craniata, prête à confusion. |

---

## 10. Hors scope de cette matrice

- Le tuning paramétrique des matériaux (caustique speed, foam density, tint color) — itératif en PIE quand l'architecture sera arbitrée.
- Les performances réseau (la couche eau est 100% client-local).
- Les comportements physiques sur le crew (buoyancy, swim) — couches gameplay distinctes.
- Le multi-sub visible (deux subs dans le frustum) — vérifier en perf review une fois l'archi choisie.

---

## 11. Conclusion provisoire

La matrice ne désigne aucun gagnant. Plusieurs combos sont défendables selon les contraintes (temps d'implémentation, profil de risque, ambition visuelle). L'arbitrage doit se faire en R&D ouverte, par expérimentation ciblée sur l'axe le plus contraignant identifié.

L'axe 1 (représentation) est probablement le plus structurant : son choix verrouille les options des axes 3 et 4. L'axe 6 (sous l'eau) est le plus indépendant et peut être prototypé en parallèle de toute décision sur le 1.
