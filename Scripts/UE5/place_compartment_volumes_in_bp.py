"""
Sub3D — Place UCompartmentVolumeComponent instances in BP_Submarine_Craniata
============================================================================

Idempotent script that pre-places one UCompartmentVolumeComponent per compartment
defined in the DA, with transform + extent derived from HydroBoundsMin/Max.

Workflow:
  1. Run import_sub_definition.py first — refreshes DA values from JSON (with
     correct hull-profile-derived bounds).
  2. Run THIS script — creates missing volumes in the BP, configures them.
  3. Manually adjust each volume in the BP editor (translate gizmo, scale gizmo)
     until it tightly encloses its compartment with door/deck cutoffs.
  4. Compile + Save BP.

Behaviour (idempotent):
  - For each DA compartment:
    * If a UCompartmentVolumeComponent with matching CompartmentId already exists
      → SKIP (preserve user adjustments).
    * Else → CREATE + configure with default bounds from DA.
  - Set FORCE_RECREATE = True at the top of this script to overwrite existing
    volumes (destroys manual adjustments — use carefully).

Run from the editor: Tools > Execute Python Script.
"""

import unreal


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

CRANIATA_BP_PATH = "/Game/Sub3D/FirstPlayableRun/BP_Submarine_Craniata"
DEF_PATH = "/Game/Sub3D/FirstPlayableRun/DA_SubDef_Craniata"

# If True, delete existing UCompartmentVolumeComponents on the BP before re-spawning
# from the DA. Destroys any manual adjustments. Default False (idempotent).
FORCE_RECREATE = False

# Component name prefix. Final name = COMPONENT_NAME_PREFIX + CompartmentId.
COMPONENT_NAME_PREFIX = "CV_"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def log(msg: str) -> None:
    unreal.log(f"[PlaceVolumes] {msg}")


def warn(msg: str) -> None:
    unreal.log_warning(f"[PlaceVolumes] {msg}")


def err(msg: str) -> None:
    unreal.log_error(f"[PlaceVolumes] {msg}")


def _get_subobject_data_subsystem():
    try:
        return unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    except Exception:
        getter = getattr(unreal.SubobjectDataSubsystem, "get", None)
        if callable(getter):
            return getter()
        raise


def _get_root_handle(sds, blueprint):
    """The actor root SceneComponent handle (anchor for child component additions)."""
    handles = sds.k2_gather_subobject_data_for_blueprint(blueprint)
    if not handles:
        raise RuntimeError("k2_gather_subobject_data_for_blueprint returned no handles")
    lib = unreal.SubobjectDataBlueprintFunctionLibrary

    # Prefer the handle marked as root.
    for handle in handles:
        data = sds.k2_find_subobject_data_from_handle(handle)
        if data is None:
            continue
        if lib.is_root_component(data):
            return handle

    # Fallback to first handle (BP self).
    return handles[0]


def _gather_existing_volumes(sds, blueprint):
    """Return {compartment_id (FName as str): (handle, component_obj)} for existing
    UCompartmentVolumeComponents on the BP."""
    handles = sds.k2_gather_subobject_data_for_blueprint(blueprint)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    found = {}
    for handle in handles:
        data = sds.k2_find_subobject_data_from_handle(handle)
        if data is None:
            continue
        obj = lib.get_object(data)
        if obj is None:
            continue
        if isinstance(obj, unreal.CompartmentVolumeComponent):
            cid = obj.get_editor_property("compartment_id")
            cid_str = str(cid) if cid else ""
            if cid_str:
                found[cid_str] = (handle, obj)
    return found


def _delete_subobject(sds, handle):
    """Remove a subobject from the BP. Returns True on success."""
    try:
        sds.delete_subobject(handle=handle, context_obj=None)
        return True
    except Exception:
        try:
            return bool(sds.delete_subobject(handle))
        except Exception as exc:
            err(f"delete_subobject failed: {exc}")
            return False


def _add_volume_component(sds, blueprint, root_handle, compartment_id: str, center, half_extent):
    """Add a UCompartmentVolumeComponent under the root. Configure CompartmentId,
    RelativeLocation, and BoxExtent. Returns (handle, component) or (None, None)."""
    params = unreal.AddNewSubobjectParams()
    params.parent_handle = root_handle
    params.new_class = unreal.CompartmentVolumeComponent
    params.blueprint_context = blueprint

    new_handle, fail_reason = sds.add_new_subobject(params)
    fail_str = str(fail_reason) if fail_reason else ""
    if fail_str:
        err(f"add_new_subobject failed for {compartment_id}: {fail_str}")
        return None, None

    desired_name = f"{COMPONENT_NAME_PREFIX}{compartment_id}"
    try:
        sds.rename_subobject(handle=new_handle, new_name=unreal.Text(desired_name))
    except Exception as exc:
        warn(f"rename_subobject failed for {compartment_id}: {exc}")

    data_obj = sds.k2_find_subobject_data_from_handle(new_handle)
    if data_obj is None:
        err(f"k2_find_subobject_data_from_handle returned None for {compartment_id}")
        return None, None

    comp_obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data_obj)
    comp = comp_obj if isinstance(comp_obj, unreal.CompartmentVolumeComponent) else None
    if comp is None:
        err(f"could not cast to CompartmentVolumeComponent for {compartment_id}: got {type(comp_obj).__name__}")
        return None, None

    # Configure properties.
    comp.set_editor_property("compartment_id", unreal.Name(compartment_id))
    # Box extent (half-extents in cm).
    try:
        comp.set_box_extent(half_extent, update_overlaps=False)
    except Exception:
        # Older API: direct property setter. SetBoxExtent triggers UBodySetup rebuild
        # which may be problematic in some contexts; fall back to direct field set.
        comp.set_editor_property("box_extent", half_extent)
    # Relative location (compartment center in BP-local space).
    try:
        comp.set_relative_location(center, sweep=False, teleport=True)
    except Exception:
        comp.set_editor_property("relative_location", center)

    return new_handle, comp


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    eal = unreal.EditorAssetLibrary

    if not eal.does_asset_exist(CRANIATA_BP_PATH):
        err(f"BP not found: {CRANIATA_BP_PATH}")
        return
    if not eal.does_asset_exist(DEF_PATH):
        err(f"Definition not found: {DEF_PATH}")
        return

    blueprint = eal.load_asset(CRANIATA_BP_PATH)
    if blueprint is None:
        err(f"Failed to load BP: {CRANIATA_BP_PATH}")
        return
    definition = eal.load_asset(DEF_PATH)
    if definition is None:
        err(f"Failed to load DA: {DEF_PATH}")
        return

    sds = _get_subobject_data_subsystem()
    if sds is None:
        err("SubobjectDataSubsystem unavailable")
        return

    root_handle = _get_root_handle(sds, blueprint)
    existing = _gather_existing_volumes(sds, blueprint)
    log(f"BP has {len(existing)} existing UCompartmentVolumeComponent(s) before run.")

    if FORCE_RECREATE and existing:
        log(f"FORCE_RECREATE=True — deleting {len(existing)} existing volumes")
        for cid, (handle, _comp) in list(existing.items()):
            if _delete_subobject(sds, handle):
                existing.pop(cid, None)

    compartments = definition.get_editor_property("compartments")
    if not compartments:
        err("Definition has no compartments — run import_sub_definition.py first.")
        return

    created = 0
    skipped_existing = 0
    failed = 0

    for comp_def in compartments:
        cid = str(comp_def.get_editor_property("compartment_id"))
        if not cid or cid == "None":
            warn(f"compartment with empty id — skipping")
            continue

        if cid in existing:
            log(f"  {cid}: already exists (preserved). Set FORCE_RECREATE=True to overwrite.")
            skipped_existing += 1
            continue

        bounds_min = comp_def.get_editor_property("hydro_bounds_min")
        bounds_max = comp_def.get_editor_property("hydro_bounds_max")
        if bounds_min is None or bounds_max is None:
            warn(f"  {cid}: missing HydroBounds — skipping")
            failed += 1
            continue

        # AABB center + half-extents in cm (BP-local space).
        center = unreal.Vector(
            (bounds_min.x + bounds_max.x) * 0.5,
            (bounds_min.y + bounds_max.y) * 0.5,
            (bounds_min.z + bounds_max.z) * 0.5,
        )
        half_extent = unreal.Vector(
            (bounds_max.x - bounds_min.x) * 0.5,
            (bounds_max.y - bounds_min.y) * 0.5,
            (bounds_max.z - bounds_min.z) * 0.5,
        )

        if half_extent.x <= 0.0 or half_extent.y <= 0.0 or half_extent.z <= 0.0:
            warn(f"  {cid}: degenerate bounds (half_extent={half_extent}) — skipping")
            failed += 1
            continue

        new_handle, comp = _add_volume_component(sds, blueprint, root_handle, cid, center, half_extent)
        if new_handle is None:
            failed += 1
            continue

        log(f"  {cid}: created (center={center}, half_extent={half_extent})")
        created += 1

    # Compile + save.
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    eal.save_loaded_asset(blueprint)

    log("=" * 60)
    log(f"BP saved: {CRANIATA_BP_PATH}")
    log(f"  created: {created}")
    log(f"  skipped (already existed): {skipped_existing}")
    log(f"  failed: {failed}")
    if skipped_existing > 0 and not FORCE_RECREATE:
        log("  (existing volumes were preserved — your manual adjustments are intact)")


if __name__ == "__main__":
    main()
