# The 1989 balaclava, modelled in Blender from the base body's own head (spec "mesh.build"): no generation
# credits, no downloaded model. Run by the clothing fit stage:
#   blender --background --factory-startup --python model.py -- <job.json>
# job = { template_blend, out }. Writes <out>/balaclava.blend (objects Balaclava and Balaclava_item),
# <out>/Balaclava_BCR.tif, <out>/Balaclava_NMO.tif and <out>/review.png.
#
# Why not a library or Meshy model: a worn garment has to sit on Bohemia's base head to the millimetre, or the skull
# shows through or the cloth floats. A balaclava is the head's own surface with a hole for the eyes, so it is
# cut from Body_LOD0 (the Samples' body, Arma Public License, never committed), pushed out 6 mm and given
# a thickness. The knit is drawn here (numpy), tiled, 1024 px.
#
# Axes (Blender, as the template): z up, the face at -y, x to the character's left.
import bpy, bmesh, json, math, os, sys
import numpy as np
from mathutils import Vector

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
out = job["out"]
os.makedirs(out, exist_ok=True)

HEAD_C = Vector((0.0, 0.01, 1.66))   # the head's middle; nothing farther than 16.5 cm belongs to the cloth
NECK_Z = 1.525          # the cloth ends on the neck, above the collar line
EYE_Z, EYE_RX, EYE_RZ = 1.690, 0.060, 0.023   # eye opening: centre height, half width, half height (a superellipse)
SKIN_SLOTS = ("Basebody_01_Male_Head", "Neck", "Head_White_02")   # material slots that are skin (not eyes, teeth, lashes)
OFFSET = 0.006          # metres out from the skin
THICK = 0.003           # the cloth's own thickness (an open edge a hair thick)
TRIS = 2000             # the shell before it gets thickness; LOD0 ends near twice that
PX = 1024
STITCHES = 32           # stitches across one texture tile
TILE_M = 0.10           # one texture tile is 10 cm of cloth: a 3 mm stitch

bpy.ops.wm.open_mainfile(filepath=job["template_blend"], load_ui=False, use_scripts=False)
body = bpy.data.objects["Body_LOD0"]
slot_names = [(s.material.name.replace("\\", "/").split("/")[-1] if s.material else "") for s in body.material_slots]
keep_slots = {i for i, n in enumerate(slot_names) if n in SKIN_SLOTS}

# 1. The head and neck skin above the neck line, minus the eye opening.
bm = bmesh.new()
bm.from_mesh(body.data)
bm.faces.ensure_lookup_table()


def in_eye_hole(c):
    if c.y > -0.02:
        return False
    nx, nz = (c.x / EYE_RX), ((c.z - EYE_Z) / EYE_RZ)
    return abs(nx) ** 3.2 + abs(nz) ** 2.4 < 1.0


drop = []
for f in bm.faces:
    c = f.calc_center_median()
    # The template's head slot also holds jaw spike helpers and long rods: nothing beyond the head.
    if f.material_index not in keep_slots or min(v.co.z for v in f.verts) < NECK_Z or in_eye_hole(c) or max((v.co - HEAD_C).length for v in f.verts) > 0.165:
        drop.append(f)
bmesh.ops.delete(bm, geom=drop, context="FACES")
loose = [v for v in bm.verts if not v.link_faces]
bmesh.ops.delete(bm, geom=loose, context="VERTS")
me = bpy.data.meshes.new("Balaclava")
bm.to_mesh(me)
bm.free()
ob = bpy.data.objects.new("Balaclava", me)
bpy.context.scene.collection.objects.link(ob)
bpy.context.view_layer.objects.active = ob
ob.select_set(True)
print("LF_BALACLAVA shell faces", len(me.polygons), "slots kept", sorted(keep_slots))

# 2. Keep only the main piece (the template's neck and head are one piece; stray islands go).
bm = bmesh.new()
bm.from_mesh(me)
bm.faces.ensure_lookup_table()
seen, islands = set(), []
for f in bm.faces:
    if f.index in seen:
        continue
    stack, isl = [f], []
    seen.add(f.index)
    while stack:
        g = stack.pop()
        isl.append(g)
        for e in g.edges:
            for h in e.link_faces:
                if h.index not in seen:
                    seen.add(h.index)
                    stack.append(h)
    islands.append(isl)
islands.sort(key=len, reverse=True)
for isl in islands[1:]:
    bmesh.ops.delete(bm, geom=isl, context="FACES_ONLY")
loose = [v for v in bm.verts if not v.link_faces]
bmesh.ops.delete(bm, geom=loose, context="VERTS")
bm.to_mesh(me)
bm.free()
me.materials.clear()
print("LF_BALACLAVA islands", [len(i) for i in islands[:6]], "bounds", [tuple(round(min(v.co[a] for v in me.vertices),3) for a in range(3)), tuple(round(max(v.co[a] for v in me.vertices),3) for a in range(3))])

# 3. Smooth the face's relief (lips, nose, brows read as bumps through knitted cloth), push out, thicken, simplify.
mod = ob.modifiers.new("smooth", "SMOOTH")
mod.factor, mod.iterations = 0.5, 18
bpy.ops.object.modifier_apply(modifier=mod.name)
mod = ob.modifiers.new("dec", "DECIMATE")
mod.ratio = min(1.0, TRIS / max(1, len(me.polygons) * 2))
bpy.ops.object.modifier_apply(modifier=mod.name)
mod = ob.modifiers.new("smooth2", "SMOOTH")
mod.factor, mod.iterations = 0.5, 4
bpy.ops.object.modifier_apply(modifier=mod.name)
bm = bmesh.new()
bm.from_mesh(me)
bmesh.ops.triangulate(bm, faces=bm.faces)
bm.normal_update()
for v in bm.verts:
    v.co += v.normal * OFFSET
    if v.is_boundary and v.co.z < NECK_Z + 0.02:
        v.co.z = NECK_Z        # a straight hem
bm.to_mesh(me)
bm.free()
# Normals out: the shell comes from a body whose faces point out already; check by the crown.
crown = max(me.vertices, key=lambda v: v.co.z)
if crown.normal.z < 0:
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.flip_normals()
    bpy.ops.object.mode_set(mode="OBJECT")
mod = ob.modifiers.new("solid", "SOLIDIFY")
mod.thickness, mod.offset, mod.use_even_offset = THICK, 1.0, False
bpy.ops.object.modifier_apply(modifier=mod.name)
print("LF_BALACLAVA worn tris", sum(len(p.vertices) - 2 for p in me.polygons))
bpy.ops.object.shade_smooth()

# 4. UVs: a box projection at the tile size, so the tiled knit keeps one stitch size everywhere.
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.uv.cube_project(cube_size=TILE_M, correct_aspect=True, scale_to_bounds=False)
bpy.ops.object.mode_set(mode="OBJECT")

# 5. The ground model: the same cloth thrown down flat, its short axis up.
item = ob.copy()
item.data = ob.data.copy()
item.name = item.data.name = "Balaclava_item"
bpy.context.scene.collection.objects.link(item)
print("LF_BALACLAVA dbg ob", tuple(ob.location), tuple(ob.scale), tuple(ob.dimensions), tuple(ob.parent.name if ob.parent else "-"))
item.rotation_euler = (math.radians(90), 0, 0)
bpy.context.view_layer.update()
item.data.transform(item.matrix_world)
item.matrix_world.identity()
item.data.transform(__import__("mathutils").Matrix.Scale(0.16, 4, Vector((0, 0, 1))))   # flatten what is now the depth
bpy.context.view_layer.update()
zs = [v.co.z for v in item.data.vertices]
xs = [v.co.x for v in item.data.vertices]
ys = [v.co.y for v in item.data.vertices]
item.data.transform(__import__("mathutils").Matrix.Translation((-(min(xs) + max(xs)) / 2, -(min(ys) + max(ys)) / 2, -min(zs))))
print("LF_BALACLAVA item size", round(max(xs) - min(xs), 3), round(max(ys) - min(ys), 3), round(max(zs) - min(zs), 3))

# 6. The knit. Height field of V stitches in rows; the normal comes from its gradient, the colour from its
# depth (dark where the yarn is tucked). Tiles seamlessly because every period is an integer count.
n = PX
u = (np.arange(n) / n)[None, :].repeat(n, 0)
v = (np.arange(n) / n)[:, None].repeat(n, 1)
cu, cv = u * STITCHES, v * STITCHES
col = np.mod(cu, 1.0)
row = np.mod(cv, 1.0)
rowid = np.floor(cv)
# each stitch is two leaning yarn loops forming a V
left = np.abs((col - 0.5) * 2.0 - (row - 0.5) * 1.1)
right = np.abs((col - 0.5) * 2.0 + (row - 0.5) * 1.1)
yarn = np.clip(1.0 - np.minimum(left, right) * 1.3, 0, 1)
h = (yarn ** 0.8) * (1.0 - 0.25 * np.abs(row - 0.5))
rng = np.random.default_rng(1989)
fuzz = rng.normal(0, 0.035, (n, n))
h = np.clip(h + fuzz, 0, 1.2)
gy, gx = np.gradient(h)
nx = -gx * n / STITCHES * 0.35
ny = gy * n / STITCHES * 0.35
nz = np.ones_like(h)
ln = np.sqrt(nx * nx + ny * ny + nz * nz)
nx, ny = nx / ln, ny / ln
ao = np.clip(0.55 + 0.45 * h, 0, 1)
shade = 0.62 + 0.38 * np.clip(h, 0, 1)
rough = np.clip(0.9 - 0.12 * h + rng.normal(0, 0.02, (n, n)), 0, 1)
# near-black wool; game lighting reads about 1.7 times lighter than the picture, so it is painted dark
base = np.array([0.045, 0.047, 0.052])
bcr = np.dstack([(base[i] * shade) for i in range(3)] + [rough])
nmo = np.dstack([nx * 0.5 + 0.5, ny * 0.5 + 0.5, np.zeros_like(h), ao])


def srgb(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - 0.055)


bcr[..., :3] = srgb(bcr[..., :3])
for name, arr, cs in (("Balaclava_BCR", bcr, "sRGB"), ("Balaclava_NMO", nmo, "Non-Color")):
    img = bpy.data.images.new(name, n, n, alpha=True, float_buffer=False)
    img.colorspace_settings.name = cs
    img.pixels = np.flipud(arr).astype(np.float32).ravel()
    img.alpha_mode = "CHANNEL_PACKED"
    img.filepath_raw = os.path.join(out, name + ".tif")
    img.file_format = "TIFF"
    img.save()

# 7. Materials (the names the import matches), then the .blend the fit stage reads.
for o, mname in ((ob, "Balaclava"), (item, "Balaclava_item")):
    mat = bpy.data.materials.new(mname)
    o.data.materials.append(mat)
for o in (body,) + tuple(x for x in bpy.data.objects if x.name not in ("Balaclava", "Balaclava_item")):
    pass
keep = {"Balaclava", "Balaclava_item"}
for o in [o for o in bpy.data.objects if o.name not in keep]:
    bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "balaclava.blend"), compress=True)

# 8. One picture for the review (workbench render, front and side beside the body head).
if job.get("review", True):
    bpy.ops.wm.open_mainfile(filepath=job["template_blend"], load_ui=False, use_scripts=False)
    with bpy.data.libraries.load(os.path.join(out, "balaclava.blend"), link=False) as (src, dst):
        dst.objects = ["Balaclava"]
    b = dst.objects[0]
    bpy.context.scene.collection.objects.link(b)
    for o in bpy.data.objects:
        if o.type == "MESH" and o.name not in ("Balaclava", "Body_LOD0"):
            o.hide_render = True
    bpy.data.objects["Body_LOD0"].hide_render = False
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.render.resolution_x, sc.render.resolution_y = 1200, 600
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "OBJECT"
    b.color = (0.1, 0.1, 0.12, 1)
    bpy.data.objects["Body_LOD0"].color = (0.8, 0.6, 0.5, 1)
    cam_d = bpy.data.cameras.new("cam")
    cam_d.type = "ORTHO"
    cam_d.ortho_scale = 0.62
    cam = bpy.data.objects.new("cam", cam_d)
    sc.collection.objects.link(cam)
    sc.camera = cam
    for i, (loc, rot) in enumerate((((0, -2, 1.66), (math.pi / 2, 0, 0)), ((2, 0, 1.66), (math.pi / 2, 0, math.pi / 2)))):
        cam.location, cam.rotation_euler = loc, rot
        sc.render.filepath = os.path.join(out, "review_%d.png" % i)
        bpy.ops.render.render(write_still=True)
print("LF_BALACLAVA ok")
