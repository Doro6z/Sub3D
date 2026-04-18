# ProcGen Legacy — DEPRECATED

These files are the original chunk-based procedural generation system (Proto 03).
Replaced by the field-based WorldGen pipeline (C1-C11) in `Source/Sub3D/WorldGen/`.

Kept for reference only. Not compiled, not used in any level or Blueprint.

## Files

| File | Original role |
|------|---------------|
| TraversalGenerator.h/.cpp | Orchestrator (4-pass pipeline) |
| TraversalGraphGenerator.h/.cpp | Graph node generation |
| ChunkAssembler.h/.cpp | Chunk assembly from templates |
| ProceduralChunkBuilder.h/.cpp | Mesh extrusion (oval tube, spheroid) |
| ChunkLibrary.h/.cpp | Chunk template library (data asset) |
| TraversalValidator.h/.cpp | Layout validation |
| TraversalTypes.h | Shared structs and enums |

## Removal

Safe to delete entirely. Zero external dependencies confirmed 2026-04-08.
