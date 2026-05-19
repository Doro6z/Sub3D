# Sub3D — Project Settings Canonical Spec

| | |
|---|---|
| **Date** | 2026-05-18 |
| **Statut** | ✅ Canonique — source de vérité Project Settings pour Sub3D |
| **Engine** | UE 5.7 |
| **Hardware cible** | i7-9700K + GTX 1660 Super, 1080p @ 60 fps |
| **Scope** | Tous settings `Edit > Project Settings` impactant rendu, perf, gameplay Sub3D |
| **Docs parallèles** | [Lighting/PP/Fog canonical](2026-05-18_lighting_pp_fog_canonical_spec.md) · [Cave abyssal material](2026-05-17_cave_abyssal_material_spec.md) |

---

## Sommaire

| # | Section | Lecture |
|---|---|---|
| 0 | [TL;DR — settings critiques](#0-tldr--settings-critiques) | 1 min |
| 1 | [Engine — General Settings](#1-engine--general-settings) | 1 min |
| 2 | [Engine — Rendering — Default Settings](#2-engine--rendering--default-settings) | 2 min |
| 3 | [Engine — Rendering — Lighting](#3-engine--rendering--lighting) | 3 min |
| 4 | [Engine — Rendering — Postprocessing](#4-engine--rendering--postprocessing) | 2 min |
| 5 | [Engine — Rendering — Translucency / Substrate](#5-engine--rendering--translucency--substrate-ue57-critique) | 3 min |
| 6 | [Engine — Rendering — Optimizations & Anti-Aliasing](#6-engine--rendering--optimizations--anti-aliasing) | 2 min |
| 7 | [Engine — Rendering — Hardware RT / Default RHI / Mobile](#7-engine--rendering--hardware-rt--default-rhi--mobile) | 2 min |
| 8 | [Engine — Streaming / World Partition](#8-engine--streaming--world-partition) | 2 min |
| 9 | [Project — Maps & Modes / Packaging](#9-project--maps--modes--packaging) | 1 min |
| 10 | [Plugins — recommandés on/off](#10-plugins--recommandés-onoff) | 2 min |
| 11 | [Settings à NE PAS toucher](#11-settings-à-ne-pas-toucher) | 1 min |
| 12 | [Validation après changement](#12-validation-après-changement) | 1 min |

---

## 0. TL;DR — settings critiques

Si tu ne lis qu'une chose, ces 8 settings ont l'impact le plus fort sur Sub3D :

| Setting | Path | Valeur | Pourquoi |
|---|---|---|---|
| **Use Fixed Frame Rate** | Engine > General > Framerate | **True (60.0)** | Memory canon — élimine jitter motion-chain |
| **Dynamic GI Method** | Engine > Rendering > Lighting | **Screen Space (Beta)** | Décision 2 canonical PP — PMC ≠ MDF |
| **Reflection Method** | Engine > Rendering > Lighting | **Lumen** | Gardé pour wet rock |
| **Support Hardware Ray Tracing** | Engine > Rendering > Lighting | **False** | GTX 1660 = no HW RT |
| **Generate Mesh Distance Fields** | Engine > Rendering > Lighting | **True** | Utile pour DF shadows (futurs static meshes) |
| **Substrate** | Engine > Rendering > Substrate | **False** | Évite conflit avec materials existants (cave) |
| **Shadow Map Method** | Engine > Rendering > Shadows | **Virtual Shadow Maps** | Modern UE5 |
| **Default RHI** | Engine > Rendering > Default RHI | **DirectX 12** | Requis pour Lumen Reflections + VSM |

---

## 1. Engine — General Settings

`Edit > Project Settings > Engine > General Settings`

| Setting | Valeur Sub3D | Pourquoi |
|---|---|---|
| **Use Fixed Frame Rate** | **✅ True** | **Critique** — memory `project_motion_chain_jitter_root_cause_2026_04_27`. PIE sans → batching variable sim → jitter aux trigger events |
| **Fixed Frame Rate** | **60.0** | Match `USubMovementComponent::FixedSimulationHz` (CLAUDE.md required config) |
| **Smooth Frame Rate** | ❌ False | Mutuellement exclusif avec Fixed |
| **Frame Rate Limit (T.MaxFPS)** | 60 | Cap shipping ; en dev tu peux unlimit via console `t.MaxFPS 0` |
| **Custom Time Step** | None | Pas de manipulation timestep custom |

> **Runtime guardrail** : `SubMovementComponent.cpp:118` warn au BeginPlay si Use Fixed Frame Rate = False. Si tu vois ce warning, tu as régressé.

---

## 2. Engine — Rendering — Default Settings

`Project Settings > Engine > Rendering > Default Settings`

| Setting | Valeur Sub3D | Pourquoi |
|---|---|---|
| **Anti-Aliasing Method** | **TSR (Temporal Super Resolution)** | UE5 défaut, stable avec marine snow + particulates |
| **Auto Exposure** | ❌ **False** | Override par PostProcessVolume manuel (canonical §2) |
| **Auto Exposure Bias** | 0 | N/A (overridé per volume) |
| **Bloom** | ✅ True | Activé pour biolum HDR pop |
| **Lens Flares** | ✅ True (intensity 0.1 per PP) | Subtile sur sub headlight uniquement |
| **Motion Blur** | ❌ **False** | OFF strict en cave (canonical §5.4) |
| **Ambient Occlusion** | ✅ True | Override per PP volume (intensity 0.8) |
| **Ambient Occlusion Static Fraction** | ❌ False | Pas de lightmass baked |
| **Subsurface Scattering** | ✅ True | Pour Quinn crew skin + future créatures |
| **Separate Translucency** | ✅ True | Évite que translucides cassent depth-based effects |
| **Bloom Convolution Texture** | T_DefaultConvolution | Si tu actives Convolution Bloom (canonical §10.17) |

---

## 3. Engine — Rendering — Lighting

`Project Settings > Engine > Rendering > Lighting`

> **Section critique** — 8 settings impactent directement perf et look.

| Setting | Valeur Sub3D | Pourquoi |
|---|---|---|
| **Dynamic Global Illumination Method** | **Screen Space (Beta)** | Décision 2 canonical — Lumen GI inefficace sur PMC sans MDF |
| **Reflection Method** | **Lumen** | Gardé cheap, utile wet rock sub |
| **Shadow Map Method** | **Virtual Shadow Maps** | Modern UE5 défaut, qualité shadow + cost OK 1660 |
| **Allow Static Lighting** | ✅ True | Garde l'option ouverte pour interior sub baking si besoin |
| **Generate Mesh Distance Fields** | ✅ **True** | Utile pour distance field shadows sur static meshes (rocks PCG) |
| **Mesh Distance Field Resolution Scale** | 1.0 | Défaut |
| **Compress Mesh Distance Fields** | ✅ True | Réduit VRAM |
| **Eight Bit Mesh Distance Fields** | ❌ False | Garde précision pour shadows précis |
| **Support Distance Field Shadows** | ✅ True | Combo avec MDF généré |
| **Skylight Real-time Capture** | ❌ False | Évite recapture continue (SkyLight cubemap statique) |
| **Skylight Real-time Capture Time Slice** | (irrelevant si Real-time = false) | — |
| **Allow Movable Lights To Generate Reflection Captures** | ❌ False | Pas besoin pour Sub3D (refl captures placées manuellement) |

---

## 4. Engine — Rendering — Postprocessing

`Project Settings > Engine > Rendering > Postprocessing`

| Setting | Valeur Sub3D | Pourquoi |
|---|---|---|
| **Custom Depth-Stencil Pass** | **Enabled with Stencil** | Permet PP materials avancés (mask par stencil — sub interior vs cave exterior) |
| **Local Exposure** | ✅ **True** | Activé pour HDR cinema look (canonical §5.2-bis) |
| **Mobile Tonemapper Film** | N/A | Sub3D PC only |
| **Lens Flares Quality** | High | Lens flares limited usage mais quality high sur ce qu'on a |
| **Bloom Quality** | High | Biolum HDR doit être propre |
| **AntiAliasing Type** | (héritera Default Settings TSR) | — |

---

## 5. Engine — Rendering — Translucency / Substrate (UE5.7 critique)

> ⚠️ **Section piège** — Substrate (nouveau UE5.7) peut être partiellement activé et casser tes materials existants. Tu as eu des shader errors `'SubstratePixelFootprint' in 'FMaterialPixelParameters'` plus tôt — signe que Substrate était partiellement actif.

`Project Settings > Engine > Rendering > Substrate`

| Setting | Valeur Sub3D | Pourquoi |
|---|---|---|
| **Substrate** | ❌ **False** | **Critique** — désactive pour éviter conflit avec materials cave existants. Substrate nécessite re-authoring complet du pipeline material |
| **Substrate Backwards Compatibility** | N/A si Substrate=false | — |
| **Strata** (ancien nom Substrate dans certaines versions) | ❌ False | Synonyme |

`Project Settings > Engine > Rendering > Translucency`

| Setting | Valeur Sub3D | Pourquoi |
|---|---|---|
| **Separate Translucency** | ✅ True | Permet PP volumes d'affecter translucides correctement |
| **Order Independent Translucency (OIT)** | ❌ False | Coûteux, pas critique pour cave |
| **Translucent Sort Policy** | Sort by Distance | Défaut UE |

**Migration future Substrate** : si tu veux activer plus tard (UE5.8+ ?), il faudra :
1. Audit tous tes materials (M_CaveAbyssal, M_PP_*, materials sub interior)
2. Re-author chaque master en Substrate
3. ~2-4 jours de work selon nombre de materials
→ Backlog post-FP.

---

## 6. Engine — Rendering — Optimizations & Anti-Aliasing

`Project Settings > Engine > Rendering > Optimizations`

| Setting | Valeur Sub3D | Pourquoi |
|---|---|---|
| **Auto Instancing** | ✅ True | Réduit draw calls (critique pour PCG rocks futurs) |
| **Forward Shading** | ❌ **False** | **Critique** — Forward incompatible Lumen Reflections |
| **Mesh Auto-Instancing** | ✅ True | |
| **Vertex Foggin****g for Opaque Pixels** | ✅ True | Permet HeightFog d'affecter pixels opaque |

`Project Settings > Engine > Rendering > Anti-Aliasing`

| Setting | Valeur Sub3D | Pourquoi |
|---|---|---|
| **TSR Quality** | Medium | Canonical recommandation (1660 Super) |
| **TAA Quality** | High (fallback) | Si tu rétrogrades en TAA |

`Project Settings > Engine > Streaming`

| Setting | Valeur Sub3D | Pourquoi |
|---|---|---|
| **Use Background Level Streaming** | ✅ True | Background load des levels streaming |
| **Texture Streaming** | ✅ True | Mip streaming auto |
| **Pool Size** | **2000 MB** (sur 6GB VRAM 1660 Super) | ~33% VRAM pour textures streaming, laisse marge pour cave PMC + lights |

---

## 7. Engine — Rendering — Hardware RT / Default RHI / Mobile

`Project Settings > Engine > Rendering > Hardware Ray Tracing`

| Setting | Valeur Sub3D | Pourquoi |
|---|---|---|
| **Support Hardware Ray Tracing** | ❌ **False** | GTX 1660 Super = no RT hardware |
| **Path Tracing** | ❌ False | Pas pour gameplay temps réel |
| **Support Compute Skin Cache** | ✅ True | Optimisation skinning, utile Quinn crew skel |
| **Software Ray Tracing Mode** | Global Tracing | Pour Lumen Reflections (fallback software) |

`Project Settings > Engine > Rendering > Misc`

| Setting | Valeur Sub3D | Pourquoi |
|---|---|---|
| **Default RHI** | **DirectX 12** | Requis pour Lumen Reflections + VSM (DX11 trop limité) |
| **Mobile HDR** | ❌ False | Sub3D PC only |
| **Velocity Pass** | Default (Opaque) | Pour TSR motion vectors |

---

## 8. Engine — Streaming / World Partition

`Project Settings > Engine > World Partition`

| Setting | Valeur Sub3D | Pourquoi |
|---|---|---|
| **Use World Partition by Default** | ✅ True | Défaut UE5, support streaming par cellule |
| **Use Concurrent Streaming** | ✅ True | Multi-thread cell loading |
| **Default Cell Streaming Quality** | High | Cave generation peut être lourde |

> **Spécifique Sub3D** : ton cave gen actuel est mono-actor (`AGeologicalCaveActor`), pas World Partition cells. Tu pourras migrer post-FP vers WP pour streaming par chunk si nécessaire. Pour l'instant WP par défaut OK, mais cave gen reste hors-WP.

`Project Settings > Engine > Garbage Collection`

| Setting | Valeur Sub3D | Pourquoi |
|---|---|---|
| **GC Mark Phase Time Limit** | Default | Pas critique tant que pas de spike GC visible |
| **Time Between Purging Pending Kill Objects** | Default (60s) | OK |
| **Cluster Garbage Collection** | ✅ True | Réduit overhead GC pour gros assets (cave PMC chunks) |

---

## 9. Project — Maps & Modes / Packaging

`Project Settings > Project > Maps & Modes`

| Setting | Valeur Sub3D | Pourquoi |
|---|---|---|
| **Default GameMode** | `ASubGameMode` | CLAUDE.md authoritative |
| **Default Pawn Class** | `BP_SubmarineCrew` | Crew character par défaut |
| **HUD Class** | `(à définir)` | Post-FP HUD assignation |
| **PlayerController Class** | `ASubPlayerController` | CLAUDE.md |
| **Spectator Class** | `DefaultSpectatorPawn` | Defaut UE |
| **GameState Class** | (à définir) | Post-FP |
| **Editor Startup Map** | `Proto02_Traversal` ou cave test map | Au choix selon focus dev |
| **Game Default Map** | `L_MainHub` ou similaire | Map jeu shipping |
| **Transition Map** | (vide) | Pas de transition map pour FP |

`Project Settings > Project > Packaging`

| Setting | Valeur Sub3D | Pourquoi |
|---|---|---|
| **Build Configuration** | Development (dev), Shipping (cooked release) | Standard |
| **Asset Manager** | Defaults | Pas customisé encore |
| **List of maps to include in a packaged build** | `Proto02_Traversal`, `L_WaterProto_*`, etc. | À fixer pour cooked builds |

---

## 10. Plugins — recommandés on/off

`Edit > Plugins`

### Plugins Sub3D custom (toujours ON)

| Plugin | Status | Rôle |
|---|---|---|
| **LevelSwitcher** | ✅ On | Editor-only level switcher |
| **RuntimeSyncDiagnostics** | ✅ On | Replication debug |
| **UnrealClaude** | ✅ On | MCP bridge IA |
| **Sub3DDebugPanel** | ✅ On | Slate dockable debug panel |

### Engine plugins critiques (ON)

| Plugin | Status | Pourquoi |
|---|---|---|
| **Enhanced Input** | ✅ On | CLAUDE.md uses EnhancedInput partout |
| **Niagara** | ✅ On | Particules biolum, bubbles, marine snow |
| **PCG (Procedural Content Generation)** | ✅ On | Backlog : scatter rocks, stalactites, mineral pockets |
| **Procedural Mesh Component** | ✅ On | `AGeologicalCaveActor` utilise PMC |
| **Gameplay Tags** | ✅ On | Tags pour systèmes Sub3D |
| **UMG (Widgets)** | ✅ On | HUD, helm, station UIs |

### Engine plugins optionnels (à activer selon besoin)

| Plugin | Status | Pourquoi |
|---|---|---|
| **Dynamic Mesh Component** | ⏸ Optionnel | Si migration PMC → DMC post-FP (pour Lumen GI compat) |
| **Geometry Script** | ⏸ Optionnel | Si manipulation runtime de mesh |
| **MetaSound** | ✅ On | Audio sub3D (MetaSound MS_Flood_Interior existe déjà) |
| **Substance 3D for Unreal** | ⏸ Optionnel | Si tu importes Substance assets directement |

### Plugins à DÉSACTIVER (inutiles Sub3D)

| Plugin | Pourquoi désactiver |
|---|---|
| **VR** plugins (OculusVR, OpenXR, SteamVR) | Sub3D pas VR |
| **AR** plugins (ARCore, ARKit) | Pas AR |
| **PixelStreaming** | Pas streaming |
| **OnlineSubsystemSteam** | Tant que pas multijoueur shipped |
| **Bridge** (Quixel Bridge plugin built-in) | Tu peux garder si tu utilises Megascans, sinon désactiver |
| **DataValidation** | OK de garder, n'impacte pas perf |
| **Linter** | Optionnel |

> Désactiver les plugins inutiles réduit le temps de startup éditeur + cook time + size build packaged.

---

## 11. Settings à NE PAS toucher

⚠️ Settings où la valeur défaut UE5.7 est correcte et toute modification risque de casser. Ne change PAS sans raison forte :

| Setting | Path | Pourquoi pas toucher |
|---|---|---|
| **Renderer Settings — Forward Shading** | Engine > Rendering > Optimizations | Forward = pas de Lumen Reflections. Décision canonical = deferred |
| **Forward Renderer Material Quality** | Engine > Rendering | N/A si Forward off |
| **GPU Skin Cache** | Engine > Rendering | Défaut OK pour Quinn skel |
| **Use Less CPU in the Background** | Engine > General | Garde True pour dev confort |
| **Network — Replication Driver** | Engine > Network | Default OK pour mono-joueur FP |
| **Physics — Default Gravity Z** | Engine > Physics | -980 cm/s² défaut Earth. Tu peux jouer dessus pour underwater feel mais c'est gameplay, pas rendu |

---

## 12. Validation après changement

Après modification de **n'importe quel setting Rendering** ou **Lighting** :

1. **Restart editor obligatoire** pour certains settings (Substrate, Forward Shading, Shadow Map Method, Default RHI)
2. **Rebuild shaders** — peut prendre 5-30 min selon impact. Préférable en fin de journée
3. **Open Sub3D test maps** :
   - `Proto02_Traversal` (cave + sub)
   - Map de test cave si tu en as une
4. **Vérifie `stat unit`** : FPS > 50, Game thread < 8 ms, GPU < 16 ms
5. **Vérifie `stat gpu`** : breakdown par section conforme à canonical §6
6. **Output Log** : pas de warning shader compile, pas de error rendering

### Commandes console utiles post-tweak

```
stat unit
stat scenerendering
stat gpu
r.VolumetricFog.GridPixelSize 8        // re-check value
r.Lumen.Reflections.ScreenSpaceReconstruction.MaxRayIntensity 20
ShowFlag.MotionBlur 0                  // verify Motion Blur off
ShowFlag.AmbientOcclusion 1            // verify AO on
ProfileGPU                             // single frame GPU profile
```

### Si quelque chose casse

- **Black scene** → check Auto Exposure + bApplyPhysicalCameraExposure (cf canonical §2)
- **Shader compile errors** → check Substrate (§5) et Forward Shading (§6)
- **Lumen leak white** → check Skylight Leaking 0 + Cubemap not null (§3 + canonical §3)
- **VSM artifacts** → check Shadow Map Method = VSM, DX12 RHI (§3 + §7)

---

## Cross-references

- **PostProcess / Fog / Lights detailed** → [Lighting/PP/Fog canonical](2026-05-18_lighting_pp_fog_canonical_spec.md)
- **Cave material spec** → [2026-05-17_cave_abyssal_material_spec.md](2026-05-17_cave_abyssal_material_spec.md) §5/§6/§7 (aligned avec canonical)
- **Observation doc frictions** → [2026-05-17_cave_abyssal_observation_doc.md](2026-05-17_cave_abyssal_observation_doc.md)
- **CLAUDE.md required config** : section "Required project config" (Use Fixed Frame Rate 60)
- **Memory canon** : `project_motion_chain_jitter_root_cause_2026_04_27`, `project_abyssal_setting_2026_05_07`

---

## Décisions verrouillées Project Settings

1. **Use Fixed Frame Rate = True (60.0)** — non négociable, memory + CLAUDE.md
2. **Substrate = False** — évite conflit cave materials, à reconsidérer post-FP
3. **Forward Shading = False** — Lumen Reflections requiert deferred
4. **Default RHI = DirectX 12** — requis VSM + Lumen Reflections
5. **Support Hardware Ray Tracing = False** — GTX 1660 = no HW RT
6. **Dynamic GI Method = Screen Space (Beta)** — PMC ≠ MDF, pas Lumen GI
7. **Shadow Map Method = Virtual Shadow Maps** — modern UE5 défaut
8. **Auto Exposure (project default) = False** — override per PostProcessVolume

Toute modification de ces 8 décisions doit faire l'objet d'un nouveau spec daté qui supersede explicitement celui-ci.
