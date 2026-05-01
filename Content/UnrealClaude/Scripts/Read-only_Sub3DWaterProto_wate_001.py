"""@UnrealClaude Script
@Description: Read-only diagnostics for Sub3DWaterProto room water renderer mesh sections and baked data.
"""
import unreal

actors = unreal.EditorLevelLibrary.get_all_level_actors()
for actor in actors:
    cls = actor.get_class().get_name()
    if "RoomActor" not in cls and "BP_RoomActor" not in cls:
        continue
    unreal.log(f"WATER_DIAG actor={actor.get_name()} label={actor.get_actor_label()} class={cls} loc={actor.get_actor_location()}")
    for prop in ["BakedData", "WaterLevelNormalized", "RoomId"]:
        try:
            unreal.log(f"WATER_DIAG actor_prop {prop}={actor.get_editor_property(prop)}")
        except Exception as exc:
            unreal.log(f"WATER_DIAG actor_prop {prop}=<err {exc}>")
    comps = actor.get_components_by_class(unreal.ActorComponent)
    for comp in comps:
        cname = comp.get_name()
        cclass = comp.get_class().get_name()
        if any(token in cname for token in ["Water", "Cap", "Skirt", "Compartment", "RoomMesh"]) or any(token in cclass for token in ["Water", "Procedural", "Box", "StaticMesh"]):
            try:
                vis = comp.is_visible() if hasattr(comp, "is_visible") else "n/a"
            except Exception as exc:
                vis = f"err {exc}"
            try:
                hgame = comp.bHiddenInGame
            except Exception:
                hgame = "n/a"
            try:
                rel_loc = comp.get_relative_location() if hasattr(comp, "get_relative_location") else "n/a"
            except Exception as exc:
                rel_loc = f"err {exc}"
            try:
                world_loc = comp.get_world_location() if hasattr(comp, "get_world_location") else "n/a"
            except Exception as exc:
                world_loc = f"err {exc}"
            try:
                bounds = comp.bounds if hasattr(comp, "bounds") else "n/a"
            except Exception as exc:
                bounds = f"err {exc}"
            unreal.log(f"WATER_DIAG comp name={cname} class={cclass} visible={vis} hidden_game={hgame} rel={rel_loc} world={world_loc} bounds={bounds}")
            if cclass == "ProceduralMeshComponent":
                for method in ["get_num_sections", "get_num_mesh_sections"]:
                    if hasattr(comp, method):
                        try:
                            unreal.log(f"WATER_DIAG {cname}.{method}={getattr(comp, method)()}")
                        except Exception as exc:
                            unreal.log(f"WATER_DIAG {cname}.{method}=<err {exc}>")
                try:
                    mat = comp.get_material(0)
                    unreal.log(f"WATER_DIAG {cname}.mat0={mat.get_path_name() if mat else None}")
                except Exception as exc:
                    unreal.log(f"WATER_DIAG {cname}.mat0=<err {exc}>")
    # load baked data directly from actor property if possible
    try:
        data = actor.get_editor_property("BakedData")
    except Exception:
        data = None
    if data:
        try:
            slices = data.get_editor_property("Slices")
            caps = data.get_editor_property("CapMeshesPerSlice")
            starts = data.get_editor_property("OpeningSegmentStarts")
            unreal.log(f"WATER_DIAG baked path={data.get_path_name()} slices={len(slices)} caps={len(caps)} openings={len(starts)} min={data.get_editor_property('LocalBoundsMin')} max={data.get_editor_property('LocalBoundsMax')}")
            if caps:
                for idx in [0, len(caps)//2, len(caps)-1]:
                    mesh = caps[idx]
                    unreal.log(f"WATER_DIAG cap[{idx}] verts={len(mesh.Vertices)} tris={len(mesh.Triangles)} normals={len(mesh.Normals)} uv={len(mesh.UV0)}")
        except Exception as exc:
            unreal.log(f"WATER_DIAG baked inspect err={exc}")