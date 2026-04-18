# Sub3D Editor — Diagnostic & Plan Final d'Implémentation
**Date:** 2026-04-04
**Contexte:** L'éditeur compile (build succeeded) mais plusieurs fonctionnalités ne marchent pas au runtime.

---

## 1. DIAGNOSTIC — Bugs identifiés

### Bug 1: ASubmarinePreviewActor ne spawn pas
**Symptôme:** Clic sur "Preview Layout" → rien dans l'Outliner. Seuls `AuthEditorActor` et `EditorRuntime` sont visibles.

**Cause probable:** `ASubmarinePreviewActor` est déclaré `UCLASS(NotBlueprintable, NotPlaceable)` + `IsEditorOnly() = true`.
`ASubmarineEditorActor` utilise `UCLASS(BlueprintType, Blueprintable)` et spawn correctement.
La combinaison `NotPlaceable + IsEditorOnly` dans UE5.7 peut empêcher `SpawnActor` de créer l'actor dans le monde éditeur.

**Fix:** Changer la UCLASS en `UCLASS(NotBlueprintable)` (retirer `NotPlaceable`). `NotPlaceable` empêche le drag-drop dans le viewport mais peut interférer avec `SpawnActor` dans certains contextes UE5.7. Alternative: marquer `BlueprintType` comme `ASubmarineEditorActor`.

**Diagnostic ajouté:** Des `UE_LOG` sont déjà en place dans `HandlePreviewLayout` — après restart éditeur, vérifier l'Output Log pour le message exact.

### Bug 2: Toolbar extension "Preview" invisible
**Symptôme:** Pas de bouton Preview dans la toolbar de l'asset editor.

**Cause:** L'extender est ajouté avec `"Asset"` comme section cible, mais cette section n'existe peut-être pas dans la toolbar d'un `FAssetEditorToolkit` standard UE5.7. L'extension est ajoutée mais le point d'ancrage est incorrect.

**Fix:** Abandon de l'approche toolbar extender. Le bouton Preview Layout a été dupliqué dans le Drydock tab (déjà fait). Alternative: utiliser `RegenerateMenusAndToolbars()` après ajout de l'extender. Pour le MVP, le bouton dans le Drydock tab suffit.

### Bug 3: ControlRingId toujours visible dans le Details Panel
**Symptôme:** `meta=(HideInDetailPanel)` n'a pas d'effet sur `FControlRingDef::ControlRingId`.

**Cause:** `HideInDetailPanel` ne fonctionne pas sur les membres de struct à l'intérieur d'un `TArray<>` dans le Details Panel UE5. Le méta s'applique uniquement aux UPROPERTY de premier niveau sur un UObject.

**Fix:** Remplacer par `meta=(EditCondition="false", EditConditionHides)` — ça cache le champ dans toutes les vues, y compris dans les éléments de TArray.

### Bug 4: Widgets Slate pas recréés après hot-reload
**Symptôme:** Les boutons ajoutés (Generate Rings, Preview Layout dans Drydock) n'apparaissent pas.

**Cause:** Le hot-reload met à jour le code binaire mais ne réexécute PAS les tab spawners Slate. Les widgets existants gardent l'ancien layout. La version du layout a été bumpée de v11 à v12, mais il faut un **redémarrage complet de l'éditeur**.

**Fix:** Fermer UE5 complètement → relancer → réouvrir l'asset. Pas un bug de code.

---

## 2. COMPARAISON — Vision vs État Actuel

| Fonctionnalité (Vision doc) | État | Action |
|---|---|---|
| FSubmarineHullDef + ProfileParams | ✅ Complet | — |
| FSailDef (15+ champs) | ✅ Complet | HullBlendLengthCm ajouté |
| FBowSectionDef (12+ champs) | ✅ Complet | bBowPlanesRetractable ajouté |
| FSternSectionDef (14+ champs) | ✅ Complet | — |
| FOuterCasingDef (12+ champs) | ✅ Complet | CasingType/Offset/Ballast ajoutés |
| AuthoringAsset [A]/[B]/[C] marqueurs | ✅ Complet | — |
| bHullGeometryConfirmed | ✅ Champ existe | Pas de bouton UI dédié |
| HullGeometryHash / LayoutHash CRC | ✅ Complet | BakeSubsystem les calcule |
| ASubmarinePreviewActor | ⚠ Code OK, spawn KO | Fix Bug 1 |
| Ring Handle Visualizer (FComponentVisualizer) | ✅ Code complet | Dépend de Bug 1 |
| SSubmarineHullStatsBar widget | ✅ Complet | — |
| SSubmarineAppendagesPanel widget | ✅ Complet | — |
| Generate Rings button + spinbox | ✅ Code complet | Visible après restart UE |
| Preview Layout button (Drydock) | ✅ Code complet | Visible après restart UE |
| **3-panel UI (presets/viewport/ring settings)** | ❌ Non implémenté | Phase 2 |
| **Viewport intégré au toolkit** | ❌ Non implémenté | Phase 2 |
| **Preset system** | ❌ Non implémenté | Phase 3 |
| **FFrameRingDef** | ❌ Non implémenté | Futur |
| **Rings colorés par état** | ❌ Non implémenté | Phase 2 |
| **Éditeurs Shipyard/CrewConfig** | ❌ Non implémenté | Futur (in-game) |

---

## 3. PLAN FINAL — Priorité par phase

### Phase 0: Fix bugs bloquants (immédiat)

| # | Action | Fichier | Effort |
|---|---|---|---|
| 0.1 | Retirer `NotPlaceable` de ASubmarinePreviewActor | SubmarinePreviewActor.h | 1 ligne |
| 0.2 | Remplacer `HideInDetailPanel` par `EditCondition="false", EditConditionHides` sur ControlRingId | Sub3DHullTypes.h | 1 ligne |
| 0.3 | Restart complet UE5, tester Preview Layout + Generate Rings | — | Test |
| 0.4 | Vérifier Output Log pour le diagnostic HandlePreviewLayout | — | Test |

### Phase 1: Preview fonctionnel (bloquant pour tout le reste)

| # | Action | Détail |
|---|---|---|
| 1.1 | Confirmer que Preview Actor spawn et montre un mesh | Si Bug 1 fix ne suffit pas, fallback: ajouter ProceduralMeshComponent directement sur ASubmarineEditorActor |
| 1.2 | Auto-preview au clic Generate Rings | Déjà codé — HandleGenerateRings appelle HandlePreviewLayout si pas de preview actif |
| 1.3 | Auto-refresh à chaque changement de propriété | Déjà codé — OnPreviewPropertiesChanged → RefreshPreview |
| 1.4 | Tester: ring drag via FComponentVisualizer | Sélectionner le PreviewActor dans viewport → clic sur sphères → drag |

### Phase 2: UI améliorée (après preview fonctionnel)

| # | Action | Détail |
|---|---|---|
| 2.1 | Viewport intégré (SEditorViewport) dans le toolkit | Remplace le workflow "prévisualise dans le monde éditeur" par un viewport dédié dans le toolkit. Pattern: `FAdvancedPreviewScene` + `SEditorViewport` |
| 2.2 | Panel gauche: hull params + appendages summary | Refactorer SpawnRingTab pour layout 3 colonnes |
| 2.3 | Panel droit: ring sélectionné + détails | Afficher les props du ring cliqué, pas toute la TArray |
| 2.4 | Rings colorés dans le viewport | Modifier `FSubmarineRingHandleVisualizer::DrawVisualization` pour colorer vert/jaune/rouge/bleu |
| 2.5 | Bouton "Confirm Hull Geometry" | Toggle bHullGeometryConfirmed + recalcul hash |

### Phase 3: Presets et polish

| # | Action |
|---|---|
| 3.1 | Système de presets (struct FSubmarinePreset + TArray hard-codé) |
| 3.2 | Bouton "Apply Preset" → remplit Hull + ControlRings + Appendages |
| 3.3 | Curve editor pour profil longitudinal (optionnel) |
| 3.4 | Export/import preset JSON (optionnel) |

---

## 4. ACTION IMMÉDIATE — Fix Bug 1

Changement exact dans `SubmarinePreviewActor.h`:

```cpp
// AVANT:
UCLASS(NotBlueprintable, NotPlaceable)

// APRÈS:
UCLASS(NotBlueprintable)
```

Changement exact dans `Sub3DHullTypes.h`:

```cpp
// AVANT:
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull", meta=(HideInDetailPanel))
FName ControlRingId = NAME_None;

// APRÈS:
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hull", meta=(EditCondition="false", EditConditionHides))
FName ControlRingId = NAME_None;
```

Après ces 2 changements: **Build → Restart UE5 complet → Test**.

---

## 5. FALLBACK si le PreviewActor ne spawn toujours pas

Si retirer `NotPlaceable` ne suffit pas, le fallback est d'**ajouter le ProceduralMeshComponent directement sur ASubmarineEditorActor** (qui existe et fonctionne déjà):

1. Ajouter `UProceduralMeshComponent* PreviewMesh` à `ASubmarineEditorActor`
2. Ajouter `RefreshPreview()` qui fait la même chose que `ASubmarinePreviewActor::RefreshPreview()`
3. `HandlePreviewLayout` dans le toolkit appelle `EditorActor->RefreshPreview()` au lieu de spawner un 2ème actor
4. Supprimer `ASubmarinePreviewActor` devenu inutile

Avantage: un seul actor, pas de problème de spawn, la preview est juste un composant mesh supplémentaire sur l'actor qui existe déjà.
