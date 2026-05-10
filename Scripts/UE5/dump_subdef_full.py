"""
Sub3D - Full SubmarineDefinition extraction.

Dumps the entire DA_SubDef_Craniata content (every property, every array entry,
every map key) to:
  1. Output Log (LogPython)
  2. A timestamped text file under Saved/Reports/

Use this as a baseline before any DA cleanup so you have a clear before/after.

Usage:
    Inside UE Editor Python console:
        exec(open(r"C:/Dev/Sub3D/Scripts/UE5/dump_subdef_full.py").read())

Override DA_PATH at the bottom if needed.
"""

import unreal
import os
import datetime


DA_PATH = "/Game/Sub3D/FirstPlayableRun/DA_SubDef_Craniata.DA_SubDef_Craniata"


# --- formatting helpers ---------------------------------------------------

def fmt_vec(v):
    if v is None:
        return "None"
    return "({:.1f}, {:.1f}, {:.1f})".format(v.x, v.y, v.z)


def fmt_xform(t):
    if t is None:
        return "Identity"
    loc = t.translation
    rot = t.rotation.rotator()
    scl = t.scale3d
    return "Loc={:s} Rot=(P={:.1f} Y={:.1f} R={:.1f}) Scl={:s}".format(
        fmt_vec(loc), rot.pitch, rot.yaw, rot.roll, fmt_vec(scl))


def fmt_box(min_v, max_v):
    size_x = max_v.x - min_v.x
    size_y = max_v.y - min_v.y
    size_z = max_v.z - min_v.z
    return "Min={:s} Max={:s} Size=({:.1f}x{:.1f}x{:.1f})".format(
        fmt_vec(min_v), fmt_vec(max_v), size_x, size_y, size_z)


def fmt_name(n):
    s = str(n) if n else ""
    return s if s and s.lower() != "none" else "<None>"


# --- output sink ----------------------------------------------------------

class DumpSink:
    """Writes to UE log AND to a text file simultaneously."""
    def __init__(self, file_path):
        self.file_path = file_path
        self.lines = []

    def __call__(self, msg):
        unreal.log("[dump_subdef] " + msg)
        self.lines.append(msg)

    def flush(self):
        os.makedirs(os.path.dirname(self.file_path), exist_ok=True)
        with open(self.file_path, "w", encoding="utf-8") as f:
            f.write("\n".join(self.lines))
        unreal.log("[dump_subdef] Wrote {0} lines to {1}".format(len(self.lines), self.file_path))


# --- section dumpers ------------------------------------------------------

def dump_compartments(sink, comps):
    sink("")
    sink("════════ COMPARTMENTS ({0}) ════════".format(len(comps)))
    for i, c in enumerate(comps):
        cid = fmt_name(c.get_editor_property("compartment_id"))
        disp = c.get_editor_property("display_name")
        sem = c.get_editor_property("semantic_type")
        cap = c.get_editor_property("capacity_liters")
        h_min = c.get_editor_property("hydro_bounds_min")
        h_max = c.get_editor_property("hydro_bounds_max")
        max_h = c.get_editor_property("max_water_height_cm")
        floor_z = c.get_editor_property("walkable_floor_z_cm")
        sink("  [{0:>2}] {1}".format(i, cid))
        sink("       DisplayName        = {0}".format(disp))
        sink("       SemanticType       = {0}".format(sem))
        sink("       CapacityLiters     = {:.1f}".format(cap))
        sink("       HydroBounds        = {0}".format(fmt_box(h_min, h_max)))
        sink("       MaxWaterHeightCm   = {:.1f}".format(max_h))
        sink("       WalkableFloorZCm   = {:.1f}".format(floor_z))


def dump_connections(sink, conns):
    sink("")
    sink("════════ CONNECTIONS ({0}) ════════".format(len(conns)))
    for i, c in enumerate(conns):
        nid = fmt_name(c.get_editor_property("connection_id"))
        a = fmt_name(c.get_editor_property("compartment_a"))
        b = fmt_name(c.get_editor_property("compartment_b"))
        ctype = c.get_editor_property("connection_type")
        flow = c.get_editor_property("flow_area_cm2")
        starts_closed = c.get_editor_property("starts_closed")
        dw = c.get_editor_property("door_width_cm")
        dh = c.get_editor_property("door_height_cm")
        xf = c.get_editor_property("local_transform")
        sink("  [{0:>2}] {1}".format(i, nid))
        sink("       A → B              = {0}  →  {1}".format(a, b))
        sink("       Type               = {0}".format(ctype))
        sink("       FlowAreaCm2        = {:.1f}".format(flow))
        sink("       StartsClosed       = {0}".format(starts_closed))
        sink("       DoorWxH            = {:.0f} x {:.0f} cm".format(dw, dh))
        sink("       LocalTransform     = {0}".format(fmt_xform(xf)))


def dump_flood_graph(sink, fg):
    if fg is None:
        sink("")
        sink("════════ FLOOD GRAPH (None) ════════")
        return
    volumes = fg.get_editor_property("volumes")
    edges = fg.get_editor_property("edges")
    sink("")
    sink("════════ FLOOD GRAPH ════════")
    sink("  Volumes ({0}):".format(len(volumes)))
    for i, v in enumerate(volumes):
        vid = fmt_name(v.get_editor_property("volume_id"))
        # FDerivedFloodVolume may have additional fields — try to read common ones
        try:
            cap = v.get_editor_property("capacity_liters")
        except Exception:
            cap = None
        try:
            max_h = v.get_editor_property("max_water_height_cm")
        except Exception:
            max_h = None
        extra = ""
        if cap is not None:
            extra += " CapL={:.0f}".format(cap)
        if max_h is not None:
            extra += " MaxH={:.0f}".format(max_h)
        sink("    [{0:>2}] {1}{2}".format(i, vid, extra))

    sink("  Edges ({0}):".format(len(edges)))
    for i, e in enumerate(edges):
        cid = fmt_name(e.get_editor_property("closure_id"))
        a = fmt_name(e.get_editor_property("volume_a"))
        b = fmt_name(e.get_editor_property("volume_b"))
        try:
            area = e.get_editor_property("passage_area_cm2")
        except Exception:
            area = None
        try:
            ext = e.get_editor_property("exterior_edge")
        except Exception:
            ext = None
        try:
            closed = e.get_editor_property("closed")
        except Exception:
            closed = None
        suffix = ""
        if area is not None:
            suffix += " Area={:.0f}cm²".format(area)
        if ext is not None:
            suffix += " EXT={0}".format(ext)
        if closed is not None:
            suffix += " Closed={0}".format(closed)
        sink("    [{0:>2}] ClosureId={1:<40s} {2:<25s} ↔ {3:<25s}{4}".format(
            i, cid, a, b, suffix))


def dump_station_slots(sink, slots):
    sink("")
    sink("════════ STATION SLOTS ({0}) ════════".format(len(slots)))
    for i, s in enumerate(slots):
        sid = fmt_name(s.get_editor_property("station_id"))
        stype = s.get_editor_property("station_type")
        cid = fmt_name(s.get_editor_property("compartment_id"))
        xf = s.get_editor_property("local_transform")
        sink("  [{0:>2}] {1}".format(i, sid))
        sink("       Type      = {0}".format(stype))
        sink("       CompId    = {0}".format(cid))
        sink("       Transform = {0}".format(fmt_xform(xf)))


def dump_spawn_points(sink, spawns):
    sink("")
    sink("════════ SPAWN POINTS ({0}) ════════".format(len(spawns)))
    for i, s in enumerate(spawns):
        sid = fmt_name(s.get_editor_property("spawn_id"))
        role = s.get_editor_property("role")
        xf = s.get_editor_property("local_transform")
        sink("  [{0:>2}] {1}".format(i, sid))
        sink("       Role      = {0}".format(role))
        sink("       Transform = {0}".format(fmt_xform(xf)))


def dump_water_bakes(sink, bakes):
    sink("")
    if bakes is None:
        sink("════════ WATER BAKES (None) ════════")
        return
    keys = list(bakes.keys())
    sink("════════ WATER BAKES ({0}) ════════".format(len(keys)))
    for k in sorted(keys, key=lambda x: str(x)):
        v = bakes[k]
        v_path = v.get_path_name() if v is not None else "<None>"
        sink("  '{0}' → {1}".format(fmt_name(k), v_path))


def dump_top_level_props(sink, da):
    """Dump every editor_property at the top level of the DA."""
    sink("")
    sink("════════ TOP-LEVEL PROPERTIES ════════")
    # Hard-coded list because get_editor_property iteration is not exposed in Python.
    candidates = [
        "compartments", "connections", "flood_graph", "station_slots", "spawn_points",
        "water_bakes",
        # Common top-level fields we expect on USubmarineDefinition (best-effort, ignore missing)
        "spec_used",
        "hull_geometry",
        "spine_length_cm",
        "outer_radius_cm",
        "max_speed_knots",
        "ballast_capacity_l",
        "spawn_role_count",
    ]
    for prop in candidates:
        try:
            val = da.get_editor_property(prop)
        except Exception:
            continue
        # Skip the big arrays/maps we already dump in detail
        if prop in ("compartments", "connections", "flood_graph", "station_slots",
                    "spawn_points", "water_bakes"):
            continue
        if val is None:
            sink("  {0:<25s} = <None>".format(prop))
        else:
            sink("  {0:<25s} = {1}".format(prop, val))


# --- main -----------------------------------------------------------------

def main():
    ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    project_dir = unreal.Paths.project_dir().rstrip("/")
    out_path = os.path.join(project_dir, "Saved", "Reports", "SubDef_Dump_{0}.txt".format(ts))
    out_path = os.path.normpath(out_path)

    sink = DumpSink(out_path)

    sink("════════════════════════════════════════════════════════════════")
    sink(" SubmarineDefinition Full Dump")
    sink(" Asset: {0}".format(DA_PATH))
    sink(" Time:  {0}".format(ts))
    sink("════════════════════════════════════════════════════════════════")

    da = unreal.EditorAssetLibrary.load_asset(DA_PATH)
    if da is None:
        unreal.log_error("[dump_subdef] Failed to load DA at " + DA_PATH)
        return

    # Top-level props (excluding the big arrays we treat below)
    dump_top_level_props(sink, da)

    # Big sections
    comps = da.get_editor_property("compartments")
    conns = da.get_editor_property("connections")
    fg = da.get_editor_property("flood_graph")
    slots = da.get_editor_property("station_slots")
    spawns = da.get_editor_property("spawn_points")
    try:
        bakes = da.get_editor_property("water_bakes")
    except Exception:
        bakes = None

    dump_compartments(sink, comps)
    dump_connections(sink, conns)
    dump_flood_graph(sink, fg)
    dump_station_slots(sink, slots)
    dump_spawn_points(sink, spawns)
    dump_water_bakes(sink, bakes)

    sink("")
    sink("════════════════════════════════════════════════════════════════")
    sink(" END OF DUMP")
    sink("════════════════════════════════════════════════════════════════")

    sink.flush()


main()
