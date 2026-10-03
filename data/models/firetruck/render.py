#!/usr/bin/env python3
"""Run with Blender --background --python data/models/firetruck/render.py."""
import math
from pathlib import Path
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'data/images/tiles'
SOURCE = Path(__file__).resolve().parent
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)

def material(name, color, metal=0.0, roughness=0.45):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    m.node_tree.nodes.clear()
    bsdf = m.node_tree.nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.name = 'Paint shader'
    output = m.node_tree.nodes.new('ShaderNodeOutputMaterial')
    m.node_tree.links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    bsdf.inputs['Metallic'].default_value = metal
    bsdf.inputs['Roughness'].default_value = roughness
    return m

red = material('Paint - fire red', (0.66, 0.023, 0.025), 0.12)
cream = material('Reflective ivory', (0.92, 0.89, 0.71))
silver = material('Brushed aluminium', (0.45, 0.51, 0.54), 0.55)
dark = material('Graphite chassis', (0.038, 0.047, 0.054))
rubber = material('Rubber', (0.016, 0.021, 0.026), 0, 0.8)
glass = material('Blue grey glazing', (0.032, 0.12, 0.18), 0.25, 0.23)
hose = material('Rolled hose', (0.56, 0.43, 0.21), 0, 0.9)
lamp = material('Headlights', (0.95, 0.90, 0.65))
beacon_red = material('Red emergency lens', (0.72, 0.015, 0.01))
beacon_blue = material('Blue emergency lens', (0.008, 0.12, 0.8))
for beacon in (beacon_red, beacon_blue):
    beacon.cycles.emission_sampling = 'NONE'
    nodes = beacon.node_tree.nodes
    ray = nodes.new('ShaderNodeLightPath')
    strength = nodes.new('ShaderNodeMath')
    strength.name = 'Camera-only flash'
    strength.operation = 'MULTIPLY'
    beacon.node_tree.links.new(ray.outputs['Is Camera Ray'], strength.inputs[0])
    beacon.node_tree.links.new(strength.outputs[0], nodes['Paint shader'].inputs['Emission Strength'])

pivot = bpy.data.objects.new('Truck ground pivot', None)
bpy.context.collection.objects.link(pivot)

def finish(obj, name, mat):
    obj.name = name
    obj.data.materials.append(mat)
    obj.parent = pivot
    return obj

def box(name, pos, size, mat, bevel=0.025):
    bpy.ops.mesh.primitive_cube_add(size=1, location=pos)
    obj = bpy.context.object
    obj.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    finish(obj, name, mat)
    if bevel:
        mod = obj.modifiers.new('Edge highlights', 'BEVEL')
        mod.width = bevel
        mod.segments = 2
        obj.modifiers.new('Corner normals', 'WEIGHTED_NORMAL')
    return obj

def cylinder(name, pos, radius, depth, mat, axis='Y', vertices=16):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
        depth=depth, location=pos)
    obj = bpy.context.object
    if axis == 'Y':
        obj.rotation_euler.x = math.pi / 2
    elif axis == 'X':
        obj.rotation_euler.y = math.pi / 2
    return finish(obj, name, mat)

box('Chassis', (0, 0, 0.42), (3.95, 1.48, 0.27), dark)
box('Equipment body', (-0.7, 0, 1.04), (2.55, 1.59, 1.12), red, 0.06)
box('Cabin', (1.19, 0, 1.12), (1.15, 1.62, 1.42), red, 0.08)
box('Cab roof', (1.2, 0, 1.87), (1.21, 1.69, 0.11), cream)
box('Windshield', (1.776, 0, 1.47), (0.018, 1.35, 0.5), glass, 0.015)
box('Windshield divider', (1.79, 0, 1.47), (0.025, 0.045, 0.51), dark, 0)
box('Front grille', (1.794, 0, 0.8), (0.035, 0.79, 0.25), silver, 0.01)
for z in (0.74, 0.81, 0.88):
    box('Grille slat', (1.82, 0, z), (0.015, 0.76, 0.025), dark, 0)
box('Front bumper', (1.91, 0, 0.49), (0.22, 1.75, 0.18), silver)
box('Rear bumper', (-2.06, 0, 0.48), (0.17, 1.73, 0.18), silver)
box('Rear panel', (-1.985, 0, 1.08), (0.024, 1.32, 0.72), silver)
for z in (0.85, 1.03, 1.21, 1.39):
    box('Rear shutter slat', (-2.005, 0, z), (0.025, 1.27, 0.018), dark, 0)
for side in (-1, 1):
    y = side * 0.822
    box('Cab side window', (1.22, y, 1.48), (0.87, 0.025, 0.48), glass)
    box('Cab window divider', (1.03, y * 1.01, 1.48), (0.045, 0.027, 0.50), red, 0)
    box('Reflective cab stripe', (1.2, y * 1.01, 1.04), (1.11, 0.024, 0.105), cream, 0)
    box('Cab door handle', (0.87, y * 1.04, 1.18), (0.19, 0.06, 0.045), silver, 0.01)
    box('Rear reflective stripe', (-0.7, side * 0.803, 0.61), (2.45, 0.024, 0.1), cream, 0)
    for x in (-1.41, -0.53, 0.20):
        width = 0.72 if x < 0 else 0.54
        box('Side equipment shutter', (x, side * 0.808, 1.15), (width, 0.025, 0.78), silver)
        for z in (0.89, 1.06, 1.23, 1.40):
            box('Shutter groove', (x, side * 0.826, z), (width - 0.035, 0.014, 0.017), dark, 0)
    for x in (-1.22, 1.21):
        cylinder('Tyre', (x, side * 0.77, 0.34), 0.35, 0.27, rubber)
        cylinder('Wheel rim', (x, side * 0.917, 0.34), 0.20, 0.025, silver)
        cylinder('Wheel hub', (x, side * 0.936, 0.34), 0.085, 0.03, dark)
    box('Headlamp', (1.80, side * 0.56, 0.80), (0.035, 0.25, 0.17), lamp)
    box('Rear stop lamp', (-2.02, side * 0.58, 0.65), (0.026, 0.16, 0.18), red)
    box('Mirror arm', (1.55, side * 0.96, 1.47), (0.065, 0.28, 0.06), dark)
    box('Mirror', (1.56, side * 1.075, 1.48), (0.13, 0.065, 0.22), silver)

# Roof ladder is part of the same model in all views.
for y in (-0.32, 0.32):
    box('Ladder rail', (-0.62, y, 1.82), (2.62, 0.075, 0.075), silver)
for i in range(9):
    box('Ladder rung', (-1.75 + i * 0.28, 0, 1.82), (0.055, 0.62, 0.055), silver, 0.012)
for x in (-1.5, 0.25):
    box('Ladder roof mount', (x, 0, 1.70), (0.1, 1.3, 0.13), dark)
cylinder('Hose reel', (-0.78, -0.86, 1.08), 0.24, 0.075, hose)
cylinder('Hose reel hub', (-0.78, -0.905, 1.08), 0.065, 0.025, silver)
box('Light bar base', (1.16, 0, 1.995), (0.24, 1.27, 0.07), dark)
box('Red warning light', (1.16, -0.43, 2.085), (0.23, 0.40, 0.15), beacon_red)
box('Blue warning light', (1.16, 0.43, 2.085), (0.23, 0.40, 0.15), beacon_blue)

scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 32
scene.cycles.use_adaptive_sampling = False
scene.cycles.use_denoising = False
scene.render.resolution_x = 320
scene.render.resolution_y = 256
scene.render.resolution_percentage = 100
scene.render.film_transparent = True
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
scene.render.image_settings.color_depth = '8'
scene.render.image_settings.compression = 90
scene.view_settings.view_transform = 'Standard'
scene.world.color = (0.35, 0.35, 0.35)

bpy.ops.object.camera_add(location=(8, -8, math.sqrt(128) * math.tan(math.pi / 6)))
camera = bpy.context.object
camera.name = 'Fixed dimetric camera'
camera.rotation_euler = (-camera.location).to_track_quat('-Z', 'Y').to_euler()
camera.data.type = 'ORTHO'
camera.data.ortho_scale = 7.0
camera.data.shift_y = 0.2
scene.camera = camera

for name, position, energy, size in (
        ('Upper left softbox', (-3, -4, 8), 650, 5),
        ('Soft fill', (4, 3, 5), 280, 4)):
    bpy.ops.object.light_add(type='AREA', location=position)
    light = bpy.context.object
    light.name = name
    light.data.energy = energy
    light.data.shape = 'DISK'
    light.data.size = size
    light.rotation_euler = (-light.location).to_track_quat('-Z', 'Y').to_euler()

SOURCE.mkdir(parents=True, exist_ok=True)
OUT.mkdir(parents=True, exist_ok=True)
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE / 'firetruck.blend'))

for phase in range(2):
    for m, on in ((beacon_red, phase == 0), (beacon_blue, phase == 1)):
        bsdf = m.node_tree.nodes.get('Paint shader')
        bsdf.inputs['Emission Color'].default_value = (*m.diffuse_color[:3], 1)
        m.node_tree.nodes['Camera-only flash'].inputs[1].default_value = 3.5 if on else 0.0
    for pose in range(16):
        direction = pose if pose < 8 else 2 * ((pose - 8) % 4)
        yaw = math.radians(-90 + direction * 45)
        pitch = 0 if pose < 8 else math.radians(-15 if pose < 12 else 15)
        pivot.rotation_euler = (0, pitch, yaw)
        target = OUT / f'firetruck-{phase * 16 + pose:02}.png'
        scene.render.filepath = str(SOURCE / '_supersampled.png')
        bpy.ops.render.render(write_still=True)
        image = bpy.data.images.load(scene.render.filepath, check_existing=False)
        image.scale(80, 64)
        image.filepath_raw = str(target)
        image.save()
        bpy.data.images.remove(image)
        print('EXPORTED', target, flush=True)
(SOURCE / '_supersampled.png').unlink()
