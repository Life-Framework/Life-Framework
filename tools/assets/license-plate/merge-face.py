# Rebuilds Assets/Vehicles/LicensePlate/LicensePlate.fbx as ONE mesh object: the plate box (material "Material")
# with its front window face (material "PIPMaterial", the render-target slot). The original FBX kept the window as a
# second object ("Cube.001") and Workbench's bake drew the whole front with the box material (#3039).
# Run: blender -b -P merge-face.py -- <in.fbx> <out.fbx>   (Blender 4.x; the FBX round-trips with the default axes)
import bpy, sys

src, dst = sys.argv[-2], sys.argv[-1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
box = bpy.data.objects["Cube"]
face = bpy.data.objects["Cube.001"]
for o in bpy.data.objects:
    o.select_set(o in (box, face))
bpy.context.view_layer.objects.active = box
bpy.ops.object.join()
box = bpy.context.active_object
box.name = "Cube"
print("PLATE_MESH polys", len(box.data.polygons), "mats", [s.material.name for s in box.material_slots])
bpy.ops.export_scene.fbx(filepath=dst, object_types={"MESH"}, use_selection=False, mesh_smooth_type="FACE", add_leaf_bones=False)
