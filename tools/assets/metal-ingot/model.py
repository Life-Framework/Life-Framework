# The metal ingot, modelled and textured in Blender: no generation credits (spec "source": "blender").
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# job.json = { spec, raw, src, px, variants: [{ name, plan }] }. Writes <raw>/model.glb, <raw>/<Variant>/tex_*.png for every
# variant, the first variant again as <raw>/tex_*.png (the clean stage's base maps), and
# <raw>/review_<Variant>.png (rendered on the maps after the material step).
#
# Shape: concept 1 "Classic trapezoid" (tmp/assets/metal-ingot/concepts/01.png, asset-concepts
# batch 1): a cast bar whose sides and ends slope in towards a rough top, with soft cast edges.
import bpy, bmesh, json, math, os, shutil, sys
from mathutils import Vector, noise

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pipeline", "blender"))
import texbake

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
spec, raw, px = job["spec"], job["raw"], job["px"]
size = spec["size_m"]
L, W, H = size["length"], size["width"], size["height"]
TOP = 0.86  # the top face is this share of the bottom's length and width

bpy.ops.wm.read_factory_settings(use_empty=True)
me = bpy.data.meshes.new("MetalIngot")
obj = bpy.data.objects.new("MetalIngot", me)
bpy.context.scene.collection.objects.link(obj)

bm = bmesh.new()
bmesh.ops.create_cube(bm, size=1.0)
bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=7, use_grid_fill=True)
for v in bm.verts:
    t = v.co.z + 0.5  # 0 at the bottom, 1 at the top
    k = 1.0 - (1.0 - TOP) * t
    v.co = Vector((v.co.x * W * k, v.co.y * L * k, t * H))
bm.normal_update()
sharp = [e for e in bm.edges if len(e.link_faces) == 2 and e.calc_face_angle() > 0.6]
bmesh.ops.bevel(bm, geom=sharp, offset=0.0045, segments=3, profile=0.5, affect="EDGES", clamp_overlap=True)
# The open top of the mould: a shallow shrinkage dip and a rough, pitted skin.
for v in bm.verts:
    if v.co.z > H - 0.0015:
        u, w = v.co.x / (W * TOP / 2), v.co.y / (L * TOP / 2)
        dip = 0.0016 * max(0.0, 1 - u * u) * max(0.0, 1 - w * w)
        v.co.z -= dip + 0.0007 * noise.noise(v.co * 160)
    else:
        v.co += v.normal * 0.00025 * noise.noise(v.co * 220)
bmesh.ops.triangulate(bm, faces=bm.faces[:])
bm.to_mesh(me)
bm.free()
for p in me.polygons:
    p.use_smooth = True

bpy.context.view_layer.objects.active = obj
obj.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.uv.smart_project(angle_limit=math.radians(55), island_margin=0.015, area_weight=0.0, scale_to_bounds=False)
bpy.ops.uv.pack_islands(rotate=True, margin=0.012)
bpy.ops.object.mode_set(mode="OBJECT")

sets = texbake.Sets(job["src"])
texbake.cycles(16)


def iron(g):
    col = g.img("Metal046B", "Color", data=False)
    scale = g.noise(55, detail=6)
    rust = g.ramp(g.noise(18, detail=5, w=3.0), 0.64, 0.72)
    edge = g.pointiness(0.52, 0.6)
    base = g.scale_color(col, (0.82, 0.83, 0.86))
    base = g.mix(base, (0.035, 0.037, 0.042), g.ramp(scale, 0.45, 0.65))  # blue-black forge scale
    base = g.mix(base, (0.3, 0.29, 0.28), g.math("MULTIPLY", edge, 0.7))  # bare steel on worn edges
    base = g.mix(base, g.img("Rust009", "Color", data=False), rust)
    rough = g.mix(g.img("Metal046B", "Roughness"), g.img("Rust009", "Roughness"), rust, kind="FLOAT")
    metal = g.math("MULTIPLY", g.img("Metal046B", "Metalness"), g.math("SUBTRACT", 1.0, rust), clamp=True)
    height = g.math("ADD", g.img("Metal046B", "Displacement"), g.math("MULTIPLY", rust, 0.25))
    return {"base": base, "rough": rough, "metal": metal, "height": height, "bump_strength": 0.6, "bump_distance": 0.0015}


def copper(g):
    col = g.img("Metal047B", "Color", data=False)
    patina = g.math("MAXIMUM", g.ramp(g.noise(22, detail=6, w=1.0), 0.7, 0.76), g.ramp(g.math("SUBTRACT", 1.0, g.ao(0.012)), 0.3, 0.6))
    warm = g.ramp(g.noise(9, detail=3, w=7.0), 0.42, 0.68)
    base = g.scale_color(col, (1.45, 1.0, 0.82))  # bright copper under the oxide
    base = g.mix(base, g.scale_color(col, (0.8, 0.55, 0.45)), g.math("MULTIPLY", warm, 0.7))  # warm brown oxide bands
    base = g.mix(base, g.img("Metal058C", "Color", data=False), g.math("MULTIPLY", patina, 0.8))
    rough = g.mix(g.img("Metal047B", "Roughness"), g.img("Metal058C", "Roughness"), patina, kind="FLOAT")
    metal = g.math("MULTIPLY", g.img("Metal047B", "Metalness"), g.math("SUBTRACT", 1.0, patina), clamp=True)
    height = g.math("ADD", g.img("Metal047B", "Displacement"), g.math("MULTIPLY", patina, 0.2))
    return {"base": base, "rough": rough, "metal": metal, "height": height, "bump_strength": 0.5, "bump_distance": 0.0012}


def gold(g):
    col = g.img("Metal048B", "Color", data=False)
    grime = g.math("MULTIPLY", g.ramp(g.math("SUBTRACT", 1.0, g.ao(0.01)), 0.2, 0.7), 0.6)
    base = g.scale_color(col, (1.0, 0.72, 0.36))  # deep yellow, not brass
    base = g.mix(base, (0.18, 0.11, 0.04), grime)
    rough = g.mix(g.img("Metal048B", "Roughness"), 0.8, grime, kind="FLOAT")
    return {"base": base, "rough": rough, "metal": g.img("Metal048B", "Metalness"), "height": g.img("Metal048B", "Displacement"),
            "bump_strength": 0.35, "bump_distance": 0.0008}


RECIPES = {"Iron": iron, "Copper": copper, "Gold": gold}
baked = {}
for v in job["variants"]:
    g = texbake.Graph("MetalIngot_" + v["name"], sets, tile=0.22)
    baked[v["name"]] = texbake.bake(obj, g, RECIPES[v["name"]](g), px, os.path.join(raw, v["name"]))
first = job["variants"][0]["name"]
for key, path in baked[first].items():
    shutil.copyfile(path, os.path.join(raw, "tex_%s.png" % key))

# The GLB the clean stage imports: the mesh, its UVs and the first variant's base colour.
obj.data.materials.clear()
obj.data.materials.append(texbake.preview_material("MetalIngot", baked[first]))
bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
bpy.ops.export_scene.gltf(filepath=os.path.join(raw, "model.glb"), use_selection=True, export_format="GLB")

obj = texbake.fresh_scene_from(os.path.join(raw, "model.glb"))  # review what the clean stage will import
texbake.review_scene(Vector((0, 0, H / 2)), 0.42)
for v in job["variants"]:
    obj.data.materials.clear()
    obj.data.materials.append(texbake.remapped_material("review_" + v["name"], os.path.join(raw, v["name"]), v["plan"], px))
    texbake.render(os.path.join(raw, "review_%s.png" % v["name"]))
print("LF_ASSET_MODEL ok", len(baked), "variants")
