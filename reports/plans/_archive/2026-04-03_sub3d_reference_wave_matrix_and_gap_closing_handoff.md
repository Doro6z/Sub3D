# Sub3D Reference Wave Matrix And Gap-Closing Handoff

Date: 2026-04-03
Workspace: `C:\Dev\Sub3D`
Reference: `C:\ACC\Projects\Sub3D\ProtoEditorV3\Sub3D_Reference_Totale_Execution_Waves_Spec_Prompts.md`

## Purpose

This document does two things:

1. establish the real state of each reference wave against the current codebase;
2. define a strict gap-closing handoff pass for every wave that is not fully closed.

This is a reference-alignment document.
It is not a rewrite plan.

## Evidence Basis

Compared against the reference document:

- `C:\ACC\Projects\Sub3D\ProtoEditorV3\Sub3D_Reference_Totale_Execution_Waves_Spec_Prompts.md`

Inspected code:

- `Source/Sub3DCore/Public/Types/*`
- `Source/Sub3DBake/Public/*`
- `Source/Sub3DBake/Private/*`
- `Source/Sub3DEditor/Public/*`
- `Source/Sub3DEditor/Private/*`
- `Source/Sub3DRuntime/Public/*`
- `Source/Sub3DRuntime/Private/*`
- `Source/Sub3DTests/Private/Automation/*`

Verified:

- `Sub3DEditor Win64 Development` compile succeeded on 2026-04-03.

Not verified in this pass:

- full automation run for all waves
- full manual editor verification
- full runtime validation in PIE

## Clarification On "Why Only Wave 6 To Wave 11?"

The earlier recommendation to focus on `Wave 6 -> Wave 11` was a prioritization shortcut, not a strict statement that `Wave 1 -> Wave 5` were fully closed.

What is true:

- `Wave 0` is functionally closed enough and compile-stable.
- `Wave 1 -> Wave 5` already have substantial code in place.
- `Wave 6 -> Wave 11` still contain the largest obvious functional gaps.

What is also true:

- `Wave 1 -> Wave 5` are not fully closed against the reference.
- several earlier waves have contract drift, missing functions, or missing verification.

Conclusion:

- only `Wave 0` can be treated as effectively closed;
- `Wave 1 -> Wave 11` still need strict reference-alignment passes.

## Important Numbering Note

Current code namespaces and some test names do not align exactly with the reference wave numbering.

Examples:

- current `Sub3DWave3` code spans reference `Wave 3` and part of reference `Wave 4`;
- current `Sub3DWave4` code corresponds to reference `Wave 5`.

This handoff uses the numbering from the reference document, not the internal namespace names.

## Matrix

| Wave | Reference intent | Real state | Main remaining gaps | Files to correct next |
|---|---|---|---|---|
| Wave 0 | foundations and empty shells | Closed enough | minor naming drift only | none unless later refactor is required |
| Wave 1 | hull evaluator and control rings | Partial | evaluator contract drift, missing bounds helpers, no Wave 1 automation in `Sub3DTests` | `Source/Sub3DBake/Public/Evaluation/SubmarineHullEvaluationLibrary.h`, `Source/Sub3DBake/Private/Evaluation/SubmarineHullEvaluationLibrary.cpp`, `Source/Sub3DBake/Public/Validation/SubmarineHullValidation.h`, `Source/Sub3DBake/Private/Validation/SubmarineHullValidation.cpp`, `Source/Sub3DTests/Private/Automation/*` |
| Wave 2 | generated ring sequence and base hull bake | Partial | main behavior exists, but file-by-file API differs from reference and closure depends on rerunning tests | `Source/Sub3DBake/Public/Bake/SubmarineRingSequenceBuilder.h`, `Source/Sub3DBake/Private/Bake/SubmarineRingSequenceBuilder.cpp`, `Source/Sub3DBake/Public/Bake/SubmarineHullBakeService.h`, `Source/Sub3DBake/Private/Bake/SubmarineHullBakeService.cpp`, `Source/Sub3DBake/Public/Data/CompiledSubmarineBaseAsset.h`, `Source/Sub3DTests/Private/Automation/RingSequenceTests.cpp`, `Source/Sub3DTests/Private/Automation/HullBakeTests.cpp` |
| Wave 3 | frame-rings and outer envelope | Partial | `BakeOuterEnvelope` exists, but no merge API and no explicit closure on silhouette validation | `Source/Sub3DCore/Public/Types/Sub3DEnvelopeTypes.h`, `Source/Sub3DBake/Public/Bake/SubmarineOuterEnvelopeBakeService.h`, `Source/Sub3DBake/Private/Bake/SubmarineOuterEnvelopeBakeService.cpp`, `Source/Sub3DTests/Private/Automation/OuterEnvelopeBakeTests.cpp` |
| Wave 4 | structural bays | Partial | bay solve exists, but capacity validation and explicit max-floor API are missing; wave numbering drift in code/tests | `Source/Sub3DCore/Public/Types/Sub3DBayTypes.h`, `Source/Sub3DBake/Public/Bake/SubmarineBaySolveService.h`, `Source/Sub3DBake/Private/Bake/SubmarineBaySolveService.cpp`, `Source/Sub3DEditor/Public/Preview/SubmarineBayDebugDraw.h`, `Source/Sub3DEditor/Private/Preview/SubmarineBayDebugDraw.cpp`, `Source/Sub3DTests/Private/Automation/BaySolveTests.cpp` |
| Wave 5 | deck levels and floor regions | Partial | floor bake exists, but support is generic only; no explicit proof for side corridor and mezzanine behaviors beyond enum presence | `Source/Sub3DCore/Public/Types/Sub3DFloorTypes.h`, `Source/Sub3DBake/Public/Bake/SubmarineFloorBakeService.h`, `Source/Sub3DBake/Private/Bake/SubmarineFloorBakeService.cpp`, `Source/Sub3DTests/Private/Automation/FloorBakeTests.cpp` |
| Wave 6 | openings, connectors, closures, navigation test | Partial | bake path exists, but `ValidateTraversalConstraints` is missing; navigation service is smoke only | `Source/Sub3DCore/Public/Types/Sub3DOpeningTypes.h`, `Source/Sub3DCore/Public/Types/Sub3DConnectorTypes.h`, `Source/Sub3DCore/Public/Types/Sub3DClosureTypes.h`, `Source/Sub3DBake/Public/Bake/SubmarineOpeningBakeService.h`, `Source/Sub3DBake/Private/Bake/SubmarineOpeningBakeService.cpp`, `Source/Sub3DEditor/Public/Tools/SubmarineNavigationTestService.h`, `Source/Sub3DEditor/Private/Tools/SubmarineNavigationTestService.cpp` |
| Wave 7 | partitions | Partial | partition layer exists, but reference APIs are collapsed into one function and partition-opening application is simplified | `Source/Sub3DCore/Public/Types/Sub3DPartitionTypes.h`, `Source/Sub3DBake/Public/Bake/SubmarinePartitionBakeService.h`, `Source/Sub3DBake/Private/Bake/SubmarinePartitionBakeService.cpp` |
| Wave 8 | derived flood volumes and flood graph | Partial | flood graph exists, but derived-volume build and propagation APIs are not separated; no flood debug overlay | `Source/Sub3DCore/Public/Types/Sub3DFloodTypes.h`, `Source/Sub3DBake/Public/Graph/SubmarineFloodGraphBuilder.h`, `Source/Sub3DBake/Private/Graph/SubmarineFloodGraphBuilder.cpp`, editor debug overlay files to add |
| Wave 9 | runtime baked-only final actor | Partial | runtime actor initializes components, but does not yet implement explicit render build, collision build, and full system init contract from the reference | `Source/Sub3DRuntime/Public/Actors/SubmarineRuntimeActor.h`, `Source/Sub3DRuntime/Private/Actors/SubmarineRuntimeActor.cpp`, `Source/Sub3DRuntime/Public/Components/SubmarineBreachRuntimeComponent.h`, `Source/Sub3DRuntime/Private/Components/SubmarineBreachRuntimeComponent.cpp`, `Source/Sub3DRuntime/Public/Components/SubmarineFloodRuntimeComponent.h`, `Source/Sub3DRuntime/Private/Components/SubmarineFloodRuntimeComponent.cpp`, additional render/collision component files if needed |
| Wave 10 | complete Slate editor shell | Partial | toolkit exists and opens, but no per-phase Slate tab classes, no explicit command binding layer, no phase state controller, no concept outliner | `Source/Sub3DEditor/Public/Slate/SubmarineEditorToolkit.h`, `Source/Sub3DEditor/Private/Slate/SubmarineEditorToolkit.cpp`, `Source/Sub3DEditor/Public/Slate/Tabs/*`, `Source/Sub3DEditor/Private/Slate/Tabs/*` |
| Wave 11 | contextual editing and product layer | Partial | product types exist, but contextual flow is not implemented; permissions are local data only; lobby and mission separation is not integrated | `Source/Sub3DCore/Public/Types/Sub3DProductTypes.h`, `Source/Sub3DRuntime/Public/Data/SubmarineCatalogEntry.h`, `Source/Sub3DRuntime/Public/Data/OwnedSubmarineState.h`, additional runtime/editor integration files as needed |

## Recommended Execution Order

Use the reference order, not the current code namespace order:

1. Wave 1 contract closure
2. Wave 2 closure and rerun of existing tests
3. Wave 3 closure
4. Wave 4 closure
5. Wave 5 closure
6. Wave 6 closure
7. Wave 7 closure
8. Wave 8 closure
9. Wave 9 closure
10. Wave 10 closure
11. Wave 11 closure
12. global validation pass

Reason:

- later waves already depend on earlier data contracts;
- if the earlier contracts stay fuzzy, later validation results remain weak.

## Handoff By Wave

Only `Wave 0` is treated as closed enough.
Every section below is a strict gap-closing pass.

### Wave 1

Status:

- partial implementation
- not closed against the file-by-file reference

Why it is not complete:

- `USubmarineHullEvaluationLibrary` is not a real `UBlueprintFunctionLibrary`;
- `EvaluateHullBoundsAtX(...)` is missing;
- `EvaluateInteriorBoundsAtX(...)` is missing;
- no dedicated Wave 1 automation files are present in `Source/Sub3DTests/Private/Automation/`.

Files to edit:

- `Source/Sub3DBake/Public/Evaluation/SubmarineHullEvaluationLibrary.h`
- `Source/Sub3DBake/Private/Evaluation/SubmarineHullEvaluationLibrary.cpp`
- `Source/Sub3DBake/Public/Validation/SubmarineHullValidation.h`
- `Source/Sub3DBake/Private/Validation/SubmarineHullValidation.cpp`
- add Wave 1 automation files under `Source/Sub3DTests/Private/Automation/`

Strict agent pass:

1. align the evaluator contract with the reference;
2. add hull bounds and interior bounds evaluation;
3. keep runtime/editor dependencies out;
4. add narrow automation tests for analytic evaluation and validation.

Your manual pass:

1. compile
2. run only Wave 1 automation
3. if the evaluator is meant to be Blueprint-visible, verify it appears in Blueprint

Exit gate:

- compile OK
- Wave 1 tests OK
- evaluator functions present exactly on the intended contract

### Wave 2

Status:

- substantial implementation exists
- closure is mostly contract and verification work

Why it is not complete:

- the reference names `BakePrimaryHull(...)`, but the current code exposes `BakeExteriorHull(...)` and `BakeInteriorHull(...)`;
- current closure relies on the fact that tests exist, but they have not been rerun in this pass;
- `UCompiledSubmarineBaseAsset` is broader than the original Wave 2 reference, which is acceptable, but the Wave 2 contract still needs an explicit close-out pass.

Files to edit:

- `Source/Sub3DBake/Public/Bake/SubmarineRingSequenceBuilder.h`
- `Source/Sub3DBake/Private/Bake/SubmarineRingSequenceBuilder.cpp`
- `Source/Sub3DBake/Public/Bake/SubmarineHullBakeService.h`
- `Source/Sub3DBake/Private/Bake/SubmarineHullBakeService.cpp`
- `Source/Sub3DBake/Public/Data/CompiledSubmarineBaseAsset.h`
- `Source/Sub3DTests/Private/Automation/RingSequenceTests.cpp`
- `Source/Sub3DTests/Private/Automation/HullBakeTests.cpp`

Strict agent pass:

1. decide whether to keep the current split API or add a reference-aligned facade;
2. keep deterministic output as the single source of truth;
3. rerun and fix existing Wave 2 automation only;
4. confirm the asset save/reload path if required by the done checklist.

Your manual pass:

1. compile
2. run `Sub3D.Wave2.*`
3. verify that the baked asset remains loadable in editor if saved

Exit gate:

- all current Wave 2 tests pass
- no contract ambiguity remains around the public bake API

### Wave 3

Status:

- frame-rings and outer envelope are present
- closure is still partial

Why it is not complete:

- `BakeOuterEnvelope(...)` exists;
- `MergeSecondaryEnvelopeIntoBaseRenderSections(...)` does not exist;
- the current outer envelope is a simple analytical bake, not a fuller merge contract;
- footprint validation is implicit, not explicit.

Files to edit:

- `Source/Sub3DCore/Public/Types/Sub3DEnvelopeTypes.h`
- `Source/Sub3DBake/Public/Bake/SubmarineOuterEnvelopeBakeService.h`
- `Source/Sub3DBake/Private/Bake/SubmarineOuterEnvelopeBakeService.cpp`
- `Source/Sub3DTests/Private/Automation/OuterEnvelopeBakeTests.cpp`

Strict agent pass:

1. keep the current analytical bake;
2. add the missing merge/validation API only if the reference requires it publicly;
3. do not broaden to advanced surfacing;
4. rerun Wave 3 tests.

Your manual pass:

1. compile
2. run Wave 3 automation
3. inspect one authoring asset with outer envelope enabled and disabled

Exit gate:

- envelope bake deterministic
- collision simplification still valid
- public contract aligned

### Wave 4

Status:

- structural bay solve exists
- API closure is incomplete

Why it is not complete:

- `SolveStructuralBays(...)` exists;
- `ValidateBayCapacity(...)` is missing as an explicit public function;
- `ComputeMaxFloorCountForBay(...)` is missing as an explicit public function;
- debug draw exists and is now callable from the toolkit, but this was not part of a formal closure pass against the reference.

Files to edit:

- `Source/Sub3DCore/Public/Types/Sub3DBayTypes.h`
- `Source/Sub3DBake/Public/Bake/SubmarineBaySolveService.h`
- `Source/Sub3DBake/Private/Bake/SubmarineBaySolveService.cpp`
- `Source/Sub3DEditor/Public/Preview/SubmarineBayDebugDraw.h`
- `Source/Sub3DEditor/Private/Preview/SubmarineBayDebugDraw.cpp`
- `Source/Sub3DTests/Private/Automation/BaySolveTests.cpp`

Strict agent pass:

1. expose capacity and max-floor helpers explicitly;
2. keep solve logic local and deterministic;
3. do not move bay logic into editor code;
4. rerun focused bay tests.

Your manual pass:

1. compile
2. run bay automation
3. open the toolkit and use `Draw Structural Bays`

Exit gate:

- explicit bay validation helpers exist
- max floor count is queryable from code
- debug draw remains editor-only

### Wave 5

Status:

- deck level and floor region bake exist
- support depth is still limited

Why it is not complete:

- `BakeDeckLevels(...)`, `BakeFloorRegions(...)`, and `ValidateFloorRegionWalkability(...)` exist;
- the current implementation is generic and does not explicitly prove support for `Side Corridor` and `Mezzanine` behaviors beyond enum presence;
- verification was not rerun in this pass.

Files to edit:

- `Source/Sub3DCore/Public/Types/Sub3DFloorTypes.h`
- `Source/Sub3DBake/Public/Bake/SubmarineFloorBakeService.h`
- `Source/Sub3DBake/Private/Bake/SubmarineFloorBakeService.cpp`
- `Source/Sub3DTests/Private/Automation/FloorBakeTests.cpp`

Strict agent pass:

1. explicitly define how each `Floor Region` kind affects walkability;
2. keep the current bay-driven constraints;
3. add or refine tests for side corridor and mezzanine;
4. do not introduce partitions or flood logic here.

Your manual pass:

1. compile
2. run floor automation
3. inspect one asset with at least two `Floor Region` kinds

Exit gate:

- explicit handling exists for the intended region kinds
- walkability validation remains deterministic

### Wave 6

Status:

- main data path exists
- navigation validation is still smoke-level only

Why it is not complete:

- `BakeOpenings(...)`, `BakeConnectors(...)`, and `BakeClosures(...)` exist;
- `ValidateTraversalConstraints(...)` is missing;
- `SubmarineNavigationTestService` exposes only `RunPIENavigationSmoke(...)`;
- no `SpawnTestPawn(...)` or `RunTraversalChecks(...)` exists.

Files to edit:

- `Source/Sub3DCore/Public/Types/Sub3DOpeningTypes.h`
- `Source/Sub3DCore/Public/Types/Sub3DConnectorTypes.h`
- `Source/Sub3DCore/Public/Types/Sub3DClosureTypes.h`
- `Source/Sub3DBake/Public/Bake/SubmarineOpeningBakeService.h`
- `Source/Sub3DBake/Private/Bake/SubmarineOpeningBakeService.cpp`
- `Source/Sub3DEditor/Public/Tools/SubmarineNavigationTestService.h`
- `Source/Sub3DEditor/Private/Tools/SubmarineNavigationTestService.cpp`
- add Wave 6 automation if needed

Strict agent pass:

1. add traversal constraint validation on top of the existing bake path;
2. keep `Opening`, `Connector`, and `Closure` strictly separate;
3. extend the navigation test service without broadening into gameplay code;
4. close with a focused editor-side smoke test.

Your manual pass:

1. compile
2. trigger the navigation test from the toolkit
3. inspect logs for traversal-specific checks, not only PIE launch

Exit gate:

- placement validation exists
- traversal validation exists
- editor test service does more than opening PIE

### Wave 7

Status:

- partitions are present
- the public contract is reduced compared to the reference

Why it is not complete:

- `BakePartitions(...)` exists as a collapsed helper;
- `BakePressureBulkheads(...)` is missing;
- `BakeInternalWalls(...)` is missing;
- `ApplyPartitionOpenings(...)` is missing;
- the current implementation is acceptable as scaffolding, but not as a closed reference match.

Files to edit:

- `Source/Sub3DCore/Public/Types/Sub3DPartitionTypes.h`
- `Source/Sub3DBake/Public/Bake/SubmarinePartitionBakeService.h`
- `Source/Sub3DBake/Private/Bake/SubmarinePartitionBakeService.cpp`
- add Wave 7 automation

Strict agent pass:

1. split the public API to match the reference;
2. keep internal shared helpers if needed;
3. keep partition ownership local to this service;
4. add minimal automation for bulkhead and wall cases.

Your manual pass:

1. compile
2. bake one asset with both `Pressure Bulkhead` and `Internal Wall`
3. inspect logs and compiled counts

Exit gate:

- public API matches the reference intent
- partition opening application is explicit

### Wave 8

Status:

- a usable flood graph path exists
- the reference split is not closed

Why it is not complete:

- `BuildFloodGraph(...)` exists;
- `BuildDerivedFloodVolumes(...)` is missing as a separate public API;
- `ComputePropagationPaths(...)` is missing;
- no dedicated flood debug overlay exists in editor.

Files to edit:

- `Source/Sub3DCore/Public/Types/Sub3DFloodTypes.h`
- `Source/Sub3DBake/Public/Graph/SubmarineFloodGraphBuilder.h`
- `Source/Sub3DBake/Private/Graph/SubmarineFloodGraphBuilder.cpp`
- add editor overlay files
- add Wave 8 automation

Strict agent pass:

1. split derived volume build from graph build;
2. add propagation-path computation without pressure logic;
3. expose a clear debug visualization path;
4. keep all logic baked and deterministic.

Your manual pass:

1. compile
2. run Wave 8 tests
3. inspect derived volumes and edges visually in editor

Exit gate:

- derived volumes are explicitly built
- propagation paths are explicitly computed
- debug overlay exists

### Wave 9

Status:

- runtime baked-only direction is present
- final runtime actor contract is not closed

Why it is not complete:

- `ASubmarineRuntimeActor` loads a runtime asset and initializes components;
- it does not expose the reference methods `LoadCompiledAsset(...)`, `BuildRenderComponents(...)`, `BuildCollisionComponents(...)`, `InitializeRuntimeSystems(...)`;
- actual render component assembly is not implemented;
- collision assembly is not implemented as a separate stage.

Files to edit:

- `Source/Sub3DRuntime/Public/Actors/SubmarineRuntimeActor.h`
- `Source/Sub3DRuntime/Private/Actors/SubmarineRuntimeActor.cpp`
- `Source/Sub3DRuntime/Public/Components/SubmarineHullComponent.h`
- `Source/Sub3DRuntime/Private/Components/SubmarineHullComponent.cpp`
- runtime render/collision component files as required
- add runtime automation or smoke fixtures

Strict agent pass:

1. keep runtime baked-only;
2. split the runtime initialization stages explicitly;
3. ensure runtime never reads the authoring asset;
4. add smoke verification for actor spawn and init.

Your manual pass:

1. compile
2. spawn `ASubmarineRuntimeActor` with a compiled runtime asset
3. verify collision and render components are built from compiled data only

Exit gate:

- runtime actor stage methods exist and work
- no authoring dependency remains

### Wave 10

Status:

- toolkit is usable
- shell is not yet the complete reference shell

Why it is not complete:

- main toolkit exists and tabs are registered;
- there is no `BindCommands(...)` layer;
- there is no explicit `SetCurrentPhase(...)` state controller;
- per-phase tab classes are not implemented;
- the current layout is a single toolkit file with inline tab content;
- there is no dedicated concept outliner.

Files to edit:

- `Source/Sub3DEditor/Public/Slate/SubmarineEditorToolkit.h`
- `Source/Sub3DEditor/Private/Slate/SubmarineEditorToolkit.cpp`
- add `Source/Sub3DEditor/Public/Slate/Tabs/*`
- add `Source/Sub3DEditor/Private/Slate/Tabs/*`
- add outliner/phase-state support files if needed

Strict agent pass:

1. split the toolkit into stable tab widgets;
2. add command binding and phase switching explicitly;
3. keep the current bake and validate actions wired;
4. do not regress the current menu-open behavior.

Your manual pass:

1. compile
2. open the toolkit from the main menu
3. verify phase switching and tool actions from the final shell

Exit gate:

- shell matches the reference structure
- actions remain wired
- editor flow is not dependent on transient fallback logic except where explicitly intended

### Wave 11

Status:

- product data types exist
- contextual editing flow is not implemented

Why it is not complete:

- `SubmarineCatalogEntry` exists;
- `OwnedSubmarineState` exists;
- permission data exists;
- lobby preview, shipyard permissions, station permissions, and mission restrictions are not connected into a working flow;
- current state is data scaffolding, not product integration.

Files to edit:

- `Source/Sub3DCore/Public/Types/Sub3DProductTypes.h`
- `Source/Sub3DRuntime/Public/Data/SubmarineCatalogEntry.h`
- `Source/Sub3DRuntime/Public/Data/OwnedSubmarineState.h`
- editor/runtime integration files that consume these assets

Strict agent pass:

1. keep product and runtime separate;
2. define who reads the catalog entry;
3. define who reads the owned state;
4. wire editing permissions by context without touching hull generation.

Your manual pass:

1. compile
2. create sample catalog and owned-state assets
3. verify context-based permission checks in editor or runtime entry flow

Exit gate:

- catalog, owned state, and runtime asset have clear roles
- editing permissions differ by context

## What Is Code Work And What Is Editor Work?

This is not only editor work.

Code work still remains in:

- Wave 1
- Wave 3
- Wave 4
- Wave 5
- Wave 6
- Wave 7
- Wave 8
- Wave 9
- Wave 10
- Wave 11

Editor and manual verification work remains in:

- Wave 0 re-smoke only if touched again
- Wave 3
- Wave 4
- Wave 6
- Wave 8
- Wave 9
- Wave 10
- Wave 11

Verification-only closure is enough mainly for:

- Wave 2, if the existing API shape is accepted as final

## Recommended Next Move

The best strict path is:

1. close `Wave 1`
2. close `Wave 2`
3. close `Wave 3`
4. close `Wave 4`
5. close `Wave 5`
6. close `Wave 6`
7. close `Wave 7`
8. close `Wave 8`
9. close `Wave 9`
10. close `Wave 10`
11. close `Wave 11`
12. run the full global validation pass

If speed is more important than perfect sequential purity, the pragmatic grouped order is:

1. `Wave 1 -> Wave 5` contract alignment
2. `Wave 6 -> Wave 8` gameplay-space closure
3. `Wave 9 -> Wave 11` runtime/editor/product closure
4. full validation

That is the reason the earlier shorthand mentioned `Wave 6 -> Wave 11`.
It was a priority block, not a claim that earlier waves were fully closed.
