"""
InteriorBuilder — Decks and bulkheads, clipped to hull interior.
"""

import math

try:
    import bmesh
except ImportError:
    pass  # bmesh only available inside Blender


class InteriorBuilder:
    """
    Builds interior elements (decks, bulkheads) clipped to the hull.

    Usage:
        interior = InteriorBuilder(hull_builder, hull_thickness=15)
        deck_verts, deck_faces = interior.build_deck(z=-15, x_start=440, x_end=4100)
        bh_verts, bh_faces = interior.build_bulkhead(x=1000, z_min=-200, z_max=380, door_w=90, door_h=185)
    """

    def __init__(self, hull, hull_thickness):
        self.hull = hull
        self.thickness = hull_thickness

    def _interior_half_width(self, x, z):
        """Half-width of hull interior at (x, z)."""
        hw = self.hull.half_width_at(x / self.hull.length, z, tolerance=35)
        return max(0, hw - self.thickness - 5)

    def build_deck(self, z, x_start, x_end, nx=None, ny=16):
        """
        Build a flat deck plate at height z, from x_start to x_end.
        Clipped to hull interior. Returns bmesh-compatible (verts, faces).
        """
        if nx is None:
            nx = max(8, int((x_end - x_start) / 55))

        grid = {}
        verts = []
        vert_idx = 0

        for ix in range(nx + 1):
            x = x_start + (x_end - x_start) * ix / nx
            hw = self._interior_half_width(x, z)
            if hw < 10:
                continue
            for iy in range(ny + 1):
                y = -hw + 2 * hw * iy / ny
                verts.append((x, y, z))
                grid[(ix, iy)] = vert_idx
                vert_idx += 1

        faces = []
        for ix in range(nx):
            for iy in range(ny):
                keys = [(ix, iy), (ix + 1, iy), (ix + 1, iy + 1), (ix, iy + 1)]
                indices = [grid.get(k) for k in keys]
                if all(i is not None for i in indices):
                    faces.append(tuple(indices))

        return verts, faces

    def build_bulkhead(self, x, z_min, z_max, door_w=90, door_h=185, door_sill_z=None,
                       gy=20, gz=18):
        """
        Build a bulkhead at position X with door cutout.
        Clipped to hull interior. Returns (verts, faces).
        """
        if door_sill_z is None:
            door_sill_z = z_min

        grid = {}
        verts = []
        vert_idx = 0

        for iz in range(gz + 1):
            z = z_min + (z_max - z_min) * iz / gz
            hw = self._interior_half_width(x, z)
            if hw < 5:
                continue
            for iy in range(gy + 1):
                y = -hw + 2 * hw * iy / gy
                # Door cutout
                if -door_w / 2 < y < door_w / 2 and door_sill_z < z < door_sill_z + door_h:
                    continue
                verts.append((x, y, z))
                grid[(iy, iz)] = vert_idx
                vert_idx += 1

        faces = []
        for iz in range(gz):
            for iy in range(gy):
                keys = [(iy, iz), (iy + 1, iz), (iy + 1, iz + 1), (iy, iz + 1)]
                indices = [grid.get(k) for k in keys]
                if all(i is not None for i in indices):
                    faces.append(tuple(indices))

        return verts, faces
