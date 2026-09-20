#!/usr/bin/env python3
"""Catalogue renders of the RUTA series: Blender Cycles, run headless.

    blender -b -P render_catalogue.py -- <job dir> <out dir> [samples] [shot ...]

The job dir holds render/*.stl from serie.scad (r_ruta, r_krok, r_fast,
r_glass, r_aa, stand) and screen.png, the dashboard the panel shows. Each
product is one PLA body, the glass, and the active area with the screen
image on it, on a white wall or a white desk, lit like a product shot: one
large soft key from the upper left, a fill, and a white world. Colours are
the brand's bone and ink; nothing is glossy but the glass.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix

ARGS = sys.argv[sys.argv.index("--") + 1:]
JOB = ARGS[0]
OUT = ARGS[1]
SAMPLES = int(ARGS[2]) if len(ARGS) > 2 else 256
ONLY = set(ARGS[3:])          # empty: every shot
MM = 0.001

# serie.scad, derived values (echoed by the model)
DEPTH = 22.3
OUTER_W, OUTER_H = 187.76, 122.48
SHELL_DY = 3.24
STAND_D, STAND_H, STAND_SLOT, STAND_TILT = 42, 18, 12, 12

BONE = (0.86, 0.83, 0.77, 1)
INK = (0.022, 0.02, 0.019, 1)
GLASS = (0.02, 0.02, 0.025, 1)


def clean():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = SAMPLES
    sc.cycles.use_denoising = True
    sc.render.resolution_x, sc.render.resolution_y = 1600, 1200
    sc.render.film_transparent = False
    sc.view_settings.view_transform = "AgX"
    sc.render.image_settings.file_format = "PNG"
    world = bpy.data.worlds.new("w")
    sc.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (1, 1, 1, 1)
    bg.inputs[1].default_value = 0.12


def matte(name, rgb, rough=0.55):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = m.node_tree.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = rgb
    p.inputs["Roughness"].default_value = rough
    p.inputs["Specular IOR Level"].default_value = 0.35
    return m


def screen_material(path):
    m = bpy.data.materials.new("screen")
    m.use_nodes = True
    nt = m.node_tree
    p = nt.nodes["Principled BSDF"]
    p.inputs["Roughness"].default_value = 0.9
    p.inputs["Specular IOR Level"].default_value = 0.15
    img = nt.nodes.new("ShaderNodeTexImage")
    img.image = bpy.data.images.load(path)
    # Generated coordinates: the bounding box of the untilted plate, so the
    # 800x480 shot lands on the 163 x 98 mm active area edge to edge.
    coord = nt.nodes.new("ShaderNodeTexCoord")
    # UPRIGHT turns the unit 180 deg about Z, which runs the bounding box's
    # x the other way: mirror U back so the clock sits top left again.
    flip = nt.nodes.new("ShaderNodeMapping")
    flip.inputs["Location"].default_value = (1, 0, 0)
    flip.inputs["Scale"].default_value = (-1, 1, 1)
    nt.links.new(coord.outputs["Generated"], flip.inputs["Vector"])
    nt.links.new(flip.outputs["Vector"], img.inputs["Vector"])
    nt.links.new(img.outputs["Color"], p.inputs["Base Color"])
    return m


def load(name, mat, matrix):
    bpy.ops.wm.stl_import(filepath=os.path.join(JOB, "render", name + ".stl"))
    ob = bpy.context.selected_objects[0]
    ob.name = name
    ob.matrix_world = matrix
    ob.data.materials.append(mat)
    for poly in ob.data.polygons:
        poly.use_smooth = False
    return ob


# SCAD frame coordinates -> a unit whose front faces -Y, glass upright.
# rotate([90,0,180]) in OpenSCAD is Rz(180) after Rx(90).
UPRIGHT = Matrix.Rotation(math.radians(180), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X")
# serie.scad in_stand(): the frame's bottom edge in the plinth's slot, leaning back
IN_STAND = (Matrix.Translation((0, STAND_D / 2, STAND_H - STAND_SLOT))
            @ Matrix.Rotation(math.radians(-STAND_TILT), 4, "X")
            @ Matrix.Translation((0, -DEPTH / 2, -(SHELL_DY - OUTER_H / 2) + 1))
            @ UPRIGHT)


def unit(kind, body_mat, screen_mat, glass_mat, place, at=(0, 0, 0)):
    """One product: PLA body, glass, active area. `place` is the SCAD->scene
    matrix in mm; `at` shifts the unit in metres."""
    world = Matrix.Translation(at) @ Matrix.Scale(MM, 4) @ place
    parts = [load("r_" + kind, body_mat, world),
             load("r_glass", glass_mat, world),
             load("r_aa", screen_mat, world)]
    return parts


def wall(y, mat):
    bpy.ops.mesh.primitive_plane_add(size=6, location=(0, y, 0), rotation=(math.radians(90), 0, 0))
    ob = bpy.context.active_object
    ob.data.materials.append(mat)
    return ob


def floor(z, mat):
    bpy.ops.mesh.primitive_plane_add(size=6, location=(0, 0.6, z))
    ob = bpy.context.active_object
    ob.data.materials.append(mat)
    return ob


def lights():
    def area(loc, energy, size, target=(0, 0, 0)):
        bpy.ops.object.light_add(type="AREA", location=loc)
        l = bpy.context.active_object
        l.data.energy = energy
        l.data.size = size
        d = (target[0] - loc[0], target[1] - loc[1], target[2] - loc[2])
        l.rotation_euler = direction_to_euler(d)
    area((-0.9, -1.1, 0.9), 110, 1.6)      # key, upper left
    area((1.1, -0.9, 0.2), 30, 1.2)        # fill, right
    area((0.2, -0.3, 1.4), 25, 1.0)        # top


def direction_to_euler(d):
    from mathutils import Vector
    return Vector(d).to_track_quat("-Z", "Y").to_euler()


def camera(loc, target=(0, 0, 0), lens=55):
    bpy.ops.object.camera_add(location=loc)
    cam = bpy.context.active_object
    cam.data.lens = lens
    d = (target[0] - loc[0], target[1] - loc[1], target[2] - loc[2])
    cam.rotation_euler = direction_to_euler(d)
    bpy.context.scene.camera = cam


def render(name):
    os.makedirs(OUT, exist_ok=True)
    bpy.context.scene.render.filepath = os.path.join(OUT, name + ".png")
    bpy.ops.render.render(write_still=True)
    print("rendered", name)


def shot_wall_unit(kind, name, body_rgb=BONE, from_back=False):
    clean()
    body = matte("pla", body_rgb)
    screen = screen_material(os.path.join(JOB, "screen.png"))
    glass = matte("glass", GLASS, rough=0.25)
    wall(DEPTH * MM + 0.001, matte("wall", (0.80, 0.80, 0.79, 1), 0.8))
    unit(kind, body, screen, glass, UPRIGHT)
    lights()
    if from_back:
        # lifted off the wall a hand's width, seen from behind and above
        for ob in list(bpy.data.objects):
            if ob.name.startswith("r_"):
                ob.matrix_world = Matrix.Translation((0, -0.10, 0)) @ Matrix.Rotation(math.radians(168), 4, "Z") @ ob.matrix_world
        camera((0.20, -0.50, 0.24), (0.0, -0.10, -0.01), 50)
    else:
        camera((-0.30, -0.42, 0.14), (0, 0, -0.005), 55)
    render(name)


def shot_stand(name):
    clean()
    body = matte("pla", BONE)
    screen = screen_material(os.path.join(JOB, "screen.png"))
    glass = matte("glass", GLASS, rough=0.25)
    desk = matte("desk", (0.82, 0.81, 0.79, 1), 0.7)
    floor(0, desk)
    wall(0.35, desk)
    world = Matrix.Scale(MM, 4)
    load("stand", body, world)
    unit("ruta", body, screen, glass, IN_STAND)
    lights()
    camera((-0.30, -0.40, 0.22), (0, 0.02, 0.06), 55)
    render(name)


def shot_family(name):
    clean()
    body = matte("pla", BONE)
    ink = matte("ink", INK)
    screen = screen_material(os.path.join(JOB, "screen.png"))
    glass = matte("glass", GLASS, rough=0.25)
    wall(DEPTH * MM + 0.001, matte("wall", (0.80, 0.80, 0.79, 1), 0.8))
    unit("ruta", body, screen, glass, UPRIGHT, at=(-0.22, 0, 0.0))
    unit("krok", ink, screen, glass, UPRIGHT, at=(0.0, 0, 0.0))
    unit("fast", body, screen, glass, UPRIGHT, at=(0.22, 0, 0.0))
    lights()
    camera((0.0, -0.95, 0.12), (0, 0, 0), 55)
    render(name)


SHOTS = {
    "ruta": lambda: shot_wall_unit("ruta", "ruta"),
    "ruta_ink": lambda: shot_wall_unit("ruta", "ruta_ink", body_rgb=INK),
    "krok_back": lambda: shot_wall_unit("krok", "krok_back", from_back=True),
    "fast_back": lambda: shot_wall_unit("fast", "fast_back", from_back=True),
    "stod": lambda: shot_stand("stod"),
    "family": lambda: shot_family("family"),
}
for name, shot in SHOTS.items():
    if not ONLY or name in ONLY:
        shot()
