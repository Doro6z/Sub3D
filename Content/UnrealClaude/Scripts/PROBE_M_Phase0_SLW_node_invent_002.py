import unreal
TAG='[PROBE_PH0]'
ar = unreal.AssetRegistryHelpers.get_asset_registry()
cands = ar.get_assets_by_class('Material', search_sub_classes=False)
matches = [a for a in cands if 'Phase0' in str(a.asset_name)]
unreal.log_warning(f'{TAG} matches={len(matches)}')
for m in matches:
    pkg = f'{m.package_name}.{m.asset_name}'
    unreal.log_warning(f'{TAG} path={pkg}')
    mat = unreal.load_asset(pkg)
    if mat is None: continue
    sm = mat.get_editor_property('shading_model')
    bm = mat.get_editor_property('blend_mode')
    unreal.log_warning(f'{TAG} class={mat.get_class().get_name()} shading={sm} blend={bm}')
    sub_v = unreal.SystemLibrary.get_console_variable_int_value('r.Substrate')
    sub_w = unreal.SystemLibrary.get_console_variable_int_value('r.Substrate.SingleLayerWater')
    unreal.log_warning(f'{TAG} r.Substrate={sub_v} r.Substrate.SingleLayerWater={sub_w}')
    try:
        exprs = mat.get_editor_property('expressions')
        unreal.log_warning(f'{TAG} expressions_count={len(exprs)}')
        for i,e in enumerate(exprs):
            unreal.log_warning(f'{TAG} expr[{i}]={e.get_class().get_name()}')
    except Exception as ex:
        unreal.log_warning(f'{TAG} expressions_err={ex}')
    try:
        outs = mat.get_editor_property('expression_collection') if hasattr(mat,'get_editor_property') else None
    except Exception as ex:
        unreal.log_warning(f'{TAG} alt_err={ex}')
