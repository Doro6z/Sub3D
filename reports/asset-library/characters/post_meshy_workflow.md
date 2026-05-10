# Post-Meshy Workflow — Crew Characters

Étapes après que `launch_meshy.py` a fini, pour amener chaque personnage de FBX brut Meshy → personnage jouable en UE5.

Ordre : **Inspection → Blender (rig manuel) → UE5 (import) → Materials → Blueprint → PIE test**.

À refaire pour chacun des 4 personnages (Crew, Captain, Engineer, Concierge).

---

## Étape 1 — Inspection rapide du résultat Meshy

**Où** : `Content/Sub3D/Assets/Characters_v2/{Role}/`

**À vérifier** :

- `SK_{Role}_Mesh.fbx` présent, taille > 100 KB
- `SK_{Role}_Mesh.glb` présent (preview navigateur)
- 4-5 textures `T_{Role}_*.png` (base_color, metallic, normal, roughness, emissive)
- `thumbnail.png` présente

**Preview rapide** : double-click sur le `.glb` (Windows 10/11 ouvre le viewer 3D natif) ou drag vers https://gltf.report — vérifier en 30 secondes :

- T-pose stricte, pas affaissé
- Symétrie OK
- Pas de tools/chapeau/harnais (le strip a marché)
- Proportions cohérentes vs Captain benchmark

**Si KO** : relancer le perso seul → supprimer son entrée dans `meshy_state.json` puis re-run `launch_meshy.py`. Crédits perdus = 30 par re-roll.

---

## Étape 2 — Blender (rig manuel)

**Pré-requis** : avoir exporté `SK_Crew_Basic_Skeleton` depuis UE5 vers FBX une seule fois (réutilisable pour les 4 persos).

**Export skeleton depuis UE5** (à faire une seule fois) :

1. UE5 → `Content/Sub3D/Characters/Meshes/Bodies/SK_Crew_Basic` → clic droit → `Asset Actions > Export...`
2. FBX, décocher tout sauf "Skeleton" et "Skin Weights"
3. Sauvegarde → `reports/asset-library/characters/SK_Crew_Basic_Skeleton.fbx`

**Pour chaque personnage** :

1. Blender → File > Import > FBX → `SK_{Role}_Mesh.fbx` (mesh Meshy)
2. Vérifier l'échelle : la hauteur du perso doit être ~178 cm. Si pas, scale dans Blender (Object Mode, S, taper la valeur). Apply Scale (Ctrl+A).
3. Import du squelette : File > Import > FBX → `SK_Crew_Basic_Skeleton.fbx`
4. Aligner le squelette sur le mesh : sélectionner l'armature, déplacer/scaler pour que les bones tombent dans les bonnes zones anatomiques (root au pelvis, head au sommet, hands aux mains)
5. Parent mesh → armature : sélectionner le mesh, Shift+sélectionner l'armature, Ctrl+P → "Armature Deform > With Automatic Weights"
6. Cleanup weights manuel en Weight Paint mode si l'auto-weight a déconné (souvent : épaules, hanches, doigts)
7. Test : Pose Mode → bouger un bras, vérifier que le mesh suit proprement
8. **Critique** : vérifier les **18 bones** ciblés par `FAnimNode_CrewProcedural` ont des poids propres :
   `spine_01/02/03`, `upperarm_l/r`, `lowerarm_l/r`, `thigh_l/r`, `calf_l/r`, `hand_l/r`, `foot_l/r`, `head`, `neck_01`, `pelvis`
9. Export : File > Export > FBX → `SK_{Role}.fbx` (sans suffixe `_Mesh`) dans le même dossier
   - Settings : "Selected Objects" coché, "Armature" + "Mesh" objects, "Apply Scalings: FBX All", "Forward: -Z Forward", "Up: Y Up", "Apply Unit: ON"

**Temps estimé** : 30-45 min par perso pour un rig propre.

---

## Étape 3 — Import UE5

**Pour chaque `SK_{Role}.fbx`** :

1. UE5 → Content Browser → `Content/Sub3D/Characters/Meshes/Bodies/{Role}/` (créer le sous-dossier)
2. Drag-drop le FBX dans le Content Browser
3. Dans le dialogue d'import :
   - **Skeleton** : `SK_Crew_Basic_Skeleton` (sélectionner l'existant — réutilise le rig commun)
   - "Import Animations" : OFF (pas d'anims dans le FBX, on a celles procédurales)
   - "Import Materials" : OFF (on les fait à la main pour contrôle)
   - "Import Textures" : OFF (idem)
   - Mesh : "Use T0AsRefPose" coché
4. Click Import All

**Si UE5 refuse "Skeleton mismatch"** : c'est que le rig Blender a divergé du skeleton de référence (bones renommés, manquants, etc.). Retour Étape 2 → vérifier que tous les 26 bones de `SK_Crew_Basic_Skeleton` sont présents dans le FBX exporté avec EXACTEMENT les mêmes noms (case-sensitive).

---

## Étape 4 — Textures & Material Instance

**Import textures** :

1. Drag les 4-5 `T_{Role}_*.png` dans `Content/Sub3D/Characters/Textures/{Role}/`
2. Pour chaque texture, vérifier dans le détail :
   - `T_{Role}_BaseColor` : Compression "Default (DXT1/5)", sRGB **ON**
   - `T_{Role}_Normal` : Compression "Normalmap", sRGB **OFF**
   - `T_{Role}_Roughness`, `T_{Role}_Metallic` : Compression "Masks (no sRGB)", sRGB **OFF**
   - `T_{Role}_Emissive` (si présente) : Compression "Default", sRGB **ON**

**Material Instance** :

1. Naviguer vers le Master Material crew (à confirmer son chemin — probablement `Content/Sub3D/Characters/Materials/M_Crew_Master`). Si inexistant, créer un Material standard avec inputs BaseColor / Normal / Roughness / Metallic.
2. Clic droit sur M_Crew_Master → Create Material Instance → `MI_{Role}`
3. Assigner les 4-5 textures dans les slots
4. Save

---

## Étape 5 — Blueprint variant

**Pour chaque perso** :

1. Dupliquer `BP_SubmarineCrew` (le BP base de l'équipage) → `BP_Crew_{Role}`
2. Ouvrir le BP → Components → Mesh component
3. Mesh : assigner `SK_{Role}` (le skeletal mesh importé étape 3)
4. Materials → assigner `MI_{Role}` au slot du body
5. Anim Class : doit déjà être `ABP_Crew` (hérité du parent)
6. Save & Compile

---

## Étape 6 — Test in-PIE

1. Ouvrir une map où l'équipage spawn (probablement `Content/Maps/Proto03_Sub_HullPrecision`)
2. Drag `BP_Crew_{Role}` dans le sub
3. PIE
4. **Vérifications minimales** :
   - Le perso est debout, T-pose initiale puis posture neutre
   - Les anims procédurales s'appliquent (idle breathing visible — épaules qui montent/descendent)
   - Si on possède le perso : marche / run / strafing → bras et jambes swinguent correctement
   - Pas d'explosion mesh, pas de bones qui se détachent
5. **Si les anims sont cassées** (bras tendus en croix, jambes rigides) : c'est un problème de skinning step 2.8 — un ou plusieurs des 18 bones ciblés n'ont pas reçu de poids. Console `AnimList` puis `Anim debug 1` pour voir quels bones n'ont pas d'effet.

---

## Étape 7 — Add-ons modulaires (post-FP, plus tard)

Une fois les 4 base bodies en jeu et propres, on rajoute les accessoires identitaires (ce qui était dans les images de réf : galons, harnais, lampe, pochettes, outils) en static meshes attachés via sockets.

Pour l'instant : focus sur **les 4 base bodies fonctionnels en jeu**. Les add-ons viennent quand le rig est validé.

---

## Récap fichiers produits par perso

```
Content/Sub3D/Assets/Characters_v2/{Role}/      ← sortie Meshy brute
├── SK_{Role}_Mesh.fbx
├── SK_{Role}_Mesh.glb
├── T_{Role}_*.png × 4-5
└── thumbnail.png

Content/Sub3D/Characters/Meshes/Bodies/{Role}/  ← après rig Blender + import UE5
└── SK_{Role}.uasset

Content/Sub3D/Characters/Textures/{Role}/       ← textures importées
└── T_{Role}_*.uasset × 4-5

Content/Sub3D/Characters/Materials/             ← MI par perso
└── MI_{Role}.uasset

Content/Sub3D/Blueprint/PlayerBP/Crew/          ← BP variants
└── BP_Crew_{Role}.uasset
```

---

## Estimation temps total

- Étape 1 (inspection) : 5 min × 4 = 20 min
- Étape 2 (Blender rig) : 40 min × 4 = ~2h40
- Étape 3 (import UE5) : 5 min × 4 = 20 min
- Étape 4 (textures + MI) : 10 min × 4 = 40 min
- Étape 5 (BP variants) : 5 min × 4 = 20 min
- Étape 6 (PIE test) : 10 min × 4 = 40 min

**Total : ~5h work**, dont 2h40 de rig manuel Blender.
