"""
HullBuilder — Lofts a hull mesh between station profiles.
"""

import math
from .profile import resample_profile, interpolate_profiles, mirror_profile


class HullBuilder:
    """
    Builds a submarine hull by lofting between station profiles.

    Usage:
        hull = HullBuilder(stations_data, length_cm=4400, pts_per_profile=32)
        verts, faces = hull.build(n_rings=100)
    """

    def __init__(self, stations, length_cm, pts_per_profile=32):
        """
        stations: list of {"x_norm": float, "profile": [[y,z], ...]}
        length_cm: total hull length
        pts_per_profile: resampled point count per half-profile
        """
        self.length = length_cm
        self.pts = pts_per_profile

        # Resample all profiles to uniform point count
        self.stations = []
        for s in stations:
            resampled = resample_profile(
                [(p[0], p[1]) for p in s["profile"]],
                self.pts
            )
            self.stations.append({
                "x_norm": s["x_norm"],
                "profile": resampled,
                "label": s.get("label", ""),
            })

        # Sort by x_norm
        self.stations.sort(key=lambda s: s["x_norm"])

    def get_profile_at(self, x_norm):
        """Interpolated half-profile at any normalized X position."""
        x_norm = max(0.0, min(1.0, x_norm))

        # Find bracketing stations
        for i in range(len(self.stations) - 1):
            x0 = self.stations[i]["x_norm"]
            x1 = self.stations[i + 1]["x_norm"]
            if x0 <= x_norm <= x1:
                t = (x_norm - x0) / max(1e-6, x1 - x0)
                return interpolate_profiles(
                    self.stations[i]["profile"],
                    self.stations[i + 1]["profile"],
                    t
                )

        return self.stations[-1]["profile"]

    def half_width_at(self, x_norm, z_query, tolerance=30.0):
        """Get max half-width (Y) at position (x_norm, z) in hull profile."""
        profile = self.get_profile_at(x_norm)
        best = 0
        for y, z in profile:
            if abs(z - z_query) < tolerance:
                best = max(best, abs(y))
        return best

    def build(self, n_rings=100):
        """
        Build hull mesh. Returns (verts, faces).
        verts: list of (x, y, z) tuples
        faces: list of (i0, i1, i2, i3) quad tuples
        """
        verts = []
        faces = []

        all_rings = []

        for i in range(n_rings + 1):
            x_norm = i / n_rings
            x = x_norm * self.length
            half = self.get_profile_at(x_norm)
            full = mirror_profile(half)
            all_rings.append(full)

        ring_size = len(all_rings[0])

        # Emit vertices
        for i, ring in enumerate(all_rings):
            x = (i / n_rings) * self.length
            for y, z in ring:
                verts.append((x, y, z))

        # Quad stitch between adjacent rings
        for i in range(n_rings):
            for j in range(ring_size):
                jn = (j + 1) % ring_size
                a = i * ring_size + j
                b = i * ring_size + jn
                c = (i + 1) * ring_size + jn
                d = (i + 1) * ring_size + j
                faces.append((a, b, c, d))

        # Bow cap (triangle fan)
        bow_center = len(verts)
        verts.append((0, 0, 0))
        for j in range(ring_size):
            jn = (j + 1) % ring_size
            faces.append((bow_center, jn, j))

        # Stern cap (triangle fan)
        stern_center = len(verts)
        verts.append((self.length, 0, 0))
        last_base = n_rings * ring_size
        for j in range(ring_size):
            jn = (j + 1) % ring_size
            faces.append((stern_center, last_base + j, last_base + jn))

        return verts, faces
