import unreal

def create_blueprint_asset(asset_name, package_path, parent_class):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', parent_class)
    
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    new_asset = asset_tools.create_asset(asset_name, package_path, unreal.Blueprint, factory)
    
    if new_asset:
        unreal.log(f"Successfully created Blueprint: {asset_name} at {package_path}")
        return new_asset
    else:
        unreal.log_error(f"Failed to create Blueprint: {asset_name}")
        return None

def create_widget_blueprint_asset(asset_name, package_path, parent_class):
    # For Widget Blueprints, we use the WidgetBlueprintFactory
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property('parent_class', parent_class)
    
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    new_asset = asset_tools.create_asset(asset_name, package_path, unreal.WidgetBlueprint, factory)
    
    if new_asset:
        unreal.log(f"Successfully created Widget Blueprint: {asset_name} at {package_path}")
        return new_asset
    else:
        unreal.log_error(f"Failed to create Widget Blueprint: {asset_name}")
        return None

def main():
    # Define assets to create: (Name, Path, ParentClass)
    # Using full class paths for C++ classes
    
    assets_to_create = [
        # Blueprints
        {"name": "BP_SubDoor", "path": "/Game/Sub3D/Blueprint/SubBP", "parent": unreal.SubDoorActor, "type": "bp"},
        {"name": "BP_BallastStation", "path": "/Game/Sub3D/Blueprint/SubBP", "parent": unreal.SubStationBase, "type": "bp"},
        {"name": "BP_EngineStation", "path": "/Game/Sub3D/Blueprint/SubBP", "parent": unreal.SubStationBase, "type": "bp"},
        {"name": "BP_TurretStation", "path": "/Game/Sub3D/Blueprint/SubBP", "parent": unreal.SubStationBase, "type": "bp"},
        {"name": "BP_RadarStation", "path": "/Game/Sub3D/Blueprint/SubBP", "parent": unreal.SubStationBase, "type": "bp"},
        
        # Widgets
        {"name": "WBP_SubBallast", "path": "/Game/Sub3D/UI", "parent": unreal.SubBallastWidget, "type": "widget"},
        {"name": "WBP_SubEngine", "path": "/Game/Sub3D/UI", "parent": unreal.SubEngineWidget, "type": "widget"},
        {"name": "WBP_SubTurret", "path": "/Game/Sub3D/UI", "parent": unreal.SubTurretWidget, "type": "widget"},
        {"name": "WBP_SubRadar", "path": "/Game/Sub3D/UI", "parent": unreal.SubRadarWidget, "type": "widget"},
    ]
    
    for asset in assets_to_create:
        if asset["type"] == "bp":
            create_blueprint_asset(asset["name"], asset["path"], asset["parent"])
        elif asset["type"] == "widget":
            create_widget_blueprint_asset(asset["name"], asset["path"], asset["parent"])

if __name__ == "__main__":
    main()
