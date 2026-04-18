"""
Sub3D - Export Craniata SM_* world transforms to JSON
=====================================================

Writes Content/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata_Transforms.json
listing every SM_* mesh object's world location/rotation/scale, as captured at
Blender export time. UE composition picks this up to position one
StaticMeshComponent per asset inside BP_Submarine_Craniata.

Why a JSON sidecar instead of FbxSceneImportFactory:
- The per-asset FBX import drops node transforms; the mesh data is imported in
  object-local space around the authored Blender pivot.
- FbxSceneImportFactory is brittle in headless commandlets, and this path only
  needs transforms, not actor hierarchy import.
- A JSON sidecar is diffable, deterministic, and lets UE rebuild the Blueprint
  composition without an interactive Editor import session.

Coordinate notes:
- Blender scene is set to centimeters by main.py (scale_length = 0.01).
- Per-mesh FBX export uses Blender's neutral settings:
    axis_forward='-Y', axis_up='Z', bake_space_transform=False
- UE then applies its standard Blender->UE basis convert on mesh vertices.
- This JSON intentionally stores raw Blender world transforms; the UE compose
  script mirrors the same basis convert on positions when placing components.
- Quaternions are dumped as [x, y, z, w] to match unreal.Quat() constructor.

Diagnostics:
- Non-unit scale and non-identity rotation are logged here before export so the
  neutral per-mesh pipeline can be validated from Blender without guessing.

Skipped objects: same set as export_fbx.py (cutters, rect-recut artifacts).

Run: Blender > Scripting > Open > Alt+P
"""

import bpy
import json
import os
from pathlib import Path


OUTPUT_PATH = Path(r"C:\Dev\Sub3D\Content\Sub3D\FirstPlayableRun\Meshes\Blockout\Craniata_Transforms.json")
PREFIX = "SM_"
SKIP_SUBSTRINGS = (
    "_Cutter_Manual",
    "_RectRecut",
    "_EnvCut",
    "_EnvelopeCut",
)
ANOMALY_TOLERANCE = 1e-4
MAX_LOGGED_ANOMALIES = 16


def _should_export(obj):
    if obj.type != "MESH":
        return False
    name = obj.name or ""
    if not name.startswith(PREFIX):
        return False
    for skip in SKIP_SUBSTRINGS:
        if skip in name:
            return False
    return True


def main():
    transforms = {}
    non_unit_scale = []
    non_identity_rotation = []

    for obj in bpy.data.objects:
        if not _should_export(obj):
            continue
        loc, quat, scale = obj.matrix_world.decompose()
        transforms[obj.name] = {
            "location_cm": [loc.x, loc.y, loc.z],
            "rotation_quat_xyzw": [quat.x, quat.y, quat.z, quat.w],
            "scale": [scale.x, scale.y, scale.z],
        }

        if (
            abs(scale.x - 1.0) > ANOMALY_TOLERANCE
            or abs(scale.y - 1.0) > ANOMALY_TOLERANCE
            or abs(scale.z - 1.0) > ANOMALY_TOLERANCE
        ):
            non_unit_scale.append((obj.name, scale.x, scale.y, scale.z))

        if (
            abs(quat.x) > ANOMALY_TOLERANCE
            or abs(quat.y) > ANOMALY_TOLERANCE
            or abs(quat.z) > ANOMALY_TOLERANCE
            or abs(quat.w - 1.0) > ANOMALY_TOLERANCE
        ):
            non_identity_rotation.append((obj.name, quat.x, quat.y, quat.z, quat.w))

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "schema_version": 1,
        "object_count": len(transforms),
        "objects": transforms,
    }
    OUTPUT_PATH.write_text(json.dumps(payload, indent=2))

    print("=" * 60)
    print(f"export_transforms: {len(transforms)} objects")
    print(f"  -> {OUTPUT_PATH}")
    print(f"  size: {os.path.getsize(OUTPUT_PATH) / 1024:.1f} KB")
    if non_unit_scale:
        print(f"  non-unit scales: {len(non_unit_scale)}")
        for row in non_unit_scale[:MAX_LOGGED_ANOMALIES]:
            print(f"    [SCALE] {row[0]} -> ({row[1]:.6f}, {row[2]:.6f}, {row[3]:.6f})")
        if len(non_unit_scale) > MAX_LOGGED_ANOMALIES:
            print(f"    ... and {len(non_unit_scale) - MAX_LOGGED_ANOMALIES} more")
    else:
        print("  non-unit scales: 0")

    if non_identity_rotation:
        print(f"  non-identity rotations: {len(non_identity_rotation)}")
        for row in non_identity_rotation[:MAX_LOGGED_ANOMALIES]:
            print(
                f"    [ROT] {row[0]} -> quat({row[1]:.6f}, {row[2]:.6f}, {row[3]:.6f}, {row[4]:.6f})"
            )
        if len(non_identity_rotation) > MAX_LOGGED_ANOMALIES:
            print(f"    ... and {len(non_identity_rotation) - MAX_LOGGED_ANOMALIES} more")
    else:
        print("  non-identity rotations: 0")
    print("=" * 60)


main()
