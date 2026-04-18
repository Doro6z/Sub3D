#!/usr/bin/env python3
"""
Spec Extraction Bridge for Craniata submarine.

Reads the frozen Blender authoring spec and emits a single JSON
that UE will ingest to author USubmarineDefinition.

Authority-max: reports/plans/2026-04-16_spec_bridge_replaces_generator.md

Inputs (read-only, must not be modified):
    Scripts/Blender/hull_blockout_gpt/main.py        (AST parse, no execution)
    Scripts/Blender/hull_blockout_gpt/data/stations.json

Output:
    Content/Sub3D/FirstPlayableRun/Craniata_Definition.json

Usage:
    python export_sub_definition.py            # writes to default output path
    python export_sub_definition.py --dry-run  # prints JSON to stdout, writes nothing
    python export_sub_definition.py --out PATH # custom output path

Exit code 0 on success, non-zero on any extraction failure.
"""

from __future__ import annotations

import argparse
import ast
import hashlib
import json
import sys
from pathlib import Path


SCHEMA_VERSION = 1

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent.parent.parent
MAIN_PY = SCRIPT_DIR / "main.py"
STATIONS_JSON = SCRIPT_DIR / "data" / "stations.json"
DEFAULT_OUTPUT = REPO_ROOT / "Content" / "Sub3D" / "FirstPlayableRun" / "Craniata_Definition.json"


SCALAR_WHITELIST = {
    "LENGTH", "SUB_NAME", "HULL_THICK", "DECK_THICK", "BH_THICK",
    "DOOR_W", "DOOR_H", "DOOR_LOWER_W", "DOOR_LOWER_H", "DOOR_THRESHOLD",
    "SAS_LENGTH_CM", "DECK_MAIN_Z", "DECK_LOWER_Z", "HYBRID_DECK_Z",
    "LOWER_HUB_START", "LOWER_HUB_END",
}

CONTAINER_WHITELIST = {
    "MAIN_BULKHEAD_SPECS", "LOWER_BULKHEAD_SPECS",
    "UPPER_AIRLOCK_EXIT_BULKHEAD", "UPPER_AIRLOCK_INNER_BULKHEAD",
    "MAIN_DECK_CUTOUTS", "UPPER_DECK_CUTOUTS", "LOWER_DECK_CUTOUTS",
    "BALLAST_PAIRS", "TURRET_HARDPOINTS",
}

WHITELIST = SCALAR_WHITELIST | CONTAINER_WHITELIST


class ExtractionError(Exception):
    pass


def _err(node: ast.AST, message: str) -> ExtractionError:
    line = getattr(node, "lineno", "?")
    return ExtractionError(f"main.py:{line}: {message}")


def _eval(node: ast.AST, ns: dict) -> object:
    if isinstance(node, ast.Constant):
        return node.value
    if isinstance(node, ast.Name):
        if node.id not in ns:
            raise _err(node, f"name '{node.id}' not yet defined (order-of-assignment issue)")
        return ns[node.id]
    if isinstance(node, ast.UnaryOp):
        v = _eval(node.operand, ns)
        if isinstance(node.op, ast.USub):
            return -v
        if isinstance(node.op, ast.UAdd):
            return +v
        raise _err(node, f"unsupported unary op {type(node.op).__name__}")
    if isinstance(node, ast.BinOp):
        a = _eval(node.left, ns)
        b = _eval(node.right, ns)
        if isinstance(node.op, ast.Add):
            return a + b
        if isinstance(node.op, ast.Sub):
            return a - b
        if isinstance(node.op, ast.Mult):
            return a * b
        if isinstance(node.op, ast.Div):
            return a / b
        raise _err(node, f"unsupported bin op {type(node.op).__name__}")
    if isinstance(node, ast.List):
        return [_eval(e, ns) for e in node.elts]
    if isinstance(node, ast.Tuple):
        return tuple(_eval(e, ns) for e in node.elts)
    if isinstance(node, ast.Set):
        return {_eval(e, ns) for e in node.elts}
    if isinstance(node, ast.Dict):
        return {_eval(k, ns): _eval(v, ns) for k, v in zip(node.keys, node.values)}
    if isinstance(node, ast.Subscript):
        container = _eval(node.value, ns)
        slice_node = node.slice
        if isinstance(slice_node, ast.Index):  # Python < 3.9 compat
            slice_node = slice_node.value  # type: ignore[attr-defined]
        key = _eval(slice_node, ns)
        return container[key]
    raise _err(node, f"unsupported node kind {type(node).__name__}")


def extract_constants(source: str) -> dict:
    """Walk top-level Assign nodes and populate a namespace.

    All statically evaluable simple name assignments are captured so that
    whitelisted containers can reference sibling constants (e.g. MAIN_DECK_CUTOUTS
    references LOWER_HUB_CENTER_NORM). Whitelisted names must all evaluate;
    non-whitelisted names that fail to evaluate are skipped silently.
    """
    tree = ast.parse(source, mode="exec")
    ns: dict = {}
    for stmt in tree.body:
        if not isinstance(stmt, ast.Assign):
            continue
        if len(stmt.targets) != 1:
            continue
        target = stmt.targets[0]
        if not isinstance(target, ast.Name):
            continue
        try:
            ns[target.id] = _eval(stmt.value, ns)
        except ExtractionError as e:
            if target.id in WHITELIST:
                raise ExtractionError(f"while extracting '{target.id}': {e}") from None
            # silently skip non-whitelisted names we can't evaluate
            continue
    missing = WHITELIST - ns.keys()
    if missing:
        raise ExtractionError(f"whitelisted names absent from main.py: {sorted(missing)}")
    return ns


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


# ---------------------------------------------------------------------------
# Topology derivation
# ---------------------------------------------------------------------------

# Mapping from deck key to the names of constants whose bulkheads live there.
# Upper airlock bulkheads sit on the upper deck per main.py naming (UPPER_*).
# Main deck partitions come from MAIN_BULKHEAD_SPECS only.
DECK_BULKHEADS = {
    "main":  ["MAIN_BULKHEAD_SPECS"],
    "lower": ["LOWER_BULKHEAD_SPECS"],
    "upper": ["UPPER_AIRLOCK_INNER_BULKHEAD", "UPPER_AIRLOCK_EXIT_BULKHEAD"],
}


# Semantic compartment names per deck, left-to-right.
# Length must match the number of derived intervals on each deck.
COMPARTMENT_NAMES = {
    "main":  ["Bow", "Fwd", "Aft"],
    "upper": ["UpperFwd", "Airlock", "UpperAft"],
    "lower": ["BallastFwd", "Hub", "BallastAft"],
}


# Stations are NOT derived from compartments. Compartment names are references,
# not constraints — the user places stations freely in UE (any station in any
# compartment, multiple stations per compartment, reassignment at runtime later).
# The bridge therefore does not emit station slots. Station authoring happens
# entirely in BP_Submarine_Craniata as ChildActorComponents or attached actors.
STATION_SLOTS: list = []


def _flatten_bulkheads(ns: dict) -> dict:
    """Return {deck_key: [bulkhead_spec, ...]} where each spec has name, x_norm, door_w, door_h."""
    out: dict = {}
    for deck, const_names in DECK_BULKHEADS.items():
        entries = []
        for cname in const_names:
            value = ns[cname]
            items = value if isinstance(value, list) else [value]
            for item in items:
                entries.append({
                    "name":    item["name"],
                    "x_norm":  float(item["x_norm"]),
                    "door_w":  float(item["door_w"]),
                    "door_h":  float(item["door_h"]),
                })
        entries.sort(key=lambda b: b["x_norm"])
        out[deck] = entries
    return out


def _deck_span(stations_json: dict, deck_name: str) -> tuple:
    """Return (x_start_norm, x_end_norm) for a deck from stations.json. Falls back to (0, 1)."""
    for deck in stations_json.get("decks", []):
        if deck["name"] == deck_name:
            return float(deck["x_start_norm"]), float(deck["x_end_norm"])
    return 0.0, 1.0


def _deck_z(ns: dict, deck_name: str, stations_json: dict) -> float:
    """Resolve a deck z in cm.

    Preference order:
      1. main.py constant if one maps to the deck (DECK_MAIN_Z, DECK_LOWER_Z, HYBRID_DECK_Z)
      2. stations.json deck entry z field
    """
    mapping = {"main": "DECK_MAIN_Z", "lower": "DECK_LOWER_Z"}
    if deck_name in mapping and mapping[deck_name] in ns:
        return float(ns[mapping[deck_name]])
    for deck in stations_json.get("decks", []):
        if deck["name"] == deck_name:
            return float(deck["z"])
    raise ExtractionError(f"cannot resolve z for deck '{deck_name}'")


def derive_compartments(ns: dict, stations_json: dict, bulkheads_per_deck: dict) -> list:
    compartments = []
    for deck in ("main", "upper", "lower"):
        x_start, x_end = _deck_span(stations_json, deck)
        bhs = bulkheads_per_deck[deck]
        x_norms = [x_start] + [b["x_norm"] for b in bhs] + [x_end]
        length_cm = float(ns["LENGTH"])
        names = COMPARTMENT_NAMES[deck]
        count = len(x_norms) - 1
        if len(names) != count:
            raise ExtractionError(
                f"deck '{deck}': derived {count} compartments but COMPARTMENT_NAMES has "
                f"{len(names)} entries. Update COMPARTMENT_NAMES in this script."
            )
        for i in range(count):
            x0, x1 = x_norms[i], x_norms[i + 1]
            semantic = names[i]
            compartments.append({
                "id":           f"C_{deck}_{semantic}",
                "deck":         deck,
                "semantic":     semantic,
                "x_norm_min":   x0,
                "x_norm_max":   x1,
                "min_x_cm":     x0 * length_cm,
                "max_x_cm":     x1 * length_cm,
                "length_cm":    (x1 - x0) * length_cm,
                "deck_z_cm":    _deck_z(ns, deck, stations_json),
            })
    return compartments


def derive_connections(ns: dict, bulkheads_per_deck: dict, compartments: list) -> list:
    connections = []
    comp_by_deck = {}
    for c in compartments:
        comp_by_deck.setdefault(c["deck"], []).append(c)
    for deck, comps in comp_by_deck.items():
        comps.sort(key=lambda c: c["x_norm_min"])

    # Horizontal connections from bulkhead doors
    for deck, bhs in bulkheads_per_deck.items():
        comps = comp_by_deck.get(deck, [])
        for bh in bhs:
            a, b = None, None
            for i in range(len(comps) - 1):
                if abs(comps[i]["x_norm_max"] - bh["x_norm"]) < 1e-9:
                    a, b = comps[i], comps[i + 1]
                    break
            if a is None or b is None:
                raise ExtractionError(
                    f"bulkhead '{bh['name']}' on deck '{deck}' does not match any compartment boundary"
                )
            is_airlock = bh["name"].startswith("UpperAirlock")
            connections.append({
                "id":          f"N_{deck}_{bh['name']}",
                "type":        "Airlock" if is_airlock else "WatertightDoor",
                "from":        a["id"],
                "to":          b["id"],
                "x_norm":      bh["x_norm"],
                "x_cm":        bh["x_norm"] * float(ns["LENGTH"]),
                "door_w_cm":   bh["door_w"],
                "door_h_cm":   bh["door_h"],
                "sill_z_cm":   a["deck_z_cm"] + float(ns["DOOR_THRESHOLD"]),
                "deck":        deck,
            })

    # External connection for airlock exit (to exterior sentinel)
    for c in list(connections):
        if c["type"] == "Airlock" and c["id"].endswith("_UpperAirlock_Exit"):
            connections.append({
                "id":          f"{c['id']}__to_EXT",
                "type":        "AirlockExterior",
                "from":        c["to"],
                "to":          "EXT",
                "x_norm":      c["x_norm"],
                "x_cm":        c["x_cm"],
                "door_w_cm":   c["door_w_cm"],
                "door_h_cm":   c["door_h_cm"],
                "sill_z_cm":   c["sill_z_cm"],
                "deck":        c["deck"],
            })

    # Vertical connections from deck cutouts
    vertical_sources = [
        ("MAIN_DECK_CUTOUTS",  "main",  "lower"),
        ("UPPER_DECK_CUTOUTS", "upper", "main"),
        ("LOWER_DECK_CUTOUTS", "lower", None),  # drops into bilge, no walkable compartment below
    ]
    length_cm = float(ns["LENGTH"])
    for const_name, deck_top, deck_bottom in vertical_sources:
        cutouts = ns.get(const_name, [])
        if deck_bottom is None:
            continue
        comps_top = comp_by_deck.get(deck_top, [])
        comps_bot = comp_by_deck.get(deck_bottom, [])
        for cut in cutouts:
            x_norm = float(cut["x_norm"])
            a = _compartment_containing(comps_top, x_norm)
            b = _compartment_containing(comps_bot, x_norm)
            if a is None or b is None:
                continue  # cutout outside a walkable deck span; skip silently
            connections.append({
                "id":          f"N_vert_{cut['name']}",
                "type":        "Hatch" if cut.get("shape") == "round" else "Ladder",
                "from":        a["id"],
                "to":          b["id"],
                "x_norm":      x_norm,
                "x_cm":        x_norm * length_cm,
                "deck":        f"{deck_top}->{deck_bottom}",
                "shape":       cut.get("shape"),
                "radius_cm":   cut.get("radius"),
                "half_length_cm": cut.get("half_length"),
                "half_width_cm":  cut.get("half_width"),
                "coaming_height_cm": cut.get("coaming_height"),
            })

    connections.sort(key=lambda c: (c["deck"], c["x_norm"], c["id"]))
    return connections


def _compartment_containing(comps: list, x_norm: float):
    for c in comps:
        if c["x_norm_min"] - 1e-9 <= x_norm <= c["x_norm_max"] + 1e-9:
            return c
    return None


def derive_flood_graph(compartments: list, connections: list) -> dict:
    nodes = [{"id": c["id"], "deck": c["deck"], "volume_proxy": c["length_cm"]} for c in compartments]
    nodes.append({"id": "EXT", "deck": "exterior", "volume_proxy": 0.0})
    edges = []
    for c in connections:
        area = 0.0
        if c.get("door_w_cm") and c.get("door_h_cm"):
            area = float(c["door_w_cm"]) * float(c["door_h_cm"])
        elif c.get("shape") == "round" and c.get("radius_cm"):
            import math
            area = math.pi * float(c["radius_cm"]) ** 2
        elif c.get("shape") == "rect" and c.get("half_length_cm") and c.get("half_width_cm"):
            area = 4.0 * float(c["half_length_cm"]) * float(c["half_width_cm"])
        edges.append({
            "connection_id": c["id"],
            "from": c["from"],
            "to": c["to"],
            "capacity_area_cm2": area,
            "type": c["type"],
        })
    edges.sort(key=lambda e: e["connection_id"])
    return {"nodes": nodes, "edges": edges}


def derive_stations(compartments: list) -> list:
    comp_index = {c["id"]: c for c in compartments}
    out = []
    for slot in STATION_SLOTS:
        cid = f"C_{slot['deck']}_{slot['compartment']}"
        comp = comp_index.get(cid)
        if comp is None:
            raise ExtractionError(f"station '{slot['type']}' targets unknown compartment '{cid}'")
        x_center_cm = 0.5 * (comp["min_x_cm"] + comp["max_x_cm"])
        out.append({
            "type":         slot["type"],
            "compartment":  cid,
            "x_cm":         x_center_cm,
            "y_cm":         slot["y_offset_cm"],
            "z_cm":         comp["deck_z_cm"],
            "yaw_deg":      slot["yaw_deg"],
        })
    return out


def derive_spawn_points(compartments: list) -> list:
    out = []
    for c in compartments:
        out.append({
            "id":          f"SP_{c['id']}",
            "compartment": c["id"],
            "x_cm":        0.5 * (c["min_x_cm"] + c["max_x_cm"]),
            "y_cm":        0.0,
            "z_cm":        c["deck_z_cm"],
            "yaw_deg":     0.0,
        })
    return out


def derive_airlock(connections: list) -> dict:
    inner = next((c for c in connections if c["id"].endswith("_UpperAirlock_Inner")), None)
    outer = next((c for c in connections if c["id"].endswith("_UpperAirlock_Exit")), None)
    outer_ext = next((c for c in connections if c["id"].endswith("_UpperAirlock_Exit__to_EXT")), None)
    if inner is None or outer is None:
        raise ExtractionError("airlock connections not found; UpperAirlock_Inner and UpperAirlock_Exit are required")
    return {
        "inner_connection_id": inner["id"],
        "outer_connection_id": outer["id"],
        "exterior_connection_id": outer_ext["id"] if outer_ext else None,
        "chamber_compartment_id": inner["to"],
    }


# ---------------------------------------------------------------------------
# Main assembly
# ---------------------------------------------------------------------------

def build_definition(ns: dict, stations_json: dict) -> dict:
    bulkheads_per_deck = _flatten_bulkheads(ns)
    compartments = derive_compartments(ns, stations_json, bulkheads_per_deck)
    connections = derive_connections(ns, bulkheads_per_deck, compartments)
    flood_graph = derive_flood_graph(compartments, connections)
    stations = derive_stations(compartments)
    spawns = derive_spawn_points(compartments)
    airlock = derive_airlock(connections)

    bulkheads_flat = []
    for deck, bhs in bulkheads_per_deck.items():
        for b in bhs:
            bulkheads_flat.append({**b, "deck": deck})

    return {
        "schema_version": SCHEMA_VERSION,
        "source_hashes": {
            "main_py":        sha256_file(MAIN_PY),
            "stations_json":  sha256_file(STATIONS_JSON),
        },
        "meta": {
            "sub_name":          ns["SUB_NAME"],
            "length_cm":         ns["LENGTH"],
            "hull_thick_cm":     ns["HULL_THICK"],
            "deck_thick_cm":     ns["DECK_THICK"],
            "bulkhead_thick_cm": ns["BH_THICK"],
        },
        "hull": {
            "profile_samples": stations_json.get("stations", []),
        },
        "decks": stations_json.get("decks", []),
        "bulkheads":          bulkheads_flat,
        "compartments":       compartments,
        "connections":        connections,
        "flood_graph":        flood_graph,
        "station_slots":      stations,
        "spawn_points":       spawns,
        "airlock":            airlock,
        "turret_hardpoints":  ns["TURRET_HARDPOINTS"],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Craniata spec extraction bridge")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUTPUT,
                        help=f"output JSON path (default: {DEFAULT_OUTPUT})")
    parser.add_argument("--dry-run", action="store_true",
                        help="print JSON to stdout, do not write")
    args = parser.parse_args()

    if not MAIN_PY.is_file():
        print(f"ERROR: main.py not found at {MAIN_PY}", file=sys.stderr)
        return 2
    if not STATIONS_JSON.is_file():
        print(f"ERROR: stations.json not found at {STATIONS_JSON}", file=sys.stderr)
        return 2

    source = MAIN_PY.read_text(encoding="utf-8")
    stations_json = json.loads(STATIONS_JSON.read_text(encoding="utf-8"))

    try:
        ns = extract_constants(source)
        definition = build_definition(ns, stations_json)
    except ExtractionError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1

    payload = json.dumps(definition, indent=2, sort_keys=True, ensure_ascii=False)
    if args.dry_run:
        sys.stdout.write(payload + "\n")
        return 0

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(payload + "\n", encoding="utf-8")
    print(f"wrote {args.out} ({len(payload)} bytes)")
    print(f"  compartments: {len(definition['compartments'])}")
    print(f"  connections:  {len(definition['connections'])}")
    print(f"  stations:     {len(definition['station_slots'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
