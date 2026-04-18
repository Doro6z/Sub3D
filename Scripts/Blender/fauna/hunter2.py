"""
Sub3D — Fauna: Hunter 2 — Charge Predator (Lovecraftian)
=========================================================
Barracuda × goblin shark × orca.
Silhouette = fast torpedo.  Close range = profondément faux.

Orientation: snout (+X) / caudal (low X)
1 voxel = 4 cm.  Corps x=5..135 → 5.2 m.

Anatomie lovecraftienne :
  - Mâchoire prognathie, dents bioluminescentes
  - Barbillon mentonnier long + bulbe leurre
  - 6 palpes chitineux sous la mâchoire
  - Yeux primaires grands, latéraux-hauts + tapetum dot
  - Taches oculaires vestigiales le long du flanc
  - Fentes branchiales : peau retirée → arches PHARYNX visiibles + rakers
  - Rangées de photophores sur les flancs (2 lignes par côté)
  - Pectorales swept-back + rayons digitiformes au tip
  - Dorsale haute + filament TENDRIL au sommet
  - Caudale lunate

4 cm voxels. Blender > Scripting > Alt+P
"""

import bpy

VOXEL = 4

BODY    = 0   # Carapace sombre — bleu-gris iridescent
BELLY   = 1   # Ventral — pâle translucide
FIN     = 2   # Nageoires — quasi-noir
TOOTH   = 3   # Dents — ivoire
EYE     = 4   # Masse oculaire — noir profond
BIOLUM  = 5   # Photophores + leurre + tapetum — cyan-vert emissif
GILL    = 6   # Fentes branchiales (inutilisé directement, PHARYNX le remplace)
TENDRIL = 7   # Barbillon + palpes + filaments — chair pâle
PHARYNX = 8   # Arches branchiales internes — cramoisi sombre

FACE_DEFS = [
    ((1,0,0),  [(1,0,0),(1,1,0),(1,1,1),(1,0,1)]),
    ((-1,0,0), [(0,0,0),(0,0,1),(0,1,1),(0,1,0)]),
    ((0,1,0),  [(0,1,0),(0,1,1),(1,1,1),(1,1,0)]),
    ((0,-1,0), [(0,0,0),(1,0,0),(1,0,1),(0,0,1)]),
    ((0,0,1),  [(0,0,1),(1,0,1),(1,1,1),(0,1,1)]),
    ((0,0,-1), [(0,0,0),(0,1,0),(1,1,0),(1,0,0)]),
]


def _fill(g, x0, y0, z0, x1, y1, z1, mat):
    for x in range(x0, x1+1):
        for y in range(y0, y1+1):
            for z in range(z0, z1+1):
                g[(x, y, z)] = mat


def _lerp(a, b, t):
    return a + (b - a) * t


# Knots : (x, y_half, z_lo, z_hi).  x=5=base caudale, x=135=museau.
_KNOTS = [
    (  5,  1,  9, 14),
    ( 14,  2,  8, 14),
    ( 28,  5,  7, 16),
    ( 45,  8,  5, 18),
    ( 60, 10,  4, 20),
    ( 75, 10,  4, 20),
    ( 90,  8,  5, 19),
    (105,  6,  7, 18),
    (120,  4,  9, 17),
    (128,  2, 10, 16),
    (135,  1, 10, 14),
]


def _body(x):
    if x <= _KNOTS[0][0]:  return _KNOTS[0][1:]
    if x >= _KNOTS[-1][0]: return _KNOTS[-1][1:]
    for i in range(len(_KNOTS) - 1):
        xa, ya, za0, za1 = _KNOTS[i]
        xb, yb, zb0, zb1 = _KNOTS[i + 1]
        if xa <= x <= xb:
            t = (x - xa) / (xb - xa)
            return (int(round(_lerp(ya, yb, t))),
                    int(round(_lerp(za0, zb0, t))),
                    int(round(_lerp(za1, zb1, t))))
    return 1, 10, 14


def build_hunter2():
    g = {}
    F = lambda x0,y0,z0,x1,y1,z1,m: _fill(g,x0,y0,z0,x1,y1,z1,m)

    # ═══════════════════════════════════════════════════════
    # 1. CORPS PRINCIPAL  x=5..135
    # ═══════════════════════════════════════════════════════
    for x in range(5, 136):
        hw, zl, zh = _body(x)
        if hw <= 0 or zl >= zh:
            continue
        F(x, -hw, zl, x, hw, zh, BODY)
        if hw >= 2:
            F(x, -(hw - 1), zl, x, hw - 1, zl + 1, BELLY)

    # ═══════════════════════════════════════════════════════
    # 2. MÂCHOIRE PROGNATHIE  x=116..135
    # Sup z=12..14 / Inf z=7..10 / Gap z=11 vidé
    # ═══════════════════════════════════════════════════════
    F(122, -4, 12, 135, 4, 14, BODY)   # mâchoire supérieure
    F(116, -4,  7, 135, 4, 10, BODY)   # mâchoire inférieure (prognathie +6 vox = 24 cm)
    F(130, -1, 10, 135, 1, 14, BODY)   # pointe museau ferme le gap au centre

    # Vider le gap z=11 (entre les deux mâchoires)
    for x in range(116, 130):
        for y in range(-4, 5):
            g.pop((x, y, 11), None)

    # Dents latérales TOOTH — visibles de profil
    for x in range(118, 136):
        for z in range(7, 11):
            g[(x, -4, z)] = TOOTH
            g[(x,  4, z)] = TOOTH
    for x in range(123, 136):
        for z in range(12, 15):
            g[(x, -4, z)] = TOOTH
            g[(x,  4, z)] = TOOTH

    # Dents BIOLUM face au gap — lueur dans la gueule ouverte
    for x in range(118, 130):
        for y in range(-4, 5):
            if (x, y, 10) in g: g[(x, y, 10)] = BIOLUM   # sommet mâchoire inf
    for x in range(123, 130):
        for y in range(-4, 5):
            if (x, y, 12) in g: g[(x, y, 12)] = BIOLUM   # dessous mâchoire sup

    # ═══════════════════════════════════════════════════════
    # 3. YEUX PRIMAIRES  x=107..113, lateral-hauts
    # ═══════════════════════════════════════════════════════
    F(107, -10, 18, 113, -9, 20, EYE)
    F(107,   9, 18, 113, 10, 20, EYE)
    g[(111, -10, 20)] = BIOLUM   # tapetum gauche
    g[(111,  10, 20)] = BIOLUM   # tapetum droit

    # Taches oculaires vestigiales — organes sensoriels aliens
    for xv in [68, 42]:
        hw, zl, zh = _body(xv)
        g[(xv, -hw, zh - 1)] = BIOLUM
        g[(xv, -hw, zh - 3)] = BIOLUM
        g[(xv,  hw, zh - 1)] = BIOLUM
        g[(xv,  hw, zh - 3)] = BIOLUM

    # ═══════════════════════════════════════════════════════
    # 4. FENTES BRANCHIALES  x=93..96
    # Peau retirée → arches PHARYNX visibles + raker TENDRIL
    # ═══════════════════════════════════════════════════════
    for xg in range(93, 97):
        hw, zl, zh = _body(xg)
        z_mid = (zl + zh) // 2
        for dz in range(-3, 4):
            g.pop((xg, -hw, z_mid + dz), None)   # peau gauche retirée
            g.pop((xg,  hw, z_mid + dz), None)   # peau droite retirée
            if (xg, -(hw - 1), z_mid + dz) in g:
                g[(xg, -(hw - 1), z_mid + dz)] = PHARYNX
            if (xg,   hw - 1,  z_mid + dz) in g:
                g[(xg,   hw - 1, z_mid + dz)]  = PHARYNX
        # Raker — prolongement chitineux
        g[(xg, -(hw + 1), z_mid)] = TENDRIL
        g[(xg,   hw + 1,  z_mid)] = TENDRIL

    # ═══════════════════════════════════════════════════════
    # 5. NAGEOIRES OSSEUSES DE PROPULSION  x=68..88
    # 3 rayons chitineux (TOOTH) + membrane FIN entre eux
    # Palette horizontale plate — 2 vox en Z, étale en Y
    # ═══════════════════════════════════════════════════════

    # Membrane FIN : fond plat entre les rayons
    for xw in range(68, 89):
        hw, zl, zh = _body(xw)
        z_mid = (zl + zh) // 2
        if xw <= 78:
            ext = int(9 + (xw - 68) / 10.0 * 4)
        else:
            ext = int(13 - (xw - 78) / 10.0 * 4)
        ext = max(4, ext)
        F(xw, -(hw + ext), z_mid - 1, xw, -hw, z_mid + 1, FIN)
        F(xw,  hw, z_mid - 1, xw,  hw + ext, z_mid + 1, FIN)

    # Rayons osseux : 3 côtes chitineuses, 3 vox larges en X
    for rx, rlen in [(86, 9), (78, 13), (70, 9)]:
        hw, zl, zh = _body(rx)
        z_mid = (zl + zh) // 2
        for step in range(rlen):
            y_out = hw + 1 + step
            F(rx - 1, -y_out, z_mid - 1, rx + 1, -y_out, z_mid + 1, TOOTH)
            F(rx - 1,  y_out, z_mid - 1, rx + 1,  y_out, z_mid + 1, TOOTH)

    # ═══════════════════════════════════════════════════════
    # 6. DORSALE  x=55..100, pic x=77, z jusqu'à 35
    # + filament TENDRIL au sommet
    # ═══════════════════════════════════════════════════════
    for x in range(55, 101):
        t = (x - 55) / 22.0 if x <= 77 else (100 - x) / 23.0
        t = max(0.0, min(1.0, t))
        hw, zl, zh = _body(x)
        z_top = zh + int(t * 15)
        if z_top <= zh:
            continue
        F(x, -1, zh, x, 1, z_top, FIN)

    for step in range(10):   # filament au-delà du sommet
        g[(77 - step, 0, 35 + step)] = TENDRIL

    # ═══════════════════════════════════════════════════════
    # 7. ANALE  x=58..90, 40% hauteur dorsale
    # ═══════════════════════════════════════════════════════
    for x in range(58, 91):
        t = (x - 58) / 19.0 if x <= 77 else (90 - x) / 13.0
        t = max(0.0, min(1.0, t))
        hw, zl, zh = _body(x)
        z_bot = zl - int(t * 6)
        if z_bot >= zl:
            continue
        F(x, -1, max(0, z_bot), x, 1, zl, FIN)

    # ═══════════════════════════════════════════════════════
    # 8. FLUKE HORIZONTALE  x=0..12  (cétacé / mosasaure, pas requin)
    # Plate en Z (z=10..12), s'étale en Y — lobes séparés par
    # une encoche centrale au bord fuyant (x bas).
    # Envergure max : 14*2*4 = 112 cm ≈ 1.1 m
    # ═══════════════════════════════════════════════════════
    for step in range(13):
        xc = 12 - step
        y_outer = 2 + step                  # 2 → 14 voxels du centre
        y_inner = max(0, step - 6)          # encoche s'ouvre à partir de step 7
        F(xc, -y_outer, 10, xc, -y_inner, 12, FIN)   # lobe gauche
        F(xc,  y_inner, 10, xc,  y_outer, 12, FIN)   # lobe droit

    F(10, -2, 10, 14, 2, 12, FIN)   # connecteur pédonculaire

    # ═══════════════════════════════════════════════════════
    # 9. BARBILLON MENTONNIER  descend depuis x=135, z=9
    # 15 voxels vers le bas (z négatif autorisé), bulbe BIOLUM
    # ═══════════════════════════════════════════════════════
    for step in range(15):
        zb = 9 - step
        g[(135,  0, zb)] = TENDRIL
        g[(135, -1, zb)] = TENDRIL
        g[(135,  1, zb)] = TENDRIL

    # Bulbe leurre au bout
    bulb_z = 9 - 15
    F(134, -1, bulb_z - 1, 136, 1, bulb_z + 1, BIOLUM)

    # ═══════════════════════════════════════════════════════
    # 10. PALPES MENTONNIERS  6 tendrilles sous la mâchoire inf
    # ═══════════════════════════════════════════════════════
    palp_roots = [
        (133, -3, 7, 5), (133,  3, 7, 5),
        (127, -3, 7, 7), (127,  3, 7, 7),
        (121, -2, 7, 6), (121,  2, 7, 6),
    ]
    for px, py, pz, length in palp_roots:
        for step in range(length):
            g[(px, py, pz - step)] = TENDRIL
        g[(px, py, pz - length + 1)] = BIOLUM   # tip lumineux

    # ═══════════════════════════════════════════════════════
    # 11. PHOTOPHORES  2 rangées par flanc, x=20..120
    # Espacement 4 voxels, deux hauteurs distinctes
    # ═══════════════════════════════════════════════════════
    for x in range(20, 121, 4):
        hw, zl, zh = _body(x)
        z_mid = (zl + zh) // 2
        g[(x, -hw, z_mid + 2)] = BIOLUM
        g[(x,  hw, z_mid + 2)] = BIOLUM
        g[(x, -hw, z_mid - 3)] = BIOLUM
        g[(x,  hw, z_mid - 3)] = BIOLUM

    return g


# ═══════════════════════════════════════════════════════════
# MESH + BLENDER HELPERS
# ═══════════════════════════════════════════════════════════

def generate_mesh_mat(grid_dict, voxel_size=4):
    half = voxel_size / 2
    cache = {}; verts = []; faces = []; fmats = []
    def vid(gx, gy, gz):
        key = (gx, gy, gz)
        if key not in cache:
            cache[key] = len(verts)
            verts.append((gx*voxel_size - half, gy*voxel_size - half, gz*voxel_size))
        return cache[key]
    for (vx, vy, vz), mat in grid_dict.items():
        for (dx, dy, dz), corners in FACE_DEFS:
            if (vx+dx, vy+dy, vz+dz) not in grid_dict:
                faces.append(tuple(vid(vx+cx, vy+cy, vz+cz) for cx, cy, cz in corners))
                fmats.append(mat)
    return verts, faces, fmats


def make_mat(name, r, g, b, metallic=0.0, roughness=0.85, emit=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1)
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness
        if emit > 0:
            try:
                bsdf.inputs["Emission Color"].default_value = (r, g, b, 1)
                bsdf.inputs["Emission Strength"].default_value = emit
            except KeyError:
                pass
    return m


def spawn(grid_dict, name, mats, loc=(0, 0, 0)):
    verts, faces, fmats = generate_mesh_mat(grid_dict, VOXEL)
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.validate(); me.update(calc_edges=True)
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    for m in mats:
        ob.data.materials.append(m)
    for i, poly in enumerate(me.polygons):
        poly.material_index = fmats[i]
    ob.location = loc
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.shade_flat()
    ob.select_set(False)
    return ob


# ═══════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════

def main():
    print("\n" + "=" * 60)
    print("Sub3D Fauna — Hunter 2 (Lovecraftian Charge Predator)")
    print("=" * 60)

    if bpy.context.active_object and bpy.context.active_object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for d in bpy.data.meshes:
        if d.users == 0: bpy.data.meshes.remove(d)
    for d in bpy.data.materials:
        if d.users == 0: bpy.data.materials.remove(d)

    s = bpy.context.scene
    s.unit_settings.system       = 'METRIC'
    s.unit_settings.scale_length = 0.01
    s.unit_settings.length_unit  = 'CENTIMETERS'
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for sp in area.spaces:
                if sp.type == 'VIEW_3D':
                    sp.clip_start = 0.1
                    sp.clip_end   = 100000

    mats = [
        make_mat("H2_Body",    0.06, 0.09, 0.18, metallic=0.20, roughness=0.50),
        make_mat("H2_Belly",   0.50, 0.55, 0.60, roughness=0.75),
        make_mat("H2_Fin",     0.03, 0.04, 0.08, roughness=0.35),
        make_mat("H2_Tooth",   0.82, 0.80, 0.70, roughness=0.60),
        make_mat("H2_Eye",     0.02, 0.02, 0.04, roughness=0.08),
        make_mat("H2_Biolum",  0.05, 0.95, 0.70, roughness=0.15, emit=6.0),
        make_mat("H2_Gill",    0.02, 0.02, 0.03, roughness=0.95),
        make_mat("H2_Tendril", 0.45, 0.35, 0.38, roughness=0.90),
        make_mat("H2_Pharynx", 0.38, 0.04, 0.06, roughness=0.80),
    ]

    creature = build_hunter2()
    spawn(creature, "Hunter2", mats)

    n = len(creature)
    print(f"  {n} voxels — ~{n * 6} polygones")
    print(f"  Corps : {130 * 4} cm   Dorsale : {(35-20)*4} cm au-dessus du dos")
    print(f"  Barbillon : {15 * 4} cm   Photophores : {len([x for x in range(20, 121, 4)])*4} points")
    print()
    print("  Numpad 3 = profil latéral (silhouette torpedo)")
    print("  Numpad 1 = face avant (gueule + barbillon)")
    print("  Numpad 7 = dessus (pectorales + dorsale)")

    bpy.ops.object.select_all(action='SELECT')
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            for sp in area.spaces:
                if sp.type == 'VIEW_3D':
                    sp.shading.type = 'MATERIAL'
            break

    print("=" * 60)


main()
