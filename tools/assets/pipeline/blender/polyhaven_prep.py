# Turn a downloaded Poly Haven glTF into what the Meshy generate stage leaves for the clean stage:
#   <raw>/model.glb          the mesh and its UVs (no embedded images)
#   <raw>/tex_base_color.png from <name>_diff_*      <raw>/tex_normal.png from <name>_nor_gl_* (OpenGL, as Meshy's)
#   <raw>/tex_roughness.png  from the G channel of <name>_arm_*     <raw>/tex_metallic.png from its B channel
#   <raw>/prep.json          triangles, native size in metres (x, y, z) and the maps found
# Poly Haven packs ambient occlusion, roughness and metallic into one "arm" map (R, G, B).
# Run by tools/assets/pipeline/source.mjs:  blender --background --factory-startup --python polyhaven_prep.py -- <job.json>
# job.json = { gltf, textures, raw }
import bpy, sys, json, os, glob
import numpy as np

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
raw = job["raw"]
os.makedirs(raw, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=job["gltf"])
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
if not meshes:
    raise SystemExit("the glTF holds no mesh")

tris = 0
lo = np.array([1e9] * 3)
hi = np.array([-1e9] * 3)
for o in meshes:
    me = o.data
    me.calc_loop_triangles()
    tris += len(me.loop_triangles)
    for c in o.bound_box:
        w = np.array(o.matrix_world @ __import__("mathutils").Vector(c))
        lo, hi = np.minimum(lo, w), np.maximum(hi, w)

try:
    bpy.ops.export_scene.gltf(filepath=os.path.join(raw, "model.glb"), export_format="GLB", export_image_format="NONE", export_yup=True)
except Exception:
    bpy.ops.export_scene.gltf(filepath=os.path.join(raw, "model.glb"), export_format="GLB", export_yup=True)


def find(tag):
    hits = sorted(glob.glob(os.path.join(job["textures"], "*_%s_*" % tag)))
    return hits[0] if hits else None


def load(path):
    img = bpy.data.images.load(path)
    w, h = img.size
    arr = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(arr)
    return arr.reshape(h, w, 4), w, h


def save(arr, w, h, name):
    img = bpy.data.images.new(name, w, h, alpha=False, float_buffer=False)
    img.colorspace_settings.name = "Non-Color"
    img.pixels.foreach_set(arr.reshape(-1))
    img.filepath_raw = os.path.join(raw, name)
    img.file_format = "PNG"
    img.save()


found = {}
for tag, out in (("diff", "tex_base_color.png"), ("nor_gl", "tex_normal.png")):
    p = find(tag)
    if p:
        a, w, h = load(p)
        a[:, :, 3] = 1.0
        save(a, w, h, out)
        found[tag] = os.path.basename(p)
p = find("arm")
if p:
    a, w, h = load(p)
    for ch, out in ((1, "tex_roughness.png"), (2, "tex_metallic.png")):
        g = np.repeat(a[:, :, ch:ch + 1], 3, axis=2)
        save(np.dstack([g, np.ones((h, w), np.float32)]), w, h, out)
    found["arm"] = os.path.basename(p)
if "diff" not in found:
    raise SystemExit("no *_diff_* base colour map in %s" % job["textures"])

json.dump({"tris": tris, "size_m": [float(v) for v in (hi - lo)], "maps": found, "meshes": len(meshes)}, open(os.path.join(raw, "prep.json"), "w"))
print("polyhaven_prep: %d triangles, %s" % (tris, found))
