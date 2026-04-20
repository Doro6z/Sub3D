"""
Sub3D — Fauna: Innsmouth Hunter (Strate 1 — Chasseur côtier)
=============================================================
Gulper eel × goblin shark × Deep One × bobbit worm.
Rien d'hydrodynamique. Un truc qui ne devrait pas exister.

Orientation : crâne (x=0) / queue (x=83)
  Mâchoire protrude jusqu'à x=−8
  Cirres pendent jusqu'à z=−3

Grille nominale : 84 × 18 × 20 — 1 voxel = 7 cm
Longueur effective avec mâchoire : 92 × 7 = 644 cm

Silhouette têtard difforme — pas une torpille.
  Tête+mâchoire 40% / Branchies+thorax 20% / Corps 25% / Queue 15%

AUCUN élément bioluminescent cyan.
AUCUNE ligne droite émissive. AUCUNE symétrie parfaite.

9 matériaux (palette strate 1) :
  DORSAL   #5A6258 — dos gris-vert maladif
  FLANK    #B8AFA0 — flancs crème tachée
  VENTRAL  #C8C0B0 — ventre pâle
  EXPOSED  #4A4238 — peau arrachée / patches sombres
  VEIN     #3A3A42 — côtes visibles / veines ventrales
  TOOTH    #D8CEB0 — ivoire sale
  EYE      #E8E4D0 — blanc laiteux, émissif 0.3
  CIRRE    #A88078 — rose chair (cirres mentonniers)
  BONE     #D0C8B8 — os (crête vertébrale)

7 cm voxels. Blender > Scripting > Alt+P
"""

import bpy

VOXEL = 7

DORSAL  = 0
FLANK   = 1
VENTRAL = 2
EXPOSED = 3
VEIN    = 4
TOOTH   = 5
EYE     = 6
CIRRE   = 7
BONE    = 8

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


# ── Profil corps : (x, y_half, z_lo, z_hi) ───────────────────
# x=0 = front crâne, x=83 = pointe caudale.
# z_lo élevé sur le crâne (6) → gap visible avec la mâchoire (z=0..4).
_KNOTS = [
    ( 0,  5,  6, 14),  # front crâne
    ( 8,  7,  6, 19),  # crâne bulbeux peak
    (15,  6,  4, 17),  # base crâne → branchies
    (22,  8,  2, 19),  # épaules
    (30,  9,  1, 19),  # thorax max — presque toute la hauteur de grille
    (35,  8,  1, 19),  # thorax plateau
    (45,  5,  3, 13),  # taper rapide
    (55,  2,  4,  9),  # pédoncule
    (65,  2,  5,  8),  # pédoncule fin
    (68,  2,  2, 16),  # base caudale
    (83,  1,  2, 18),  # pointe caudale
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
    return 1, 6, 10


def _body_color(z, zl, zh):
    """Assigne DORSAL / FLANK / VENTRAL selon la hauteur."""
    zr = zh - zl
    if z >= zh - zr // 4:   return DORSAL
    if z <= zl + 1:          return VENTRAL
    return FLANK


def build_innsmouth():
    g = {}
    F = lambda x0,y0,z0,x1,y1,z1,m: _fill(g,x0,y0,z0,x1,y1,z1,m)

    # ═══════════════════════════════════════════════════════
    # 1. CORPS PRINCIPAL  x=0..83
    #    3 tons : DORSAL / FLANK / VENTRAL
    # ═══════════════════════════════════════════════════════
    for x in range(0, 84):
        hw, zl, zh = _body(x)
        if hw <= 0 or zl >= zh: continue
        for y in range(-hw, hw + 1):
            for z in range(zl, zh + 1):
                g[(x, y, z)] = _body_color(z, zl, zh)

    # ── Bruit de surface sur le crâne (seed fixe, ±1 voxel) ─
    for x in range(0, 16):
        hw, zl, zh = _body(x)
        for y in range(-hw, hw + 1):
            for z in range(zl, zh + 1):
                is_surf = (abs(y) == hw or z == zh or z == zl)
                if is_surf and (x * 7 + (y + 9) * 11 + z * 13) % 5 == 0:
                    g.pop((x, y, z), None)   # retrait ~20% surface

    # ═══════════════════════════════════════════════════════
    # 2. MÂCHOIRE INFÉRIEURE  x=−8..12, z=0..4
    #    Décrochée du crâne (gap z=5) — scoop mandibulaire goblin
    #    Plus étroite que le crâne (y_half=4 vs hw=5..7)
    # ═══════════════════════════════════════════════════════
    for x in range(-8, 13):
        jaw_hw = max(2, 5 - abs(x - 2) // 4)   # rétrécit aux extrémités
        for y in range(-jaw_hw, jaw_hw + 1):
            for z in range(0, 5):
                g[(x, y, z)] = FLANK

    # Dents irrégulières — montent au-dessus de la mâchoire (z=5+)
    # Positions X et hauteurs selon spec
    jaw_teeth = [(-7,2),(-5,1),(-3,3),(-1,1),(2,2),(4,3),(7,1)]
    for tx, t_h in jaw_teeth:
        for z in range(5, 5 + t_h):
            for y in range(-2, 3):
                g[(tx, y, z)] = TOOTH
        # Voxels ivoire sur la face supérieure de la mâchoire (z=4)
        for y in range(-3, 4):
            g[(tx, y, 4)] = TOOTH

    # ═══════════════════════════════════════════════════════
    # 3. YEUX — 3 par flanc, positions asymétriques
    #    Gauche ≠ Droite — tailles et positions différentes
    # ═══════════════════════════════════════════════════════
    left_eyes  = [(5, -7, 13, 2), (7, -8, 15, 1), (9, -7, 12, 2)]
    right_eyes = [(6,  8, 14, 1), (8,  7, 12, 2), (10, 8, 15, 1)]

    for ex, ey, ez, es in left_eyes:
        for dx in range(es):
            for dz in range(es):
                g[(ex + dx, ey, ez + dz)] = EYE

    for ex, ey, ez, es in right_eyes:
        for dx in range(es):
            for dz in range(es):
                g[(ex + dx, ey, ez + dz)] = EYE

    # ═══════════════════════════════════════════════════════
    # 4. FENTES BRANCHIALES ASYMÉTRIQUES
    #    Gauche : 4 fentes (X = 16, 18, 20, 22)
    #    Droite  : 5 fentes (X = 15, 17, 19, 20, 22)  ← asymétrie
    #    Peau retirée, intérieur EXPOSED visible
    # ═══════════════════════════════════════════════════════
    left_gills  = [16, 18, 20, 22]
    right_gills = [15, 17, 19, 20, 22]

    for xg in left_gills:
        hw, zl, zh = _body(xg)
        z_mid = (zl + zh) // 2
        for dz in range(-3, 4):
            g.pop((xg,     -hw, z_mid + dz), None)
            g.pop((xg - 1, -hw, z_mid + dz), None)   # 2 vox larges
            if (xg, -(hw - 1), z_mid + dz) in g:
                g[(xg, -(hw - 1), z_mid + dz)] = EXPOSED

    for xg in right_gills:
        hw, zl, zh = _body(xg)
        z_mid = (zl + zh) // 2
        for dz in range(-2, 3):   # fentes plus petites
            g.pop((xg, hw, z_mid + dz), None)
            if (xg, hw - 1, z_mid + dz) in g:
                g[(xg, hw - 1, z_mid + dz)] = EXPOSED

    # ═══════════════════════════════════════════════════════
    # 5. CRÊTE VERTÉBRALE  x=18..60
    #    7 pointes BONE, espacées inégalement, hauteurs variables
    #    Lecture : vertèbres qui percent la peau
    # ═══════════════════════════════════════════════════════
    spike_defs = [(18,3),(24,5),(29,4),(36,6),(44,4),(52,3),(60,2)]
    for sx, s_h in spike_defs:
        hw, zl, zh = _body(sx)
        for step in range(s_h):
            zs = zh + 1 + step
            if step < s_h - 1:     # base : 2 voxels larges
                for dy in range(-1, 2):
                    g[(sx, dy, zs)] = BONE
            else:                   # pointe : 1 voxel
                g[(sx, 0, zs)] = BONE

    # ═══════════════════════════════════════════════════════
    # 6. NODULES / BOSSES  x=22..35
    #    4 protubérances asymétriques (non-miroir)
    # ═══════════════════════════════════════════════════════
    nodules = [(28, 7, 14, 2), (31, -6, 12, 2), (25, 5, 8, 3), (33, -8, 16, 2)]
    for nx, ny, nz, nr in nodules:
        for dx in range(-nr, nr + 1):
            for dy in range(-nr, nr + 1):
                for dz in range(-nr, nr + 1):
                    if dx*dx + dy*dy + dz*dz <= nr*nr:
                        g[(nx+dx, ny+dy, nz+dz)] = FLANK  # bosse chair

    # ═══════════════════════════════════════════════════════
    # 7. NAGEOIRES PECTORALES SQUELETTIQUES  x=28..38, z=2..4
    #    "Doigts" osseux parallèles — main palmée décharnée
    #    Gauche 4 doigts ≠ Droite 4 doigts (asymétrie longueur)
    # ═══════════════════════════════════════════════════════
    left_fingers  = [(-9, 5), (-11, 6), (-13, 4), (-14, 5)]
    right_fingers = [( 9, 6), ( 11, 5), ( 12, 6), ( 13, 3)]  # 3e doigt cassé

    for fy, flen in left_fingers:
        for x in range(28, 28 + flen):
            for z in range(2, 5):
                g[(x, fy, z)] = BONE

    for fy, flen in right_fingers:
        for x in range(28, 28 + flen):
            for z in range(2, 5):
                g[(x, fy, z)] = BONE

    # Membrane fine entre les doigts (VENTRAL — peau pâle)
    for x in range(28, 33):
        for z in [3]:
            for fy in range(-13, -9):
                if (x, fy, z) not in g:
                    g[(x, fy, z)] = VENTRAL
            for fy in range(9, 12):
                if (x, fy, z) not in g:
                    g[(x, fy, z)] = VENTRAL

    # ═══════════════════════════════════════════════════════
    # 8. PATCHES DE PEAU MANQUANTE  (zones EXPOSED)
    # ═══════════════════════════════════════════════════════
    F(40, -6,  6, 44, -4, 10, EXPOSED)
    F(48,  5, 12, 52,  7, 15, EXPOSED)
    F(37,  0,  4, 39,  2,  6, EXPOSED)

    # ═══════════════════════════════════════════════════════
    # 9. CÔTES VISIBLES  x=20..50
    #    Voxels VEIN sur la couche ventrale — arcs transversaux
    # ═══════════════════════════════════════════════════════
    for rx in range(20, 51, 6):
        hw, zl, zh = _body(rx)
        for y in range(-(hw - 1), hw):
            z_rib = zl + abs(y) // 3
            if (rx, y, z_rib) in g:
                g[(rx, y, z_rib)] = VEIN

    # ═══════════════════════════════════════════════════════
    # 10. CAUDALE DÉCHIRÉE  x=68..83
    #     Lame verticale (Y=−1..2, Z=2..18), ~15% des bords manquants
    # ═══════════════════════════════════════════════════════
    for x in range(68, 84):
        for y in range(-1, 3):    # légèrement asymétrique en Y
            for z in range(2, 19):
                g[(x, y, z)] = FLANK

    # Voxels manquants sur les bords (déterministe, ~15%)
    for x in range(68, 84):
        for y in range(-1, 3):
            for z in [2, 18]:    # bords haut et bas
                if (x * 11 + abs(y) * 7 + z * 13) % 7 == 0:
                    g.pop((x, y, z), None)
        for z in range(2, 19):
            for y in [-1, 2]:    # bords gauche et droit
                if (x * 11 + abs(y) * 7 + z * 13) % 7 == 0:
                    g.pop((x, y, z), None)

    # Trous spécifiques plus grands
    for hx, hy, hz in [(75, 0, 16), (78, 1, 4), (72, 0, 13)]:
        for dx in range(-2, 3):
            for dz in range(-2, 3):
                g.pop((hx + dx, hy, hz + dz), None)

    # ═══════════════════════════════════════════════════════
    # 11. CIRRES MENTONNIERS  (pendent sous la mâchoire)
    #     2 tentacules 1 voxel d'épaisseur, courbes différentes
    # ═══════════════════════════════════════════════════════
    # Gauche : 6 voxels, courbe bas-arrière
    for vx, vy, vz in [(-4,-2,0),(-3,-2,-1),(-2,-2,-1),(-1,-2,-2),(0,-2,-3),(1,-2,-3)]:
        g[(vx, vy, vz)] = CIRRE
    # Droite : 7 voxels, courbe différente
    for vx, vy, vz in [(-3,2,0),(-2,2,-1),(-1,2,-1),(0,2,-2),(1,2,-2),(2,2,-3),(3,2,-3)]:
        g[(vx, vy, vz)] = CIRRE

    return g


# ═══════════════════════════════════════════════════════════
# MESH + BLENDER
# ═══════════════════════════════════════════════════════════

def generate_mesh_mat(grid_dict, voxel_size=7):
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
    print("Sub3D Fauna — Innsmouth Hunter (Strate 1)")
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

    # Palette strate 1 — chasseur côtier, 0 bioluminescence
    mats = [
        make_mat("IH_Dorsal",  0.353, 0.384, 0.345, roughness=0.80),           # DORSAL
        make_mat("IH_Flank",   0.722, 0.686, 0.627, roughness=0.85),           # FLANK
        make_mat("IH_Ventral", 0.784, 0.753, 0.690, roughness=0.90),           # VENTRAL
        make_mat("IH_Exposed", 0.290, 0.259, 0.220, roughness=0.95),           # EXPOSED
        make_mat("IH_Vein",    0.227, 0.227, 0.259, roughness=0.95),           # VEIN
        make_mat("IH_Tooth",   0.847, 0.808, 0.690, roughness=0.70),           # TOOTH
        make_mat("IH_Eye",     0.910, 0.894, 0.816, roughness=0.10, emit=0.3), # EYE
        make_mat("IH_Cirre",   0.659, 0.502, 0.471, roughness=0.90),           # CIRRE
        make_mat("IH_Bone",    0.816, 0.784, 0.722, roughness=0.75),           # BONE
    ]

    creature = build_innsmouth()
    spawn(creature, "InnsmouthHunter", mats)

    n = len(creature)
    print(f"  {n} voxels — ~{n * 6} polygones")
    print(f"  Corps  : {84 * 7} cm   + mâchoire {8 * 7} cm")
    print(f"  Thorax max : {9 * 2 * 7} cm largeur / {19 * 7} cm hauteur")
    print()
    print("  Numpad 3 = profil latéral (têtard difforme)")
    print("  Numpad 1 = face avant (mâchoire décrochée + yeux asymétriques)")
    print("  Numpad 7 = dessus (crête irrégulière + nodules)")

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
