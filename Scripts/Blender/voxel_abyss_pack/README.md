# Sub3D - Voxel Abyss Pack

Pack Blender Python entierement voxel, separe des scripts legacy.

## Contrat technique

- Voxel de base character style: `2 cm`
- Detail voxel style "mains character": `voxel_size / 3`
- Certains modules (giants, outposts, submarine) utilisent des voxels plus gros pour atteindre des tailles 10-30m
- Tous les scripts sont lancables seuls
- `build_all.py` orchestre le package complet

## Execution

1. Ouvrir Blender
2. Ouvrir un script dans `Scripts/Blender/voxel_abyss_pack/`
3. Lancer `Alt+P`

Batch pack:

- `build_all.py`
- ou `Scripts/Blender/run_voxel_abyss_pack.py`

## Scripts generateurs

- `mobs_scouts.py`
- `mobs_brutes.py`
- `mobs_giants.py`
- `voxel_craniata.py`
- `submarine_modular.py`
- `outposts.py`
- `cave_columns.py`
- `stalactites.py`
- `metal_intrusions.py`
- `rock_formations.py`
- `abyssal_flora.py`
- `seafloor_clutter.py`
- `vent_fields.py`

## Contenu actuel

- `13` scripts generateurs
- `185` meshes
- manifeste: `pack_manifest.py`
- orchestration: `build_all.py`
- helpers communs: `core.py`
