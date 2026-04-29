"""
Sub3D - Voxel Abyss Pack Core
=============================

Common helpers for the abyssal voxel pack.

- Base voxel size matches the character scripts: 2 cm.
- Detail voxel size matches the character hand detail pass: 0.67 cm.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

import bpy

VOXEL = 2.0
DETAIL_VOXEL = VOXEL / 3.0

FACE_DEFS = [
    ((1, 0, 0), [(1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1)]),
    ((-1, 0, 0), [(0, 0, 0), (0, 0, 1), (0, 1, 1), (0, 1, 0)]),
    ((0, 1, 0), [(0, 1, 0), (0, 1, 1), (1, 1, 1), (1, 1, 0)]),
    ((0, -1, 0), [(0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)]),
    ((0, 0, 1), [(0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)]),
    ((0, 0, -1), [(0, 0, 0), (0, 1, 0), (1, 1, 0), (1, 0, 0)]),
]


@dataclass(frozen=True)
class MaterialDef:
    label: str
    color: Tuple[float, float, float]
    metallic: float = 0.0
    roughness: float = 0.85
    emission: float = 0.0


class VoxelGrid:
    def __init__(self, voxel_size: float = VOXEL):
        self.voxel_size = float(voxel_size)
        self.cells: Dict[Tuple[int, int, int], int] = {}

    def set(self, x: int, y: int, z: int, material: int = 0) -> None:
        self.cells[(int(x), int(y), int(z))] = material

    def discard(self, x: int, y: int, z: int) -> None:
        self.cells.pop((int(x), int(y), int(z)), None)

    def get(self, x: int, y: int, z: int) -> Optional[int]:
        return self.cells.get((int(x), int(y), int(z)))

    def fill_box(
        self,
        x0: int,
        y0: int,
        z0: int,
        x1: int,
        y1: int,
        z1: int,
        material: int = 0,
    ) -> None:
        xa, xb = sorted((int(x0), int(x1)))
        ya, yb = sorted((int(y0), int(y1)))
        za, zb = sorted((int(z0), int(z1)))
        for x in range(xa, xb + 1):
            for y in range(ya, yb + 1):
                for z in range(za, zb + 1):
                    self.cells[(x, y, z)] = material

    def carve_box(self, x0: int, y0: int, z0: int, x1: int, y1: int, z1: int) -> None:
        xa, xb = sorted((int(x0), int(x1)))
        ya, yb = sorted((int(y0), int(y1)))
        za, zb = sorted((int(z0), int(z1)))
        for x in range(xa, xb + 1):
            for y in range(ya, yb + 1):
                for z in range(za, zb + 1):
                    self.cells.pop((x, y, z), None)

    def fill_ellipse_x(self, x: int, cy: int, cz: int, ry: int, rz: int, material: int = 0) -> None:
        ry = max(0, int(ry))
        rz = max(0, int(rz))
        if ry == 0 and rz == 0:
            self.set(x, cy, cz, material)
            return
        for y in range(cy - ry, cy + ry + 1):
            for z in range(cz - rz, cz + rz + 1):
                dy = 0.0 if ry == 0 else (y - cy) / float(max(ry, 1))
                dz = 0.0 if rz == 0 else (z - cz) / float(max(rz, 1))
                if dy * dy + dz * dz <= 1.05:
                    self.set(x, y, z, material)

    def fill_ellipse_y(self, y: int, cx: int, cz: int, rx: int, rz: int, material: int = 0) -> None:
        rx = max(0, int(rx))
        rz = max(0, int(rz))
        if rx == 0 and rz == 0:
            self.set(cx, y, cz, material)
            return
        for x in range(cx - rx, cx + rx + 1):
            for z in range(cz - rz, cz + rz + 1):
                dx = 0.0 if rx == 0 else (x - cx) / float(max(rx, 1))
                dz = 0.0 if rz == 0 else (z - cz) / float(max(rz, 1))
                if dx * dx + dz * dz <= 1.05:
                    self.set(x, y, z, material)

    def fill_ellipse_z(self, z: int, cx: int, cy: int, rx: int, ry: int, material: int = 0) -> None:
        rx = max(0, int(rx))
        ry = max(0, int(ry))
        if rx == 0 and ry == 0:
            self.set(cx, cy, z, material)
            return
        for x in range(cx - rx, cx + rx + 1):
            for y in range(cy - ry, cy + ry + 1):
                dx = 0.0 if rx == 0 else (x - cx) / float(max(rx, 1))
                dy = 0.0 if ry == 0 else (y - cy) / float(max(ry, 1))
                if dx * dx + dy * dy <= 1.05:
                    self.set(x, y, z, material)

    def fill_sphere(
        self,
        cx: int,
        cy: int,
        cz: int,
        rx: int,
        ry: int,
        rz: int,
        material: int = 0,
    ) -> None:
        rx = max(0, int(rx))
        ry = max(0, int(ry))
        rz = max(0, int(rz))
        for x in range(cx - rx, cx + rx + 1):
            for y in range(cy - ry, cy + ry + 1):
                for z in range(cz - rz, cz + rz + 1):
                    dx = 0.0 if rx == 0 else (x - cx) / float(max(rx, 1))
                    dy = 0.0 if ry == 0 else (y - cy) / float(max(ry, 1))
                    dz = 0.0 if rz == 0 else (z - cz) / float(max(rz, 1))
                    if dx * dx + dy * dy + dz * dz <= 1.10:
                        self.set(x, y, z, material)

    def fill_segment(
        self,
        start: Tuple[float, float, float],
        end: Tuple[float, float, float],
        radius: int,
        material: int = 0,
    ) -> None:
        dx = end[0] - start[0]
        dy = end[1] - start[1]
        dz = end[2] - start[2]
        steps = max(int(abs(dx)), int(abs(dy)), int(abs(dz)), 1) * 2
        for step in range(steps + 1):
            t = step / float(steps)
            px = lerp(start[0], end[0], t)
            py = lerp(start[1], end[1], t)
            pz = lerp(start[2], end[2], t)
            self.fill_sphere(int(round(px)), int(round(py)), int(round(pz)), radius, radius, radius, material)

    def fill_polyline(
        self,
        points: Sequence[Tuple[float, float, float]],
        radius: int,
        material: int = 0,
    ) -> None:
        if len(points) < 2:
            return
        for index in range(len(points) - 1):
            self.fill_segment(points[index], points[index + 1], radius, material)

    def paint_filter(self, predicate, material: int) -> None:
        for pos in list(self.cells.keys()):
            if predicate(*pos):
                self.cells[pos] = material

    def stamp(
        self,
        other: "VoxelGrid",
        offset: Tuple[int, int, int] = (0, 0, 0),
        overwrite: bool = True,
    ) -> None:
        ox, oy, oz = offset
        for (x, y, z), material in other.cells.items():
            key = (x + ox, y + oy, z + oz)
            if overwrite or key not in self.cells:
                self.cells[key] = material

    def translated(self, offset: Tuple[int, int, int]) -> "VoxelGrid":
        clone = VoxelGrid(self.voxel_size)
        clone.stamp(self, offset)
        return clone


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def lerp_int(a: float, b: float, t: float) -> int:
    return int(round(lerp(a, b, t)))


def interpolate_knots(position: float, knots: Sequence[Sequence[float]]) -> Tuple[int, ...]:
    if position <= knots[0][0]:
        return tuple(int(round(value)) for value in knots[0][1:])
    if position >= knots[-1][0]:
        return tuple(int(round(value)) for value in knots[-1][1:])
    for index in range(len(knots) - 1):
        a = knots[index]
        b = knots[index + 1]
        if a[0] <= position <= b[0]:
            span = b[0] - a[0]
            t = 0.0 if span == 0 else (position - a[0]) / span
            return tuple(lerp_int(a[item], b[item], t) for item in range(1, len(a)))
    return tuple(int(round(value)) for value in knots[-1][1:])


def ensure_scene(clear_scene: bool = True) -> None:
    if bpy.context.active_object and bpy.context.active_object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")

    if clear_scene:
        bpy.ops.object.select_all(action="SELECT")
        bpy.ops.object.delete(use_global=False)

        for mesh in list(bpy.data.meshes):
            if mesh.users == 0:
                bpy.data.meshes.remove(mesh)
        for material in list(bpy.data.materials):
            if material.users == 0:
                bpy.data.materials.remove(material)
        for collection in list(bpy.data.collections):
            if collection.users == 0:
                bpy.data.collections.remove(collection)

    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 0.01
    scene.unit_settings.length_unit = "CENTIMETERS"

    for area in bpy.context.screen.areas:
        if area.type == "VIEW_3D":
            for space in area.spaces:
                if space.type == "VIEW_3D":
                    space.clip_start = 0.1
                    space.clip_end = 100000
                    space.shading.type = "MATERIAL"


def ensure_collection(name: str, parent: Optional[bpy.types.Collection] = None) -> bpy.types.Collection:
    collection = bpy.data.collections.get(name)
    if collection is None:
        collection = bpy.data.collections.new(name)

    if parent is None:
        root = bpy.context.scene.collection
        if collection.name not in root.children:
            try:
                root.children.link(collection)
            except RuntimeError:
                pass
        return collection

    if collection.name not in parent.children:
        try:
            parent.children.link(collection)
        except RuntimeError:
            pass
    return collection


def ensure_material(name: str, definition: MaterialDef) -> bpy.types.Material:
    material = bpy.data.materials.get(name)
    if material is None:
        material = bpy.data.materials.new(name)

    material.use_nodes = True
    bsdf = material.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*definition.color, 1.0)
        bsdf.inputs["Metallic"].default_value = definition.metallic
        bsdf.inputs["Roughness"].default_value = definition.roughness
        if definition.emission > 0.0:
            try:
                bsdf.inputs["Emission Color"].default_value = (*definition.color, 1.0)
                bsdf.inputs["Emission Strength"].default_value = definition.emission
            except KeyError:
                pass
    return material


def ensure_palette(prefix: str, palette: Sequence[MaterialDef]) -> List[bpy.types.Material]:
    return [ensure_material(f"{prefix}_{entry.label}", entry) for entry in palette]


def build_mesh_layers(grids: Sequence[VoxelGrid]) -> Tuple[List[Tuple[float, float, float]], List[Tuple[int, int, int, int]], List[int]]:
    cache: Dict[Tuple[float, float, float], int] = {}
    verts: List[Tuple[float, float, float]] = []
    faces: List[Tuple[int, int, int, int]] = []
    face_materials: List[int] = []

    def vertex_id(wx: float, wy: float, wz: float) -> int:
        key = (round(wx, 4), round(wy, 4), round(wz, 4))
        if key not in cache:
            cache[key] = len(verts)
            verts.append(key)
        return cache[key]

    for grid in grids:
        if not grid.cells:
            continue
        half = grid.voxel_size / 2.0
        for (vx, vy, vz), material in grid.cells.items():
            for (dx, dy, dz), corners in FACE_DEFS:
                if (vx + dx, vy + dy, vz + dz) in grid.cells:
                    continue
                face = []
                for cx, cy, cz in corners:
                    wx = (vx + cx) * grid.voxel_size - half
                    wy = (vy + cy) * grid.voxel_size - half
                    wz = (vz + cz) * grid.voxel_size
                    face.append(vertex_id(wx, wy, wz))
                faces.append(tuple(face))
                face_materials.append(material)

    return verts, faces, face_materials


def spawn_asset(
    name: str,
    grids: Sequence[VoxelGrid],
    materials: Sequence[bpy.types.Material],
    collection: bpy.types.Collection,
    location: Tuple[float, float, float] = (0.0, 0.0, 0.0),
) -> bpy.types.Object:
    verts, faces, face_materials = build_mesh_layers(grids)
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.validate()
    mesh.update(calc_edges=True)

    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    for material in materials:
        obj.data.materials.append(material)
    for index, poly in enumerate(mesh.polygons):
        poly.material_index = face_materials[index]
    obj.location = location

    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.shade_flat()
    obj.select_set(False)
    return obj


def focus_collection(collection: bpy.types.Collection) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    for obj in collection.all_objects:
        obj.select_set(True)

    if collection.all_objects:
        bpy.context.view_layer.objects.active = collection.all_objects[0]

    for area in bpy.context.screen.areas:
        if area.type == "VIEW_3D":
            with bpy.context.temp_override(area=area, region=area.regions[-1]):
                bpy.ops.view3d.view_selected()
            break


def grid_origin(index: int, columns: int = 3, step_x: float = 2400.0, step_y: float = 2200.0) -> Tuple[float, float, float]:
    row = index // columns
    column = index % columns
    return (column * step_x, row * step_y, 0.0)

