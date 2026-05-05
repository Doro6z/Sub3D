# Sub3D Water — Mini-tests proto (pré-validation Phase 3-4)

**Date** : 2026-05-04
**Auteur** : Claude (analyse code-en-main)
**Contexte** : doc compagnon de `reports/plans/2026-05-04_water_implementation_plan.md`
**Branche** : `water-proto` (HEAD = `58e97d5`)

---

## Pourquoi ces mini-tests

Le plan d'implémentation Phase 3 (heightfield + slosh + matériau) et Phase 4 (sync au bord) ont chacun des hypothèses techniques qui méritent validation **avant** de commettre aux 2-3 jours de portage. Ces mini-tests utilisent l'environnement isolé du proto pour valider ces hypothèses sur du code et des assets qui existent déjà.

**Important — pas de polish proto** : ces tests **ne** sont **pas** des améliorations du proto pour le proto. Ils sont des *expériences ponctuelles* qui valident une question technique spécifique. À l'issue, le résultat est noté ici, et on passe au portage. Aucun code de mini-test n'est conservé pour la prod.

**Cycle attendu par mini-test** : 30 min à 2 heures, pas plus. Si un mini-test demande plus, c'est qu'il révèle un blocage qui aurait coûté plus cher à découvrir Phase 3/4 — gain net.

---

## Setup commun

1. **Branche** :
   ```
   git checkout water-proto
   git pull
   ```
   Ne PAS commit les modifs des mini-tests sur cette branche. Travailler en branche éphémère :
   ```
   git checkout -b water-proto-minitest-<id>
   ```
   À la fin du test : `git stash` ou `git checkout water-proto && git branch -D water-proto-minitest-<id>`.

2. **Niveau** : `Content/Maps/L_WaterProto_TwoRooms.umap` (existant, 2 rooms + 1 door).

3. **Assets clés à connaître** :
   - `Content/Sub3DWaterProto/BakedData/BD_Room_01.uasset`, `BD_Room_02.uasset`
   - `Content/Sub3DWaterProto/Materials/M_Phase0_Test.uasset` (matériau heightfield + Gerstner)
   - `Source/Sub3DWaterProto/Public/RoomWaterRenderer.h` (~130 lignes API publique)
   - `Source/Sub3DWaterProto/Private/RoomWaterRenderer.cpp` lignes 466-491 (wave equation)
   - `Source/Sub3DWaterProto/Public/DoorWaterBridge.h` (bridge component, partiellement implémenté)

4. **Build / log** :
   - Build `Sub3DEditor Win64 Development`
   - Log dédié : `Saved/Logs/WaterProto.log` (FOutputDeviceFile dans `Sub3DWaterProto.cpp`)
   - Log catégorie : `LogWaterProto`

5. **Console commands utiles** :
   - `WaterProto.SetRoomLevel <RoomId> <Level01>` — si exposée (sinon utiliser le slider dans `BP_RoomActor`)
   - `WaterProto.InjectAt <RoomId> <X> <Y> <Strength>` — idem

---

## Mini-test P-T3 — Validation matériau Substrate + WPO + R32F

**Pré-requis Phase concernée** : Phase 3 du plan d'implémentation principal.

**Hypothèse à valider** : `M_Phase0_Test` (proto) sample correctement une texture R32F via WPO dans le contexte de production Sub3D. Si vrai, le portage du matériau dans `M_CompartmentWater` est trivial. Si faux, le risque R-4 est confirmé et il faut prévoir un fallback (Single Layer Water classique au lieu de Substrate).

**Scope** : strictement matériau + composant. Pas de gameplay.

### Étapes

**P-T3.1** — Capture de référence en proto isolé
- Action : ouvrir `L_WaterProto_TwoRooms.umap`, lancer PIE
- Click dans `Room_01` (raycast → InjectAt déjà câblé via `BP_WaterProtoController`)
- Capture screenshot de l'onde
- **Critère** : onde radiale visible et propagation correcte. Si NON dans le proto isolé : le proto a régressé sur main, investiguer avant tout test de portage.

**P-T3.2** — Test matériau dans contexte Sub3D production
- Action : dans `L_Craniata_FP` (ou level Craniata standard), placer manuellement un `ARoomActor` du proto avec son `BD_Room_01` baked data assignée
- Reset des lights / volumes Sub3D normaux
- Lancer PIE, observer : matériau heightfield + WPO Gerstner s'affichent-ils correctement dans le post-process / lighting de Sub3D ?
- **Critères** :
  - ✅ WPO heightfield visible quand on click → ondes propagent
  - ✅ Animation Gerstner ambient continue
  - ✅ Pas de glitch graphique (z-fighting, transparence cassée, etc.)
  - ❌ Si l'un des trois échoue : noter le contexte (Substrate enabled? PostProcess Volume affecte? Lighting probe?), capturer screenshot

**P-T3.3** — Stress test : 2 ARoomActors visibles simultanément
- Action : placer 2 `ARoomActor` côte à côte dans Craniata level, PIE
- Inject sur les deux à la suite, observer
- **Critères** :
  - ✅ Aucun crash, FPS stable
  - ✅ Les deux surfaces sont distinctes (matériau MID per-instance OK)

### Livrable

Mise à jour de cette section avec :
- ✅/❌ pour chaque critère
- Screenshots dans `reports/tests/assets/2026-05-04_pt3/`
- Si KO → recommandation pour Phase 3 (fallback SLW classique vs Substrate / changement d'approche WPO)

### Estimation
**0.5 - 1 heure**.

---

## Mini-test P-T4 — Validation sync au bord (boundary heightfield merge)

**Pré-requis Phase concernée** : Phase 4 du plan d'implémentation principal.

**Hypothèse à valider** : un sync au bord par moyenne pondérée entre 2 heightfields adjacents (séparés par une porte) produit visuellement une surface continue ET propage les ondes à travers la frontière SANS oscillations résonantes. Si vrai, l'algorithme Phase 4 est validé. Si faux : ajuster les coefficients ou repenser l'approche (heightfield-connected pur, ou autre).

**Scope** : modification chirurgicale dans le proto, prototype l'algorithme du futur `USubmarineDoorWaterCoordinator`. Pas de bake éditeur, pas de bijection de cellules pré-calculée — on calcule à la volée.

### Étapes

**P-T4.0** — Dump des transforms (5 minutes, fait avant tout le reste)
- Action : ajouter une ligne de log dans `ARoomActor::BeginPlay` :
  ```cpp
  UE_LOG(LogWaterProto, Warning, TEXT("[ROOM] %s WorldXform=%s LocalBoxExtent=%s"),
      *RoomId.ToString(),
      *GetActorTransform().ToString(),
      *(CompartmentVolume ? CompartmentVolume->GetScaledBoxExtent() : FVector::ZeroVector).ToString());
  ```
- Idem dans `UDoorWaterBridge::BeginPlay` (ou ResolveRenderers) pour la porte :
  ```cpp
  UE_LOG(LogWaterProto, Warning, TEXT("[DOOR] %s WorldXform=%s ParentDoorWorldLoc=%s"),
      *GetName(),
      *GetOwner()->GetActorTransform().ToString(),
      ParentDoor ? *ParentDoor->GetActorLocation().ToString() : TEXT("<null>"));
  ```
- Lancer PIE sur `L_WaterProto_TwoRooms`, copier les 3 lignes de log
- Tracer schéma sur papier : où est Room_01, où est Room_02, où est la porte, quelle est l'orientation X/Y locale de chaque room par rapport à la porte
- **Critère** : avoir une vue déterministe avant de commencer à modifier le code. **Évite le tâtonnement** (P-T4.3 devient une vérification, pas une exploration).

**P-T4.1** — Identification des cellules frontière dans le proto
- Action : ouvrir `Source/Sub3DWaterProto/Public/RoomWaterRenderer.h`, identifier où les Heights[]/Velocities[] sont accessibles publiquement.
- Si non publics : ajouter temporairement `public:` getters/setters in-place :
  ```cpp
  TArray<float>& MutableHeights() { return Heights; }
  TArray<float>& MutableVelocities() { return Velocities; }
  int32 GetGridX() const { return GridX; }
  int32 GetGridY() const { return GridY; }
  float GetCellSizeCm() const { return CellSizeCm; }
  FTransform GetGridLocalTransform() const { return /* offset/rotation */; }
  ```
- **Critère** : code compile, accessible depuis `UDoorWaterBridge`.

**P-T4.2** — Implémentation sync trivial dans `UDoorWaterBridge`
- Fichier : `Source/Sub3DWaterProto/Private/DoorWaterBridge.cpp`
- Ajouter méthode `TickSyncBoundary()` appelée chaque tick :
  ```cpp
  void UDoorWaterBridge::TickSyncBoundary(float DeltaTime)
  {
      if (!RendererA || !RendererB) return;
      if (!ParentDoor || ParentDoor->bClosed) return;

      // Pour le proto : approximation grossière
      // - Frontière côté A = cellules à la frontière du compartiment A vers B
      // - On suppose la porte alignée sur l'axe X local de A et B (à valider sur L_WaterProto_TwoRooms)
      // - Cellules frontière = dernière colonne de A, première colonne de B (ou inverse selon orientation)

      const int32 GridXA = RendererA->GetGridX();
      const int32 GridYA = RendererA->GetGridY();
      const int32 GridXB = RendererB->GetGridX();
      const int32 GridYB = RendererB->GetGridY();

      TArray<float>& HeightsA = RendererA->MutableHeights();
      TArray<float>& HeightsB = RendererB->MutableHeights();
      TArray<float>& VelsA = RendererA->MutableVelocities();
      TArray<float>& VelsB = RendererB->MutableVelocities();

      // Calcul indices frontière — DÉPEND de l'orientation des grilles relatives à la porte.
      // À déterminer empiriquement en plaçant le dump debug.
      // Ci-dessous : version "dernière colonne de A vers première colonne de B"
      const int32 BoundaryColA = GridXA - 1;
      const int32 BoundaryColB = 0;

      const int32 NumCells = FMath::Min(GridYA, GridYB);
      const float HeightSync = SyncStrengthHeights;       // tunable, default 0.5
      const float VelocitySync = SyncStrengthVelocities;   // tunable, default 0.1

      for (int32 i = 0; i < NumCells; ++i)
      {
          const int32 idxA = BoundaryColA + i * GridXA;
          const int32 idxB = BoundaryColB + i * GridXB;

          if (!HeightsA.IsValidIndex(idxA) || !HeightsB.IsValidIndex(idxB)) continue;

          const float AvgH = 0.5f * (HeightsA[idxA] + HeightsB[idxB]);
          HeightsA[idxA] = FMath::Lerp(HeightsA[idxA], AvgH, HeightSync);
          HeightsB[idxB] = FMath::Lerp(HeightsB[idxB], AvgH, HeightSync);

          const float AvgV = 0.5f * (VelsA[idxA] + VelsB[idxB]);
          VelsA[idxA] = FMath::Lerp(VelsA[idxA], AvgV, VelocitySync);
          VelsB[idxB] = FMath::Lerp(VelsB[idxB], AvgV, VelocitySync);
      }
  }
  ```
- Tunables temporaires (UPROPERTY EditAnywhere) :
  - `float SyncStrengthHeights = 0.5f;`
  - `float SyncStrengthVelocities = 0.1f;`
- Appel : dans `UDoorWaterBridge::TickComponent`, après le slosh existant.
- **Critère** : compile, s'exécute sans crash en PIE.

**P-T4.3** — Vérification de l'orientation grilles ↔ porte (basée sur P-T4.0)
- Le dump P-T4.0 a déjà donné l'orientation déterministe. Ici on vérifie visuellement, pas on tâtonne.
- Action : ajouter un draw debug temporaire dans `TickSyncBoundary` :
  ```cpp
  // Highlight les cellules synchronisées en cyan dans Room_01, magenta dans Room_02
  FVector CellWorldA = RendererA->GetGridLocalTransform().TransformPosition(
      FVector(BoundaryColA * CellSizeCm, i * CellSizeCm, 0));
  DrawDebugSphere(World, CellWorldA, 5.f, 6, FColor::Cyan, false, -1.f);
  // idem côté B en magenta
  ```
- Calculer `BoundaryColA` / `BoundaryColB` à partir des transforms dumpés en P-T4.0 :
  - Si Room_01 forward = +X et Room_02 forward = -X (typique) avec porte entre les deux : BoundaryColA = GridXA - 1 (dernière X de A), BoundaryColB = 0 (première X de B)
  - Si autres orientations : déduire des transforms
- Lancer PIE, vérifier que les sphères sont dans le footprint de la porte des deux côtés.
- **Critère** : sphères cyan + magenta visiblement dans la zone de la porte. Si KO : revoir P-T4.0, pas tâtonner aveugle.
- **Estimation** : 5 minutes (vérification), pas 30 minutes (tâtonnement).

**P-T4.4** — Test scénario A→B (validation 4 critères clés)
- Setup en PIE :
  1. Room_01 : level = 0.5 (slider via BP_RoomActor)
  2. Room_02 : level = 0
  3. Porte fermée (`ParentDoor->bClosed = true` au démarrage)
- Action : ouvrir la porte (input ou console)
- Observations attendues sur 10-15 secondes :
  - **C1** : Niveaux convergent (Room_01 baisse, Room_02 monte) — déjà OK gameplay (FloodComponent simulé manuellement par les sliders)
  - **C2** : Surface visuellement continue à travers la porte (niveau Z correspond aux deux côtés au bord)
  - **C3** : Vagues injectées (click dans Room_01) se propagent à travers la porte vers Room_02
  - **C4** : Aucune oscillation résonante (les deux surfaces ne "ping-pong" pas indéfiniment après équilibre)
- Pour C4, observer pendant 30s+ après équilibrage. Si oscillations : noter l'amplitude et la période.

**P-T4.5** — Tuning
- Si C2 OK mais C4 KO (oscillations) :
  - Réduire `SyncStrengthVelocities` à 0.05, 0.02, 0
  - Si toujours oscillations : ajouter damping après sync : `VelsA[idxA] *= 0.95;` (et idem B)
  - Capturer les valeurs qui produisent une réponse stable
- Si C2 KO (discontinuité visible) :
  - Augmenter `SyncStrengthHeights` à 0.7, 0.9
  - Si toujours KO : la grille est probablement mal alignée → revoir P-T4.3
- Si C3 KO (vagues ne traversent pas) :
  - C'est le cas le plus problématique : Velocities sync trop faible
  - Tester `SyncStrengthVelocities` à 0.2, 0.3
  - Si C3 OK mais C4 KO simultanément : il y a une tension intrinsèque, accepter un compromis ou repenser

### Livrable

Mise à jour de cette section avec :
- Valeurs SyncStrengthHeights / SyncStrengthVelocities qui marchent
- Damping post-sync nécessaire ? (oui/non, valeur)
- Screenshots avant/après (équilibre + traversée d'onde)
- Conclusion : algorithme Phase 4 validé tel quel ? Adaptations à porter dans le plan principal ?

### Estimation
**1.5 - 2 heures**. Dont 30 min de tâtonnement P-T4.3 (orientation grille).

---

## Mini-test P-T5 (skippable) — Slosh modal sur sub mobile

**Statut** : test purement de tuning. **Recommandation** : skipper et faire directement Phase 3.6, sauf si le doute est important sur le fait que le slosh modal "valent les ~80 lignes".

Le slosh modal a été ajouté au plan par rapport aux 14 fondamentaux initiaux comme couche de juiciness. Si en Phase 3.6 le résultat est insatisfaisant, retrait trivial (~80 lignes). Le mini-test ici est principalement une pré-mesure des coefficients pour économiser du temps de tuning Phase 3.6.

Si vous faites P-T3 et P-T4 (les deux qui dérisquent les Phase 3-4 réellement), vous pouvez vous arrêter là. Total mini-tests à 2-3h au lieu de 3-4h.

---

**Pré-requis Phase concernée** : Phase 3 P3.6.

**Hypothèse à valider** : un slosh modal 2D parametrique excité par `SubMovement->GetVelocity()` produit un offset Z + tilt visiblement crédible quand le sub vire ou accélère, sans simulation lourde.

**Scope** : test indépendant, pas de couplage proto-Sub3D. Peut se faire dans `L_Craniata_FP` directement avec une nouvelle composante temporaire `USloshTestComponent`.

### Étapes

**P-T5.1** — Composant test
- Fichier : `Source/Sub3D/Submarine/SloshTestComponent.{h,cpp}` (temporaire, à supprimer après test)
- Composant ajouté au constructor de `ASubmarineBase` (ou attaché manuellement)
- Logique :
  ```cpp
  void TickComponent(...)
  {
      const FVector V = SubMovement->GetVelocityCmS();
      const FVector dV = (V - LastV) / DeltaTime;        // accélération
      LastV = V;

      // Excitation horizontale (slosh latéral)
      TiltVel += FVector2D(dV.Y, dV.X) * ExcitationGain;

      // Spring-damper update
      TiltVel += -Tilt * Stiffness * DeltaTime - TiltVel * Damping * DeltaTime;
      Tilt += TiltVel * DeltaTime;
      Tilt = ClampMagnitude(Tilt, MaxTiltDeg * Pi/180);

      // Apply visual tilt to a target plane (quick-and-dirty)
      if (DebugWaterPlane) {
          FRotator R = DebugWaterPlane->GetComponentRotation();
          R.Pitch = Tilt.X * 180/Pi;
          R.Roll = Tilt.Y * 180/Pi;
          DebugWaterPlane->SetWorldRotation(R);
      }
  }
  ```
- Tunables : Stiffness, Damping, ExcitationGain, MaxTiltDeg.
- **Critère** : compile, attache à un plan visible.

**P-T5.2** — Test PIE
- Lancer PIE Craniata, embarquer, prendre le helm, faire des virages brusques + accélérations / freinages
- Observer : DebugWaterPlane tilte de manière oscillante avec amortissement
- **Critères** :
  - ✅ Le plan tilte visiblement dans le sens opposé du virage
  - ✅ Amortissement : oscillation s'atténue après quelques secondes
  - ✅ Pas de chaos visuel (Tilt clampé)
- Tuner Stiffness/Damping pour un ressenti satisfaisant. Capturer les valeurs.

### Livrable

Valeurs Stiffness/Damping/ExcitationGain à utiliser comme défaut Phase 3.6.

### Estimation
**1 heure**. Optionnel — peut se faire directement Phase 3.6 si le test est jugé redondant.

---

## Récapitulatif

| Test | Validation | Phase impactée | Estimation | Priorité |
|---|---|---|---|---|
| **P-T3** | Matériau Substrate + WPO + R32F dans contexte Sub3D | 3 | 0.5-1h | HAUTE (R-4 mitigation) |
| **P-T4** | Sync au bord — heightfield boundary merge | 4 | 1-1.5h (P-T4.0 dump déterministe) | HAUTE (R-6 mitigation) |
| **P-T5** | Slosh modal 2D parametrique | 3.6 | 1h | SKIPPABLE (résultat Phase 3.6 directement) |

**Total recommandé (P-T3 + P-T4)** : **2-3 heures de mini-tests**. Pré-validation des deux risques techniques majeurs (R-4 matériau + R-6 résonance) avant d'investir 4-6 jours en Phase 3-4.

**Total étendu (avec P-T5)** : 3-4 heures. Choix selon disponibilité.

---

## RÉSULTATS DES MINI-TESTS — exécution 2026-05-05

### P-T3 — VALIDÉ ✅

| Sous-test | Verdict |
|---|---|
| **P-T3.1** baseline proto isolé | ✅ ondes radiales, Gerstner ambient OK, pas de glitch |
| **P-T3.2** matériau dans contexte Sub3D production (`Proto03_Sub_HullPrecision`) | ✅ Substrate + WPO + R32F fonctionnent dans le contexte gameplay |
| **P-T3.3** stress test 2 ARoomActors | ✅ MID per-instance OK, pas d'interférence entre rooms |

**Conclusion P-T3** : **R-4 (Substrate matériau) dérisqué**. Le matériau du proto est portable dans `M_CompartmentWater` Sub3D-side avec confiance.

### P-T4 — VALIDÉ ✅ (avec bug critique trouvé en cours)

| Sous-test | Verdict |
|---|---|
| **P-T4.0** dump transforms | ✅ géométrie déterministe (Room_01 Yaw=0, Room_02 Yaw=180, door world(-299,0,77), 18 cells par côté attendu) |
| **P-T4.1+P-T4.2** accessors + RecomputeCellPairs + TickSyncBoundary | ✅ implémentés, build OK |
| **P-T4.3** vérification orientation via debug spheres | ✅ 18 paires cyan↔magenta correctement alignées au footprint porte (yellow lines) |
| **P-T4.4** scénario A→B inject | ❌ initialement KO (eau accroché au bord, pas de propagation) → 🔧 bug trouvé → ✅ après fix |

**Bug critique trouvé** : le wave equation de `URoomWaterRenderer::TickHeightfield` ([RoomWaterRenderer.cpp:466-491](../../Source/Sub3DWaterProto/Private/RoomWaterRenderer.cpp#L466-L491) avant fix) ne traitait QUE les cellules internes (`x=1..W-2`, `y=1..H-2`). Les cellules de bord n'étaient jamais updatées par le wave equation → pas de Laplacian, pas de damping. Toute valeur écrite par un système externe (mon sync au bord) **restait accrochée pour toujours**, agissant comme une source permanente sans amortissement.

**Fix appliqué** : Neumann reflective boundary condition (out-of-grid neighbor = mirror du centre), boucle élargie à toutes les cellules (`x=0..W-1`, `y=0..H-1`). Les cellules de bord ont maintenant un Laplacian et leurs Velocities sont dampées comme les internes. Voir le diff sur cette branche `water-proto-minitest-pt4`.

**Implication architecturale (DOIT être appliquée Phase 3 du plan principal)** : le heightfield CPU porté dans `UFloodWaterPlaneComponent` (P3.4) doit traiter **toutes les cellules** dès l'écriture, avec Neumann BC. Sinon le sync au bord (Phase 4) ne fonctionnera pas pour la même raison. **Bug invisible jusqu'au moment où un système externe écrit dans les bords** — donc pas attrapable en testant juste le heightfield isolé.

### P-T4 — valeurs validées (à reporter dans le plan principal)

| Tunable | Valeur validée | Comportement observé |
|---|---|---|
| `SyncStrengthHeights` | **0.5** | Continuité visuelle nette aux bords, pas de step Z visible à l'équilibre |
| `SyncStrengthVelocities` | **0.1** | Onde traverse la frontière, pas de résonance R-6 |
| `VelocitiesPostSyncDamping` | **0.95** | Amortissement progressif, pas d'oscillation parasite |

**Pairing utilisé** : nearest-neighbor world-space (générique). Marche pour orientations arbitraires (validé sur Yaw=180 de Room_02).

### P-T5 — non exécuté (skip décidé)

Décision : faire directement Phase 3.6 du plan principal. Le résultat est observable directement Phase 3, retrait trivial si insatisfaisant.

---

## Conclusion mini-tests

✅ Les deux risques techniques majeurs (R-4 matériau, R-6 résonance) **sont dérisqués**. L'algorithme de boundary sync est validé géométriquement et dynamiquement.

🔧 Un bug subtil du proto a été trouvé et corrigé en cours de validation (Neumann BC manquant). **Le plan principal doit inclure ce fix dès l'écriture du heightfield Phase 3** — sinon Phase 4 échouera.

📌 **Branche éphémère `water-proto-minitest-pt4`** : conservée comme référence pour le portage Sub3D (montre l'algorithme de RecomputeCellPairs + TickSyncBoundary qui sera réécrit proprement dans `USubmarineDoorWaterCoordinator`). **Pas de merge sur `water-proto`** — code mini-test, pas du polish proto.

📌 **Le sync ne synchronise QUE le heightfield Heights[]** (ondes/deltas). Il ne synchronise PAS les niveaux absolus (`CurrentWaterLevelLocalZ`). En Sub3D production, les niveaux sont pilotés par `USubFloodComponent`, le sync au bord transfère uniquement l'activité ondulatoire. Cohérent avec le fondamental #4 (visuel suit FloodComponent).

**Prêt pour Phase 0 du plan principal.**

---

## Garde-fous

1. **Pas de polish** : aucun de ces tests ne polit le proto. Ils ajoutent du code temporaire qu'on jette ou stash après mesure.
2. **Pas de commit sur water-proto** : tout en branche éphémère.
3. **Documentation in-line** : compléter ce doc au fur et à mesure avec les résultats. Si une hypothèse est invalidée, c'est une mise à jour du plan principal, pas un échec.
4. **Garde-fou résonance** : si P-T4 montre des oscillations même avec damping fort, ouvrir la question de l'algorithme avec l'humain avant Phase 4 (peut-être heightfield-connected pur, ou autre approche du matrice de research).

---

**Fin du document.**
