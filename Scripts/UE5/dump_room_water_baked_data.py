"""
Dump complet d'un URoomWaterBakedData en JSON + résumé Markdown.

USAGE
-----
1) Depuis la console Python de l'éditeur (Window → Developer Tools → Python Console) :

       exec(open('C:/Dev/Sub3D/Scripts/UE5/dump_room_water_baked_data.py').read())

2) Ou via UnrealClaude MCP execute_script avec le contenu du fichier en `script_content`.

3) Pour cibler un autre asset : éditer ASSET_PATH ci-dessous, ou appeler
   `dump_baked_data("/Game/.../BD_OtherRoom")` depuis la console après l'exec().

OUTPUT
------
- Saved/Logs/BakedData_<RoomId>.json — dump complet, indenté
- Saved/Logs/BakedData_<RoomId>.md   — résumé tabulaire lisible

CHAMPS EXTRAITS
---------------
- SourceRoomId, LocalBoundsMin/Max, Span calculé
- Slices[*] : SliceZ_Local, GridWidth/Height, SignedDistance (flat W*H), ContourPolygon, stats
- CapMeshesPerSlice[*] : Vertices, Triangles, Normals, UV0
- Openings[*] : Start, End (paires)
"""
import unreal
import json
import os

ASSET_PATH = "/Game/Sub3DWaterProto/BakedData/BD_Room_01"


def vec3(v):
    return [round(float(v.x), 3), round(float(v.y), 3), round(float(v.z), 3)]


def vec2(v):
    return [round(float(v.x), 3), round(float(v.y), 3)]


def get(obj, name):
    """Helper : accède à une UPROPERTY par son nom C++ (PascalCase) via get_editor_property."""
    return obj.get_editor_property(name)


def dump_baked_data(asset_path=ASSET_PATH):
    asset = unreal.load_asset(asset_path)
    if asset is None:
        unreal.log_error(f"[dump_baked_data] Failed to load asset: {asset_path}")
        return None

    # ── Top-level ──
    bounds_min = vec3(get(asset, "LocalBoundsMin"))
    bounds_max = vec3(get(asset, "LocalBoundsMax"))
    span = [
        round(bounds_max[0] - bounds_min[0], 3),
        round(bounds_max[1] - bounds_min[1], 3),
        round(bounds_max[2] - bounds_min[2], 3),
    ]

    data = {
        "asset_path": asset_path,
        "source_room_id": str(get(asset, "SourceRoomId")),
        "local_bounds_min": bounds_min,
        "local_bounds_max": bounds_max,
        "span": span,
        "slices": [],
        "cap_meshes_per_slice": [],
        "openings": [],
    }

    # ── Slices ──
    slices = get(asset, "Slices")
    for i, s in enumerate(slices):
        sd = [round(float(v), 3) for v in get(s, "SignedDistance")]
        contour = [vec2(p) for p in get(s, "ContourPolygon")]
        inside = sum(1 for v in sd if v < 0.0)
        sd_min = min(sd) if sd else 0.0
        sd_max = max(sd) if sd else 0.0
        data["slices"].append({
            "index": i,
            "slice_z_local": round(float(get(s, "SliceZ_Local")), 3),
            "grid_width": int(get(s, "GridWidth")),
            "grid_height": int(get(s, "GridHeight")),
            "stats": {
                "inside_count": inside,
                "total": len(sd),
                "inside_pct": round(100.0 * inside / max(1, len(sd)), 1),
                "sdf_min": round(sd_min, 3),
                "sdf_max": round(sd_max, 3),
            },
            "contour_polygon": contour,
            "signed_distance": sd,
        })

    # ── Cap meshes ──
    caps = get(asset, "CapMeshesPerSlice")
    for i, m in enumerate(caps):
        verts = [vec3(v) for v in get(m, "Vertices")]
        tris = [int(t) for t in get(m, "Triangles")]
        normals = [vec3(n) for n in get(m, "Normals")]
        uvs = [vec2(uv) for uv in get(m, "UV0")]
        data["cap_meshes_per_slice"].append({
            "index": i,
            "vertex_count": len(verts),
            "triangle_count": len(tris) // 3,
            "vertices": verts,
            "triangles": tris,
            "normals": normals,
            "uv0": uvs,
        })

    # ── Openings ──
    starts = get(asset, "OpeningSegmentStarts")
    ends = get(asset, "OpeningSegmentEnds")
    for i in range(min(len(starts), len(ends))):
        data["openings"].append({
            "index": i,
            "start": vec2(starts[i]),
            "end": vec2(ends[i]),
        })

    # ── Write JSON ──
    log_dir = unreal.Paths.project_log_dir()
    os.makedirs(log_dir, exist_ok=True)
    room_id = data["source_room_id"] or "Unknown"
    json_path = os.path.join(log_dir, f"BakedData_{room_id}.json")
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)

    # ── Write Markdown summary ──
    md_path = os.path.join(log_dir, f"BakedData_{room_id}.md")
    with open(md_path, 'w', encoding='utf-8') as f:
        f.write(f"# BakedData dump — {room_id}\n\n")
        f.write(f"**Asset path** : `{asset_path}`\n\n")
        f.write(f"**Bounds Min** : {bounds_min}\n\n")
        f.write(f"**Bounds Max** : {bounds_max}\n\n")
        f.write(f"**Span** : {span}\n\n")

        f.write("## Slices summary\n\n")
        f.write("| # | Z_local | Grid | Inside | SDF range | Contour pts |\n")
        f.write("|---|---|---|---|---|---|\n")
        for s in data["slices"]:
            st = s["stats"]
            f.write(
                f"| {s['index']} | {s['slice_z_local']:.2f} | "
                f"{s['grid_width']}×{s['grid_height']} | "
                f"{st['inside_count']}/{st['total']} ({st['inside_pct']}%) | "
                f"[{st['sdf_min']:.2f}, {st['sdf_max']:.2f}] | "
                f"{len(s['contour_polygon'])} |\n"
            )

        f.write("\n## Cap meshes per slice\n\n")
        f.write("| # | Vertices | Triangles |\n")
        f.write("|---|---|---|\n")
        for m in data["cap_meshes_per_slice"]:
            f.write(f"| {m['index']} | {m['vertex_count']} | {m['triangle_count']} |\n")

        f.write(f"\n## Openings\n\n{len(data['openings'])} segment(s)\n\n")
        if data["openings"]:
            f.write("| # | Start | End |\n|---|---|---|\n")
            for o in data["openings"]:
                f.write(f"| {o['index']} | {o['start']} | {o['end']} |\n")

    unreal.log(f"[dump_baked_data] {asset_path} -> {json_path}")
    unreal.log(f"[dump_baked_data] Summary: {len(data['slices'])} slices, "
               f"{len(data['cap_meshes_per_slice'])} caps, {len(data['openings'])} openings")
    unreal.log(f"[dump_baked_data] Markdown summary: {md_path}")
    return json_path


# Auto-run quand exec'd depuis la console (dans tous les cas où unreal est dispo).
if "unreal" in dir():
    dump_baked_data()
