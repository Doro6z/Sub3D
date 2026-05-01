import unreal
TAG='[ROOMPROBE]'
world = unreal.UnrealEditorSubsystem().get_editor_world()
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
for a in actors:
    cls = a.get_class().get_name()
    if 'RoomActor' not in cls and 'BP_RoomActor' not in str(a.get_actor_label()):
        continue
    loc = a.get_actor_location()
    unreal.log_warning(f'{TAG} actor={a.get_actor_label()} cls={cls} loc=({loc.x:.0f},{loc.y:.0f},{loc.z:.0f})')
    comps = a.get_components_by_class(unreal.SceneComponent)
    for c in comps:
        cn = c.get_name()
        cl = c.get_class().get_name()
        wl = c.get_world_location()
        rl = c.get_relative_location()
        vis = c.is_visible() if hasattr(c, 'is_visible') else 'n/a'
        unreal.log_warning(f'{TAG}   comp={cn} ({cl}) wloc=({wl.x:.0f},{wl.y:.0f},{wl.z:.0f}) rloc=({rl.x:.0f},{rl.y:.0f},{rl.z:.0f}) vis={vis}')
        if 'ProceduralMesh' in cl:
            sections = c.get_num_sections() if hasattr(c, 'get_num_sections') else -1
            bounds = c.calc_bounds(c.get_component_transform()) if hasattr(c, 'calc_bounds') else None
            unreal.log_warning(f'{TAG}     procmesh sections={sections} bounds_extent={bounds.box_extent if bounds else None}')
            mat = c.get_material(0)
            unreal.log_warning(f'{TAG}     material={mat.get_name() if mat else None}')
