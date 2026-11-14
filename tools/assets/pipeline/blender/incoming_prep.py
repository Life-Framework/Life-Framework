# Turn a model that was downloaded or bought (glTF, GLB, FBX, OBJ or .blend) into what the Meshy generate stage leaves
# for the clean stage, like polyhaven_prep.py does for Poly Haven:
#   <raw>/model.glb  <raw>/tex_base_color.png  <raw>/tex_normal.png  <raw>/tex_roughness.png  <raw>/tex_metallic.png
#   <raw>/prep.json  triangles, native size in metres (x, y, z), materials, the maps found
# The maps are found by following the Principled BSDF inputs back to their image node, whatever the file is called.
# A roughness or metallic read through a Separate node takes that channel (G and B of an ORM map).
# job.json = { model, raw }.  Run by source.mjs: blender --background --factory-startup --python incoming_prep.py -- <job.json>
import bpy, sys, json, os
import numpy as np

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
raw = job["raw"]
os.makedirs(raw, exist_ok=True)
path = job["model"]
ext = os.path.splitext(path)[1].lower()

bpy.ops.wm.read_factory_settings(use_empty=True)
if ext in (".glb", ".gltf"):
    bpy.ops.import_scene.gltf(filepath=path)
elif ext == ".fbx":
    bpy.ops.import_scene.fbx(filepath=path)
elif ext == ".obj":
    bpy.ops.wm.obj_import(filepath=path)
elif ext == ".blend":
    bpy.ops.wm.open_mainfile(filepath=path)
else:
    raise SystemExit("unsupported model format " + ext)

meshes = [o for o in bpy.data.objects if o.type == "MESH"]
if not meshes:
    raise SystemExit("the file holds no mesh")

tris = 0
lo = np.array([1e9] * 3)
hi = np.array([-1e9] * 3)
mats = set()
for o in meshes:
    me = o.data
    me.calc_loop_triangles()
    tris += len(me.loop_triangles)
    for m in me.materials:
        if m:
            mats.add(m)
    for c in o.bound_box:
        w = np.array(o.matrix_world @ __import__("mathutils").Vector(c))
        lo, hi = np.minimum(lo, w), np.maximum(hi, w)


def source_image(sock, depth=0):
    """Follow a socket back to an image node. Returns (image, channel or None)."""
    if depth > 6 or not sock.is_linked:
        return None, None
    node = sock.links[0].from_node
    out = sock.links[0].from_socket.name
    if node.type == "TEX_IMAGE":
        return node.image, None
    for inp in node.inputs:
        img, ch = source_image(inp, depth + 1)
        if img:
            if node.type in ("SEPARATE_COLOR", "SEPARATE_RGB"):
                ch = {"Red": 0, "Green": 1, "Blue": 2, "R": 0, "G": 1, "B": 2}.get(out)
            return img, ch
    return None, None


def principled(m):
    if not m.use_nodes:
        return None
    return next((n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)


maps = {}
bases = set()
for m in mats:
    p = principled(m)
    if not p:
        continue
    for key, name in (("base", "Base Color"), ("normal", "Normal"), ("rough", "Roughness"), ("metal", "Metallic")):
        img, ch = source_image(p.inputs[name])
        if img and key == "base":
            bases.add(img.name)
        if img and key not in maps:
            maps[key] = (img, ch)
if len(bases) > 1:
    raise SystemExit("%d base colour textures (%s): the clean stage packs one atlas; merge them in Blender first" % (len(bases), ", ".join(sorted(bases))))


def pixels(img):
    w, h = img.size
    arr = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(arr)
    return arr.reshape(h, w, 4), w, h


def save(arr, w, h, name, noncolor):
    img = bpy.data.images.new(name, w, h, alpha=False, float_buffer=False)
    if noncolor:
        img.colorspace_settings.name = "Non-Color"
    img.pixels.foreach_set(arr.reshape(-1))
    img.filepath_raw = os.path.join(raw, name)
    img.file_format = "PNG"
    img.save()


found = {}
for key, out, noncolor in (("base", "tex_base_color.png", False), ("normal", "tex_normal.png", True), ("rough", "tex_roughness.png", True), ("metal", "tex_metallic.png", True)):
    if key not in maps:
        continue
    img, ch = maps[key]
    a, w, h = pixels(img)
    if ch is not None:
        g = np.repeat(a[:, :, ch:ch + 1], 3, axis=2)
        a = np.dstack([g, np.ones((h, w), np.float32)])
    a[:, :, 3] = 1.0
    save(a, w, h, out, noncolor)
    found[key] = img.name
if "base" not in found:
    # No base colour texture: a flat colour from the first material, so the clean stage still has an atlas to pack.
    col = (0.5, 0.5, 0.5, 1.0)
    for m in mats:
        p = principled(m)
        if p:
            col = tuple(p.inputs["Base Color"].default_value)
            break
    save(np.tile(np.array(col, np.float32), (4, 4, 1)), 4, 4, "tex_base_color.png", False)
    found["base"] = "flat colour"

try:
    bpy.ops.export_scene.gltf(filepath=os.path.join(raw, "model.glb"), export_format="GLB", export_image_format="NONE", export_yup=True)
except Exception:
    bpy.ops.export_scene.gltf(filepath=os.path.join(raw, "model.glb"), export_format="GLB", export_yup=True)

unit = bpy.context.scene.unit_settings.scale_length or 1.0
size = [float(v) * unit for v in (hi - lo)]
json.dump({"tris": tris, "size_m": size, "maps": found, "meshes": len(meshes), "materials": len(mats)}, open(os.path.join(raw, "prep.json"), "w"))
print("incoming_prep: %d triangles, %s" % (tris, found))
