"""
Sub3D - Bulk set collision preset on Craniata per-mesh assets
=============================================================

Iterates the 141 SM_* assets under
/Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata_PerMesh/ and assigns a
collision profile + simple collision primitives based on asset name prefix.

Why simple primitives instead of UseComplexAsSimple:
The meshes were imported with collision=False, so no triangle-mesh physics data
was ever cooked. Setting CollisionTraceFlag=UseComplexAsSimple alone does not
regenerate that data, so the runtime had nothing to collide against. We now
generate real AggGeom primitives (Box / NDOP / Convex hulls) which always work
at runtime and are cheaper.

Custom collision profiles declared in Config/DefaultEngine.ini :
  - SubmarineHull         : exterior hull, QueryAndPhysics, blocks Pawn=Ignore
                            (crew inside doesn't collide with its own hull)
  - SubInteriorWalkable   : decks, stairs, bulkheads, cassette - blocks Pawn
  - SubInteriorVisual     : interior decor that still blocks crew
  - NoCollision           : visual-only, no physics

Mode selector:
  - MODE = "hull_only"  : apply only to SM_Hull (PIE test 1, minimal setup)
  - MODE = "all"        : apply to every asset matching a mapping rule
  - MODE = "dry_run"    : log what WOULD change without touching any asset

Run headless (editor closed):
    "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        "C:/Dev/Sub3D/Sub3D.uproject" ^
        -ExecutePythonScript="C:/Dev/Sub3D/Scripts/UE5/setup_craniata_collisions.py" ^
        -stdout -FullStdOutLogOutput -unattended -nop4

Run from the editor Python console:
    exec(open(r"C:/Dev/Sub3D/Scripts/UE5/setup_craniata_collisions.py").read())

Safe to re-run: existing simple collisions are removed before the new shape is
added, so the script is idempotent.
"""

import unreal


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SM_FOLDER = "/Game/Sub3D/FirstPlayableRun/Meshes/Blockout/Craniata_PerMesh"

# Choose one: "hull_only", "all", "dry_run"
MODE = "all"

# Shape kinds generated as simple collision (AggGeom):
#   "None"   - no simple collision (use with NoCollision profile)
#   "Box"    - AABB box. Fast. Perfect for flat decks, bulkheads, crates, valves.
#   "NDOP18" - 18-DOP envelope. Tighter than box. Good for cylindrical / tube shapes.
#   "NDOP26" - 26-DOP envelope. Tightest convex envelope while staying single-hull.
#   "Convex" - convex decomposition (multiple hulls). Use for stairs / complex shapes.
#
# Rules are scanned in declaration order; first match wins. Keep specific
# prefixes (e.g. SM_Hull_Weld_) before more generic ones (SM_Hull).
PREFIX_RULES = [
    # --- Exterior hull (the one that takes OnHullHit) ---------------------
    ("SM_Hull_Weld_",              ("NoCollision",         "None")),
    # Convex decomposition gives a tight multi-hull approximation of the
    # organic hull shape so OnHullHit impact points stay meaningful.
    ("SM_Hull",                    ("SubmarineHull",       "Convex")),

    # --- Walkable interior surfaces ---------------------------------------
    ("SM_Deck_",                   ("SubInteriorWalkable", "Box")),
    # Stairs need multi-hull so the character's step-up logic can climb
    # each step instead of hitting the single AABB envelope.
    ("SM_Stair_",                  ("SubInteriorWalkable", "Convex")),
    ("SM_StairRail_",              ("SubInteriorWalkable", "Box")),
    ("SM_StairRailPost_",          ("SubInteriorWalkable", "Box")),
    ("SM_StairSupport_",           ("SubInteriorWalkable", "Box")),
    ("SM_StairStringer_",          ("SubInteriorWalkable", "Box")),
    ("SM_StairHanger_",            ("SubInteriorWalkable", "Box")),
    ("SM_Ladder_",                 ("SubInteriorWalkable", "NDOP18")),
    ("SM_BallastPasserelle_",      ("SubInteriorWalkable", "Box")),
    ("SM_AftTopAccess_",           ("SubInteriorWalkable", "Convex")),

    # --- Bulkheads + cassette (block crew) --------------------------------
    ("SM_BH_",                     ("SubInteriorWalkable", "Box")),
    ("SM_Airlock_Cassette",        ("SubInteriorWalkable", "Box")),

    # --- Static interior props (block crew but not walked on) -------------
    ("SM_Pipe_",                   ("SubInteriorVisual",   "NDOP18")),
    ("SM_Valve",                   ("SubInteriorVisual",   "Box")),
    ("SM_Junction",                ("SubInteriorVisual",   "Box")),
    ("SM_Reactor",                 ("SubInteriorVisual",   "Box")),
    ("SM_Engine",                  ("SubInteriorVisual",   "Box")),

    # --- Ballast tanks (interior volumes) ---------------------------------
    ("SM_Ballast_Service",         ("SubInteriorVisual",   "Box")),
    ("SM_BallastTank_",            ("SubInteriorVisual",   "Box")),
    ("SM_BallastAccess_",          ("SubInteriorWalkable", "Box")),
    ("SM_Ballast",                 ("SubInteriorVisual",   "Box")),

    # --- Door frames (static, fixed parts of bulkheads) -------------------
    ("SM_Door_",                   ("NoCollision",         "None")),  # replaced by CAC
    ("SM_Airlock_Door_",           ("NoCollision",         "None")),  # replaced by CAC
    ("SM_HatchDoor_",              ("NoCollision",         "None")),  # replaced by CAC
    ("SM_Hatch_",                  ("SubInteriorWalkable", "Box")),

    # --- External control surfaces (visual only, rotated via input) -------
    ("SM_Hydro_Fairings",          ("NoCollision",         "None")),
    ("SM_Hydro_",                  ("NoCollision",         "None")),
    ("SM_FinAssembly",             ("NoCollision",         "None")),
    ("SM_Fin_",                    ("NoCollision",         "None")),
    ("SM_RudderAssembly",          ("NoCollision",         "None")),
    ("SM_Rudder",                  ("NoCollision",         "None")),
    ("SM_Skeg",                    ("NoCollision",         "None")),
    ("SM_Propeller",               ("NoCollision",         "None")),
    ("SM_Propulsor",               ("NoCollision",         "None")),
    ("SM_Duct",                    ("NoCollision",         "None")),

    # --- Superstructure / antennas / periscope ----------------------------
    ("SM_Periscope",               ("NoCollision",         "None")),
    ("SM_Antenna",                 ("NoCollision",         "None")),
    ("SM_Flag_",                   ("NoCollision",         "None")),

    # --- Turrets (replaced by CAC ChildActor, static mesh NoCollision) ---
    ("SM_TurretStation_",          ("NoCollision",         "None")),
    ("SM_TurretSocket_",           ("NoCollision",         "None")),
    ("SM_Hardpoint_",              ("NoCollision",         "None")),
    ("SM_Turret_",                 ("NoCollision",         "None")),

    # --- Mooring / misc hull exterior fixtures ----------------------------
    ("SM_MooringLug_",             ("SubmarineHull",       "Box")),

    # --- Fallback: NoCollision -------------------------------------------
    ("SM_",                        ("NoCollision",         "None")),
]

# Assets containing any of these substrings are skipped entirely (cutters,
# boolean artifacts that should have been cleaned up at export time).
SKIP_SUBSTRINGS = (
    "_Cutter_Manual",
    "_RectRecut",
    "_EnvCut",
    "_EnvelopeCut",
)

# Convex decomposition tuning (used by "Convex" shape).
#   HULL_COUNT       : number of convex pieces. Higher = tighter, more cost.
#   MAX_HULL_VERTS   : max verts per piece. Higher = more precise.
#   HULL_PRECISION   : voxel resolution. Higher = better approximation.
CONVEX_HULL_COUNT = 8
CONVEX_MAX_HULL_VERTS = 16
CONVEX_HULL_PRECISION = 100000


# ---------------------------------------------------------------------------
# Logging helpers
# ---------------------------------------------------------------------------

def log(msg):   unreal.log(f"[CraniataCollisions] {msg}")
def warn(msg):  unreal.log_warning(f"[CraniataCollisions] {msg}")
def err(msg):   unreal.log_error(f"[CraniataCollisions] {msg}")


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _resolve_rule(asset_name: str):
    """Return (profile_name, shape_kind) for this asset, or None if skipped."""
    for skip in SKIP_SUBSTRINGS:
        if skip in asset_name:
            return None
    for prefix, rule in PREFIX_RULES:
        if asset_name.startswith(prefix):
            return rule
    return None


def _shape_enum(value: str):
    """Map shape-kind string to unreal.ScriptCollisionShapeType."""
    # Prefer the new enum name; fall back to the deprecated one on older
    # engine versions that still ship with ScriptingCollisionShapeType.
    enum_cls = getattr(unreal, "ScriptCollisionShapeType",
                       getattr(unreal, "ScriptingCollisionShapeType", None))
    if enum_cls is None:
        raise RuntimeError("neither ScriptCollisionShapeType nor "
                           "ScriptingCollisionShapeType are available")
    m = {
        "Box":     enum_cls.BOX,
        "Sphere":  enum_cls.SPHERE,
        "Capsule": enum_cls.CAPSULE,
        "NDOP10X": enum_cls.NDOP10_X,
        "NDOP10Y": enum_cls.NDOP10_Y,
        "NDOP10Z": enum_cls.NDOP10_Z,
        "NDOP18":  enum_cls.NDOP18,
        "NDOP26":  enum_cls.NDOP26,
    }
    if value not in m:
        raise ValueError(f"unknown shape: {value}")
    return m[value]


def _list_sm_assets():
    eal = unreal.EditorAssetLibrary
    all_assets = eal.list_assets(SM_FOLDER, recursive=True, include_folder=False)
    out = []
    for path in all_assets:
        asset = eal.load_asset(path)
        if asset is None:
            continue
        if isinstance(asset, unreal.StaticMesh):
            out.append((path, asset))
    return out


def _apply_to_asset(asset, profile_name: str, shape_kind: str, dry_run: bool, mesh_editor):
    """Returns True if the asset was changed."""
    body_setup = asset.get_editor_property("body_setup")
    if body_setup is None:
        return False

    changed = False

    # --- 1) Simple collision primitives ------------------------------------
    # Always remove-then-add so the script is idempotent (re-runs replace
    # existing collisions with the rule's requested shape).
    if not dry_run:
        mesh_editor.remove_collisions(asset)

    if shape_kind == "None":
        # No simple collision - nothing to add
        pass
    elif shape_kind == "Convex":
        if not dry_run:
            mesh_editor.set_convex_decomposition_collisions(
                asset,
                CONVEX_HULL_COUNT,
                CONVEX_MAX_HULL_VERTS,
                CONVEX_HULL_PRECISION,
            )
    else:
        if not dry_run:
            mesh_editor.add_simple_collisions(asset, _shape_enum(shape_kind))

    changed = True

    # --- 2) Collision profile (on DefaultInstance) -------------------------
    default_inst = body_setup.get_editor_property("default_instance")
    current_profile = default_inst.get_editor_property("collision_profile_name")
    if str(current_profile) != profile_name:
        if not dry_run:
            default_inst.set_editor_property("collision_profile_name", profile_name)
        changed = True

    # --- 3) Collision trace flag -------------------------------------------
    # With real AggGeom primitives we always rely on simple collision for
    # queries. CTF_USE_DEFAULT resolves to "use simple" via project config.
    wanted_trace = unreal.CollisionTraceFlag.CTF_USE_DEFAULT
    current_trace = body_setup.get_editor_property("collision_trace_flag")
    if current_trace != wanted_trace:
        if not dry_run:
            body_setup.set_editor_property("collision_trace_flag", wanted_trace)
        changed = True

    # Note: add_simple_collisions / set_convex_decomposition_collisions both
    # accept apply_changes=True (default) which rebuilds the physics data.
    # save_asset() will then persist the cooked data to disk.

    return changed


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    mode = MODE.lower()
    if mode not in ("hull_only", "all", "dry_run"):
        err(f"invalid MODE: {MODE}. use hull_only / all / dry_run")
        return

    dry_run = (mode == "dry_run")
    log(f"mode: {mode}")
    log(f"folder: {SM_FOLDER}")

    mesh_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if mesh_editor is None:
        err("StaticMeshEditorSubsystem not available")
        return

    assets = _list_sm_assets()
    log(f"found {len(assets)} static mesh assets")

    touched = 0
    skipped_no_rule = 0
    skipped_filter = 0
    unchanged = 0

    saved_paths = []

    for path, asset in assets:
        name = asset.get_name()

        rule = _resolve_rule(name)
        if rule is None:
            skipped_filter += 1
            continue

        if mode == "hull_only" and name != "SM_Hull":
            skipped_filter += 1
            continue

        profile, shape_kind = rule
        try:
            changed = _apply_to_asset(asset, profile, shape_kind, dry_run, mesh_editor)
        except Exception as e:
            err(f"  [FAIL] {name}: {e}")
            continue

        if changed:
            touched += 1
            log(f"  [SET] {name:<40} -> profile={profile}, shape={shape_kind}")
            if not dry_run:
                saved_paths.append(path)
        else:
            unchanged += 1

    # Save all modified assets in one batch
    if saved_paths:
        eal = unreal.EditorAssetLibrary
        for path in saved_paths:
            eal.save_asset(path, only_if_is_dirty=True)
        log(f"saved {len(saved_paths)} asset(s) to disk")

    log("=" * 60)
    log(f"total scanned:        {len(assets)}")
    log(f"  touched / changed:  {touched}")
    log(f"  unchanged:          {unchanged}")
    log(f"  skipped (filter):   {skipped_filter}")
    log(f"  skipped (no rule):  {skipped_no_rule}")
    if dry_run:
        log("DRY RUN: nothing was written to disk")
    log("=" * 60)


main()
