# Tiling rock-with-ore textures for the quarry nodes (kind "material", spec "source": "blender").
# Run by the generate stage:  blender --background --factory-startup --python paint.py -- <job.json>
# job.json = { spec, raw, src, px, variants: [{ name, plan }] }. Bakes each variant on a 1 x 1 plane
# into <raw>/<Variant>/tex_*.png and renders <raw>/review_<Variant>.png (the tile repeated 2 x 2 on a
# boulder-sized sphere).
#
# Every input repeats once per UV tile: the CC0 sets are sampled at whole-number UV scales and the
# procedural masks are 4D noise on a torus (texbake.Graph.torus), so the result tiles with no seam.
# The emat (rock-node.emat.txt) lays the tile over the vanilla granite boulder with its own mask and
# macro maps, so the rock keeps its shape and shading and only the stone itself changes.
import bpy, json, math, os, sys
from mathutils import Vector

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pipeline", "blender"))
import texbake

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
raw, px = job["raw"], job["px"]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.mesh.primitive_plane_add(size=1.0)
plane = bpy.context.view_layer.objects.active
sets = texbake.Sets(job["src"])
texbake.cycles(4)


def host(g):
    """Grey granite-like host rock, the same under every ore so only the veins tell them apart."""
    uv = g.uv_tiles(2)
    col = g.scale_color(g.img("Rock058", "Color", data=False, vec=uv), (1.2, 1.17, 1.1))
    return col, g.img("Rock058", "Roughness", vec=uv), g.img("Rock058", "Displacement", vec=uv), uv


def veins(g, scale, width, w0, distortion=0.6):
    v, w = g.torus(1.0)
    n = g.noise(scale, detail=6, rough=0.6, vec=v, w=g.math("ADD", w, w0), distortion=distortion)
    return g.ridge(n, width), g.ridge(n, width * 4.0), v, w


def iron(g):
    col, rough, height, uv = host(g)
    core, halo, v, w = veins(g, 0.8, 0.012, 0.0)
    patch = g.ramp(g.noise(1.6, detail=4, vec=v, w=g.math("ADD", w, 5.0)), 0.55, 0.68)
    rust = g.img("Rust009", "Color", data=False, vec=uv)
    stain = g.math("MAXIMUM", g.math("MULTIPLY", halo, 0.4), g.math("MULTIPLY", patch, 0.55))
    base = g.mix(col, g.scale_color(rust, (0.9, 0.7, 0.55)), stain)  # iron staining around the veins
    base = g.mix(base, g.mix((0.16, 0.06, 0.04), rust, 0.35), core)  # dark red hematite in the veins
    rough = g.mix(rough, g.img("Rust009", "Roughness", vec=uv), stain, kind="FLOAT")
    height = g.math("SUBTRACT", height, g.math("MULTIPLY", core, 0.15))
    return {"base": base, "rough": rough, "metal": 0.0, "height": height, "bump_strength": 0.8, "bump_distance": 0.01}


def copper(g):
    col, rough, height, uv = host(g)
    core, halo, v, w = veins(g, 0.75, 0.013, 13.0)
    green = g.img("Metal058C", "Color", data=False, vec=g.uv_tiles(3))
    blob = g.ramp(g.noise(2.2, detail=4, vec=v, w=g.math("ADD", w, 21.0)), 0.6, 0.7)
    base = g.mix(col, g.scale_color(green, (0.55, 0.85, 0.7)), g.math("MULTIPLY", halo, 0.45))  # green staining
    base = g.mix(base, g.scale_color(green, (0.25, 0.75, 0.42)), g.math("MAXIMUM", core, blob))  # malachite
    azurite = g.math("MULTIPLY", blob, g.ramp(g.noise(6, vec=v, w=g.math("ADD", w, 30.0)), 0.6, 0.7))
    base = g.mix(base, (0.04, 0.12, 0.42), azurite)  # azurite blue in the richest pockets
    rough = g.mix(rough, 0.6, core, kind="FLOAT")
    height = g.math("ADD", height, g.math("MULTIPLY", g.math("MAXIMUM", core, blob), 0.1))
    return {"base": base, "rough": rough, "metal": 0.0, "height": height, "bump_strength": 0.8, "bump_distance": 0.01}


def gold(g):
    col, rough, height, uv = host(g)
    core, halo, v, w = veins(g, 0.6, 0.018, 41.0, distortion=0.5)
    quartz = g.mix((0.8, 0.66, 0.34), (0.55, 0.47, 0.28), g.ramp(g.noise(9, vec=v, w=g.math("ADD", w, 3.0)), 0.4, 0.7))
    base = g.mix(col, g.scale_color(g.img("Rust009", "Color", data=False, vec=uv), (0.9, 0.75, 0.6)), g.math("MULTIPLY", halo, 0.18))
    base = g.mix(base, quartz, core)  # ochre quartz reefs
    fleck = g.math("MULTIPLY", g.math("LESS_THAN", g.voronoi(14, vec=v, w=g.math("ADD", w, 7.0), randomness=0.9), 0.26), core)
    base = g.mix(base, g.scale_color(g.img("Metal048B", "Color", data=False, vec=uv), (1.0, 0.72, 0.2)), fleck)
    rough = g.mix(g.mix(rough, 0.45, core, kind="FLOAT"), 0.3, fleck, kind="FLOAT")
    height = g.math("ADD", height, g.math("MULTIPLY", core, 0.2))
    return {"base": base, "rough": rough, "metal": fleck, "height": height, "bump_strength": 0.8, "bump_distance": 0.01}


RECIPES = {"Iron": iron, "Copper": copper, "Gold": gold}
for v in job["variants"]:
    g = texbake.Graph("OreVein_" + v["name"], sets, tile=1.0)
    texbake.bake(plane, g, RECIPES[v["name"]](g), px, os.path.join(raw, v["name"]))

# Review: a boulder-sized sphere with the tile repeated twice each way, as the emat lays it on.
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.mesh.primitive_uv_sphere_add(radius=0.7, segments=48, ring_count=24)
ball = bpy.context.view_layer.objects.active
bpy.ops.object.shade_smooth()
for loop in ball.data.uv_layers[0].data:
    loop.uv = loop.uv * 2.0
texbake.review_scene(Vector((0, 0, 0)), 3.0)
for v in job["variants"]:
    ball.data.materials.clear()
    ball.data.materials.append(texbake.remapped_material("review_" + v["name"], os.path.join(raw, v["name"]), v["plan"], px))
    texbake.render(os.path.join(raw, "review_%s.png" % v["name"]))
print("LF_ASSET_MODEL ok", len(job["variants"]), "variants")
