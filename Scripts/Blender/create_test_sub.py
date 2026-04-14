import unreal

def create_complete_submarine():
    package_path = "/Game/Submarines"
    asset_name = "Sub_Alpha_Auth"
    
    # 1. Ensure directory
    if not unreal.EditorAssetLibrary.does_directory_exist(package_path):
        unreal.EditorAssetLibrary.make_directory(package_path)
    
    full_path = f"{package_path}/{asset_name}"
    
    # Check if asset exists, delete if it does to start fresh
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.EditorAssetLibrary.delete_asset(full_path)
        
    # 2. Create the Asset
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    
    # Find the class. Since it's a C++ class, it's just unreal.Sub3DSubmarineAuthoringAsset
    asset_class = unreal.Sub3DSubmarineAuthoringAsset
    
    new_asset = asset_tools.create_asset(asset_name, package_path, asset_class, factory)
    
    if not new_asset:
        unreal.log_error(f"Failed to create Submarine Authoring Asset at {full_path}")
        return
        
    unreal.log(f"Successfully created asset: {new_asset.get_name()}")
    
    # 3. Setup Level A - Hull Geometry
    profile_params = unreal.HullProfileParams(
        profile=unreal.Sub3DHullLongitudinalProfile.MYRING,
        myring_nose_exponent=2.0
    )
    
    hull_def = unreal.SubmarineHullDef(
        length_cm=8500.0,
        default_half_width_cm=450.0,
        default_half_height_cm=500.0,
        default_wall_thickness_cm=15.0,
        profile_params=profile_params
    )
    
    new_asset.set_editor_property("hull", hull_def)
    
    # Appendages
    sail = unreal.SailDef(
        b_enabled=True,
        length_cm=1200.0,
        width_cm=200.0,
        height_cm=450.0,
        position_x=2500.0
    )
    new_asset.set_editor_property("sail", sail)
    
    bow = unreal.BowSectionDef(
        b_enabled=True,
        sonar_dome_diam_cm=300.0
    )
    new_asset.set_editor_property("bow_section", bow)
    
    stern = unreal.SternSectionDef(
        b_enabled=True,
        propulsor_diam_cm=400.0
    )
    new_asset.set_editor_property("stern_section", stern)
    
    # Frame Rings
    frame_rings = []
    
    for i in range(5):
        fr = unreal.FrameRingDef(
            spine_alpha=0.1 + (i * 0.2),
            thickness_cm=20.0,
            depth_cm=40.0,
            b_is_bay_boundary=True
        )
        frame_rings.append(fr)
    
    new_asset.set_editor_property("frame_rings", frame_rings)
    
    # Confirm Geometry to unlock B/C
    new_asset.set_editor_property("b_hull_geometry_confirmed", True)
    
    # 4. Setup Level B - Layout (Decks & Bays)
    deck_levels = [
        unreal.DeckLevelDef(z_offset_cm=-150.0),
        unreal.DeckLevelDef(z_offset_cm=150.0)
    ]
    
    new_asset.set_editor_property("deck_levels", deck_levels)
    
    # 5. Save the Asset
    unreal.EditorAssetLibrary.save_loaded_asset(new_asset, only_if_is_dirty=False)
    unreal.log("Submarine completed and saved! You can now double-click it in the Content Browser.")

if __name__ == "__main__":
    create_complete_submarine()
