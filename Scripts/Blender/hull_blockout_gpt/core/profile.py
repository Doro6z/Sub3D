"""
Station profile loading, resampling, and interpolation.
"""

import math


def resample_profile(points, n_target):
    """
    Resample a profile to exactly n_target points with uniform arc-length spacing.
    Input: [(y, z), ...] ordered keel-to-top.
    Output: [(y, z), ...] with exactly n_target points.
    """
    if len(points) <= 1:
        return [(0, 0)] * n_target

    # Compute cumulative arc lengths
    arc = [0.0]
    for i in range(1, len(points)):
        dy = points[i][0] - points[i - 1][0]
        dz = points[i][1] - points[i - 1][1]
        arc.append(arc[-1] + math.sqrt(dy * dy + dz * dz))

    total = arc[-1]
    if total < 1e-6:
        return [points[0]] * n_target

    # Resample at uniform arc-length intervals
    result = []
    for j in range(n_target):
        target_arc = total * j / (n_target - 1) if n_target > 1 else 0

        # Find bracketing segment
        seg = 0
        for k in range(len(arc) - 1):
            if arc[k] <= target_arc <= arc[k + 1]:
                seg = k
                break
            if k == len(arc) - 2:
                seg = k

        seg_len = arc[seg + 1] - arc[seg]
        if seg_len < 1e-6:
            t = 0.0
        else:
            t = (target_arc - arc[seg]) / seg_len

        y = points[seg][0] + t * (points[seg + 1][0] - points[seg][0])
        z = points[seg][1] + t * (points[seg + 1][1] - points[seg][1])
        result.append((y, z))

    return result


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def interpolate_profiles(p0, p1, t):
    """
    Interpolate between two profiles (same point count) with smoothstep.
    Returns new profile.
    """
    t = smoothstep(t)
    return [
        (p0[i][0] + (p1[i][0] - p0[i][0]) * t,
         p0[i][1] + (p1[i][1] - p0[i][1]) * t)
        for i in range(len(p0))
    ]


def mirror_profile(half_profile):
    """
    Create full ring from half-profile (starboard keel-to-top).
    Returns full ring: starboard + port (mirrored, no duplicate endpoints).
    """
    full = list(half_profile)
    # Mirror: reverse, negate Y, skip first and last to avoid duplicate
    for y, z in reversed(half_profile[1:-1]):
        full.append((-y, z))
    return full
