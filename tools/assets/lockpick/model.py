# The lock-picking set, modelled and textured in Blender: no generation credits (spec "source": "blender").
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# job.json = { spec, raw, src, px, variants }. Writes <raw>/model.glb, <raw>/tex_*.png, <raw>/review.png.
#
# Why not Meshy: its lockpick read as a pen, because a thin rod is all a text-to-3D model keeps of a
# pick. The set is a small stitched leather wallet, with a hook pick and a tension wrench laid on it
# under two straps, their steel ends sticking out past the wallet: a 1989 locksmith's kit, readable
# from above on the ground, tilted in the inventory and as a bundle in the fist.
#
# Axes (Blender; Enfusion x = X, y = Z up, z = Y): the set lies flat, long on Y, the tools' tips at +Y.
# Vertex colour "part" tells the bake which material a piece is: 0 leather, 0.3 thread, 0.5 brass,
# 0.7 steel, 1 black handle.
import bpy, bmesh, json, math, os, sys
from mathutils import Vector

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pipeline", "blender"))
import texbake

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
spec, raw, px = job["spec"], job["raw"], job["px"]
os.makedirs(raw, exist_ok=True)

LEATHER, THREAD, BRASS, STEEL, HANDLE = 0.0, 0.3, 0.5, 0.7, 1.0
CASE_X, CASE_Y0, CASE_Y1, CASE_HZ = 0.028, -0.075, 0.015, 0.0055   # wallet: half width, y range, half thickness
TOP = CASE_HZ                                                       # the wallet's top face; the tools lie on it
SHAFT_R = 0.0022
HOOK_X, WRENCH_X = -0.012, 0.014
SHAFT_Z = TOP + SHAFT_R

parts = []   # (bmesh, part id)


def piece(pid):
    bm = bmesh.new()
    parts.append((bm, pid))
    return bm


def block(bm, x0, x1, y0, y1, z0, z1, bevel=0.0012, seg=2):
    t = bmesh.ops.create_cube(bm, size=1.0)
    for v in t["verts"]:
        v.co = Vector((x0 if v.co.x < 0 else x1, y0 if v.co.y < 0 else y1, z0 if v.co.z < 0 else z1))
    keep = set(t["verts"])
    bm.edges.ensure_lookup_table()
    edges = [e for e in bm.edges if e.verts[0] in keep and e.verts[1] in keep]
    if bevel > 0:
        bmesh.ops.bevel(bm, geom=edges, offset=bevel, segments=seg, profile=0.6, affect="EDGES", clamp_overlap=True)


def lathe_y(bm, x, z, profile, seg=14):
    """A turned part along Y: profile = [(y, radius)], end radius 0 closes the end."""
    rings = []
    for y, r in profile:
        if r <= 0:
            rings.append(None)
        else:
            rings.append([bm.verts.new((x + r * math.cos(2 * math.pi * s / seg), y, z + r * math.sin(2 * math.pi * s / seg))) for s in range(seg)])
    for a in range(len(profile) - 1):
        ra, rb = rings[a], rings[a + 1]
        if ra is None or rb is None:
            continue
        for s in range(seg):
            bm.faces.new((ra[s], rb[s], rb[(s + 1) % seg], ra[(s + 1) % seg]))
    first = next(r for r in rings if r is not None)
    last = [r for r in rings if r is not None][-1]
    bm.faces.new(list(reversed(first)))
    bm.faces.new(last)


def sweep(bm, path, r, seg=8):
    """An open round tube along a 3D path, capped at both ends (the pick's shaft and hooked tip)."""
    rings = []
    up = Vector((1, 0, 0))
    for i, p in enumerate(path):
        t = (path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]).normalized()
        side = t.cross(up).normalized()
        up2 = side.cross(t).normalized()
        rings.append([bm.verts.new(p + r * (math.cos(2 * math.pi * s / seg) * side + math.sin(2 * math.pi * s / seg) * up2)) for s in range(seg)])
    for i in range(len(path) - 1):
        for s in range(seg):
            bm.faces.new((rings[i][s], rings[i + 1][s], rings[i + 1][(s + 1) % seg], rings[i][(s + 1) % seg]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])


# ---------------------------------------------------------------- the wallet
case = piece(LEATHER)
block(case, -CASE_X, CASE_X, CASE_Y0, CASE_Y1, -CASE_HZ, CASE_HZ, bevel=0.0028, seg=3)
for y0 in (-0.031, 0.002):            # two straps hold the tools down
    block(case, -CASE_X - 0.0008, CASE_X + 0.0008, y0, y0 + 0.0085, -CASE_HZ - 0.0008, TOP + 2 * SHAFT_R + 0.0023, bevel=0.0012)
thread = piece(THREAD)
for i in range(11):                   # stitching down both long edges and across the foot
    y = CASE_Y0 + 0.006 + i * 0.0076
    for sx in (-1, 1):
        block(thread, sx * (CASE_X - 0.0032) - 0.0004, sx * (CASE_X - 0.0032) + 0.0004, y, y + 0.0042, TOP - 0.0003, TOP + 0.0004, bevel=0.0001, seg=1)
for i in range(6):
    x = -CASE_X + 0.0085 + i * 0.0077
    block(thread, x, x + 0.0042, CASE_Y0 + 0.0028, CASE_Y0 + 0.0036, TOP - 0.0003, TOP + 0.0004, bevel=0.0001, seg=1)
brass = piece(BRASS)
# the snap on the wallet's foot, between the handles: a short domed cylinder on the top face
snap = brass
rings = []
for z, r in ((TOP, 0.0034), (TOP + 0.0016, 0.0034), (TOP + 0.0022, 0.0026)):
    rings.append([snap.verts.new((0.0 + r * math.cos(2 * math.pi * s / 14), -0.062 + r * math.sin(2 * math.pi * s / 14), z)) for s in range(14)])
for a in range(len(rings) - 1):
    for s in range(14):
        snap.faces.new((rings[a][s], rings[a][(s + 1) % 14], rings[a + 1][(s + 1) % 14], rings[a + 1][s]))
snap.faces.new(list(reversed(rings[0])))
snap.faces.new(rings[-1])

# ---------------------------------------------------------------- the hook pick
handle = piece(HANDLE)
lathe_y(handle, HOOK_X, TOP + 0.0046, [(-0.075, 0.0), (-0.0745, 0.0028), (-0.0715, 0.0042), (-0.060, 0.0046), (-0.049, 0.0042), (-0.0425, 0.0036), (-0.040, 0.0028), (-0.040, 0.0)])
shaft = piece(STEEL)
path = [Vector((HOOK_X, y, SHAFT_Z + 0.0027)) for y in (-0.0405, -0.02, 0.0, 0.02, 0.04, 0.056)]
hook_r = 0.0036
for k in range(1, 9):                 # the tip rises and curls back: a hook pick's profile
    a = math.radians(-90 + 125 * k / 8)
    path.append(Vector((HOOK_X, 0.056 + hook_r * math.cos(a), SHAFT_Z + 0.0027 + hook_r + hook_r * math.sin(a))))
sweep(shaft, path, SHAFT_R)

# ---------------------------------------------------------------- the tension wrench: a flat L of spring steel
wrench = piece(STEEL)
block(wrench, WRENCH_X - 0.0030, WRENCH_X + 0.0030, -0.0405, 0.0765, TOP + 0.0004, TOP + 0.0027, bevel=0.0004, seg=1)
block(wrench, WRENCH_X - 0.0024, WRENCH_X + 0.0150, 0.0720, 0.0765, TOP + 0.0004, TOP + 0.0027, bevel=0.0004, seg=1)

# ---------------------------------------------------------------- join, centre, UV
verts, faces, cols = [], [], []
for bm, pid in parts:
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    idx = {}
    for v in bm.verts:
        idx[v] = len(verts)
        verts.append(tuple(v.co))
        cols.append(pid)
    for f in bm.faces:
        faces.append([idx[v] for v in f.verts])
lo = Vector([min(v[k] for v in verts) for k in range(3)])
hi = Vector([max(v[k] for v in verts) for k in range(3)])
mid = (lo + hi) / 2
verts = [tuple(Vector(v) - mid) for v in verts]
print("LF extent", [round(float(hi[k] - lo[k]), 4) for k in range(3)], "centre shift (Enfusion)", [round(float(-mid[0]), 5), round(float(-mid[2]), 5), round(float(-mid[1]), 5)])
print("LF wallet centre y", round(float((CASE_Y0 + CASE_Y1) / 2 - mid[1]), 5), "wallet top z", round(float(TOP - mid[2]), 5))

me = bpy.data.meshes.new("Lockpick")
me.from_pydata(verts, [], faces)
me.update()
attr = me.color_attributes.new("part", "FLOAT_COLOR", "POINT")
for i, c in enumerate(cols):
    attr.data[i].color = (c, c, c, 1.0)
obj = bpy.data.objects.new("Lockpick", me)
bpy.context.scene.collection.objects.link(obj)
for p in me.polygons:
    p.use_smooth = len(p.vertices) <= 4
bpy.context.view_layer.objects.active = obj
obj.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.uv.smart_project(angle_limit=math.radians(55), island_margin=0.012, area_weight=0.0, scale_to_bounds=False)
bpy.ops.uv.pack_islands(rotate=True, margin=0.01)
bpy.ops.object.mode_set(mode="OBJECT")
print("LF tris", sum(len(p.vertices) - 2 for p in me.polygons))

# ---------------------------------------------------------------- materials by part
sets = texbake.Sets(job["src"])
texbake.cycles(16)


def kit(g):
    vc = g.node("ShaderNodeVertexColor", layer_name="part")
    sep = g.node("ShaderNodeSeparateColor")
    g.link(vc.outputs["Color"], sep.inputs["Color"])
    pid = sep.outputs[0]

    def band(lo, hi):
        return g.math("MULTIPLY", g.ramp(pid, lo - 0.02, lo), g.math("SUBTRACT", 1.0, g.ramp(pid, hi, hi + 0.02)))

    thread_m, brass_m, steel_m, handle_m = band(0.3, 0.3), band(0.5, 0.5), band(0.7, 0.7), g.ramp(pid, 0.98, 1.0)
    metal_m = g.math("MAXIMUM", brass_m, steel_m)
    grain = g.noise(420, detail=4)
    pits = g.noise(90, detail=5, w=1.0)
    wear = g.pointiness(0.5, 0.64)
    ao = g.ao(0.004)
    crev = g.math("SUBTRACT", 1.0, ao)
    # Wallet: worn brown hide from ambientCG Leather014 (CC0): its colour, grain and scuffs, rubbed paler
    # at the edges and dark in the folds.
    hide = g.img("Leather014", "Color", data=False)
    leather = g.mix(hide, (0.3, 0.18, 0.1), g.math("MULTIPLY", wear, 0.45))
    leather = g.mix(leather, (0.03, 0.017, 0.01), g.math("MULTIPLY", g.ramp(crev, 0.3, 0.7), 0.7))
    # The picks: spring steel, blued and a little scratched. A smooth, light steel mirrors the noon sky
    # (white-blue), so it is dark, rougher and scratched by Metal038 (CC0).
    scratch = g.img("Metal038", "Color", data=False)
    steel = g.mix((0.04, 0.042, 0.046), (0.075, 0.078, 0.083), g.ramp(scratch, 0.2, 0.5))
    steel = g.mix(steel, (0.18, 0.185, 0.195), g.math("MULTIPLY", wear, 0.6))
    steel = g.mix(steel, (0.015, 0.015, 0.017), g.math("MULTIPLY", g.ramp(pits, 0.5, 0.7), 0.5))
    base = leather
    base = g.mix(base, (0.62, 0.55, 0.4), thread_m)
    base = g.mix(base, (0.3, 0.2, 0.06), brass_m)
    base = g.mix(base, steel, steel_m)
    base = g.mix(base, (0.03, 0.03, 0.035), handle_m)
    leather_rough = g.math("ADD", 0.45, g.math("MULTIPLY", g.img("Leather014", "Roughness"), 0.5))
    metal_rough = g.math("ADD", 0.5, g.math("ADD", g.math("MULTIPLY", g.img("Metal038", "Roughness"), 0.4), g.math("MULTIPLY", pits, 0.15)), clamp=True)
    rough = g.mix(leather_rough, metal_rough, metal_m, kind="FLOAT")
    rough = g.mix(rough, 0.5, handle_m, kind="FLOAT")
    metal = g.math("MULTIPLY", metal_m, 0.95)
    height = g.math("ADD", g.math("MULTIPLY", g.img("Leather014", "Displacement"), 0.6), g.math("MULTIPLY", grain, 0.2))
    return {"base": base, "rough": rough, "metal": metal, "height": height, "bump_strength": 0.8, "bump_distance": 0.0006,
            "ao": g.math("ADD", 0.35, g.math("MULTIPLY", ao, 0.65))}


g = texbake.Graph("Lockpick_set", sets, tile=0.14)
files = texbake.bake(obj, g, kit(g), px, raw)
obj.data.materials.clear()
obj.data.materials.append(texbake.preview_material("Lockpick", files))
bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
bpy.ops.export_scene.gltf(filepath=os.path.join(raw, "model.glb"), use_selection=True, export_format="GLB")

obj = texbake.fresh_scene_from(os.path.join(raw, "model.glb"))
texbake.review_scene(Vector((0, 0, 0)), 0.32)
texbake.render(os.path.join(raw, "review.png"))
print("LF_ASSET_MODEL ok")
