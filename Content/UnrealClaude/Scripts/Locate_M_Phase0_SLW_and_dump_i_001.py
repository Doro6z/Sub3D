import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
candidates = ar.get_assets_by_class('Material', search_sub_classes=False)
matches = [a for a in candidates if 'Phase0' in str(a.asset_name)]
print('Found matches:', len(matches))
for m in matches:
    print('  Path:', m.package_name, '|', m.asset_name)
if matches:
    pkg = str(matches[0].package_name) + '.' + str(matches[0].asset_name)
    mat = unreal.load_asset(pkg)
    print('Loaded:', mat.get_class().get_name())
    sm = mat.get_editor_property('shading_model') if mat.get_editor_property('shading_model') else 'unknown'
    print('ShadingModel:', sm)
    bm = mat.get_editor_property('blend_mode')
    print('BlendMode:', bm)
    print('Substrate enabled (project):', unreal.SystemLibrary.get_console_variable_int_value('r.Substrate'))
    try:
        exprs = mat.get_editor_property('expressions')
        print('Expressions count:', len(exprs))
        for e in exprs:
            print('  -', e.get_class().get_name(), '@', e.material_expression_editor_x, ',', e.material_expression_editor_y)
    except Exception as ex:
        print('expressions access failed:', ex)
