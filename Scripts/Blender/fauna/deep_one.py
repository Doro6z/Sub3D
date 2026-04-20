"""
Sub3D — Fauna: Deep One (Innsmouth)
=====================================
Pas un requin. Pas hydrodynamique. Pas symétrique.
Anatomie d'Innsmouth : proportions fausses, textures fausses,
ne devrait pas exister.

Orientation: mâchoire (+X) / queue déchiquetée (low X)
1 voxel = 3 cm.  Grille : x=0..83, y=-9..9, z=0..19

Anatomie :
  1. Tête surdimensionnée — 40% de la longueur, bloat soudain à x=50
  2. Mâchoire goblin-shark prognathie — dents irrégulières, gap bioluminescent
  3. 3 yeux laiteux par flanc — asymétriques (côtés différents)
  4. Fentes branchiales décalées — 3 grandes à gauche, 4 petites à droite
  5. Crête de piques vertébraux — irréguliers, pas une nageoire
  6. Nageoires pectorales squelettiques — basses, ventrales, BONE+BELLY
  7. Côtes visibles à travers le ventre (BONE sur BELLY)
  8. Nodules tumoraux sur les flancs
  9. Queue déchiquetée — 60% remplie, pas de symétrie
  10. Veines bioluminescentes + gueule qui luit

3 cm voxels. Blender > Scripting > Alt+P
"""

import bpy

VOXEL = 3

BODY  = 0   # Gris-vert malsain
BELLY = 1   # Crème pâle — ventre translucide
BONE  = 2   # Os jauni — piques, côtes, nageoire
TOOTH = 3   # Ivoire irrégulier — dents
EYE   = 4   # Blanc laiteux — yeux
BIOLUM= 5   # Cyan-vert — veines emissives
GILL  = 6   # Très sombre — intérieur des branchies
TUMOR = 7   # Nodule — rouge-gris plus sombre

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


# Knots : (x, y_half, z_lo, z_hi)
# Corps étroit → tête qui explose soudainement à x=50.  Intentionnel.
_KNOTS = [
    ( 0,  1,  8, 11),
    ( 5,  2,  6, 12),
    (15,  4,  5, 13),
    (25,  4,  5, 13),   # corps reste plat et constant
    (38,  5,  5, 13),
    (45,  5,  4, 14),
    (50,  9,  2, 17),   # bloat soudain — cassure volontaire
    (63,  9,  2, 18),   # tête au maximum
    (72,  8,  3, 17),
    (78,  6,  4, 15),
    (83,  4,  5, 12),
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
    return 1, 8, 11


def build_deep_one():
    g = {}
    F = lambda x0,y0,z0,x1,y1,z1,m: _fill(g,x0,y0,z0,x1,y1,z1,m)

    # ═══════════════════════════════════════════════════════
    # 1. CORPS PRINCIPAL  x=5..83
    # ═══════════════════════════════════════════════════════
    for x in range(5, 84):
        hw, zl, zh = _body(x)
        if hw <= 0 or zl >= zh: continue
        F(x, -hw, zl, x, hw, zh, BODY)
        # Ventre crème — 2 rangées basses, inset 1 en Y
        if hw >= 2:
            F(x, -(hw - 1), zl, x, hw - 1, zl + 1, BELLY)

    # ═══════════════════════════════════════════════════════
    # 2. QUEUE DÉCHIQUETÉE  x=0..14
    # S'étale en Y, perforée (60% fill), pas de forme propre
    # ═══════════════════════════════════════════════════════
    for step in range(15):
        xc = 14 - step
        y_max = 1 + step
        hw, zl, zh = _body(max(5, xc))
        z_c = (zl + zh) // 2
        for y in range(-y_max, y_max + 1):
            for z in range(z_c - 1, z_c + 2):
                if (xc * 13 + abs(y) * 7 + z * 11) % 5 >= 2:
                    g[(xc, y, z)] = BODY

    # ═══════════════════════════════════════════════════════
    # 3. MÂCHOIRE GOBLIN-SHARK  x=62..83
    # Sup z=10..11 / Inf z=5..8 / Gap z=9 vidé
    # Mâchoire inf protrude 8 vox = 24 cm au-delà de la sup
    # ═══════════════════════════════════════════════════════
    F(70, -5,  10, 83, 5, 11, BODY)   # mâchoire supérieure
    F(62, -5,   5, 83, 5,  8, BODY)   # mâchoire inférieure (prognathie)
    F(80, -2,   8, 83, 2, 11, BODY)   # fermeture labiale au museau

    # Vider gap z=9
    for x in range(62, 80):
        for y in range(-5, 6):
            g.pop((x, y, 9), None)

    # Dents inférieures irrégulières (z=8 = top mâchoire inf, face au gap)
    for tx, ty in [
        (83,-3),(83,-1),(83,1),(83,3),(83,0),
        (81,-4),(81,-2),(81,2),(81,4),
        (79,-3),(79,0),(79,2),
        (76,-2),(76,-4),(76,3),
        (73,-1),(73,-3),(73,2),(73,4),
        (70,-2),(70,1),(70,-4),
        (67,-1),(67,3),(67,-3),
        (64,-2),(64,1),(64,-4),(64,4),
    ]:
        if (tx, ty, 8) in g:
            g[(tx, ty, 8)] = TOOTH
            # Dent longue (irrégulière) : certaines font 2 voxels
            if (tx + abs(ty)) % 3 == 0 and (tx, ty, 7) in g:
                g[(tx, ty, 7)] = TOOTH

    # Dents supérieures (z=10 = bottom mâchoire sup après gap)
    for tx, ty in [
        (83,-2),(83,2),(83,0),
        (81,-3),(81,1),(81,-1),
        (78,-2),(78,3),(78,-4),
        (75,0),(75,-3),(75,2),
        (72,1),(72,-2),(72,4),
        (70,-1),(70,2),(70,-4),
    ]:
        if (tx, ty, 10) in g:
            g[(tx, ty, 10)] = TOOTH

    # Lueur bioluminescente dans la gueule ouverte
    for x in range(64, 79):
        for y in range(-4, 5):
            if (x, y, 8) in g and g.get((x, y, 8)) not in (TOOTH,):
                g[(x, y, 8)] = BIOLUM
            if (x, y, 10) in g and g.get((x, y, 10)) not in (TOOTH,):
                g[(x, y, 10)] = BIOLUM

    # ═══════════════════════════════════════════════════════
    # 4. YEUX ASYMÉTRIQUES — 3 par flanc, positions différentes
    # Côté gauche (y<0) ≠ côté droit (y>0)
    # ═══════════════════════════════════════════════════════

    # GAUCHE — cluster légèrement en arrière, plus groupé
    left_eyes = [
        (67, -9, 14, 3, 3),   # grand oeil : haut-arrière
        (73, -9, 12, 2, 2),   # moyen : milieu
        (64, -8, 11, 2, 2),   # petit : bas, plus médial
    ]
    # DROIT — positions décalées, pas miroir
    right_eyes = [
        (76,  9, 15, 2, 2),   # haut-avant
        (70,  9, 13, 2, 2),   # milieu-arrière
        (80,  8, 10, 2, 2),   # bas-très-avant (crée asymétrie verticale)
    ]

    for ex, ey, ez, sy, sz in left_eyes:
        for dx in range(sz // 2 + 1):
            for dz in range(sz):
                g[(ex + dx, ey, ez + dz)] = EYE
        # Point lumineux au centre de l'oeil (iris alien)
        g[(ex, ey, ez + sz // 2)] = BIOLUM

    for ex, ey, ez, sy, sz in right_eyes:
        for dx in range(sz // 2 + 1):
            for dz in range(sz):
                g[(ex + dx, ey, ez + dz)] = EYE
        g[(ex, ey, ez + sz // 2)] = BIOLUM

    # ═══════════════════════════════════════════════════════
    # 5. FENTES BRANCHIALES ASYMÉTRIQUES
    # Gauche : 3 grandes (4-6 vox), Droite : 4 petites (3 vox)
    # Peau retirée → intérieur GILL visible
    # ═══════════════════════════════════════════════════════

    # Gauche (y négatif) — 3 grandes fentes, décalées en Z
    for xg, gill_h, z_bias in [(57, 6, -1), (53, 5, 0), (49, 4, 1)]:
        hw, zl, zh = _body(xg)
        z_c = (zl + zh) // 2 + z_bias
        for dz in range(-gill_h // 2, gill_h // 2 + 1):
            for dx in range(2):
                g.pop((xg - dx, -hw, z_c + dz), None)
            if (xg, -(hw - 1), z_c + dz) in g:
                g[(xg, -(hw - 1), z_c + dz)] = GILL

    # Droite (y positif) — 4 petites fentes, positions décalées
    for xg, z_bias in [(55, 1), (52, -1), (49, 2), (46, 0)]:
        hw, zl, zh = _body(xg)
        z_c = (zl + zh) // 2 + z_bias
        for dz in range(-1, 2):
            g.pop((xg, hw, z_c + dz), None)
            if (xg, hw - 1, z_c + dz) in g:
                g[(xg, hw - 1, z_c + dz)] = GILL

    # ═══════════════════════════════════════════════════════
    # 6. CRÊTE VERTÉBRALE  x=12..60
    # Pics BONE inégaux — pas une nageoire, juste des épines
    # ═══════════════════════════════════════════════════════
    spike_data = [
        (12,2,0),(16,3,1),(20,2,-1),(24,4,0),(28,3,1),
        (32,5,0),(36,3,-1),(40,4,1),(44,2,0),(47,3,0),
        (50,2,-1),(53,3,0),(56,2,1),(59,1,0),
    ]
    for sx, s_h, s_lean in spike_data:
        hw, zl, zh = _body(sx)
        for step in range(s_h):
            zs = zh + step
            yl = s_lean if step > 0 else 0
            g[(sx, yl, zs)] = BONE
            if step == 0:    # base plus large
                g[(sx - 1, yl, zs)] = BONE
                g[(sx + 1, yl, zs)] = BONE
                g[(sx, yl + 1, zs)] = BONE

    # ═══════════════════════════════════════════════════════
    # 7. NAGEOIRES PECTORALES SQUELETTIQUES  x=20..44
    # Basses (z=zl+1), 3 rayons BONE + fine membrane BELLY
    # Légèrement asymétriques gauche/droite
    # ═══════════════════════════════════════════════════════

    # Membrane fine
    for xw in range(19, 45):
        hw, zl, zh = _body(xw)
        z_fin = zl + 1
        ext_l = 3 + max(0, 7 - abs(xw - 32) // 2)
        ext_r = 3 + max(0, 6 - abs(xw - 30) // 2)  # légèrement décalé
        F(xw, -(hw + ext_l), z_fin, xw, -hw, z_fin, BELLY)
        F(xw,  hw, z_fin, xw,  hw + ext_r, z_fin, BELLY)

    # 3 rayons osseux sur chaque côté
    for rx, rlen_l, rlen_r in [(41, 5, 7), (32, 8, 7), (23, 4, 5)]:
        hw, zl, zh = _body(rx)
        z_fin = zl + 1
        for step in range(rlen_l):
            g[(rx, -(hw + 1 + step), z_fin)] = BONE
        for step in range(rlen_r):
            g[(rx,  hw + 1 + step,   z_fin)] = BONE

    # ═══════════════════════════════════════════════════════
    # 8. CÔTES VISIBLES  x=15..45
    # Arcs BONE sur la couche BELLY — visibles par transparence
    # ═══════════════════════════════════════════════════════
    for rx in range(15, 46, 6):
        hw, zl, zh = _body(rx)
        for y in range(-(hw - 1), hw):
            z_rib = zl + abs(y) // 3    # courbe légère vers les bords
            if (rx, y, z_rib) in g:
                g[(rx, y, z_rib)] = BONE

    # ═══════════════════════════════════════════════════════
    # 9. NODULES TUMORAUX sur les flancs
    # ═══════════════════════════════════════════════════════
    tumor_defs = [
        (30,-5, 9,2),(35, 7,10,2),(42,-8, 7,3),(48, 6, 6,2),
        (55,-7,10,2),(60, 8,11,2),(25, 5, 8,2),(38,-6, 7,2),
        (63,-8, 9,2),(22,-4, 7,2),(70, 7, 6,2),(45, 9, 8,2),
    ]
    for tx, ty, tz, tr in tumor_defs:
        for dx in range(-(tr // 2), tr // 2 + 1):
            for dy in range(-(tr // 2), tr // 2 + 1):
                for dz in range(-(tr // 2), tr // 2 + 1):
                    g[(tx + dx, ty + dy, tz + dz)] = TUMOR

    # ═══════════════════════════════════════════════════════
    # 10. VEINES BIOLUMINESCENTES
    # Ligne latérale + branches + réseau ventral
    # ═══════════════════════════════════════════════════════
    for x in range(8, 74):
        hw, zl, zh = _body(x)
        z_mid = (zl + zh) // 2
        g[(x, -hw, z_mid)] = BIOLUM
        g[(x,  hw, z_mid)] = BIOLUM

    for x in range(8, 74, 9):
        hw, zl, zh = _body(x)
        z_mid = (zl + zh) // 2
        for dz in range(1, 6):
            if z_mid + dz < zh - 1:
                g[(x, -hw, z_mid + dz)] = BIOLUM
                g[(x,  hw, z_mid + dz)] = BIOLUM
            if z_mid - dz > zl + 2:
                g[(x, -hw, z_mid - dz)] = BIOLUM
                g[(x,  hw, z_mid - dz)] = BIOLUM

    # Réseau ventral (visible à travers le ventre pale)
    for x in range(15, 50, 5):
        hw, zl, zh = _body(x)
        g[(x, 0, zl + 1)] = BIOLUM
        if hw > 3:
            g[(x,  hw - 2, zl + 1)] = BIOLUM
            g[(x, -(hw - 2), zl + 1)] = BIOLUM

    return g


# ═══════════════════════════════════════════════════════════
# MESH + BLENDER HELPERS
# ═══════════════════════════════════════════════════════════

def generate_mesh_mat(grid_dict, voxel_size=3):
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
    print("Sub3D Fauna — Deep One (Innsmouth)")
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
        make_mat("DO_Body",   0.28, 0.38, 0.26, roughness=0.75),           # BODY
        make_mat("DO_Belly",  0.76, 0.72, 0.60, roughness=0.90),           # BELLY
        make_mat("DO_Bone",   0.68, 0.62, 0.40, roughness=0.80),           # BONE
        make_mat("DO_Tooth",  0.80, 0.76, 0.58, roughness=0.70),           # TOOTH
        make_mat("DO_Eye",    0.82, 0.82, 0.80, roughness=0.10),           # EYE
        make_mat("DO_Biolum", 0.08, 0.88, 0.58, roughness=0.15, emit=4.5), # BIOLUM
        make_mat("DO_Gill",   0.04, 0.05, 0.04, roughness=0.95),           # GILL
        make_mat("DO_Tumor",  0.38, 0.30, 0.24, roughness=0.85),           # TUMOR
    ]

    creature = build_deep_one()
    spawn(creature, "DeepOne", mats)

    n = len(creature)
    print(f"  {n} voxels — ~{n * 6} polygones")
    print(f"  Corps total : {84 * 3} cm")
    print(f"  Tête (x=50..83) : {34 * 3} cm = 40% du corps")
    print(f"  Tête largeur max : {9 * 2 * 3} cm")
    print()
    print("  Numpad 3 = profil latéral (cassure corps→tête)")
    print("  Numpad 1 = face avant (yeux asymétriques + gueule)")
    print("  Numpad 7 = dessus (nodules + crête irrégulière)")

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
