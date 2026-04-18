"""
Sub3D — Fauna: Y Hunter
========================
Approche : corps triangulaire central, puis bras qui poussent.

1. Corps triangulaire  — profil lozenge, spike en bas, tête en haut
2. Bras                — poussent depuis les épaules (z=38..44),
                         sweep 45° vers le haut-extérieur
3. Oeil               — masse violette qui dépasse en +X depuis la tête
4. Mandibules         — deux cornes hydrauliques au-dessus de la tête
5. Glow               — bandes cyan sur la surface avant/arrière du corps
                         et veine centrale sur chaque bras

Single mesh. Rig & animate externally.
2cm voxels — Blender > Scripting > Alt+P
"""

import bpy

VOXEL = 2

BODY  = 0   # Exosquelette sombre — quasi-noir, léger bleu
GLOW  = 1   # Bandes bioluminescentes — cyan vif
EYE   = 2   # Oeil unique — violet violent
PUPIL = 3   # Pupille slit verticale — noir vide
SPIKE = 4   # Dents de scie, pointe mandibules, spike queue — os pâle

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


def bw(z):
    """
    Demi-largeur (Y) du corps triangulaire à la hauteur z.
    Crée un profil lozenge : spike → épaules larges → cou serré → tête.
    """
    if z <= 1:  return 0
    if z <= 5:  return z - 1                               # spike taper : 0→4
    if z <= 38: return int(3 + (z - 5) * 0.21)            # corps bas : 3→~10
    if z <= 44: return 10                                   # épaules plateau
    if z <= 50: return max(2, int(10 - (z - 44) * 1.4))   # cou serré : 10→2
    if z <= 62: return 4                                    # tête
    return max(0, 4 - (z - 62))                            # taper sommet


def build_y_hunter():
    g = {}
    F = lambda x0,y0,z0,x1,y1,z1,m: _fill(g, x0,y0,z0,x1,y1,z1,m)

    # ═══════════════════════════════════════════════════
    # 1. CORPS TRIANGULAIRE  z = 0..65
    # ═══════════════════════════════════════════════════

    # Pointe spike à la base
    g[(0, 0, 0)] = SPIKE
    g[(0, 0, 1)] = SPIKE

    # Corps — profil lozenge en Y, épaisseur fixe en X (-3..3)
    for z in range(2, 66):
        w = bw(z)
        if w > 0:
            F(-3, -w, z, 3, w, z, BODY)

    # Glow sur face avant/arrière — deux rythmes superposés :
    #   Bandes larges espacées  (z%8 in 3,4)  : pleine largeur, peu fréquentes
    #   Colonne centrale fine   (z%4 == 0)    : y=-1..1, plus fréquente
    # Résultat : effet core bioluminescent, pas de barcode uniforme.
    for z in range(4, 65):
        w = bw(z)
        if w == 0:
            continue
        if z % 8 in (3, 4):                    # bande pleine largeur
            for y in range(-w, w + 1):
                g[( 3, y, z)] = GLOW
                g[(-3, y, z)] = GLOW
        if z % 4 == 0:                         # colonne vertébrale centrale
            for y in range(-1, 2):
                g[( 3, y, z)] = GLOW
                g[(-3, y, z)] = GLOW

    # ═══════════════════════════════════════════════════
    # 2. OEIL  z = 46..56, x = 4..9
    # Posé APRÈS le corps pour écraser les voxels BODY/GLOW.
    # Dépasse en +X depuis la face avant de la tête.
    # ═══════════════════════════════════════════════════

    # Masse oeil — plus grande, dépasse davantage en +X
    F( 4, -5, 44, 10,  5, 58, EYE)
    F( 9, -4, 45, 12,  4, 57, EYE)    # iris profond
    F(11, -2, 46, 13,  2, 56, PUPIL)  # fente pupille verticale

    # Halo GLOW — bordure du socket sur la face avant du corps (x=3)
    for z in range(42, 60):
        w_z = bw(z)
        g[(3, -min(6, w_z), z)] = GLOW
        g[(3,  min(6, w_z), z)] = GLOW
    for y in range(-5, 6):
        g[(3, y, 42)] = GLOW
        g[(3, y, 59)] = GLOW

    # Débordement GLOW devant l'oeil (bleed dans l'eau)
    for z in range(46, 57, 2):
        for dy in range(-2, 3):
            g[(13, dy, z)] = GLOW

    # ═══════════════════════════════════════════════════
    # 3. MANDIBULES  z = 64..73
    # Deux cornes hydrauliques qui courbent vers l'extérieur
    # à partir du sommet de la tête.
    # ═══════════════════════════════════════════════════

    for step in range(12):
        t   = step / 11.0
        z   = 64 + step
        x   = int(t * 2)                   # dérive légèrement en avant (+X)
        mat = SPIKE if step >= 10 else BODY
        yl  = int(-3 - t * 10)             # gauche : -3 → -13
        yr  = int( 3 + t * 10)             # droite  :  3 → 13
        F(x-2, yl-2, z, x+2, yl, z, mat)
        F(x-2, yr,   z, x+2, yr+2, z, mat)

    # ═══════════════════════════════════════════════════
    # 4. BRAS  z = 40..61
    # Poussent depuis les épaules du corps (z≈40, y=±bw).
    # Chaque step : +1z, bord extérieur +1.2y (45° environ).
    # Largeur du bras diminue vers le tip.
    # Bord intérieur = bord du corps (jonction seamless).
    # ═══════════════════════════════════════════════════

    SHOULDER_Y = 10   # demi-largeur aux épaules (plateau z=38..44)

    for step in range(22):
        z     = 40 + step
        body  = bw(z)                          # bord corps à cette hauteur
        hw    = max(1, 8 - step // 2)          # demi-épaisseur du bras (taper)
        y_far = SHOULDER_Y + int(step * 1.2) + hw   # bord extérieur absolu
        y_near = body                          # bord intérieur = bord corps

        if y_far <= y_near:
            continue

        xh = max(2, 4 - step // 7)        # épaisseur X : 4 épaules → 2 tip

        # Bras gauche  (y négatif)
        F(-xh, -y_far, z, xh, -y_near, z, BODY)
        # Bras droit   (y positif, miroir)
        F(-xh,  y_near, z, xh,  y_far, z, BODY)

        # Veine GLOW centrale du bras sur les faces +X/-X
        arm_cy_l = -(y_near + y_far) // 2
        arm_cy_r =  (y_near + y_far) // 2
        gw = max(1, hw // 3)
        F( xh, arm_cy_l - gw, z,  xh, arm_cy_l + gw, z, GLOW)
        F(-xh, arm_cy_l - gw, z, -xh, arm_cy_l + gw, z, GLOW)
        F( xh, arm_cy_r - gw, z,  xh, arm_cy_r + gw, z, GLOW)
        F(-xh, arm_cy_r - gw, z, -xh, arm_cy_r + gw, z, GLOW)

        # Dents de scie sur le bord extérieur (toutes les 3 tranches)
        if step % 3 == 1:
            F(-2, -y_far - 2, z, 2, -y_far - 1, z, SPIKE)
            F(-2,  y_far + 1, z, 2,  y_far + 2, z, SPIKE)

    # Pointes tips des bras
    y_tip = SHOULDER_Y + int(21 * 1.2) + 1    # y_far au dernier step
    F(-1, -y_tip - 3, 62, 1, -y_tip - 1, 62, SPIKE)
    F(-1,  y_tip + 1, 62, 1,  y_tip + 3, 62, SPIKE)

    return g


# ═══════════════════════════════════════════════════════════
# MESH GENERATOR
# ═══════════════════════════════════════════════════════════

def generate_mesh_mat(grid_dict, voxel_size=2):
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
    print("Sub3D Fauna — Y Hunter")
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
    s.unit_settings.system      = 'METRIC'
    s.unit_settings.scale_length = 0.01
    s.unit_settings.length_unit = 'CENTIMETERS'
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for sp in area.spaces:
                if sp.type == 'VIEW_3D':
                    sp.clip_start = 0.1
                    sp.clip_end   = 100000

    mats = [
        make_mat("YH_Body",  0.04, 0.06, 0.14, metallic=0.2,  roughness=0.60),
        make_mat("YH_Glow",  0.02, 0.90, 0.95, roughness=0.20, emit=5.0),
        make_mat("YH_Eye",   0.50, 0.02, 0.90, roughness=0.15, emit=3.0),
        make_mat("YH_Pupil", 0.01, 0.01, 0.02, roughness=0.05),
        make_mat("YH_Spike", 0.68, 0.70, 0.62, roughness=0.65),
    ]

    creature = build_y_hunter()
    spawn(creature, "YHunter", mats)

    n = len(creature)
    print(f"  {n} voxels — ~{n*8} polygones")
    print(f"  Hauteur : ~{66*2}cm   Envergure : ~{(10+25+8)*2*2}cm")
    print()
    print("  Numpad 1 = face avant (silhouette Y + oeil)")
    print("  Numpad 3 = profil lateral (shard plat)")
    print("  Numpad 7 = dessus (envergure des bras)")

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
