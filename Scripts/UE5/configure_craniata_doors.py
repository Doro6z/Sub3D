"""
Sub3D - Configure DoorId / CompartmentA / CompartmentB on every door + hatch
Child Actor Component placed inside BP_Submarine_Craniata.
============================================================================

What this script does
---------------------
The handmade Craniata Blueprint has door + hatch ChildActorComponents pre-placed
in the Simple Construction Script (SCS). Each one has its own `ChildActorTemplate`
- the per-component override of the spawned BP_SubDoor / BP_SubHatch defaults that
the runtime instance receives. That template is the place where DoorId,
CompartmentA, CompartmentB must be authored so the flood graph + interaction
system can bind each door instance to the correct edge in the connection graph
defined by `DA_SubDef_Craniata`.

Algorithm:
  1. Walk the BP's SCS and pick out every UChildActorComponent whose
     ChildActorClass derives from ASubDoorActor (covers BP_SubDoor,
     BP_SubDoor_Exterior, BP_SubHatch).
  2. Map each component name to a ConnectionId from CONNECTION_TABLE using
     EXPLICIT_OVERRIDES first, then fuzzy matching by stripped tail.
  3. Hard-abort if any component is unmapped or if two components claim the
     same ConnectionId. Print a verbose diagnostic so the dict can be updated.
  4. With DRY_RUN=True (default) only report. With DRY_RUN=False write
     DoorId / CompartmentA / CompartmentB to each ChildActorTemplate, compile
     the BP and save it.

Idempotent. Re-running with DRY_RUN=False after adding a new door requires only
a new entry in EXPLICIT_OVERRIDES (or a name that the fuzzy matcher resolves).

Run from the editor:
  Tools -> Execute Python Script -> point at this file.

If the BP_Submarine_Craniata editor tab is OPEN, close it first - python
modifications applied to a loaded-in-editor Blueprint can fail silently.

Run headless (preferred when editor is closed):
    "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
        "C:/Dev/Sub3D/Sub3D.uproject" ^
        -ExecutePythonScript="C:/Dev/Sub3D/Scripts/UE5/configure_craniata_doors.py" ^
        -stdout -FullStdOutLogOutput -unattended -nop4 -NullRHI
"""

import json
import os
import re

import unreal


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

CRANIATA_BP_PATH = "/Game/Sub3D/FirstPlayableRun/BP_Submarine_Craniata"

# Master switch. Default True so the first run produces a mapping report
# without writing. Flip to False once mapping is confirmed.
DRY_RUN = True

# Mapping report dump (always written, even in DRY_RUN, for inspection).
REPORT_JSON = "C:/Dev/Sub3D/reports/2026-05-09_craniata_door_mapping.json"

# Authoritative connection table - kept in sync with DA_SubDef_Craniata.
# Tuple = (ConnectionId, CompartmentA, CompartmentB, expected child class hint).
# Empty CompartmentB ("") = ExteriorHatch / drains-to-ocean.
CONNECTION_TABLE = [
    ("N_main_Fwd",                         "C_main_Bow",         "C_main_Fwd",       "BP_SubDoor"),
    ("N_main_Control",                     "C_main_Fwd",         "C_main_Aft",       "BP_SubDoor"),
    ("N_lower_BallastFwd",                 "C_lower_BallastFwd", "C_lower_Hub",      "BP_SubDoor"),
    ("N_lower_BallastAft",                 "C_lower_Hub",        "C_lower_BallastAft","BP_SubDoor"),
    ("N_upper_UpperAirlock_Inner",         "C_upper_UpperFwd",   "C_upper_Airlock",  "BP_SubDoor"),
    ("N_upper_UpperAirlock_Exit",          "C_upper_Airlock",    "C_upper_UpperAft", "BP_SubDoor"),
    ("N_upper_UpperAirlock_Exit__to_EXT",  "C_upper_UpperAft",   "",                 "BP_SubDoor_Exterior"),
    ("N_vert_LowerAccess",                 "C_main_Fwd",         "C_lower_Hub",      "BP_SubHatch"),
    ("N_vert_UpperAccess",                 "C_upper_UpperFwd",   "C_main_Fwd",       "BP_SubHatch"),
]

# Explicit overrides: cleaned component name -> ConnectionId.
# Cleaned name = component name with "_GEN_VARIABLE" suffix stripped, case
# preserved. Populate this from the DRY_RUN report when the fuzzy matcher
# cannot resolve a component (e.g. when the user used a legacy compartment
# name like "Armory" that no longer matches the connection ID directly).
EXPLICIT_OVERRIDES = {
    # Example (uncomment + adjust after first DRY_RUN report):
    # "BP_SubDoor_UpperD_Armory": "N_upper_UpperAirlock_Exit",
}

# Components matching any of these regex patterns are silently skipped (do not
# count toward unmapped component conflicts). Useful for child actor components
# that intentionally are NOT door connections (e.g. station spawn points).
SKIP_PATTERNS = (
    # Add patterns here if some ChildActorComponents are not doors/hatches.
)


# ---------------------------------------------------------------------------
# Logging helpers
# ---------------------------------------------------------------------------

def log(msg):    unreal.log(f"[ConfigureDoors] {msg}")
def warn(msg):   unreal.log_warning(f"[ConfigureDoors] {msg}")
def err(msg):    unreal.log_error(f"[ConfigureDoors] {msg}")


# ---------------------------------------------------------------------------
# Subobject helpers
# ---------------------------------------------------------------------------

def _get_subobject_data_subsystem():
    try:
        return unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    except Exception:
        getter = getattr(unreal.SubobjectDataSubsystem, "get", None)
        if callable(getter):
            return getter()
        raise


def _strip_gen_variable(name: str) -> str:
    return re.sub(r"_GEN_VARIABLE$", "", name)


def _is_door_actor_class(cls) -> bool:
    """True if `cls` derives from ASubDoorActor (covers SubDoor + SubHatch + exterior)."""
    if cls is None:
        return False
    door_base = unreal.SubDoorActor.static_class()
    try:
        return cls.is_child_of(door_base)
    except Exception:
        # Fallback: walk parents manually.
        try:
            walker = cls
            while walker is not None:
                if walker == door_base:
                    return True
                walker = walker.get_super_class()
        except Exception:
            return False
    return False


# ---------------------------------------------------------------------------
# Mapping
# ---------------------------------------------------------------------------

def _normalize_for_match(token: str) -> str:
    """Lowercase + strip non-alphanumerics for fuzzy comparison."""
    return re.sub(r"[^a-z0-9]", "", token.lower())


def _map_component_to_connection(clean_name: str) -> str:
    """
    Resolve a cleaned component name (no _GEN_VARIABLE) to a ConnectionId.

    Strategy:
      1. EXPLICIT_OVERRIDES wins.
      2. Strip leading "BP_SubDoor_" / "BP_SubHatch_" / "BP_SubDoor_Exterior_".
         Replace deck-letter shortcuts: MainD -> main, LowerD -> lower, UpperD -> upper.
         Compare normalized result against the normalized tail of every ConnectionId
         (ConnectionId minus leading "N_").
      3. Empty string if no match (caller will surface as a conflict).
    """
    # 1) explicit override
    if clean_name in EXPLICIT_OVERRIDES:
        return EXPLICIT_OVERRIDES[clean_name]

    # 2) prefix strip
    work = clean_name
    for prefix in ("BP_SubDoor_Exterior_", "BP_SubHatch_", "BP_SubDoor_"):
        if work.startswith(prefix):
            work = work[len(prefix):]
            break

    # deck-letter shortcuts (case-sensitive on the shortcut, value stays as-is)
    work = work.replace("MainD_", "main_")
    work = work.replace("LowerD_", "lower_")
    work = work.replace("UpperD_", "upper_")

    target = _normalize_for_match(work)
    if not target:
        return ""

    matches = []
    for connection_id, _ca, _cb, _cls in CONNECTION_TABLE:
        tail = connection_id[2:] if connection_id.startswith("N_") else connection_id
        if _normalize_for_match(tail) == target:
            matches.append(connection_id)

    if len(matches) == 1:
        return matches[0]

    # 3) no clean unique match
    return ""


def _connection_row(connection_id: str):
    for row in CONNECTION_TABLE:
        if row[0] == connection_id:
            return row
    return None


def _is_skipped(clean_name: str) -> bool:
    for pat in SKIP_PATTERNS:
        if re.search(pat, clean_name):
            return True
    return False


# ---------------------------------------------------------------------------
# Property writes on ChildActorTemplate
# ---------------------------------------------------------------------------

def _apply_to_template(child_actor_component, door_id: str, comp_a: str, comp_b: str):
    """
    Set DoorId/CompartmentA/CompartmentB on the per-component ChildActorTemplate
    (the persistent, BP-default editable instance, NOT the runtime spawn).

    Returns the (DoorId, CompartmentA, CompartmentB) tuple actually read back.
    """
    template = child_actor_component.get_editor_property("child_actor_template")
    if template is None:
        raise RuntimeError("child_actor_template is None - has the BP been compiled at least once?")

    template.set_editor_property("door_id", unreal.Name(door_id))
    template.set_editor_property("compartment_a", unreal.Name(comp_a))
    if comp_b:
        template.set_editor_property("compartment_b", unreal.Name(comp_b))
    else:
        # Empty FName for the exterior door: write NAME_None.
        template.set_editor_property("compartment_b", unreal.Name("None"))

    actual_id = template.get_editor_property("door_id")
    actual_a  = template.get_editor_property("compartment_a")
    actual_b  = template.get_editor_property("compartment_b")
    return (str(actual_id), str(actual_a), str(actual_b))


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    eal = unreal.EditorAssetLibrary
    if not eal.does_asset_exist(CRANIATA_BP_PATH):
        err(f"Blueprint not found: {CRANIATA_BP_PATH}")
        return 1

    blueprint = eal.load_asset(CRANIATA_BP_PATH)
    log(f"loaded {CRANIATA_BP_PATH}")
    log(f"DRY_RUN = {DRY_RUN}")

    sds = _get_subobject_data_subsystem()
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = sds.k2_gather_subobject_data_for_blueprint(blueprint)
    if not handles:
        err("no subobject handles for BP")
        return 1

    # Collect all UChildActorComponent whose child class derives from ASubDoorActor.
    candidates = []
    for h in handles:
        data = sds.k2_find_subobject_data_from_handle(h)
        if data is None:
            continue
        obj = lib.get_object(data)
        if obj is None:
            continue
        if not isinstance(obj, unreal.ChildActorComponent):
            continue
        try:
            child_class = obj.get_editor_property("child_actor_class")
        except Exception:
            child_class = None
        if not _is_door_actor_class(child_class):
            continue
        candidates.append(obj)

    log(f"found {len(candidates)} door/hatch ChildActorComponents on BP")

    # Resolve mapping for every candidate, surface conflicts before any writes.
    mapping = []  # list of (component, clean_name, connection_id, child_class_path)
    used_connections = {}  # connection_id -> clean_name (for duplicate detection)
    unmapped = []
    skipped = []

    for comp in candidates:
        raw_name = comp.get_name()
        clean_name = _strip_gen_variable(raw_name)
        if _is_skipped(clean_name):
            skipped.append(clean_name)
            continue

        cls = comp.get_editor_property("child_actor_class")
        try:
            cls_path = cls.get_path_name() if cls is not None else ""
        except Exception:
            cls_path = str(cls)

        connection_id = _map_component_to_connection(clean_name)
        if not connection_id:
            unmapped.append((clean_name, cls_path))
            continue

        if connection_id in used_connections:
            err(
                f"duplicate connection: {connection_id} claimed by both "
                f"{used_connections[connection_id]!r} and {clean_name!r}"
            )
            unmapped.append((clean_name, cls_path))
            continue

        used_connections[connection_id] = clean_name
        mapping.append((comp, clean_name, connection_id, cls_path))

    # Build the report (written regardless of DRY_RUN).
    report_rows = []
    for comp, clean_name, connection_id, cls_path in mapping:
        row = _connection_row(connection_id)
        report_rows.append({
            "component_name": clean_name,
            "child_actor_class": cls_path,
            "connection_id": connection_id,
            "compartment_a": row[1],
            "compartment_b": row[2],
            "starts_closed": True,
        })

    used_ids = set(used_connections.keys())
    missing_connections = [r[0] for r in CONNECTION_TABLE if r[0] not in used_ids]

    payload = {
        "blueprint_path": CRANIATA_BP_PATH,
        "dry_run": DRY_RUN,
        "candidate_count": len(candidates),
        "mapped_count": len(mapping),
        "skipped": skipped,
        "unmapped": [{"component_name": n, "child_actor_class": c} for n, c in unmapped],
        "missing_connections": missing_connections,
        "mapping": report_rows,
    }
    os.makedirs(os.path.dirname(REPORT_JSON), exist_ok=True)
    with open(REPORT_JSON, "w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2)
    log(f"report written: {REPORT_JSON}")

    log("=" * 72)
    log("PROPOSED MAPPING")
    log("=" * 72)
    for row in report_rows:
        b = row["compartment_b"] if row["compartment_b"] else "None"
        log(
            f"  {row['component_name']:50s} -> {row['connection_id']:38s} "
            f"A={row['compartment_a']}  B={b}"
        )
    if skipped:
        log("-" * 72)
        log(f"skipped ({len(skipped)}):")
        for s in skipped:
            log(f"  {s}")

    # ---- Conflict surface --------------------------------------------------
    failed = False

    if unmapped:
        err("-" * 72)
        err(f"UNMAPPED ({len(unmapped)}) - cannot proceed:")
        for name, cls_path in unmapped:
            err(f"  {name}   (class: {cls_path})")
        err("Resolve by adding the component to EXPLICIT_OVERRIDES at the top of this script.")
        failed = True

    if missing_connections:
        # Missing = a connection in the DA has no placed component yet.
        # This is a WARNING (not a fatal) - we can still configure what we have.
        warn("-" * 72)
        warn(f"missing connections (no component placed for these {len(missing_connections)} connections):")
        for cid in missing_connections:
            warn(f"  {cid}")

    if failed:
        err("-" * 72)
        err("ABORTED before writing - fix conflicts and re-run.")
        return 1

    if DRY_RUN:
        log("-" * 72)
        log("DRY_RUN=True - no properties written. Set DRY_RUN=False at the top to apply.")
        return 0

    # ---- Apply -------------------------------------------------------------
    log("-" * 72)
    log(f"APPLYING to {len(mapping)} components ...")
    written = 0
    for comp, clean_name, connection_id, _cls_path in mapping:
        row = _connection_row(connection_id)
        try:
            actual = _apply_to_template(comp, row[0], row[1], row[2])
        except Exception as e:
            err(f"  failed on {clean_name}: {e}")
            continue
        log(
            f"  {clean_name:50s}  DoorId={actual[0]:38s} "
            f"A={actual[1]}  B={actual[2]}"
        )
        written += 1

    log("-" * 72)
    log("compiling BP ...")
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    log("saving BP ...")
    eal.save_loaded_asset(blueprint)

    log("=" * 72)
    log(f"DONE. components configured: {written}/{len(mapping)}")
    log(f"BP saved: {CRANIATA_BP_PATH}")
    log("=" * 72)
    return 0


main()
