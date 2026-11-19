# Writes the reaper model (#3292, harvester slice H5b): LF_Reaper_01, a trailed sickle-bar mower as a 1989
# kolkhoz kit: a 3 m cutter bar with finger guards and a toothed knife, a frame with two steel wheels, a small grain
# hopper with its feed tube, and an A-frame drawbar with a hitch eye and a jack foot. Built from simple parts, no
# texture of ours: the materials use the base game's shared metal maps (see build.mjs).
#
# Run: blender -b --factory-startup -P reaper.py -- <out-dir> <report.json>
# tools/blender/harvest-reaper/build.mjs runs this and writes the .meta, .emat and prefab files.
#
# The model is built in the GAME frame (x right, y up, z toward the towing vehicle) and mapped to Blender with
# P(): game x is Blender -x and game z is Blender y, so the FBX exporter's default axes put the drawbar on +Z in the engine.
#   - the origin is on the cutter bar's centre line, on the ground: the bar's underside is 0.30 m above it
#     (LF_Hitches.conf puts this origin 3.4 m behind a UAZ469's origin, 1.53 m past its rear bound);
#   - the drawbar tip is 1.53 m ahead of the origin, so it meets the UAZ's rear bumper;
#   - the wheels and the jack foot rest on y = 0; the bar clears the stubble by 0.30 m;
#   - four materials, one per kind of metal (never a second material inside a part): Paint, Red, Steel, Dark;
#   - no collider: the implement has no physics body and never snags the vehicle.
import bpy, bmesh, sys, os, json, math, mathutils

OUT, REPORT = sys.argv[-2], sys.argv[-1]

MATS = ["LF_Reaper_Paint", "LF_Reaper_Red", "LF_Reaper_Steel", "LF_Reaper_Dark"]
PAINT, RED, STEEL, DARK = 0, 1, 2, 3

BAR_HALF = 1.5       # half of the 3 m cutter bar
BAR_Y = 0.30         # the bar's underside above the ground
HITCH_Z = 1.53       # the drawbar tip, ahead of the origin


def P(v):
    """Game frame (x, y, z) to Blender (-x, z, y): a half turn about the up axis, so the engine sees +Z toward the vehicle."""
    return mathutils.Vector((-v[0], v[2], v[1]))


class Builder:
    """Collects closed parts into one bmesh; every part is made outward-facing on its own, then merged."""

    def __init__(self):
        self.bm = bmesh.new()
        self.seg_scale = 1.0

    def part(self, verts, faces, mat):
        bm = self.bm
        vs = [bm.verts.new(P(v)) for v in verts]
        fs = []
        for f in faces:
            face = bm.faces.new([vs[i] for i in f])
            face.material_index = mat
            fs.append(face)
        bmesh.ops.recalc_face_normals(bm, faces=fs)

    def box(self, c, size, mat, yaw=0.0):
        hx, hy, hz = size[0] / 2.0, size[1] / 2.0, size[2] / 2.0
        s, co = math.sin(yaw), math.cos(yaw)
        pts = []
        for sx in (-1, 1):
            for sy in (-1, 1):
                for sz in (-1, 1):
                    x, z = sx * hx, sz * hz
                    pts.append((c[0] + x * co + z * s, c[1] + sy * hy, c[2] - x * s + z * co))
        self.part(pts, BOX_FACES, mat)

    def beam(self, a, b, w, h, mat):
        """A square-section beam from a to b; w is the width across (horizontal), h the height."""
        a = mathutils.Vector(a)
        b = mathutils.Vector(b)
        d = (b - a).normalized()
        up = mathutils.Vector((0, 1, 0))
        side = up.cross(d)
        if side.length < 1e-4:
            side = mathutils.Vector((1, 0, 0))
        side.normalize()
        u = d.cross(side).normalized()
        pts = []
        for sx in (-1, 1):
            for sy in (-1, 1):
                for e in (a, b):
                    pts.append(tuple(e + side * (sx * w / 2.0) + u * (sy * h / 2.0)))
        # corner order: (sx, sy, end) with end fastest; BOX_FACES is written for (sx, sy, sz) with sz fastest.
        self.part(pts, BOX_FACES, mat)

    def cyl(self, a, b, r, mat, segs=14, r2=None):
        """A cylinder (or a cone frustum when r2 is given) from a to b."""
        a = mathutils.Vector(a)
        b = mathutils.Vector(b)
        d = (b - a).normalized()
        ref = mathutils.Vector((0, 1, 0))
        if abs(d.dot(ref)) > 0.95:
            ref = mathutils.Vector((1, 0, 0))
        s = d.cross(ref).normalized()
        u = d.cross(s).normalized()
        r2 = r if r2 is None else r2
        segs = max(6, int(segs * self.seg_scale))
        pts = []
        for e, rr in ((a, r), (b, r2)):
            for i in range(segs):
                t = 2.0 * math.pi * i / segs
                pts.append(tuple(e + s * (math.cos(t) * rr) + u * (math.sin(t) * rr)))
        pts.append(tuple(a))
        pts.append(tuple(b))
        faces = []
        for i in range(segs):
            j = (i + 1) % segs
            faces.append((i, j, segs + j, segs + i))
            faces.append((2 * segs, j, i))
            faces.append((2 * segs + 1, segs + i, segs + j))
        self.part(pts, faces, mat)

    def frustum(self, cx, cz, y0, y1, bottom, top, mat):
        """A box that widens upward: bottom and top are (size x, size z)."""
        pts = []
        for y, (sx, sz) in ((y0, bottom), (y1, top)):
            for dx, dz in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                pts.append((cx + dx * sx / 2.0, y, cz + dz * sz / 2.0))
        faces = [(0, 1, 2, 3), (7, 6, 5, 4), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        self.part(pts, faces, mat)

    def wedge(self, x, z0, z1, y0, y1, w, mat):
        """A triangular prism lying flat: base at z0 (width w), tip at z1, from y0 to y1."""
        pts = [(x - w / 2.0, y0, z0), (x + w / 2.0, y0, z0), (x, y0, z1),
               (x - w / 2.0, y1, z0), (x + w / 2.0, y1, z0), (x, y1, z1)]
        faces = [(0, 2, 1), (3, 4, 5), (0, 1, 4, 3), (1, 2, 5, 4), (2, 0, 3, 5)]
        self.part(pts, faces, mat)

    def finish(self, name):
        bm = self.bm
        # Box-projected UVs in metres, so the shared 1 m tiling maps read at their real scale.
        uv = bm.loops.layers.uv.new("UVMap")
        for face in bm.faces:
            n = face.normal
            ax = max(range(3), key=lambda i: abs(n[i]))
            for loop in face.loops:
                p = loop.vert.co
                if ax == 0:
                    loop[uv].uv = (p.y, p.z)
                elif ax == 1:
                    loop[uv].uv = (p.x, p.z)
                else:
                    loop[uv].uv = (p.x, p.y)
        mesh = bpy.data.meshes.new(name)
        bm.to_mesh(mesh)
        bm.free()
        for m in MATS:
            mesh.materials.append(bpy.data.materials.get(m) or bpy.data.materials.new(m))
        for poly in mesh.polygons:
            poly.use_smooth = False
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.collection.objects.link(obj)
        return obj


# Corner order (sx, sy, sz) with sz fastest: index = sx*4 + sy*2 + sz over (-1, 1).
BOX_FACES = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]


def reaper(detail):
    b = Builder()
    if not detail:
        b.seg_scale = 0.5                # LOD1: half the segments on every round part
    n = 38 if detail else 0             # finger guards and knife teeth across the 3 m bar
    pitch = 2 * BAR_HALF / max(n, 1)

    # --- the cutter bar (steel): finger bar, fingers, a toothed knife riding above it ---
    b.beam((-BAR_HALF, BAR_Y + 0.03, 0.0), (BAR_HALF, BAR_Y + 0.03, 0.0), 0.09, 0.06, STEEL)
    if detail:
        for i in range(n):
            x = -BAR_HALF + pitch * (i + 0.5)
            b.wedge(x, 0.04, 0.34, BAR_Y, BAR_Y + 0.03, 0.05, STEEL)          # a finger guard
            b.wedge(x, 0.02, 0.17, BAR_Y + 0.06, BAR_Y + 0.085, 0.07, STEEL)  # a knife tooth
    else:
        b.box((0, BAR_Y + 0.015, 0.19), (2 * BAR_HALF, 0.03, 0.30), STEEL)
        b.box((0, BAR_Y + 0.075, 0.10), (2 * BAR_HALF, 0.025, 0.16), STEEL)
    # Skid shoes at both ends and the knife-head housing on the right (the hopper side).
    for s in (-1, 1):
        b.beam((s * 1.53, 0.0 + 0.02, 0.36), (s * 1.57, BAR_Y + 0.28, -0.12), 0.035, 0.10, STEEL)
        b.box((s * 1.55, BAR_Y + 0.05, 0.02), (0.07, 0.12, 0.16), STEEL)
    b.box((1.42, 0.40, -0.10), (0.30, 0.16, 0.20), DARK)

    # --- the frame (green paint): cross beam, bar arms, wheel stubs ---
    b.beam((-1.42, 0.70, -0.45), (1.42, 0.70, -0.45), 0.10, 0.10, PAINT)
    for x in (-1.42, -0.55, 0.55, 1.42):
        b.beam((x, 0.70, -0.45), (x, BAR_Y + 0.06, -0.03), 0.07, 0.08, PAINT)
    for s in (-1, 1):
        b.beam((s * 1.0, 0.70, -0.45), (s * 1.0, 0.34, -0.78), 0.08, 0.08, PAINT)
    b.beam((-1.0, 0.34, -0.78), (1.0, 0.34, -0.78), 0.06, 0.06, STEEL)         # the axle

    # --- wheels: a worn steel rim with a red hub, a dark tyre ---
    for s in (-1, 1):
        x = s * 1.0
        b.cyl((x - 0.07, 0.32, -0.78), (x + 0.07, 0.32, -0.78), 0.32, DARK, 20)
        b.cyl((x - 0.085, 0.32, -0.78), (x + 0.085, 0.32, -0.78), 0.22, STEEL, 20)
        b.cyl((x - 0.11, 0.32, -0.78), (x + 0.11, 0.32, -0.78), 0.075, RED, 10)

    # --- the grain hopper (faded red) on a paint base, with a lid and the feed tube from the bar ---
    b.box((0.0, 0.80, -0.50), (1.0, 0.10, 0.78), PAINT)
    b.frustum(0.0, -0.50, 0.85, 1.45, (0.52, 0.42), (1.14, 0.80), RED)
    b.box((0.0, 1.48, -0.50), (1.22, 0.05, 0.88), STEEL)                       # the rim
    b.box((-0.28, 1.54, -0.50), (0.5, 0.07, 0.42), PAINT)                      # the hinged lid
    b.cyl((0.05, 0.43, -0.02), (0.46, 1.12, -0.40), 0.075, STEEL, 12)          # the feed tube
    b.cyl((0.46, 1.10, -0.40), (0.52, 1.22, -0.44), 0.095, DARK, 12)           # its cuff

    # --- the drive: a chain case on the right wheel and the pitman arm to the knife head ---
    b.box((1.16, 0.50, -0.62), (0.07, 0.30, 0.34), DARK)
    b.beam((1.2, 0.58, -0.45), (1.42, 0.42, -0.10), 0.03, 0.03, STEEL)

    # --- the A-frame drawbar, hitch eye and jack foot ---
    for s in (-1, 1):
        b.beam((s * 0.62, 0.70, -0.45), (s * 0.06, 0.68, HITCH_Z - 0.12), 0.08, 0.08, PAINT)
    b.beam((-0.5, 0.64, 0.45), (0.5, 0.64, 0.45), 0.06, 0.06, PAINT)           # the brace
    b.box((0.0, 0.68, HITCH_Z - 0.08), (0.18, 0.10, 0.14), STEEL)              # the clevis block
    b.cyl((0.0, 0.62, HITCH_Z - 0.01), (0.0, 0.74, HITCH_Z - 0.01), 0.07, DARK, 12)   # the hitch eye (reaches HITCH_Z)
    b.cyl((0.0, 0.025, 1.05), (0.0, 0.64, 1.05), 0.032, STEEL, 8)              # the jack leg
    b.box((0.0, 0.015, 1.05), (0.20, 0.03, 0.20), STEEL)                       # its foot
    return b.finish


def tris(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


def bounds(obj):
    vs = [obj.matrix_world @ v.co for v in obj.data.vertices]
    return ([min(v[i] for v in vs) for i in range(3)], [max(v[i] for v in vs) for i in range(3)])


def main():
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    lods = []
    for detail, suffix in ((True, "LOD0"), (False, "LOD1")):
        finish = reaper(detail)
        lods.append(finish("LF_Reaper_01_" + suffix))
    for o in bpy.data.objects:
        o.select_set(False)
    for o in lods:
        o.select_set(True)
    bpy.context.view_layer.objects.active = lods[0]
    dst = os.path.join(OUT, "LF_Reaper_01.fbx")
    bpy.ops.export_scene.fbx(filepath=dst, check_existing=False, use_selection=True,
                             object_types={"MESH", "OTHER", "EMPTY"}, use_custom_props=True,
                             add_leaf_bones=False, bake_anim=False)
    info = {"fbx": dst, "lods": []}
    for o in lods:
        lo, hi = bounds(o)
        # Report the bounds in the game frame (x, y up, z toward the vehicle).
        info["lods"].append({"name": o.name, "tris": tris(o), "verts": len(o.data.vertices),
                             "materials": [s.material.name for s in o.material_slots],
                             "min": [-hi[0], lo[2], lo[1]], "max": [-lo[0], hi[2], hi[1]]})
    with open(REPORT, "w") as f:
        json.dump({"LF_Reaper_01": info}, f, indent=1)
    print("REAPER", [(l["name"], l["tris"], l["min"], l["max"]) for l in info["lods"]])


main()
