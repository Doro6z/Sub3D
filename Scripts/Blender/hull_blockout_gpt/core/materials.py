"""
Sub3D - Solid-PBR material assignment (Craniata, lowpoly/voxel art direction)
============================================================================
No textures, no decals. Flat color + metallic + roughness only, to match the
lowpoly character aesthetic. The hull texture pipeline from the earlier
assign_materials.py has been intentionally stripped.

New material keys added for the SF detail kit (pipes tri-color, valves,
junctions, periscope, antennas) and for the central airlock seal.

Calling convention:
    from core.materials import assign_materials
    assign_materials()
"""

import bpy


PALETTE = {
    # Hull: darker gunmetal, near-full metallic. Previous values were too light
    # and felt painted rather than bare steel.
    "hull":         {"color": (0.09, 0.11, 0.13), "metal": 0.92, "rough": 0.42},
    "deck":         {"color": (0.23, 0.24, 0.26), "metal": 0.65, "rough": 0.70},
    "deck_upper":   {"color": (0.25, 0.26, 0.28), "metal": 0.60, "rough": 0.68},
    "bulkhead":     {"color": (0.30, 0.32, 0.35), "metal": 0.35, "rough": 0.78},
    "bulkhead_lwr": {"color": (0.25, 0.27, 0.30), "metal": 0.40, "rough": 0.76},
    "door":         {"color": (0.33, 0.34, 0.36), "metal": 0.80, "rough": 0.50},
    "door_frame":   {"color": (0.12, 0.13, 0.14), "metal": 0.55, "rough": 0.84},
    "hatch":        {"color": (0.28, 0.29, 0.31), "metal": 0.70, "rough": 0.60},
    "fin":          {"color": (0.22, 0.24, 0.26), "metal": 0.70, "rough": 0.54},
    "propulsor":    {"color": (0.35, 0.32, 0.27), "metal": 0.95, "rough": 0.34},
    "duct":         {"color": (0.16, 0.17, 0.19), "metal": 0.85, "rough": 0.45},
    # SF deco pipe tri-color (per user: bleu, jaune foncé industriel, rouge brun)
    "pipe_water":   {"color": (0.12, 0.26, 0.46), "metal": 0.45, "rough": 0.70},
    "pipe_hyd":     {"color": (0.58, 0.44, 0.10), "metal": 0.42, "rough": 0.66},
    "pipe_reactor": {"color": (0.46, 0.15, 0.12), "metal": 0.44, "rough": 0.66},
    "pipe":         {"color": (0.15, 0.22, 0.31), "metal": 0.45, "rough": 0.74},
    "valve":        {"color": (0.40, 0.34, 0.12), "metal": 0.55, "rough": 0.56},
    "junction":     {"color": (0.18, 0.18, 0.18), "metal": 0.55, "rough": 0.62},
    "periscope":    {"color": (0.14, 0.15, 0.16), "metal": 0.82, "rough": 0.38},
    "antenna":      {"color": (0.22, 0.22, 0.24), "metal": 0.80, "rough": 0.34},
    "flag":         {"color": (0.72, 0.18, 0.12), "metal": 0.05, "rough": 0.80},
    # Manual cutter meshes: bright red so they're impossible to miss. Not part
    # of the final visible submarine -- user deletes / boolean-cuts with them.
    "cutter":       {"color": (0.90, 0.08, 0.08), "metal": 0.00, "rough": 0.95},
    "catwalk":      {"color": (0.17, 0.18, 0.19), "metal": 0.72, "rough": 0.74},
    "ladder":       {"color": (0.46, 0.38, 0.19), "metal": 0.28, "rough": 0.70},
    "turret":       {"color": (0.21, 0.22, 0.23), "metal": 0.72, "rough": 0.58},
    "mount":        {"color": (0.17, 0.18, 0.20), "metal": 0.75, "rough": 0.56},
    "engine":       {"color": (0.13, 0.14, 0.16), "metal": 0.70, "rough": 0.66},
    "reactor":      {"color": (0.10, 0.12, 0.15), "metal": 0.60, "rough": 0.72},
    "storage":      {"color": (0.24, 0.25, 0.27), "metal": 0.30, "rough": 0.82},
    "seal":         {"color": (0.06, 0.06, 0.07), "metal": 0.0,  "rough": 0.92},
    "ui":           {"color": (0.10, 0.22, 0.25), "metal": 0.10, "rough": 0.25},
    "default":      {"color": (0.24, 0.25, 0.27), "metal": 0.40, "rough": 0.72},
}


# Ordered by specificity: longest prefix first wins at classify time.
MATERIAL_PATTERNS = {
    # Auxiliary meshes shipped for the user to wield manually (boolean cutters)
    "SM_SAS_HullCutter":          "cutter",
    "SM_Hull":                    "hull",
    # Pipes (tri-color)
    "SM_Pipe_Water":              "pipe_water",
    "SM_Pipe_Hyd":                "pipe_hyd",
    "SM_Pipe_Reactor":            "pipe_reactor",
    "SM_Pipe_":                   "pipe",
    # Decks
    "SM_Deck_engine_upper":       "deck",
    "SM_Deck_engine_lower":       "deck",
    "SM_Deck_lower_main":         "deck",
    "SM_Deck_upper":              "deck_upper",
    "SM_Deck_main":               "deck",
    # Bulkheads
    "SM_BH_Lower":                "bulkhead_lwr",
    "SM_BH_Main":                 "bulkhead",
    "SM_BH_Upper":                "bulkhead",
    # Doors / hatches (central rubber seal is welded into the Port battant mesh,
    # so SM_Airlock_Seal_Center is normally not present; the key stays here as a
    # fallback in case a future Blender tweak extracts it as a separate object)
    "SM_Airlock_Seal_Center":     "seal",
    "SM_Airlock_Door":            "door",
    "SM_Airlock_Cassette":        "door_frame",
    "SM_Door_":                   "door",
    "SM_HatchDoor_":              "hatch",
    "SM_Hatch_":                  "hatch",
    "SM_Battant_":                "door",
    # Control surfaces
    "SM_FinAssembly":             "fin",
    "SM_Fin_":                    "fin",
    "SM_Hydro_":                  "fin",
    "SM_RudderAssembly":          "fin",
    "SM_Rudder":                  "fin",
    "SM_Skeg":                    "fin",
    "SM_Propeller":               "propulsor",
    "SM_Propulsor_Duct":          "duct",
    "SM_Duct":                    "duct",
    # SF deco
    "SM_Valve":                   "valve",
    "SM_Junction":                "junction",
    "SM_Periscope":               "periscope",
    "SM_Flag_":                   "flag",
    "SM_Antenna":                 "antenna",
    # Traversal
    "SM_Ladder_":                 "ladder",
    "SM_Stair_":                  "ladder",
    "SM_StairRail_":              "ladder",
    "SM_StairRailPost_":          "ladder",
    "SM_StairSupport_":           "ladder",
    "SM_StairStringer_":          "ladder",
    "SM_StairHanger_":            "ladder",
    # Ballast / catwalk
    "SM_Ballast":                 "mount",
    "SM_Catwalk":                 "catwalk",
    # Turrets / weapons
    "SM_TurretStation_":          "mount",
    "SM_Turret_":                 "turret",
    "SM_Hardpoint_":              "mount",
    "SM_TurretSocket_":           "mount",
    # UI / props
    "SM_HelmDisplay":             "ui",
    "SM_HelmConsole":             "mount",
    "SM_CrewLocker_":             "storage",
    "SM_CrewProp_":               "storage",
    "SM_UpperStorage":            "storage",
    # Propulsion
    "SM_Engine":                  "engine",
    "SM_Reactor":                 "reactor",
}


def _get_or_create_principled(name):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name=name)
        mat.use_nodes = True
    return mat


def _create_solid_material(name, color, metallic, roughness):
    mat = _get_or_create_principled(name)
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()

    out = nodes.new(type="ShaderNodeOutputMaterial")
    out.location = (360, 0)
    bsdf = nodes.new(type="ShaderNodeBsdfPrincipled")
    bsdf.location = (80, 0)
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])

    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    return mat


def _classify(obj_name):
    # Longest-prefix match so "SM_Pipe_Water" beats "SM_Pipe_".
    for pattern, key in sorted(MATERIAL_PATTERNS.items(), key=lambda item: -len(item[0])):
        if obj_name.startswith(pattern):
            return key
    return "default"


def _assign(obj, material):
    obj.data.materials.clear()
    obj.data.materials.append(material)


def assign_materials():
    """Walk every mesh object, classify by name, and assign the matching solid PBR."""
    mesh_objects = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    cache = {}
    assigned = 0

    for obj in sorted(mesh_objects, key=lambda o: o.name):
        key = _classify(obj.name)
        mat_name = f"M_{key}"
        if mat_name not in cache:
            props = PALETTE.get(key, PALETTE["default"])
            cache[mat_name] = _create_solid_material(
                mat_name,
                props["color"],
                props["metal"],
                props["rough"],
            )
        _assign(obj, cache[mat_name])
        assigned += 1

    print(f"[materials] Assigned {assigned} meshes, {len(cache)} unique materials.")
    return assigned


if __name__ == "__main__":
    assign_materials()
