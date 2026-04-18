"""
Standardized bulkhead and door builders for the Blender hull blockout.
"""

import math


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
    rough_margin_x=6.0,
    rough_margin_z=7.0,
    leaf_overlap=2.0,
    y_offset=0.0,
    color=(0.30, 0.31, 0.30),
):
    def append_quad(faces, a, b, c, d):
        faces.append((a, b, c, d))

    def add_ring_loops(verts, x, center_z, hw, hh, count):
        start = len(verts)
        lower_scale = 0.82
        for i in range(count):
            a = (2.0 * math.pi * i) / count
            y = y_offset + hw * math.cos(a)
            s = math.sin(a)
            z = center_z + (hh * s if s >= 0.0 else hh * lower_scale * s)
            verts.append((x, y, z))
        return start

    def bridge_loops(faces, loop_a, loop_b, count, flip=False):
        for i in range(count):
            ni = (i + 1) % count
            if flip:
                append_quad(faces, loop_a + i, loop_b + i, loop_b + ni, loop_a + ni)
            else:
                append_quad(faces, loop_a + i, loop_a + ni, loop_b + ni, loop_b + i)

    frame_verts = []
    frame_faces = []
    leaf_verts = []
    leaf_faces = []

    center_z = sill_z + height * 0.5
    clear_half_w = width * 0.5
    clear_half_h = height * 0.5

    inner_hw = clear_half_w + rough_margin_x
    inner_hh = clear_half_h + max(2.0, rough_margin_z - 6.0)
    outer_hw = inner_hw + frame_margin
    outer_hh = inner_hh + frame_margin * 0.8

    lip = max(3.0, frame_depth * 0.35)
    frame_x0 = x_center - frame_depth * 0.5 - lip
    frame_x1 = x_center + frame_depth * 0.5 + lip
    ring_count = 28

    # Oval frame as a continuous ring, protruding on both sides of the wall.
    outer0 = add_ring_loops(frame_verts, frame_x0, center_z, outer_hw, outer_hh, ring_count)
    outer1 = add_ring_loops(frame_verts, frame_x1, center_z, outer_hw, outer_hh, ring_count)
    inner0 = add_ring_loops(frame_verts, frame_x0, center_z, inner_hw, inner_hh, ring_count)
    inner1 = add_ring_loops(frame_verts, frame_x1, center_z, inner_hw, inner_hh, ring_count)

    bridge_loops(frame_faces, outer0, outer1, ring_count, flip=False)  # Outside wall
    bridge_loops(frame_faces, inner1, inner0, ring_count, flip=False)  # Inside tunnel (reverse winding)
    bridge_loops(frame_faces, outer0, inner0, ring_count, flip=False)  # Front lip
    bridge_loops(frame_faces, inner1, outer1, ring_count, flip=False)  # Rear lip

    # Separate oval door leaf.
    leaf_hw = max(8.0, inner_hw - leaf_inset + leaf_overlap)
    leaf_hh = max(8.0, inner_hh - leaf_inset + leaf_overlap)
    leaf_x0 = x_center + frame_depth * 0.10
    leaf_x1 = leaf_x0 + leaf_depth
    leaf0 = add_ring_loops(leaf_verts, leaf_x0, center_z, leaf_hw, leaf_hh, ring_count)
    leaf1 = add_ring_loops(leaf_verts, leaf_x1, center_z, leaf_hw, leaf_hh, ring_count)
    bridge_loops(leaf_faces, leaf0, leaf1, ring_count, flip=False)
    leaf_faces.append(tuple(leaf0 + i for i in range(ring_count)))
    leaf_faces.append(tuple(leaf1 + i for i in reversed(range(ring_count))))

    # Center wheel + spokes.
    wheel_x0 = leaf_x1 + 0.2
    wheel_x1 = wheel_x0 + 4.2
    wheel_y = y_offset
    wheel_z = center_z
    append_box_fn(leaf_verts, leaf_faces, wheel_x0, wheel_x1, wheel_y - 7.0, wheel_y + 7.0, wheel_z - 7.0, wheel_z + 7.0)
    append_box_fn(leaf_verts, leaf_faces, wheel_x1, wheel_x1 + 14.0, wheel_y - 1.6, wheel_y + 1.6, wheel_z - 1.6, wheel_z + 1.6)
    append_box_fn(leaf_verts, leaf_faces, wheel_x1, wheel_x1 + 11.0, wheel_y - 1.6, wheel_y + 1.6, wheel_z + 7.0, wheel_z + 10.2)
    append_box_fn(leaf_verts, leaf_faces, wheel_x1, wheel_x1 + 11.0, wheel_y - 1.6, wheel_y + 1.6, wheel_z - 10.2, wheel_z - 7.0)

    # Opposite side locking bar + central cam block.
    bar_x0 = leaf_x1 - 0.6
    bar_x1 = bar_x0 + 4.2
    bar_y = y_offset - leaf_hw * 0.42
    append_box_fn(
        leaf_verts,
        leaf_faces,
        bar_x0,
        bar_x1,
        bar_y - 2.2,
        bar_y + 2.2,
        center_z - leaf_hh * 0.62,
        center_z + leaf_hh * 0.62,
    )
    append_box_fn(
        leaf_verts,
        leaf_faces,
        bar_x1,
        bar_x1 + 6.0,
        bar_y - 5.0,
        bar_y + 5.0,
        center_z - 6.0,
        center_z + 6.0,
    )

    frame_obj = make_fn(f"{name}_Frame", frame_verts, frame_faces, color[0], color[1], color[2], smooth=False)
    leaf_obj = make_fn(name, leaf_verts, leaf_faces, color[0], color[1], color[2], smooth=False)
    return frame_obj, leaf_obj


def build_sliding_split_door(
    *,
    name_prefix,
    x_center,
    sill_z,
    make_fn,
    append_box_fn,
    width=100.0,
    height=200.0,
    panel_gap=2.0,
    panel_margin=4.0,
    panel_depth=7.0,
    frame_depth=12.0,
    seal_center_width=2.0,
    color=(0.30, 0.31, 0.30),
):
    """Two sliding battants that cover (width x height) minus small margins.

    Panels are sized so that (panel_gap + 2 * panel_margin) is the only lost
    clearance. Defaults give a 96 x 192 cm coverage of a 100 x 200 cm opening,
    which preserves the 190 cm character clearance (180 cm character height).

    A thin central rubber seal is welded INTO the PORT battant's mesh (same
    object, same material) so it slides with it when the door opens. User spec:
    "le SealCenter doit etre joint a un des battant".
    """
    panel_half_width = (width - panel_gap - panel_margin * 2.0) * 0.5
    panel_z0 = sill_z + panel_margin
    panel_z1 = sill_z + height - panel_margin
    slide_track_depth = max(panel_depth + 2.0, frame_depth * 0.75)
    seal_half = max(0.5, seal_center_width * 0.5)

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
        # Track stub on the aft face for visual depth.
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

        # Weld the central seal strip into the Port battant mesh so it follows
        # the port leaf as one static mesh. The seal straddles y=0 and the full
        # height of the battant, slightly thicker than the battant to read as
        # rubber gasket against the stbd leaf.
        if side < 0.0:
            append_box_fn(
                verts,
                faces,
                x_center - slide_track_depth * 0.5 - 0.2,
                x_center + slide_track_depth * 0.5 + 0.2,
                -seal_half,
                seal_half,
                panel_z0 + 1.0,
                panel_z1 - 1.0,
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
