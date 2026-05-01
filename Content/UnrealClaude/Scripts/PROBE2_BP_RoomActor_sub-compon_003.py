import unreal
TAG='[ROOMPROBE2]'
world = unreal.UnrealEditorSubsystem().get_editor_world()
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
for a in actors:
    if 'BP_RoomActor' not in str(a.get_actor_label()) and 'BP_RoomActor' not in a.get_class().get_name():
        continue
    loc = a.get_actor_location()
    unreal.log_warning(f'{TAG} ACTOR {a.get_actor_label()} loc=({loc.x:.0f},{loc.y:.0f},{loc.z:.0f})')
    try:
        comps = a.get_components_by_class(unreal.SceneComponent.static_class())
    except Exception as e:
        unreal.log_warning(f'{TAG} get_components failed: {e}')
        comps = []
    unreal.log_warning(f'{TAG} num_components={len(comps)}')
    for c in comps:
        cn = c.get_name()
        cl = c.get_class().get_name()
        try:
            wl = c.get_world_location()
            wlstr = f'({wl.x:.0f},{wl.y:.0f},{wl.z:.0f})'
        except Exception:
            wlstr = 'n/a'
        unreal.log_warning(f'{TAG} C {cn}|{cl} wloc={wlstr}')
