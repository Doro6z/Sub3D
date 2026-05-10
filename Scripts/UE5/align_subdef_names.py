"""
Sub3D - Align SubDef compartment + connection names.

Problem:
    Manual edits, JSON imports, and partial renames have left DA_SubDef_Craniata with
    multiple naming conventions live at once across:
        - Compartments[].CompartmentId
        - Connections[].CompartmentA / CompartmentB / ConnectionId
        - FloodGraph.Volumes[].VolumeId
        - FloodGraph.Edges[].ClosureId / VolumeA / VolumeB
        - StationSlots[].CompartmentId
        - WaterBakes map keys
    Plus orphan references (e.g. N_Upper_Airlock_Inner pointing to non-existent
    C_Upper_Fwd) and a typo (C_main_Hun in WaterBakes).

Solution:
    1. Use Compartments[].CompartmentId as the **canonical** compartment name set.
    2. Use Connections[].ConnectionId as the **canonical** connection name set.
    3. For every other reference, try case-insensitive match against canonical:
         - exact match     → skip (already aligned)
         - case-insensitive match → rewrite to canonical case
         - no match        → log as ORPHAN, leave untouched (user fix needed)

Usage:
    1. DRY_RUN = True   → only analyse, prints what would change. Run first.
    2. DRY_RUN = False  → apply changes + save asset.

    Inside UE Editor Python console:
        exec(open(r"C:/Dev/Sub3D/Scripts/UE5/align_subdef_names.py").read())

    Close DA_SubDef_Craniata in any editor window before APPLY mode.
"""

import unreal


DA_PATH = "/Game/Sub3D/FirstPlayableRun/DA_SubDef_Craniata.DA_SubDef_Craniata"
DRY_RUN = False   # ← toggle to False to actually rewrite

# Explicit rename map: applied BEFORE the case-insensitive canonical resolver.
# Use this for renames the resolver can't catch (e.g. C_main_Fwd renamed to C_Main_Hub —
# different word, not just casing). Keys/values are FName-as-string.
# Pre-filled from the dry-run dump of 2026-05-10. Edit/extend if your DA differs.
OVERRIDE_MAP = {
    # Compartment renames (legacy → canonical)
    "C_main_Fwd":          "C_Main_Hub",
    "C_main_Hun":          "C_Main_Bow",        # typo recovery
    "C_Upper_Fwd":         "C_Upper_Hub",
    "C_upper_UpperFwd":    "C_Upper_Hub",
    "C_upper_UpperAft":    "C_Upper_Aft",
    # Connection ID renames
    "N_main_Fwd":          "N_Main_Hub",
    "N_main_Control":      "N_Main_Aft",
    "N_upper_UpperAirlock_Inner":         "N_Upper_Airlock_Inner",
    "N_upper_UpperAirlock_Exit":          "N_Upper_Airlock_Exit",
    "N_upper_UpperAirlock_Exit__to_EXT":  "N_Upper_Airlock_Exit_to_EXT",  # also fix double underscore
}


def log(msg):
    unreal.log("[align_subdef] " + msg)


def warn(msg):
    unreal.log_warning("[align_subdef] " + msg)


def err(msg):
    unreal.log_error("[align_subdef] " + msg)


def to_str(name):
    """unreal.Name -> python str."""
    return str(name) if name else ""


def name_eq_ci(a, b):
    """Case-insensitive name compare."""
    return to_str(a).lower() == to_str(b).lower()


def resolve_canonical(value, canonical_list, label):
    """
    Map `value` to the canonical entry in `canonical_list`.
    Resolution order:
      1. Empty / None       → ("", "empty")
      2. OVERRIDE_MAP hit   → (mapped, "rewrite")     ← explicit renames (different word)
      3. Exact match        → (s, "exact")
      4. Case-insensitive   → (canon, "rewrite")
      5. Else               → (s, "orphan")
    """
    s = to_str(value)
    if not s or s.lower() == "none":
        return ("", "empty")

    # 2. Explicit override (must come before canonical match — canonical_list may not contain s).
    #    Lookup is case-insensitive so a single map entry covers casing variants
    #    (e.g. C_upper_UpperFwd AND C_Upper_UpperFwd both → C_Upper_Hub).
    s_lower = s.lower()
    for legacy_key, canonical_value in OVERRIDE_MAP.items():
        if legacy_key.lower() == s_lower:
            return (canonical_value, "rewrite")

    # 3-4. Canonical resolution.
    for canon in canonical_list:
        canon_s = to_str(canon)
        if canon_s == s:
            return (canon_s, "exact")
        if canon_s.lower() == s.lower():
            return (canon_s, "rewrite")
    return (s, "orphan")


def main():
    log("─── Loading DA ───")
    da = unreal.EditorAssetLibrary.load_asset(DA_PATH)
    if da is None:
        err("Failed to load DA at " + DA_PATH)
        return

    log("Mode: " + ("DRY RUN" if DRY_RUN else "APPLY (will save)"))

    # ── Read canonical sets from the user-edited arrays ──
    compartments = da.get_editor_property("compartments")
    connections = da.get_editor_property("connections")

    canonical_comp_ids = []
    for c in compartments:
        cid = to_str(c.get_editor_property("compartment_id"))
        if cid:
            canonical_comp_ids.append(cid)

    canonical_conn_ids = []
    for c in connections:
        cid = to_str(c.get_editor_property("connection_id"))
        if cid:
            canonical_conn_ids.append(cid)

    log("Canonical Compartments ({0}):".format(len(canonical_comp_ids)))
    for n in canonical_comp_ids:
        log("  - " + n)
    log("Canonical Connections ({0}):".format(len(canonical_conn_ids)))
    for n in canonical_conn_ids:
        log("  - " + n)

    rewrites = 0   # successfully aligned
    orphans = 0    # references with no canonical match — user must fix

    def fix_ref(getter, setter, value, canonical_list, ctx):
        """Generic align for a name-typed property accessed via getter/setter."""
        nonlocal rewrites, orphans
        canonical, status = resolve_canonical(value, canonical_list, ctx)
        if status == "empty" or status == "exact":
            return
        if status == "rewrite":
            log("  REWRITE {0}: '{1}' → '{2}'".format(ctx, value, canonical))
            rewrites += 1
            if not DRY_RUN:
                setter(unreal.Name(canonical))
        elif status == "orphan":
            warn("  ORPHAN  {0}: '{1}' (no canonical match — fix manually)".format(ctx, value))
            orphans += 1

    # ── 1. Connections: align CompartmentA, CompartmentB ──
    log("─── Connections ───")
    for i, conn in enumerate(connections):
        cid = to_str(conn.get_editor_property("connection_id"))
        ctx_a = "Connection[{0}={1}].CompartmentA".format(i, cid)
        ctx_b = "Connection[{0}={1}].CompartmentB".format(i, cid)

        ca = conn.get_editor_property("compartment_a")
        fix_ref(
            lambda: ca,
            lambda v: conn.set_editor_property("compartment_a", v),
            ca, canonical_comp_ids, ctx_a)

        cb = conn.get_editor_property("compartment_b")
        # CompartmentB == None is valid (exterior edge), skip it.
        if to_str(cb) and to_str(cb).lower() != "none":
            fix_ref(
                lambda: cb,
                lambda v: conn.set_editor_property("compartment_b", v),
                cb, canonical_comp_ids, ctx_b)

    # ── 2. FloodGraph.Volumes: align VolumeId ──
    log("─── FloodGraph.Volumes ───")
    flood_graph = da.get_editor_property("flood_graph")
    fg_volumes = flood_graph.get_editor_property("volumes")
    for i, vol in enumerate(fg_volumes):
        vid = vol.get_editor_property("volume_id")
        ctx = "FloodGraph.Volumes[{0}].VolumeId".format(i)
        fix_ref(
            lambda: vid,
            lambda v: vol.set_editor_property("volume_id", v),
            vid, canonical_comp_ids, ctx)

    # ── 3. FloodGraph.Edges: align ClosureId, VolumeA, VolumeB ──
    log("─── FloodGraph.Edges ───")
    fg_edges = flood_graph.get_editor_property("edges")
    for i, edge in enumerate(fg_edges):
        cid = to_str(edge.get_editor_property("closure_id"))
        ctx_id = "FloodGraph.Edges[{0}].ClosureId".format(i)
        ctx_a = "FloodGraph.Edges[{0}={1}].VolumeA".format(i, cid)
        ctx_b = "FloodGraph.Edges[{0}={1}].VolumeB".format(i, cid)

        # ClosureId aligns to Connections.ConnectionId
        eid = edge.get_editor_property("closure_id")
        fix_ref(
            lambda: eid,
            lambda v: edge.set_editor_property("closure_id", v),
            eid, canonical_conn_ids, ctx_id)

        va = edge.get_editor_property("volume_a")
        fix_ref(
            lambda: va,
            lambda v: edge.set_editor_property("volume_a", v),
            va, canonical_comp_ids, ctx_a)

        vb = edge.get_editor_property("volume_b")
        # VolumeB == None for exterior edges — skip
        if to_str(vb) and to_str(vb).lower() != "none":
            fix_ref(
                lambda: vb,
                lambda v: edge.set_editor_property("volume_b", v),
                vb, canonical_comp_ids, ctx_b)

    # Re-write the modified flood_graph back if not dry run
    if not DRY_RUN:
        flood_graph.set_editor_property("volumes", fg_volumes)
        flood_graph.set_editor_property("edges", fg_edges)
        da.set_editor_property("flood_graph", flood_graph)

    # ── 4. StationSlots: align CompartmentId ──
    log("─── StationSlots ───")
    slots = da.get_editor_property("station_slots")
    for i, slot in enumerate(slots):
        sid = to_str(slot.get_editor_property("station_id"))
        ctx = "StationSlots[{0}={1}].CompartmentId".format(i, sid)
        scid = slot.get_editor_property("compartment_id")
        fix_ref(
            lambda: scid,
            lambda v: slot.set_editor_property("compartment_id", v),
            scid, canonical_comp_ids, ctx)

    # ── 5. WaterBakes map: keys must match canonical Compartments ──
    log("─── WaterBakes map ───")
    water_bakes = da.get_editor_property("water_bakes")
    # water_bakes is a TMap<FName, TObjectPtr<UCompartmentWaterBake>>; in Python it
    # round-trips as a dict-like proxy. Renaming a key requires del+set.
    if water_bakes is not None:
        # Build a list of (old_key, new_key, value) tuples first
        rebuild = []
        for k in list(water_bakes.keys()):
            v = water_bakes[k]
            canonical, status = resolve_canonical(k, canonical_comp_ids, "WaterBakes key")
            if status == "exact" or status == "empty":
                rebuild.append((k, k, v))
            elif status == "rewrite":
                log("  REWRITE WaterBakes key: '{0}' → '{1}'".format(k, canonical))
                rewrites += 1
                rebuild.append((k, unreal.Name(canonical), v))
            elif status == "orphan":
                warn("  ORPHAN  WaterBakes key: '{0}' (no canonical match — fix manually, asset may be misnamed bake or stale entry)".format(k))
                orphans += 1
                rebuild.append((k, k, v))

        if not DRY_RUN:
            # Clear and re-add. The TMap proxy in Python may not support direct reassignment;
            # safest is to clear the property and rebuild via set_editor_property.
            new_map = {}
            for (old_k, new_k, v) in rebuild:
                new_map[new_k] = v
            da.set_editor_property("water_bakes", new_map)

    log("─── Summary ───")
    log("  Rewrites planned/applied: {0}".format(rewrites))
    log("  Orphan references (need manual fix): {0}".format(orphans))

    if DRY_RUN:
        log("DRY RUN — no changes saved. Set DRY_RUN=False to apply.")
    else:
        log("Saving DA...")
        success = unreal.EditorAssetLibrary.save_asset(DA_PATH, only_if_is_dirty=False)
        if success:
            log("DA saved successfully.")
        else:
            err("Failed to save DA. Make sure it is not open in any editor window.")


main()
