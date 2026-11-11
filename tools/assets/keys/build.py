# Builds the two placeholder key meshes (#613). Run:
#   blender --background --factory-startup --python tools/assets/keys/build.py -- <outdir>
# House key: brass, round bow, toothed blade. Vehicle key: black remote fob with a red button
# and a steel blade. Sizes are real (metres): house key 6.5 cm, fob 8 cm. Y is the long axis.
import bpy, sys, math, os

out = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)

def mat(name, rgb):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (rgb[0], rgb[1], rgb[2], 1)
    return m

def box(name, sx, sy, sz, loc, m):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o = bpy.context.object
    o.name = name
    o.scale = (sx, sy, sz)
    bpy.ops.object.transform_apply(scale=True)
    o.data.materials.append(m)
    return o

def cyl(name, r, depth, loc, m, verts=24, r2=None):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=depth, location=loc,
                                        rotation=(math.pi / 2, 0, 0))
    o = bpy.context.object
    o.name = name
    o.data.materials.append(m)
    return o

def ring(name, ro, ri, depth, loc, m):
    o = cyl(name, ro, depth, loc, m, 32)
    c = cyl(name + "_h", ri, depth * 3, loc, m, 32)
    mod = o.modifiers.new("b", "BOOLEAN")
    mod.operation = "DIFFERENCE"
    mod.object = c
    mod.solver = "EXACT"
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.modifier_apply(modifier="b")
    bpy.data.objects.remove(c, do_unlink=True)
    return o

def finish(mesh_name, parts, col, path):
    bpy.ops.object.select_all(action="DESELECT")
    for p in parts:
        p.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    o = bpy.context.object
    o.name = mesh_name
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    sx, sy, sz, cx, cy, cz = col
    bpy.ops.mesh.primitive_cube_add(size=1, location=(cx, cy, cz))
    c = bpy.context.object
    c.name = "UBX_" + mesh_name
    c.scale = (sx, sy, sz)
    bpy.ops.object.transform_apply(scale=True)
    c.display_type = "WIRE"
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True,
                             global_scale=1.0, axis_forward="-Z", axis_up="Y",
                             object_types={"MESH"}, mesh_smooth_type="FACE",
                             bake_space_transform=True)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()

# House key: lies along Y, bow at -Y end, blade toward +Y. Thickness 2.5 mm.
brass = mat("KeyBrass", (0.78, 0.55, 0.16))
parts = [ring("bow", 0.0125, 0.0055, 0.0025, (0, -0.0275, 0), brass),
         box("shaft", 0.006, 0.040, 0.0025, (0, 0.002, 0), brass),
         box("t1", 0.0048, 0.004, 0.0025, (0.0054, 0.010, 0), brass),
         box("t2", 0.0035, 0.004, 0.0025, (0.0047, 0.0185, 0), brass),
         box("t3", 0.0055, 0.004, 0.0025, (0.0057, 0.027, 0), brass)]
finish("HouseKey", parts, (0.020, 0.065, 0.004, 0, -0.0, 0), os.path.join(out, "HouseKey.fbx"))

# Vehicle key: black fob with red button, steel blade folded out the +Y end.
black = mat("FobBlack", (0.025, 0.025, 0.03))
steel = mat("FobSteel", (0.75, 0.77, 0.80))
red = mat("FobRed", (0.75, 0.05, 0.05))
parts = [box("fob", 0.032, 0.048, 0.011, (0, -0.012, 0), black),
         ring("eye", 0.007, 0.0035, 0.004, (0, -0.0415, 0), black),
         box("blade", 0.008, 0.034, 0.0022, (0, 0.029, 0), steel),
         box("btn", 0.012, 0.012, 0.004, (0, -0.005, 0.0125), red),
         box("btn2", 0.012, 0.012, 0.004, (0, -0.022, 0.0125), red)]
finish("VehicleKey", parts, (0.032, 0.085, 0.012, 0, -0.005, 0), os.path.join(out, "VehicleKey.fbx"))
