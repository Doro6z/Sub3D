"""
Sub3D - DA cleanup pass after Option B HydroBounds removal (2026-05-10).

What this does (DRY_RUN to preview, then APPLY):

  1. ZERO  every Compartment.HydroBoundsMin/Max.
       Reason: these fields are now deprecated and no longer read at runtime
       (CV components are the single source of truth). Zeroing avoids the
       confusion of seeing stale legacy values in the editor.

  2. LIST  every Edge in FloodGraph + every Connection.
       The user manually decides which to delete / rename / fix. This script
       does NOT auto-rewrite topology — only flags candidates.

  3. APPLY the user-supplied EDGE_RENAMES + EDGES_TO_DELETE + COMPARTMENTS_TO_DELETE
       maps if non-empty. Edit them inline below before running APPLY mode.

Usage:
    Inside UE Editor Python console:
        exec(open(r"C:/Dev/Sub3D/Scripts/UE5/cleanup_subdef_post_optionb.py").read())

    Close DA_SubDef_Craniata in any editor window before APPLY mode.
"""

import unreal


DA_PATH = "/Game/Sub3D/FirstPlayableRun/DA_SubDef_Craniata.DA_SubDef_Craniata"
DRY_RUN = True   # ← flip to False to apply

# Toggle each cleanup pass independently.
DO_ZERO_HYDROBOUNDS   = True
DO_LIST_EDGES         = True
DO_APPLY_EDGE_RENAMES = True
DO_DELETE_EDGES       = True
DO_DELETE_VOLUMES     = True
DO_DELETE_CONNECTIONS = True

# ── Edge renames (ClosureId only — VolumeA/B follow the alignment script) ──
# Example: {"N_Old_Bad_Name": "N_New_Clean_Name"}
EDGE_RENAMES = {
    # Add explicit FloodGraph.Edge.ClosureId renames here.
}

# ── Connections to rename ──
CONNECTION_RENAMES = {
    # "N_OldId": "N_NewId",
}

# ── FloodGraph.Edges to delete (by ClosureId) ──
EDGES_TO_DELETE = [
    # "N_Upper_Airlock_Exit",   # legacy edge into deleted UpperAft chamber
]

# ── FloodGraph.Volumes to delete (by VolumeId) ──
VOLUMES_TO_DELETE = [
    # "C_Upper_Aft",
]

# ── Compartments to delete (Compartments[] entries by CompartmentId) ──
COMPARTMENTS_TO_DELETE = [
    # "C_Upper_Aft",
]

# ── Connections to delete (by ConnectionId) ──
CONNECTIONS_TO_DELETE = [
    # "N_Upper_Airlock_Exit",
]


def log(msg):  unreal.log("[cleanup_subdef] " + msg)
def warn(msg): unreal.log_warning("[cleanup_subdef] " + msg)
def err(msg):  unreal.log_error("[cleanup_subdef] " + msg)


def to_str(name):
    return str(name) if name else ""


def main():
    da = unreal.EditorAssetLibrary.load_asset(DA_PATH)
    if da is None:
        err("Failed to load DA at " + DA_PATH)
        return

    log("Mode: " + ("DRY RUN" if DRY_RUN else "APPLY (will save)"))

    compartments = da.get_editor_property("compartments")
    connections = da.get_editor_property("connections")
    flood_graph = da.get_editor_property("flood_graph")
    fg_volumes = flood_graph.get_editor_property("volumes")
    fg_edges = flood_graph.get_editor_property("edges")
    try:
        water_bakes = da.get_editor_property("water_bakes")
    except Exception:
        water_bakes = None

    changes = 0

    # ── 1. Zero HydroBounds on every compartment ──
    if DO_ZERO_HYDROBOUNDS:
        log("─── Pass 1: Zero HydroBoundsMin/Max ───")
        zero_v = unreal.Vector(0.0, 0.0, 0.0)
        for c in compartments:
            cid = to_str(c.get_editor_property("compartment_id"))
            cur_min = c.get_editor_property("hydro_bounds_min")
            cur_max = c.get_editor_property("hydro_bounds_max")
            if cur_min == zero_v and cur_max == zero_v:
                continue
            log("  ZERO {0}: Min={1} Max={2} → (0,0,0) (0,0,0)".format(cid, cur_min, cur_max))
            changes += 1
            if not DRY_RUN:
                c.set_editor_property("hydro_bounds_min", zero_v)
                c.set_editor_property("hydro_bounds_max", zero_v)

    # ── 2. List edges + connections for review ──
    if DO_LIST_EDGES:
        log("─── Pass 2: Edge + Connection inventory (review for manual action) ───")
        log("  Connections ({0}):".format(len(connections)))
        for i, conn in enumerate(connections):
            nid = to_str(conn.get_editor_property("connection_id"))
            a = to_str(conn.get_editor_property("compartment_a"))
            b = to_str(conn.get_editor_property("compartment_b"))
            ctype = conn.get_editor_property("connection_type")
            log("    [{0:>2}] {1:<35s} {2:<25s} ↔ {3:<25s}  Type={4}".format(i, nid, a, b or "<EXT>", ctype))

        log("  FloodGraph.Edges ({0}):".format(len(fg_edges)))
        for i, e in enumerate(fg_edges):
            cid = to_str(e.get_editor_property("closure_id"))
            a = to_str(e.get_editor_property("volume_a"))
            b = to_str(e.get_editor_property("volume_b"))
            log("    [{0:>2}] ClosureId={1:<35s} {2:<25s} ↔ {3:<25s}".format(i, cid, a, b or "<EXT>"))

    # ── 3. Apply renames / deletions if maps non-empty ──
    if DO_APPLY_EDGE_RENAMES and EDGE_RENAMES:
        log("─── Pass 3a: Edge ClosureId renames ───")
        for e in fg_edges:
            cid = to_str(e.get_editor_property("closure_id"))
            if cid in EDGE_RENAMES:
                new_cid = EDGE_RENAMES[cid]
                log("  RENAME edge ClosureId: '{0}' → '{1}'".format(cid, new_cid))
                changes += 1
                if not DRY_RUN:
                    e.set_editor_property("closure_id", unreal.Name(new_cid))

    if DO_APPLY_EDGE_RENAMES and CONNECTION_RENAMES:
        log("─── Pass 3b: Connection ConnectionId renames ───")
        for c in connections:
            nid = to_str(c.get_editor_property("connection_id"))
            if nid in CONNECTION_RENAMES:
                new_nid = CONNECTION_RENAMES[nid]
                log("  RENAME connection: '{0}' → '{1}'".format(nid, new_nid))
                changes += 1
                if not DRY_RUN:
                    c.set_editor_property("connection_id", unreal.Name(new_nid))

    if DO_DELETE_EDGES and EDGES_TO_DELETE:
        log("─── Pass 4a: Delete FloodGraph.Edges ───")
        keep_edges = []
        for e in fg_edges:
            cid = to_str(e.get_editor_property("closure_id"))
            if cid in EDGES_TO_DELETE:
                log("  DELETE edge: '{0}'".format(cid))
                changes += 1
                continue
            keep_edges.append(e)
        if not DRY_RUN and len(keep_edges) != len(fg_edges):
            flood_graph.set_editor_property("edges", keep_edges)

    if DO_DELETE_VOLUMES and VOLUMES_TO_DELETE:
        log("─── Pass 4b: Delete FloodGraph.Volumes ───")
        keep_vols = []
        for v in fg_volumes:
            vid = to_str(v.get_editor_property("volume_id"))
            if vid in VOLUMES_TO_DELETE:
                log("  DELETE volume: '{0}'".format(vid))
                changes += 1
                continue
            keep_vols.append(v)
        if not DRY_RUN and len(keep_vols) != len(fg_volumes):
            flood_graph.set_editor_property("volumes", keep_vols)

    if DO_DELETE_CONNECTIONS and CONNECTIONS_TO_DELETE:
        log("─── Pass 4c: Delete Connections ───")
        keep_conns = []
        for c in connections:
            nid = to_str(c.get_editor_property("connection_id"))
            if nid in CONNECTIONS_TO_DELETE:
                log("  DELETE connection: '{0}'".format(nid))
                changes += 1
                continue
            keep_conns.append(c)
        if not DRY_RUN and len(keep_conns) != len(connections):
            da.set_editor_property("connections", keep_conns)

    if COMPARTMENTS_TO_DELETE:
        log("─── Pass 4d: Delete Compartments + WaterBake keys ───")
        keep_comps = []
        for c in compartments:
            cid = to_str(c.get_editor_property("compartment_id"))
            if cid in COMPARTMENTS_TO_DELETE:
                log("  DELETE compartment: '{0}'".format(cid))
                changes += 1
                continue
            keep_comps.append(c)
        if not DRY_RUN and len(keep_comps) != len(compartments):
            da.set_editor_property("compartments", keep_comps)

        # Also drop matching WaterBakes keys.
        if water_bakes is not None:
            dropped = []
            for k in list(water_bakes.keys()):
                if to_str(k) in COMPARTMENTS_TO_DELETE:
                    log("  DROP WaterBakes key: '{0}'".format(k))
                    changes += 1
                    dropped.append(k)
            if not DRY_RUN and dropped:
                # Rebuild the map without the dropped keys.
                new_map = {}
                for k in water_bakes.keys():
                    if k in dropped:
                        continue
                    new_map[k] = water_bakes[k]
                da.set_editor_property("water_bakes", new_map)

    # ── 5. Re-write flood_graph back if we mutated its arrays ──
    if not DRY_RUN:
        da.set_editor_property("flood_graph", flood_graph)

    log("─── Summary ───")
    log("  Changes planned/applied: {0}".format(changes))

    if DRY_RUN:
        log("DRY RUN — no changes saved. Set DRY_RUN=False to apply.")
    else:
        log("Saving DA...")
        ok = unreal.EditorAssetLibrary.save_asset(DA_PATH, only_if_is_dirty=False)
        if ok:
            log("DA saved successfully.")
        else:
            err("Failed to save DA. Make sure it is not open in any editor window.")


main()
