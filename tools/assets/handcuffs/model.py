# The handcuffs, modelled and textured in Blender: no generation credits (spec "source": "blender").
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# job.json = { spec, raw, src, px, variants }. Writes <raw>/model.glb, <raw>/tex_*.png and
# <raw>/review.png, and <raw>/layout.json: the hinge pivots and the face boxes of the two swinging
# strands, in Enfusion model space after the clean stage's fit, which spec.anim.bones copies.
#
# Why not Meshy: its handcuffs fused each ring into one surface, so the strand that swings open could
# not be cut off (docs/asset-pipeline/states.md "Use states"). Here a cuff is a lock body, a fixed
# bow and a swinging strand with its hinge boss, separate meshes, and the pivot is exact.
#
# Layout (Blender axes; Enfusion x = X, y = Z up, z = Y): the rings lie flat, their axes along Z,
# one at +Y and its mirror at -Y, the lock bodies facing each other on the -X side with a three-link
# chain between them. Same footprint as the model this replaces: 0.18 m long, 0.09 m wide, 0.032 m high.
import bpy, bmesh, json, math, os, sys
from mathutils import Vector

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pipeline", "blender"))
import texbake

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
spec, raw, px = job["spec"], job["raw"], job["px"]
os.makedirs(raw, exist_ok=True)

CX, CY = 0.005, 0.0525          # ring centre of the +Y cuff (the other is mirrored)
R_IN, R_OUT, HH = 0.0290, 0.0375, 0.013   # bow: inner and outer radius, half height
R_MID = (R_IN + R_OUT) / 2
HINGE_A, TIP_A = 230.0, 285.0   # degrees in the +Y cuff's plane: where the strand turns, where its curved part ends
BOW_START_A, BOW_END_A = 160.0, -10.0   # the fixed bow runs the other way round, clockwise from 160 degrees (inside the lock body) to -10
STRAND_CUT, BOW_CUT = 0.022, 0.026   # both end on a vertical line, 4 mm apart: the seam where the cuff closes
BODY_X0, BODY_X1, BODY_Y = -0.0455, -0.0225, 0.0195   # lock body box: x range relative to the ring centre, half length
STEP = 4.0                      # degrees per arc segment

pieces = {}   # name -> bmesh


def piece(name):
    pieces[name] = bmesh.new()
    return pieces[name]


def octagon(r0, r1, hh, ch):
    return [(r0 + ch, -hh), (r1 - ch, -hh), (r1, -hh + ch), (r1, hh - ch), (r1 - ch, hh), (r0 + ch, hh), (r0, hh - ch), (r0, -hh + ch)]


def arc(bm, a0, a1, r0, r1, hh, ch, cut=None, cx=CX, cy=CY):
    """A band of constant section swept about (cx, cy) from a0 to a1 degrees, capped at both ends.
    `cut` (metres from the ring centre along x) ends it with a flat vertical face on that line instead,
    one more ring past a1: a part that swings off the ring is then separable by an axis-aligned box."""
    prof = octagon(r0, r1, hh, ch)
    n = max(2, int(abs(a1 - a0) / STEP) + 1)
    rings = []
    for i in range(n + 1):
        th = math.radians(a0 + (a1 - a0) * i / n)
        rings.append([bm.verts.new((cx + r * math.cos(th), cy + r * math.sin(th), z)) for r, z in prof])
    if cut is not None:
        ring = []
        for r, z in prof:
            th = -math.acos(cut / r) + (2 * math.pi if a0 > 180 else 0)
            ring.append(bm.verts.new((cx + cut, cy + r * math.sin(th), z)))
        rings.append(ring)
    k = len(prof)
    for i in range(len(rings) - 1):
        for j in range(k):
            bm.faces.new((rings[i][j], rings[i][(j + 1) % k], rings[i + 1][(j + 1) % k], rings[i + 1][j]))
    bm.faces.new(rings[0])
    bm.faces.new(list(reversed(rings[-1])))


def cylinder(bm, x, y, z0, z1, r, ch=0.0005, seg=16):
    prof = [(0, z0), (r - ch, z0), (r, z0 + ch), (r, z1 - ch), (r - ch, z1), (0, z1)]
    rings = []
    for rr, z in prof:
        if rr > 0:
            rings.append([bm.verts.new((x + rr * math.cos(2 * math.pi * s / seg), y + rr * math.sin(2 * math.pi * s / seg), z)) for s in range(seg)])
        else:
            rings.append(None)
    for a in range(len(prof) - 1):
        ra, rb = rings[a], rings[a + 1]
        if ra is None or rb is None:
            continue
        for s in range(seg):
            bm.faces.new((ra[s], ra[(s + 1) % seg], rb[(s + 1) % seg], rb[s]))
    bm.faces.new(list(reversed(rings[1])))
    bm.faces.new(rings[-2])


def block(bm, x0, x1, y0, y1, z0, z1, bevel=0.0018):
    t = bmesh.ops.create_cube(bm, size=1.0)
    for v in t["verts"]:
        v.co = Vector((x0 if v.co.x < 0 else x1, y0 if v.co.y < 0 else y1, z0 if v.co.z < 0 else z1))
    keep = set(t["verts"])
    bm.edges.ensure_lookup_table()
    edges = [e for e in bm.edges if e.verts[0] in keep and e.verts[1] in keep]
    bmesh.ops.bevel(bm, geom=edges, offset=bevel, segments=2, profile=0.6, affect="EDGES", clamp_overlap=True)


def tube(bm, path, normal, r, seg=8):
    """A closed tube of radius r along a closed 3D path; `normal` is the axis normal to the path's plane."""
    n = len(path)
    rings = []
    for i in range(n):
        t = (path[(i + 1) % n] - path[i - 1]).normalized()
        side = t.cross(normal).normalized()
        rings.append([bm.verts.new(path[i] + r * (math.cos(2 * math.pi * s / seg) * side + math.sin(2 * math.pi * s / seg) * normal)) for s in range(seg)])
    for i in range(n):
        for s in range(seg):
            bm.faces.new((rings[i][s], rings[i][(s + 1) % seg], rings[(i + 1) % n][(s + 1) % seg], rings[(i + 1) % n][s]))


def link(bm, centre, length, width, flat, wire=0.0017):
    """A stadium chain link centred on `centre`, long along Y; flat in the XY plane or standing in YZ."""
    straight = length / 2 - width / 2
    pts = []
    for i in range(14):
        a = math.pi * i / 13
        pts.append((width / 2 * math.cos(a), straight + width / 2 * math.sin(a)))
    for i in range(14):
        a = math.pi + math.pi * i / 13
        pts.append((width / 2 * math.cos(a), -straight + width / 2 * math.sin(a)))
    if flat:
        path = [centre + Vector((u, v, 0)) for u, v in pts]
        normal = Vector((0, 0, 1))
    else:
        path = [centre + Vector((0, v, u)) for u, v in pts]
        normal = Vector((1, 0, 0))
    tube(bm, path, normal, wire)


def polar(a, r, cx=CX, cy=CY):
    return Vector((cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a)), 0))


# ---------------------------------------------------------------- the +Y cuff
body = piece("body")
block(body, CX + BODY_X0, CX + BODY_X1, CY - BODY_Y, CY + BODY_Y, -0.016, 0.016)
for dy in (-0.0105, 0.0105):          # rivets on the lock body's top
    cylinder(body, CX + (BODY_X0 + BODY_X1) / 2 + 0.003, CY + dy, 0.0155, 0.0175, 0.0022, ch=0.0006)
LUG_X, LUG_Y = CX + BODY_X0 + 0.0045, CY - BODY_Y + 0.0035
cylinder(body, LUG_X, LUG_Y, -0.0095, 0.0095, 0.0023)   # the lug the chain hangs from
bow = piece("bow")
arc(bow, BOW_START_A, BOW_END_A, R_IN, R_OUT, HH, 0.0013, cut=BOW_CUT)
strand = piece("strand")
arc(strand, HINGE_A, TIP_A, R_IN, R_OUT, HH, 0.0013, cut=STRAND_CUT)
hp = polar(HINGE_A, R_MID)
cylinder(strand, hp.x, hp.y, -0.0165, 0.0165, 0.0046, ch=0.0008)    # the boss the strand turns on

# ---------------------------------------------------------------- the chain, between the two lugs
chain = piece("chain")
for i, k in enumerate((-1, 0, 1)):
    link(chain, Vector((LUG_X, k * 0.0215, 0)), 0.0270, 0.0125, flat=(i % 2 == 0))


def mirrored(src):
    m = bmesh.new()
    vmap = {}
    for v in src.verts:
        vmap[v] = m.verts.new((v.co.x, -v.co.y, v.co.z))
    for f in src.faces:
        m.faces.new([vmap[v] for v in reversed(f.verts)])
    return m


for name in ("body", "bow", "strand"):
    pieces[name + "_b"] = mirrored(pieces[name])
for bm in pieces.values():
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)

verts, faces, owner = [], [], []
for name, bm in pieces.items():
    idx = {}
    for v in bm.verts:
        idx[v] = len(verts)
        verts.append(tuple(v.co))
    for f in bm.faces:
        faces.append([idx[v] for v in f.verts])
        owner.append(name)
lo = Vector([min(v[k] for v in verts) for k in range(3)])
hi = Vector([max(v[k] for v in verts) for k in range(3)])
mid = (lo + hi) / 2
verts = [tuple(Vector(v) - mid) for v in verts]
print("LF extent", [round(float(hi[k] - lo[k]), 4) for k in range(3)])


def enf(v):   # Blender model-space point -> Enfusion
    return [round(float(v[0]), 5), round(float(v[2]), 5), round(float(v[1]), 5)]


def centre_of(tri):
    return sum((Vector(verts[i]) for i in tri), Vector()) / len(tri)


layout = {"centre_shift": enf(-mid), "bones": {}}
for name, flip in (("strand", 1), ("strand_b", -1)):
    cs = [centre_of(t) for t, who in zip(faces, owner) if who == name]
    blo = Vector([min(c[k] for c in cs) - 0.0003 for k in range(3)])
    bhi = Vector([max(c[k] for c in cs) + 0.0003 for k in range(3)])
    foreign = {}
    for t, who in zip(faces, owner):
        if who != name:
            c = centre_of(t)
            if all(blo[k] <= c[k] <= bhi[k] for k in range(3)):
                foreign[who] = foreign.get(who, 0) + 1
    hinge = Vector((hp.x, hp.y * flip, 0)) - mid
    layout["bones"][name] = {"pivot": enf(hinge), "box": enf(blo) + enf(bhi), "foreign_faces_in_box": foreign}
json.dump(layout, open(os.path.join(raw, "layout.json"), "w"), indent=1)

me = bpy.data.meshes.new("Handcuffs")
me.from_pydata(verts, [], faces)
me.update()
obj = bpy.data.objects.new("Handcuffs", me)
bpy.context.scene.collection.objects.link(obj)
for p in me.polygons:
    p.use_smooth = len(p.vertices) <= 4   # the flat end caps keep their own normals
bpy.context.view_layer.objects.active = obj
obj.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.uv.smart_project(angle_limit=math.radians(55), island_margin=0.012, area_weight=0.0, scale_to_bounds=False)
bpy.ops.uv.pack_islands(rotate=True, margin=0.01)
bpy.ops.object.mode_set(mode="OBJECT")
print("LF tris", sum(len(p.vertices) - 2 for p in me.polygons))

# ---------------------------------------------------------------- the nickel-plated steel
sets = texbake.Sets(job["src"])
texbake.cycles(16)


def nickel(g):
    # Dark nickel / blued steel, the 1989 duty cuff: mid-grey metal (metalness near 1, so it reflects
    # instead of lighting like paint), fairly smooth with fine scratches from the ambientCG set
    # Metal038 (CC0), dark in the recesses (an occlusion bake goes to the NMO alpha).
    wear = g.pointiness(0.5, 0.62)
    ao = g.ao(0.006)
    crev = g.math("SUBTRACT", 1.0, ao)
    fine = g.noise(260, detail=3)
    streak = g.ramp(g.noise(70, detail=5, w=2.0), 0.55, 0.7)
    scratch = g.img("Metal038", "Color", data=False)
    base = g.mix((0.028, 0.029, 0.031), (0.055, 0.056, 0.059), g.ramp(scratch, 0.2, 0.5))     # gunmetal like the base-game M9: darker and near neutral, scratches pick out lighter
    base = g.mix(base, (0.11, 0.112, 0.116), g.math("MULTIPLY", wear, 0.6))                # bright worn edges, still steel not chrome
    base = g.mix(base, (0.012, 0.012, 0.014), g.math("MULTIPLY", g.ramp(crev, 0.25, 0.65), 0.85))   # dark in the recesses
    base = g.mix(base, (0.04, 0.04, 0.043), g.math("MULTIPLY", streak, 0.4))                # hand-worn dull patches
    rough = g.math("ADD", 0.6, g.math("ADD", g.math("MULTIPLY", g.img("Metal038", "Roughness"), 0.45), g.math("ADD", g.math("MULTIPLY", streak, 0.18), g.math("MULTIPLY", crev, 0.2))), clamp=True)
    metal = g.math("SUBTRACT", 0.82, g.math("MULTIPLY", g.ramp(crev, 0.5, 0.9), 0.15))
    height = g.math("ADD", g.math("MULTIPLY", g.img("Metal038", "Displacement"), 0.5), g.math("MULTIPLY", fine, 0.15))
    return {"base": base, "rough": rough, "metal": metal, "height": height, "bump_strength": 0.5, "bump_distance": 0.0006,
            "ao": g.math("ADD", 0.35, g.math("MULTIPLY", ao, 0.65))}


g = texbake.Graph("Handcuffs_nickel", sets, tile=0.2)
files = texbake.bake(obj, g, nickel(g), px, raw)
obj.data.materials.clear()
obj.data.materials.append(texbake.preview_material("Handcuffs", files))
bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
bpy.ops.export_scene.gltf(filepath=os.path.join(raw, "model.glb"), use_selection=True, export_format="GLB")

obj = texbake.fresh_scene_from(os.path.join(raw, "model.glb"))
texbake.review_scene(Vector((0, 0, 0)), 0.4)
texbake.render(os.path.join(raw, "review.png"))
print("LF_ASSET_MODEL ok")
