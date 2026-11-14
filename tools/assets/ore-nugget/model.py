# The ore nugget, modelled and textured in Blender: no generation credits (spec "source": "blender").
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# job.json = { spec, raw, src, px, variants: [{ name, plan }] }. Writes <raw>/model.glb,
# <raw>/<Variant>/tex_*.png, the first variant again as <raw>/tex_*.png and <raw>/review_<Variant>.png.
#
# Shape: a fist-sized lump broken off a rock face: a rounded core cut by flat fracture planes, with
# a rough skin. Each ore is a material variant: iron is red-brown hematite and rust, copper is dark
# rock with banded green malachite, gold is milky quartz with iron staining and gold flecks.
import bpy, bmesh, json, math, os, random, shutil, sys
from mathutils import Vector, noise

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pipeline", "blender"))
import texbake

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
spec, raw, px = job["spec"], job["raw"], job["px"]
size = spec["size_m"]
L, W, H = size["length"], size["width"], size["height"]

bpy.ops.wm.read_factory_settings(use_empty=True)
me = bpy.data.meshes.new("OreNugget")
obj = bpy.data.objects.new("OreNugget", me)
bpy.context.scene.collection.objects.link(obj)

rng = random.Random(1989)
bm = bmesh.new()
bmesh.ops.create_icosphere(bm, subdivisions=4, radius=1.0)
# Fracture faces: every vertex beyond a plane is pressed onto it.
planes = []
for _ in range(11):
    n = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-0.8, 0.8))).normalized()
    planes.append((n, rng.uniform(0.5, 0.74)))
for v in bm.verts:
    for n, d in planes:
        k = v.co.dot(n)
        if k > d:
            v.co -= n * (k - d)
bm.normal_update()
for v in bm.verts:
    v.co += v.normal * (0.05 * noise.noise(v.co * 2.2) + 0.02 * noise.noise(v.co * 9.0 + Vector((3, 1, 2))))
for v in bm.verts:
    v.co = Vector((v.co.x * W / 2, v.co.y * L / 2, v.co.z * H / 2))
bm.to_mesh(me)
bm.free()
for p in me.polygons:
    p.use_smooth = True

bpy.context.view_layer.objects.active = obj
obj.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.012, area_weight=0.0, scale_to_bounds=False)
bpy.ops.uv.pack_islands(rotate=True, margin=0.01)
bpy.ops.object.mode_set(mode="OBJECT")

sets = texbake.Sets(job["src"])
texbake.cycles(16)


def crevice(g):
    return g.ramp(g.math("SUBTRACT", 1.0, g.ao(0.01)), 0.15, 0.55)


def iron(g):
    rock = g.mix(g.img("Rock035", "Color", data=False), (0.2, 0.07, 0.05), 0.55)  # dark red-grey hematite
    rust = g.scale_color(g.img("Rust009", "Color", data=False), (0.8, 0.62, 0.5))  # brown limonite, not paint rust
    patch = g.ramp(g.noise(30, detail=6, w=2.0, distortion=0.6), 0.48, 0.6)
    base = g.mix(rock, rust, patch)
    base = g.mix(base, (0.05, 0.03, 0.025), g.math("MULTIPLY", crevice(g), 0.7))
    rough = g.mix(g.img("Rock035", "Roughness"), g.img("Rust009", "Roughness"), patch, kind="FLOAT")
    height = g.mix(g.img("Rock035", "Displacement"), g.img("Rust009", "Displacement"), patch, kind="FLOAT")
    return {"base": base, "rough": rough, "metal": 0.0, "height": height, "bump_strength": 0.9, "bump_distance": 0.003}


def copper(g):
    rock = g.scale_color(g.img("Rock035", "Color", data=False), (0.75, 0.75, 0.78))
    wave = g.node("ShaderNodeTexWave", wave_type="RINGS", rings_direction="SPHERICAL")
    wave.inputs["Scale"].default_value = 9.0
    wave.inputs["Distortion"].default_value = 9.0
    wave.inputs["Detail"].default_value = 4.0
    g.link(g.raw, wave.inputs["Vector"])
    green = g.scale_color(g.img("Metal058C", "Color", data=False), (0.25, 0.75, 0.42))  # deep malachite, greener than verdigris
    banded = g.mix(green, g.scale_color(green, (0.35, 0.55, 0.45)), g.ramp(wave.outputs["Fac"], 0.35, 0.65))
    vein = g.ramp(g.noise(16, detail=5, w=5.0, distortion=0.8), 0.5, 0.58)
    base = g.mix(rock, banded, vein)
    speck = g.math("MULTIPLY", g.math("LESS_THAN", g.voronoi(260), 0.07), g.ramp(g.noise(10, w=9.0), 0.5, 0.6))
    base = g.mix(base, g.scale_color(g.img("Metal047B", "Color", data=False), (1.4, 1.0, 0.8)), speck)
    base = g.mix(base, (0.03, 0.03, 0.03), g.math("MULTIPLY", crevice(g), 0.6))
    rough = g.mix(g.img("Rock035", "Roughness"), 0.55, vein, kind="FLOAT")
    height = g.math("ADD", g.img("Rock035", "Displacement"), g.math("MULTIPLY", vein, 0.15))
    return {"base": base, "rough": rough, "metal": speck, "height": height, "bump_strength": 0.9, "bump_distance": 0.003}


def gold(g):
    grain = g.img("Rock035", "Color", data=False)
    quartz = g.mix((0.72, 0.71, 0.67), g.scale_color(grain, 2.2), 0.35)  # milky quartz with grey grain
    quartz = g.mix(quartz, (0.42, 0.43, 0.45), g.ramp(g.noise(6, detail=3, w=4.0), 0.52, 0.66))  # grey glassy bands
    stain = g.ramp(g.noise(11, detail=5, w=8.0, distortion=0.5), 0.44, 0.58)
    base = g.mix(quartz, g.scale_color(g.img("Rust009", "Color", data=False), (0.8, 0.6, 0.45)), g.math("MULTIPLY", stain, 0.8))  # limonite staining
    base = g.mix(base, (0.12, 0.08, 0.05), g.math("MULTIPLY", crevice(g), 0.55))
    cluster = g.ramp(g.noise(7, detail=2, w=11.0), 0.4, 0.55)
    fleck = g.math("MULTIPLY", g.math("LESS_THAN", g.voronoi(80, randomness=0.9), 0.2), cluster)
    base = g.mix(base, g.scale_color(g.img("Metal048B", "Color", data=False), (1.0, 0.7, 0.3)), fleck)
    rough = g.mix(g.mix(0.4, g.img("Rock058", "Roughness"), 0.4, kind="FLOAT"), 0.3, fleck, kind="FLOAT")
    height = g.math("ADD", g.math("MULTIPLY", g.img("Rock058", "Displacement"), 0.7), g.math("MULTIPLY", fleck, 0.3))
    return {"base": base, "rough": rough, "metal": fleck, "height": height, "bump_strength": 0.7, "bump_distance": 0.0025}


RECIPES = {"Iron": iron, "Copper": copper, "Gold": gold}
baked = {}
for v in job["variants"]:
    g = texbake.Graph("OreNugget_" + v["name"], sets, tile=0.14)
    baked[v["name"]] = texbake.bake(obj, g, RECIPES[v["name"]](g), px, os.path.join(raw, v["name"]))
first = job["variants"][0]["name"]
for key, path in baked[first].items():
    shutil.copyfile(path, os.path.join(raw, "tex_%s.png" % key))

obj.data.materials.clear()
obj.data.materials.append(texbake.preview_material("OreNugget", baked[first]))
bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
bpy.ops.export_scene.gltf(filepath=os.path.join(raw, "model.glb"), use_selection=True, export_format="GLB")

obj = texbake.fresh_scene_from(os.path.join(raw, "model.glb"))  # review what the clean stage will import
texbake.review_scene(Vector((0, 0, 0)), 0.3)
for v in job["variants"]:
    obj.data.materials.clear()
    obj.data.materials.append(texbake.remapped_material("review_" + v["name"], os.path.join(raw, v["name"]), v["plan"], px))
    texbake.render(os.path.join(raw, "review_%s.png" % v["name"]))
print("LF_ASSET_MODEL ok", len(baked), "variants")
