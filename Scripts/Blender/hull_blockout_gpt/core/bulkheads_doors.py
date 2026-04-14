"""
Standardized bulkhead and door builders for the Blender hull blockout.
"""


def _clamp(value, lo, hi):
    return max(lo, min(hi, value))


def _append_quad(verts, faces, p0, p1, p2, p3):
    if abs(p1[1] - p0[1]) < 0.5 and abs(p2[1] - p3[1]) < 0.5:
        return
    if abs(p3[2] - p0[2]) < 0.5 and abs(p2[2] - p1[2]) < 0.5:
        return

    base = len(verts)
    verts.extend((p0, p1, p2, p3))
    faces.append((base + 0, base + 1, base + 2, base + 3))


def _build_z_samples(z_min, z_max, z_step, extra_values):
    values = [z_min, z_max]

    z = z_min
    while z < z_max:
        values.append(z)
        z += z_step

    values.extend(extra_values)

    unique = []
    seen = set()
    for value in sorted(values):
        if value < z_min or value > z_max:
            continue
        key = round(value, 4)
        if key in seen:
            continue
        seen.add(key)
        unique.append(value)
    return unique


def compute_bulkhead_top_z(
    x_norm,
    upper_z,
    deck_main_z,
    sample_curve_fn,
    superstructure_state_fn,
    min_main_height=380.0,
    min_upper_clearance=250.0,
    top_margin=14.0,
):
    x_norm = _clamp(x_norm, 0.0, 1.0)
    sb, _, super_h_local, _, _ = superstructure_state_fn(x_norm)
    hull_top = sample_curve_fn(x_norm)
    super_top = hull_top + super_h_local * max(sb, 0.0)
    return max(deck_main_z + min_main_height, upper_z + min_upper_clearance, super_top + top_margin)


def build_compartment_bulkhead(
    *,
    name,
    x_cm,
    z_min,
    z_max,
    door_sill_z,
    interior_hw_fn,
    make_fn,
    add_solidify_fn,
    bh_thickness,
    door_w=100.0,
    door_h=200.0,
    z_step=6.0,
    gy=None,
    gz=None,
    opening_shape=None,
    opening_clearance=None,
    color=(0.44, 0.42, 0.38),
):
    del gy, gz, opening_shape, opening_clearance

    x = x_cm
    door_half = door_w * 0.5
    door_top = door_sill_z + door_h
    has_opening = door_w > 0.0 and door_h > 0.0

    z_values = _build_z_samples(
        z_min,
        z_max,
        z_step,
        [door_sill_z, door_top],
    )
    widths = {z: max(0.0, interior_hw_fn(x, z)) for z in z_values}

    verts = []
    faces = []

    for z0, z1 in zip(z_values, z_values[1:]):
        hw0 = widths[z0]
        hw1 = widths[z1]
        if max(hw0, hw1) < 1.0:
            continue

        if not has_opening or z1 <= door_sill_z or z0 >= door_top:
            _append_quad(
                verts,
                faces,
                (x, -hw0, z0),
                (x, hw0, z0),
                (x, hw1, z1),
                (x, -hw1, z1),
            )
            continue

        left_y0 = -door_half
        left_y1 = -door_half
        right_y0 = door_half
        right_y1 = door_half

        if hw0 - door_half > 0.5 and hw1 - door_half > 0.5:
            _append_quad(
                verts,
                faces,
                (x, -hw0, z0),
                (x, left_y0, z0),
                (x, left_y1, z1),
                (x, -hw1, z1),
            )
            _append_quad(
                verts,
                faces,
                (x, right_y0, z0),
                (x, hw0, z0),
                (x, hw1, z1),
                (x, right_y1, z1),
            )

    if not verts:
        return None

    obj = make_fn(name, verts, faces, color[0], color[1], color[2], smooth=False)
    add_solidify_fn(obj, bh_thickness, 0)
    return obj


def place_bulkhead_with_clearance(
    *,
    name,
    x_cm,
    z_min,
    z_max,
    door_sill_z,
    interior_hw_fn,
    make_fn,
    add_solidify_fn,
    bh_thickness,
    door_w=100.0,
    door_h=200.0,
    opening_shape=None,
    opening_clearance=None,
    door_clearance_h=200.0,
    top_margin=36.0,
    max_z_cap=None,
    gy=None,
    gz=None,
    color=(0.44, 0.42, 0.38),
):
    del opening_shape, opening_clearance

    required_top = door_sill_z + max(door_h, door_clearance_h) + top_margin
    final_z_max = max(z_max, required_top)
    if max_z_cap is not None:
        final_z_max = min(final_z_max, max_z_cap)
    return build_compartment_bulkhead(
        name=name,
        x_cm=x_cm,
        z_min=z_min,
        z_max=final_z_max,
        door_sill_z=door_sill_z,
        interior_hw_fn=interior_hw_fn,
        make_fn=make_fn,
        add_solidify_fn=add_solidify_fn,
        bh_thickness=bh_thickness,
        door_w=door_w,
        door_h=door_h,
        gy=gy,
        gz=gz,
        color=color,
    )


def build_standard_pressure_door(
    *,
    name,
    x_center,
    sill_z,
    make_fn,
    append_box_fn,
    width=100.0,
    height=200.0,
    frame_margin=12.0,
    frame_depth=12.0,
    leaf_depth=8.0,
    threshold=10.0,
    leaf_inset=4.0,
    color=(0.30, 0.31, 0.30),
):
    verts = []
    faces = []

    outer_half_w = width * 0.5 + frame_margin
    outer_top = sill_z + height + frame_margin
    outer_bottom = sill_z - threshold
    x0 = x_center - frame_depth * 0.5
    x1 = x_center + frame_depth * 0.5

    append_box_fn(verts, faces, x0, x1, -outer_half_w, -width * 0.5, outer_bottom, outer_top)
    append_box_fn(verts, faces, x0, x1, width * 0.5, outer_half_w, outer_bottom, outer_top)
    append_box_fn(verts, faces, x0, x1, -width * 0.5, width * 0.5, sill_z + height, outer_top)
    append_box_fn(verts, faces, x0, x1, -width * 0.5, width * 0.5, outer_bottom, sill_z)

    leaf_x0 = x_center + frame_depth * 0.12
    leaf_x1 = leaf_x0 + leaf_depth
    leaf_y0 = -width * 0.5 + leaf_inset
    leaf_y1 = width * 0.5 - leaf_inset
    leaf_z0 = sill_z + leaf_inset
    leaf_z1 = sill_z + height - leaf_inset
    append_box_fn(verts, faces, leaf_x0, leaf_x1, leaf_y0, leaf_y1, leaf_z0, leaf_z1)

    inset = 14.0
    append_box_fn(
        verts,
        faces,
        leaf_x1,
        leaf_x1 + 3.5,
        leaf_y0 + inset,
        leaf_y1 - inset,
        leaf_z0 + inset,
        leaf_z1 - inset,
    )

    hub_cx0 = leaf_x1
    hub_cx1 = hub_cx0 + 5.0
    hub_y0 = -8.0
    hub_y1 = 8.0
    hub_z0 = sill_z + height * 0.53 - 8.0
    hub_z1 = sill_z + height * 0.53 + 8.0
    append_box_fn(verts, faces, hub_cx0, hub_cx1, hub_y0, hub_y1, hub_z0, hub_z1)
    append_box_fn(verts, faces, hub_cx1, hub_cx1 + 18.0, -2.0, 2.0, hub_z0 + 6.0, hub_z1 - 6.0)
    append_box_fn(verts, faces, hub_cx1, hub_cx1 + 16.0, -2.0, 2.0, hub_z0 + 16.0, hub_z0 + 22.0)
    append_box_fn(verts, faces, hub_cx1, hub_cx1 + 16.0, -2.0, 2.0, hub_z1 - 22.0, hub_z1 - 16.0)

    for side in (-1.0, 1.0):
        dog_y = side * (width * 0.30)
        append_box_fn(
            verts,
            faces,
            leaf_x1 - 1.0,
            leaf_x1 + 5.0,
            dog_y - 6.0,
            dog_y + 6.0,
            sill_z + height * 0.46,
            sill_z + height * 0.56,
        )

    return make_fn(name, verts, faces, color[0], color[1], color[2], smooth=False)


def build_sliding_split_door(
    *,
    name_prefix,
    x_center,
    sill_z,
    make_fn,
    append_box_fn,
    width=100.0,
    height=200.0,
    panel_gap=8.0,
    panel_margin=12.0,
    panel_depth=7.0,
    frame_depth=12.0,
    color=(0.30, 0.31, 0.30),
):
    panel_half_width = (width - panel_gap - panel_margin * 2.0) * 0.5
    panel_z0 = sill_z + panel_margin
    panel_z1 = sill_z + height - panel_margin
    slide_track_depth = max(panel_depth + 2.0, frame_depth * 0.75)

    for side, suffix in ((-1.0, "Port"), (1.0, "Stbd")):
        inner_y = side * (panel_gap * 0.5)
        outer_y = inner_y + side * panel_half_width
        y0, y1 = sorted((inner_y, outer_y))
        verts = []
        faces = []
        append_box_fn(
            verts,
            faces,
            x_center - slide_track_depth * 0.5,
            x_center + slide_track_depth * 0.5,
            y0,
            y1,
            panel_z0,
            panel_z1,
        )
        append_box_fn(
            verts,
            faces,
            x_center + slide_track_depth * 0.5,
            x_center + slide_track_depth * 0.5 + 2.0,
            y0 + 6.0,
            y1 - 6.0,
            panel_z0 + 10.0,
            panel_z1 - 10.0,
        )
        make_fn(f"{name_prefix}_{suffix}", verts, faces, color[0], color[1], color[2], smooth=False)


def build_standard_bulkhead(**kwargs):
    return build_compartment_bulkhead(**kwargs)


def build_standard_watertight_door(
    *,
    append_extruded_profile_x_fn=None,
    **kwargs,
):
    del append_extruded_profile_x_fn
    return build_standard_pressure_door(**kwargs)
