# Builds the bank vault kit (tools/assets/bank-vault/kit.json) in headless Blender:
#   blender --background --factory-startup --python build.py -- <job.json>
# job.json = { kit, root, out, blender_lib }. Writes into <out>:
#   <Model>.fbx           one per model: LODs, UBX_ box colliders, and for the door its hinge bone
#   tex/<Mat>_BCR.tif, tex/<Mat>_NMO.tif   Enfusion channel packing (as clean.py writes it)
#   review/*.png          renders an agent reads
#   report.json           models, materials, colliders, triangles, bounds, the hinge pivot
#
# Only the door detail is generated (Meshy image-to-3D of front concept 1). The wall block, the round
# tunnel, the collar, the steel room, the deposit box wall and the cash trolley are built here as
# exact geometry: a generated wall is solid behind the door, and a fine grid of box doors comes out
# of image-to-3D as mush. Sizes are in metres.
#
# Space: build in Blender axes (X right, Y forward, Z up); B(x, y, z) takes Enfusion axes
# (x right, y up, z forward). The FBX export lands Blender (X, Y, Z) on Enfusion (x, z, y).
import bpy, bmesh, sys, json, os, math, struct
import numpy as np
from mathutils import Vector, Matrix

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
kit, root, out = job["kit"], job["root"], job["out"]
sys.path.insert(0, job["blender_lib"])
import materials as matlib  # tools/assets/pipeline/blender/materials.py

L = kit["layout_m"]
G = kit["generated"]
os.makedirs(os.path.join(out, "tex"), exist_ok=True)
os.makedirs(os.path.join(out, "review"), exist_ok=True)
report = {"models": {}, "materials": {}, "notes": []}
rng = np.random.default_rng(1989)


def B(x, y, z):
    return Vector((x, z, y))


def b2e(v):
    return [round(float(v[0]), 4), round(float(v[2]), 4), round(float(v[1]), 4)]


# ================================================================ textures
def write_tiff(path, rgba):
    # Baseline uncompressed 8-bit RGBA TIFF with unassociated alpha (the same writer as clean.py).
    h, w, _ = rgba.shape
    data = rgba.tobytes()
    tags = [(256, 4, 1, w), (257, 4, 1, h), (258, 3, 4, None), (259, 3, 1, 1), (262, 3, 1, 2), (273, 4, 1, None),
            (277, 3, 1, 4), (278, 4, 1, h), (279, 4, 1, len(data)), (284, 3, 1, 1), (338, 3, 1, 2)]
    ifd_at = 8
    bits_at = ifd_at + 2 + len(tags) * 12 + 4
    data_at = bits_at + 8
    with open(path, "wb") as f:
        f.write(struct.pack("<2sHI", b"II", 42, ifd_at))
        f.write(struct.pack("<H", len(tags)))
        for tag, typ, count, value in tags:
            if tag == 258:
                value = bits_at
            if tag == 273:
                value = data_at
            if typ == 3 and count == 1:
                f.write(struct.pack("<HHIHH", tag, typ, count, value, 0))
            else:
                f.write(struct.pack("<HHII", tag, typ, count, value))
        f.write(struct.pack("<I", 0))
        f.write(struct.pack("<4H", 8, 8, 8, 8))
        f.write(data)


def noise(px, sx, sy=None):
    # Periodic (tileable) gaussian noise, zero mean, unit deviation; sx, sy in pixels.
    sy = sx if sy is None else sy
    w = rng.standard_normal((px, px))
    fy = np.fft.fftfreq(px)[:, None]
    fx = np.fft.fftfreq(px)[None, :]
    g = np.exp(-2 * (np.pi ** 2) * ((fx * sx) ** 2 + (fy * sy) ** 2))
    n = np.real(np.fft.ifft2(np.fft.fft2(w) * g))
    return (n - n.mean()) / (n.std() + 1e-9)


def normal_from_height(h, strength):
    # OpenGL-style normal map in 0..1 from a periodic height field (rows run down the image).
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5 * strength
    dy = (np.roll(h, 1, 0) - np.roll(h, -1, 0)) * 0.5 * strength
    n = np.dstack([-dx, -dy, np.ones_like(h)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    return n * 0.5 + 0.5


TEX = {}  # material name -> {"base_png": path, "px": px, "tile_m": metres per texture repeat}


def save_material(name, base, rough, metal, normal, tile_m):
    px = base.shape[0]
    bcr = np.dstack([base[:, :, :3], rough])
    nmo = np.dstack([normal[:, :, 0], 1.0 - normal[:, :, 1], metal, np.ones((px, px))])
    write_tiff(os.path.join(out, "tex", name + "_BCR.tif"), (np.clip(bcr, 0, 1) * 255 + 0.5).astype(np.uint8))
    write_tiff(os.path.join(out, "tex", name + "_NMO.tif"), (np.clip(nmo, 0, 1) * 255 + 0.5).astype(np.uint8))
    png = os.path.join(out, "tex", name + "_base.png")
    img = bpy.data.images.new(name + "_base", px, px)
    img.pixels.foreach_set(np.dstack([np.clip(base[:, :, :3], 0, 1), np.ones((px, px))])[::-1].astype(np.float32).ravel())
    img.filepath_raw = png
    img.file_format = "PNG"
    img.save()
    TEX[name] = {"base_png": png, "px": px, "tile_m": tile_m}
    report["materials"][name] = {"px": px, "tile_m": tile_m, "mean_base": [round(float(v), 3) for v in base[:, :, :3].mean(axis=(0, 1))],
                                 "mean_rough": round(float(rough.mean()), 3), "mean_metal": round(float(metal.mean()), 3)}


def scratches(px, count, length, value, into, mask_into=None):
    # Short straight scratches, wrapped at the edges so the texture still tiles.
    for _ in range(count):
        x, y = rng.uniform(0, px, 2)
        a = rng.uniform(0, math.pi)
        n = int(rng.uniform(0.3, 1.0) * length)
        for t in range(n):
            i, j = int(y + math.sin(a) * t) % px, int(x + math.cos(a) * t) % px
            into[i, j] = value
            if mask_into is not None:
                mask_into[i, j] = 1.0


# ---------------------------------------------------------------- the door: Meshy's maps, remapped
def load_png(path, px):
    img = bpy.data.images.load(path)
    img.scale(px, px)
    arr = np.empty(px * px * 4, np.float32)
    img.pixels.foreach_get(arr)
    return arr.reshape(px, px, 4)[::-1]


dpx = kit["texture_px"]["door"]
mdir = os.path.join(root, G["maps"])
dbase = load_png(os.path.join(mdir, "tex_base_color.png"), dpx)
drough = load_png(os.path.join(mdir, "tex_roughness.png"), dpx)[:, :, 0]
dmetal = load_png(os.path.join(mdir, "tex_metallic.png"), dpx)[:, :, 0]
dnormal = load_png(os.path.join(mdir, "tex_normal.png"), dpx)
plan = kit["materials"]["door"]
masks = matlib.region_masks(plan, dmetal, None)
b3, r1, m1 = matlib.apply(plan, dbase[:, :, :3], drough, dmetal, masks)
save_material("BankVault_Door", b3, r1, m1, dnormal, None)
# The kit's painted steel takes the door's own paint colour, so the frame and the door match.
# The atlas also holds the generated concrete block, so sample the texture under the door plate's
# own UVs (faces in front of the wall within the door radius) and keep the painted, non-metal pixels.
bpy.ops.import_scene.gltf(filepath=os.path.join(root, G["glb"]))
samples = []
for o in [o for o in bpy.data.objects if o.type == "MESH"]:
    me = o.data
    uvl = me.uv_layers[0].data
    for poly in me.polygons:
        c = o.matrix_world @ poly.center
        if c.y < G["cut_y"] and math.hypot(c.x - G["centre"][0], c.z - G["centre"][1]) < G["door_radius"] * 0.9:
            # The face's UV centre: its corners sit on the atlas island borders, which are dark padding.
            u = sum(uvl[li].uv[0] for li in poly.loop_indices) / poly.loop_total
            v = sum(uvl[li].uv[1] for li in poly.loop_indices) / poly.loop_total
            samples.append((int(min(max(1 - v, 0), 0.9999) * dpx), int(min(max(u, 0), 0.9999) * dpx), poly.area))
smp = np.array(samples)
rows, cols_, area = smp[:, 0].astype(int), smp[:, 1].astype(int), smp[:, 2]
rgb = dbase[rows, cols_, :3]
# Weighted by face area: the flat painted plate is a few big faces, the rivets and edges many small ones.
report["paint_samples"] = [len(samples), round(float(np.median(dmetal[rows, cols_])), 3)]
paint_rgb = (rgb * area[:, None]).sum(axis=0) / area.sum() if len(samples) > 200 else np.array([0.40, 0.45, 0.42])
report["paint_rgb"] = [round(float(v), 3) for v in paint_rgb]

kpx = kit["texture_px"]["kit"]

# ---------------------------------------------------------------- concrete
h = noise(kpx, 1.5) * 0.5 + noise(kpx, 6) * 0.35 + noise(kpx, 40) * 0.6
pores = noise(kpx, 1.0) > 2.3
base = np.ones((kpx, kpx, 3)) * np.array([0.47, 0.46, 0.43])
base += (noise(kpx, 90) * 0.035 + noise(kpx, 12) * 0.02)[:, :, None]
base[pores] *= 0.55
save_material("BankVault_Concrete", np.clip(base, 0, 1), np.full((kpx, kpx), 0.92), np.zeros((kpx, kpx)),
              normal_from_height(h - pores * 2.0, 0.6), 2.0)

# ---------------------------------------------------------------- painted steel (grey-green, worn)
base = np.ones((kpx, kpx, 3)) * paint_rgb
base *= (1 + noise(kpx, 70) * 0.04 + noise(kpx, 5) * 0.015)[:, :, None]
scr = np.zeros((kpx, kpx))
bright = np.zeros((kpx, kpx))
scratches(kpx, 160, 60, 1.0, bright, scr)
base = base * (1 - scr[:, :, None]) + np.array([0.55, 0.56, 0.55]) * scr[:, :, None]
rough = np.full((kpx, kpx), 0.62) - scr * 0.2
metal = scr * 0.6
save_material("BankVault_Paint", np.clip(base, 0, 1), rough, metal, normal_from_height(noise(kpx, 3) * 0.3 - scr, 0.8), 1.5)

# ---------------------------------------------------------------- brushed steel panels (the room's walls, floor and ceiling)
streak = noise(kpx, 80, 0.7)
base = np.ones((kpx, kpx, 3)) * 0.50
base += (streak * 0.03 + noise(kpx, 200) * 0.03)[:, :, None]
seam = np.zeros((kpx, kpx))
w = max(2, kpx // 256)
seam[:w, :] = 1
seam[-w:, :] = 1
seam[:, :w] = 1
seam[:, -w:] = 1
rivet = np.zeros((kpx, kpx))
yy, xx = np.mgrid[0:kpx, 0:kpx]
step = kpx // 8
for c in range(0, kpx, step):
    for (cy, cx) in ((3 * w, c + step // 2), (c + step // 2, 3 * w), (kpx - 3 * w, c + step // 2), (c + step // 2, kpx - 3 * w)):
        rivet[((yy - cy) ** 2 + (xx - cx) ** 2) <= (1.6 * w) ** 2] = 1
base = base * (1 - seam[:, :, None] * 0.65) * (1 + rivet[:, :, None] * 0.25)
rough = np.clip(0.42 + streak * 0.04 + seam * 0.3, 0, 1)
metal = np.clip(0.85 - seam * 0.5, 0, 1)
save_material("BankVault_Steel", np.clip(base, 0, 1), rough, metal, normal_from_height(streak * 0.15 - seam * 3 + rivet * 2, 0.7), 1.0)

# ---------------------------------------------------------------- dark: keyholes, casters, gaps
spx = 256
save_material("BankVault_Dark", np.clip(np.ones((spx, spx, 3)) * 0.06 + noise(spx, 4)[:, :, None] * 0.01, 0, 1),
              np.full((spx, spx), 0.85), np.zeros((spx, spx)), normal_from_height(noise(spx, 2) * 0.2, 0.5), 0.5)

# ---------------------------------------------------------------- cash: a banknote brick atlas
# u 0..0.5 is the face of the top note, u 0.5..1 the side of the stack; a paper band across the middle.
cpx = kpx
base = np.zeros((cpx, cpx, 3))
half = cpx // 2
note = np.array([0.62, 0.67, 0.58])
band = np.array([0.80, 0.74, 0.60])
face = np.ones((cpx, half, 3)) * note
fy, fx = np.mgrid[0:cpx, 0:half] / np.array([cpx, half])[:, None, None]
pattern = 0.03 * np.sin(fx * 90) * np.sin(fy * 70) + 0.02 * np.sin((fx + fy) * 160)
face += pattern[:, :, None]
frame = ((np.abs(fx - 0.5) > 0.43) | (np.abs(fy - 0.5) > 0.40)) & ((np.abs(fx - 0.5) < 0.455) & (np.abs(fy - 0.5) < 0.425))
face[frame] *= 0.8
face[np.abs(fx - 0.5) < 0.11] = band
side = np.ones((cpx, half, 3)) * note
lines = (np.arange(cpx) // max(2, cpx // 256)) % 2
side *= (0.9 + 0.1 * lines)[:, None, None]
side[np.abs(fx - 0.5) < 0.11] = band
base[:, :half] = face
base[:, half:] = side
save_material("BankVault_Cash", np.clip(base, 0, 1), np.full((cpx, cpx), 0.88), np.zeros((cpx, cpx)),
              normal_from_height(lines[:, None] * np.ones((1, cpx)) * 0.4, 0.4), None)

MATS = ["BankVault_Door", "BankVault_Concrete", "BankVault_Paint", "BankVault_Steel", "BankVault_Dark", "BankVault_Cash"]


def blender_material(name):
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name)  # the FBX SourceMaterial name the .xob.meta maps to the .emat
    m.use_nodes = True
    tex = m.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(TEX[name]["base_png"])
    m.node_tree.links.new(tex.outputs["Color"], m.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
    m.node_tree.nodes.active = tex
    return m


# ================================================================ geometry
class Part:
    """A bmesh with a material per face and box-projected UVs (in metres / the material's tile)."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.auto = self.bm.faces.layers.int.new("auto")
        self.mats = []

    def mi(self, mat):
        if mat not in self.mats:
            self.mats.append(mat)
        return self.mats.index(mat)

    def face(self, pts, mat, want=None, uvs=None):
        vs = [self.bm.verts.new(p) for p in pts]
        f = self.bm.faces.new(vs)
        f.normal_update()
        if want is not None and f.normal.dot(want) < 0:
            f.normal_flip()
        f.material_index = self.mi(mat)
        if uvs:
            by_vert = dict(zip(vs, uvs))  # a flip reverses the loops, so map the UVs by vertex
            for loop in f.loops:
                loop[self.uv].uv = by_vert[loop.vert]
        else:
            f[self.auto] = 1
        return f

    def box(self, lo, hi, mat, mats=None, skip=()):
        # lo, hi in Enfusion axes. mats maps a side ("+x", "-x", "+y", "-y", "+z", "-z") to a material.
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        c = {
            "+x": ([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], B(1, 0, 0)),
            "-x": ([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], B(-1, 0, 0)),
            "+y": ([(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)], B(0, 1, 0)),
            "-y": ([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], B(0, -1, 0)),
            "+z": ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], B(0, 0, 1)),
            "-z": ([(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)], B(0, 0, -1)),
        }
        for side, (pts, n) in c.items():
            if side in skip:
                continue
            self.face([B(*p) for p in pts], (mats or {}).get(side, mat), want=n)

    def cylinder(self, centre, axis, radius, length, mat, segs=24, caps=True, cap_mat=None):
        # centre: Enfusion point at the start; axis: "x", "y" or "z" (Enfusion); runs +length along it.
        bm2 = bmesh.new()
        bmesh.ops.create_cone(bm2, cap_ends=caps, cap_tris=False, segments=segs, radius1=radius, radius2=radius, depth=length)
        # create_cone runs along Blender Z (Enfusion y), centred: turn it onto the wanted axis, then move.
        ax = {"x": B(1, 0, 0), "y": B(0, 1, 0), "z": B(0, 0, 1)}[axis]
        m = Matrix.Translation(B(*centre) + ax * (length / 2)) @ {"y": Matrix.Identity(4), "z": Matrix.Rotation(math.radians(-90), 4, "X"), "x": Matrix.Rotation(math.radians(90), 4, "Y")}[axis]
        bmesh.ops.transform(bm2, matrix=m, verts=bm2.verts)
        self.merge(bm2, mat, cap_mat)

    def merge(self, bm2, mat, cap_mat=None):
        for f in bm2.faces:
            pts = [v.co.copy() for v in f.verts]
            n = f.normal.copy()
            is_cap = cap_mat and len(f.verts) > 4
            self.face(pts, cap_mat if is_cap else mat, want=n)
        bm2.free()

    def finish(self):
        # Box-projected UVs for every face that has none, then one mesh object.
        for f in self.bm.faces:
            if not f[self.auto]:
                continue
            mat = self.mats[f.material_index]
            tile = TEX[mat]["tile_m"] or 1.0
            n = f.normal
            ax = max(range(3), key=lambda k: abs(n[k]))
            for loop in f.loops:
                p = loop.vert.co
                if ax == 0:
                    u, v = p.y, p.z
                elif ax == 1:
                    u, v = p.x, p.z
                else:
                    u, v = p.x, p.y
                loop[self.uv].uv = (u / tile, v / tile)
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=0.0001)
        me = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(me)
        self.bm.free()
        o = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(o)
        for mat in self.mats:
            o.data.materials.append(blender_material(mat))
        return o


def activate(o):
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o


def tris(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


def join(objs, name):
    objs = [o for o in objs if o is not None]
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    o.name = name
    o.data.name = name
    return o


def copy(o, name):
    c = o.copy()
    c.data = o.data.copy()
    c.name = name
    bpy.context.scene.collection.objects.link(c)
    return c


def decimate(o, ratio):
    if ratio >= 1:
        return
    activate(o)
    mod = o.modifiers.new("d", "DECIMATE")
    mod.ratio = ratio
    mod.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier="d")


def collider(model, i, lo, hi, cfg):
    # One UBX_ box, its origin at its own centre (wiki: FBX Import).
    name = "UBX_%s_%02d" % (model, i)
    bm2 = bmesh.new()
    bmesh.ops.create_cube(bm2, size=1)
    a, b = B(*lo), B(*hi)
    size = Vector((abs(b.x - a.x), abs(b.y - a.y), abs(b.z - a.z)))
    for v in bm2.verts:
        v.co = Vector((v.co.x * size.x, v.co.y * size.y, v.co.z * size.z))
    me = bpy.data.meshes.new(name)
    bm2.to_mesh(me)
    bm2.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    o.location = (a + b) / 2
    o["usage"] = cfg["layerPreset"]
    o.display_type = "WIRE"
    o.hide_render = True
    return o


# ================================================================ the generated door and frame detail
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=os.path.join(root, G["glb"]))
src = [o for o in bpy.data.objects if o.type == "MESH"]
gen = join(src, "generated")
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for o in list(bpy.data.objects):
    if o is not gen:
        bpy.data.objects.remove(o, do_unlink=True)
while len(gen.data.uv_layers) > 1:
    gen.data.uv_layers.remove(gen.data.uv_layers[-1])
gen.data.uv_layers[0].name = "UVMap"
gen.data.materials.clear()
gen.data.materials.append(blender_material("BankVault_Door"))
report["generated"] = {"tris": tris(gen)}

s = G["scale"]
cx, cz = G["centre"]
cy0 = L["door_centre_y"]
# Model units -> metres: centre the door on x, put the wall face at z 0 (Enfusion), turn it to face +z.
#   Enfusion x = -(mx - cx) * s ; y = (mz - cz) * s + door_centre_y ; z = -(my - wall_face_y) * s
to_m = Matrix.Translation(B(0, cy0, 0)) @ Matrix.Diagonal((-s, -s, s, 1)) @ Matrix.Translation(Vector((-cx, -G["wall_face_y"], -cz)))

bm = bmesh.new()
bm.from_mesh(gen.data)
door_faces, frame_faces, drop = [], [], []
for f in bm.faces:
    c = f.calc_center_median()
    r = math.hypot(c.x - cx, c.z - cz)
    if c.y > G["cut_y"]:
        drop.append(f)  # the generated wall block: rebuilt below as real geometry with a tunnel
    elif r < G["door_radius"] or (r < G["plate_radius"] and c.y < G["plate_y"]) or (c.x < G["hinge_side_x"] and abs(c.z - cz) < G["door_radius"] + 0.2):
        door_faces.append(f)  # the door, its hinge arm and the hinge barrels turn about the barrel axis
    else:
        frame_faces.append(f)
bm.free()


def split(faces_keep_test, name):
    o = copy(gen, name)
    bm2 = bmesh.new()
    bm2.from_mesh(o.data)
    kill = []
    for f in bm2.faces:
        c = f.calc_center_median()
        if not faces_keep_test(c):
            kill.append(f)
    bmesh.ops.delete(bm2, geom=kill, context="FACES")
    bm2.transform(to_m)
    bm2.normal_update()
    bm2.to_mesh(o.data)
    bm2.free()
    for p in o.data.polygons:
        p.use_smooth = True
    return o


def is_door(c):
    r = math.hypot(c.x - cx, c.z - cz)
    # The door plate, the hinge straps standing proud of the frame ring at 12 and 6 o'clock, and the
    # hinge arm with its barrels on the left.
    return c.y <= G["cut_y"] and (r < G["door_radius"] or (r < G["plate_radius"] and c.y < G["plate_y"]) or
                                  (c.x < G["hinge_side_x"] and abs(c.z - cz) < G["door_radius"] + 0.2))


def is_frame(c):
    return c.y <= G["cut_y"] and not is_door(c)


gen_door = split(is_door, "gen_door")
gen_frame = split(is_frame, "gen_frame")
bpy.data.objects.remove(gen, do_unlink=True)
pivot = [-(G["hinge_x"] - cx) * s, cy0, -(G["hinge_y"] - G["wall_face_y"]) * s]  # Enfusion metres, in the front's space
report["pivot"] = [round(v, 4) for v in pivot]
door_r = G["door_radius"] * s
cut_z = -(G["cut_y"] - G["wall_face_y"]) * s
report["door"] = {"radius_m": round(door_r, 3), "back_z": round(cut_z, 3), "generated_tris": tris(gen_door)}
report["frame_generated_tris"] = tris(gen_frame)

# ================================================================ models
MODELS = {}


def model(name, lods, colliders, extra=None):
    MODELS[name] = {"lods": lods, "colliders": colliders, "extra": extra or []}


def lod_set(name, full, simple=None, ratios=kit["lod_ratios"], generated=None):
    """LOD0 is every part; later LODs decimate the generated part and drop the small details."""
    lods = []
    for i in range(len(ratios) + 1):
        parts = [copy(o, "%s_p%d_%d" % (name, i, k)) for k, o in enumerate(full if i == 0 else (simple or full))]
        if generated is not None:
            g = copy(generated, "%s_g%d" % (name, i))
            if i > 0:
                decimate(g, ratios[i - 1])
            parts.append(g)
        lods.append(join(parts, "%s_LOD%d" % (name, i)))
    for o in full + (simple or []):
        if o.name in bpy.data.objects:
            bpy.data.objects.remove(o, do_unlink=True)
    if generated is not None:
        bpy.data.objects.remove(generated, do_unlink=True)
    return lods


cyc = L["door_centre_y"]
tr = L["tunnel_radius"]
W = L["wall"]
hw, wh, wd = W["half_width"], W["height"], W["depth"]
floor_y = L["floor_y"]

# ---------------------------------------------------------------- VaultFront: wall block, tunnel, collar, step
front = Part("front_full")
segs = 64
angles = sorted(set([i * 2 * math.pi / segs for i in range(segs)] + [math.atan2(wh - cyc, hw), math.atan2(wh - cyc, -hw), math.atan2(-cyc, -hw), math.atan2(-cyc, hw)]))
angles = [a % (2 * math.pi) for a in angles]
angles = sorted(set(angles))


def square_pt(a):
    dx, dy = math.cos(a), math.sin(a)
    t = min(hw / abs(dx) if abs(dx) > 1e-9 else 1e9, ((wh - cyc) if dy > 0 else cyc) / abs(dy) if abs(dy) > 1e-9 else 1e9)
    return dx * t, cyc + dy * t


n = len(angles)
for i in range(n):
    a0, a1 = angles[i], angles[(i + 1) % n]
    s0, s1 = square_pt(a0), square_pt(a1)
    c0 = (math.cos(a0) * tr, cyc + math.sin(a0) * tr)
    c1 = (math.cos(a1) * tr, cyc + math.sin(a1) * tr)
    for z, want in ((0.0, B(0, 0, 1)), (-wd, B(0, 0, -1))):
        front.face([B(s0[0], s0[1], z), B(s1[0], s1[1], z), B(c1[0], c1[1], z), B(c0[0], c0[1], z)], "BankVault_Concrete", want)
    # The tunnel lining faces the axis.
    mid = (a0 + a1) / 2
    front.face([B(c0[0], c0[1], 0), B(c1[0], c1[1], 0), B(c1[0], c1[1], -wd), B(c0[0], c0[1], -wd)], "BankVault_Steel",
               B(-math.cos(mid), -math.sin(mid), 0))
front.box((-hw, 0, -wd), (hw, wh, 0), "BankVault_Concrete", skip=("+z", "-z"))
# The tunnel floor: a steel plate at floor height, its front 2 mm behind the wall face.
half_chord = math.sqrt(max(tr ** 2 - (floor_y - cyc) ** 2, 0))
front.box((-half_chord, cyc - tr, -wd + 0.002), (half_chord, floor_y, -0.002), "BankVault_Steel")
# The collar: a painted steel ring behind the generated frame ring, so no gap shows when the door opens.
collar = (tr, door_r + 0.25, 0.08)
for i in range(segs):
    a0, a1 = 2 * math.pi * i / segs, 2 * math.pi * (i + 1) / segs
    p = lambda r, a, z: B(math.cos(a) * r, cyc + math.sin(a) * r, z)
    front.face([p(collar[0], a0, collar[2]), p(collar[1], a0, collar[2]), p(collar[1], a1, collar[2]), p(collar[0], a1, collar[2])], "BankVault_Paint", B(0, 0, 1))
    mid = (a0 + a1) / 2
    front.face([p(collar[1], a0, 0), p(collar[1], a1, 0), p(collar[1], a1, collar[2]), p(collar[1], a0, collar[2])], "BankVault_Paint", B(math.cos(mid), math.sin(mid), 0))
    front.face([p(collar[0], a0, 0), p(collar[0], a1, 0), p(collar[0], a1, collar[2]), p(collar[0], a0, collar[2])], "BankVault_Steel", B(-math.cos(mid), -math.sin(mid), 0))
# A steel tread step in front of the sill.
st = L["step"]
front.box((-st["half_width"], 0, 0.30), (st["half_width"], st["height"], 0.30 + st["depth"]), "BankVault_Steel")
front_obj = front.finish()
cfg = kit["colliders"]["VaultFront"]
cols = []
# The wall around the round opening as stacked boxes that follow the circle, with a walkable gap.
bands = 10
y_lo = floor_y
for k in range(bands):
    ya = floor_y + (cyc + tr - floor_y) * k / bands
    yb = floor_y + (cyc + tr - floor_y) * (k + 1) / bands
    yc = (ya + yb) / 2
    half = math.sqrt(max(tr ** 2 - (yc - cyc) ** 2, 0)) - 0.02
    half = max(half, 0.0)
    cols.append(((-hw, ya, -wd), (-half, yb, 0)))
    cols.append(((half, ya, -wd), (hw, yb, 0)))
cols.append(((-hw, cyc + tr, -wd), (hw, wh, 0)))          # above the opening
cols.append(((-hw, 0, -wd), (hw, floor_y, 0)))            # the sill and the tunnel floor
cols.append(((-0.75, 0, 0), (0.75, 0.24, 0.30)))           # the frame ring's bottom lip
cols.append(((-st["half_width"], 0, 0.30), (st["half_width"], st["height"], 0.30 + st["depth"])))  # the step
front_cols = [collider("VaultFront", i, lo, hi, cfg) for i, (lo, hi) in enumerate(cols)]
model("VaultFront", lod_set("VaultFront", [front_obj], generated=gen_frame), front_cols)

# ---------------------------------------------------------------- VaultDoor: the generated door on its hinge bone, with the plug
pl = L["plug"]
door = Part("door_full")
back_z = cut_z
# The back of the door around the plug, and the plug itself: thick steel with locking bolts.
for i in range(segs):
    a0, a1 = 2 * math.pi * i / segs, 2 * math.pi * (i + 1) / segs
    p = lambda r, a, z: B(math.cos(a) * r, cyc + math.sin(a) * r, z)
    door.face([p(pl["radius"], a0, back_z), p(door_r, a0, back_z), p(door_r, a1, back_z), p(pl["radius"], a1, back_z)], "BankVault_Paint", B(0, 0, -1))
door.cylinder((0, cyc, back_z - pl["depth"]), "z", pl["radius"], pl["depth"] + 0.01, "BankVault_Steel", segs=48, cap_mat="BankVault_Steel")
door_simple = Part("door_simple")
door_simple.cylinder((0, cyc, back_z - pl["depth"]), "z", pl["radius"], pl["depth"] + 0.01, "BankVault_Steel", segs=24, cap_mat="BankVault_Steel")
for i in range(12):
    a = 2 * math.pi * i / 12
    if math.sin(a) < -0.6:
        continue  # no bolt where the plug meets the tunnel floor
    bx, by = math.cos(a), math.sin(a)
    zmid = back_z - pl["depth"] / 2
    # Each bolt is a short cylinder along its radius: build it on z, then turn it in the xy plane.
    bm2 = bmesh.new()
    bmesh.ops.create_cone(bm2, cap_ends=True, cap_tris=False, segments=12, radius1=pl["bolt_radius"], radius2=pl["bolt_radius"], depth=pl["bolt_length"])
    # Blender Z (Enfusion y) is the cone axis; point it along (bx, by) in Enfusion x/y.
    rot = Vector((0, 0, 1)).rotation_difference(Vector((bx, 0, by))).to_matrix().to_4x4()
    centre = B(bx * (pl["radius"] + pl["bolt_length"] / 2 - 0.02), cyc + by * (pl["radius"] + pl["bolt_length"] / 2 - 0.02), zmid)
    bmesh.ops.transform(bm2, matrix=Matrix.Translation(centre) @ rot, verts=bm2.verts)
    door.merge(bm2, "BankVault_Steel")
door_obj = door.finish()
door_simple_obj = door_simple.finish()
door_lods = lod_set("VaultDoor", [door_obj], [door_simple_obj], generated=gen_door)
# Move the door so its origin is the hinge pivot: the door entity sits at `pivot` in the front's space.
shift = Matrix.Translation(-B(*pivot))
for o in door_lods:
    o.data.transform(shift)
cfg = kit["colliders"]["VaultDoor"]
door_cols = [collider("VaultDoor", 0, (-door_r - pivot[0], cyc - door_r - pivot[1], back_z - pivot[2]),
                      (door_r + 0.25 - pivot[0], cyc + door_r - pivot[1], back_z + 0.32 - pivot[2]), cfg)]
model("VaultDoor", door_lods, door_cols, extra=["bone"])

# ---------------------------------------------------------------- VaultRoom: the steel room behind the front
R = L["room"]
rw, rd, rh, t = R["width"], R["depth"], R["height"], R["wall"]
z_front = -wd
z_back = z_front - rd
y_top = floor_y + rh
room = Part("room_full")
room.box((-rw / 2 - t, 0, z_back - t), (rw / 2 + t, floor_y, z_front), "BankVault_Concrete", mats={"+y": "BankVault_Steel"}, skip=("+z",))
room.box((-rw / 2 - t, y_top, z_back - t), (rw / 2 + t, y_top + t, z_front), "BankVault_Concrete", mats={"-y": "BankVault_Steel"}, skip=("+z",))
room.box((-rw / 2 - t, floor_y, z_back - t), (-rw / 2, y_top, z_front), "BankVault_Concrete", mats={"+x": "BankVault_Steel"}, skip=("+z", "+y", "-y"))
room.box((rw / 2, floor_y, z_back - t), (rw / 2 + t, y_top, z_front), "BankVault_Concrete", mats={"-x": "BankVault_Steel"}, skip=("+z", "+y", "-y"))
room.box((-rw / 2, floor_y, z_back - t), (rw / 2, y_top, z_back), "BankVault_Concrete", mats={"+z": "BankVault_Steel"}, skip=("+y", "-y", "+x", "-x"))
# A skirting rail and a ceiling rail in painted steel, where panels meet.
for y0, y1 in ((floor_y, floor_y + 0.08), (y_top - 0.06, y_top)):
    room.box((-rw / 2, y0, z_back), (-rw / 2 + 0.03, y1, z_front), "BankVault_Paint")
    room.box((rw / 2 - 0.03, y0, z_back), (rw / 2, y1, z_front), "BankVault_Paint")
    room.box((-rw / 2, y0, z_back), (rw / 2, y1, z_back + 0.03), "BankVault_Paint")
room_obj = room.finish()
cfg = kit["colliders"]["VaultRoom"]
room_cols = [collider("VaultRoom", i, lo, hi, cfg) for i, (lo, hi) in enumerate([
    ((-rw / 2 - t, 0, z_back - t), (rw / 2 + t, floor_y, z_front)),
    ((-rw / 2 - t, y_top, z_back - t), (rw / 2 + t, y_top + t, z_front)),
    ((-rw / 2 - t, floor_y, z_back - t), (-rw / 2, y_top, z_front)),
    ((rw / 2, floor_y, z_back - t), (rw / 2 + t, y_top, z_front)),
    ((-rw / 2, floor_y, z_back - t), (rw / 2, y_top, z_back)),
])]
model("VaultRoom", lod_set("VaultRoom", [room_obj]), room_cols)

# ---------------------------------------------------------------- VaultDepositWall: four riveted bays of boxes (concept 9)
# Standing on its origin, its back on z 0, its face toward +z.
dw = Part("deposit_full")
ds = Part("deposit_simple")
bays, cols_per, rows = 4, 3, 14
W2, H2, D2 = 2.6, 2.2, 0.45
post, plinth, top = 0.07, 0.10, 0.08
for prt in (dw, ds):
    prt.box((-W2 / 2, 0, 0), (W2 / 2, H2, D2), "BankVault_Paint")
bay_w = (W2 - post * (bays + 1)) / bays
cell_w = bay_w / cols_per
cell_h = (H2 - plinth - top) / rows
gap = 0.006
for b in range(bays + 1):
    x = -W2 / 2 + b * (bay_w + post)
    for prt in (dw, ds):
        prt.box((x, 0, D2), (x + post, H2, D2 + 0.03), "BankVault_Paint")
    for k in range(9):
        y = plinth + (H2 - plinth - top) * (k + 0.5) / 9
        dw.box((x + post / 2 - 0.012, y - 0.012, D2 + 0.03), (x + post / 2 + 0.012, y + 0.012, D2 + 0.042), "BankVault_Paint")
for prt in (dw, ds):
    prt.box((-W2 / 2, 0, D2), (W2 / 2, plinth, D2 + 0.03), "BankVault_Dark")
    prt.box((-W2 / 2, H2 - top, D2), (W2 / 2, H2, D2 + 0.035), "BankVault_Paint")
for b in range(bays):
    x0 = -W2 / 2 + post + b * (bay_w + post)
    for c in range(cols_per):
        for r in range(rows):
            lo = (x0 + c * cell_w + gap, plinth + r * cell_h + gap, D2)
            hi = (x0 + (c + 1) * cell_w - gap, plinth + (r + 1) * cell_h - gap, D2 + 0.014)
            dw.box(lo, hi, "BankVault_Paint", skip=("-z",))
            mx = (lo[0] + hi[0]) / 2
            my = (lo[1] + hi[1]) / 2
            # Two narrow keyholes side by side on a steel escutcheon at the right, the pull at the left.
            kx0 = hi[0] - 0.05
            dw.box((kx0 - 0.004, my - 0.016, hi[2]), (kx0 + 0.036, my + 0.016, hi[2] + 0.002), "BankVault_Steel", skip=("-z",))
            for kx in (kx0 + 0.008, kx0 + 0.024):
                dw.face([B(kx - 0.0025, my - 0.007, hi[2] + 0.0025), B(kx + 0.0025, my - 0.007, hi[2] + 0.0025),
                         B(kx + 0.0025, my + 0.007, hi[2] + 0.0025), B(kx - 0.0025, my + 0.007, hi[2] + 0.0025)], "BankVault_Dark", B(0, 0, 1))
            dw.box((lo[0] + 0.015, my - 0.012, hi[2]), (lo[0] + 0.025, my + 0.012, hi[2] + 0.012), "BankVault_Steel", skip=("-z",))
    # The simple LOD: one flat panel per bay instead of the boxes.
    ds.box((x0, plinth, D2), (x0 + bay_w, H2 - top, D2 + 0.012), "BankVault_Paint")
dw_obj = dw.finish()
ds_obj = ds.finish()
cfg = kit["colliders"]["VaultDepositWall"]
model("VaultDepositWall", lod_set("VaultDepositWall", [dw_obj], [ds_obj]), [collider("VaultDepositWall", 0, (-W2 / 2, 0, 0), (W2 / 2, H2, D2 + 0.045), cfg)])

# ---------------------------------------------------------------- VaultCashTrolley: flat steel trolley with a block of cash (concept 2)
ct = Part("trolley_full")
cs = Part("trolley_simple")
TL, TW, deck_y = 1.0, 0.6, 0.24
for prt in (ct, cs):
    prt.box((-TW / 2, deck_y - 0.04, -TL / 2), (TW / 2, deck_y, TL / 2), "BankVault_Paint")
    prt.box((-TW / 2 - 0.015, deck_y - 0.06, -TL / 2 - 0.015), (TW / 2 + 0.015, deck_y - 0.03, TL / 2 + 0.015), "BankVault_Paint")
# The push bar at the back (-z): two uprights and a cross bar.
for x in (-TW / 2 + 0.03, TW / 2 - 0.03):
    ct.cylinder((x, deck_y, -TL / 2 + 0.03), "y", 0.018, 0.75, "BankVault_Paint", segs=12)
    cs.box((x - 0.018, deck_y, -TL / 2 + 0.012), (x + 0.018, deck_y + 0.75, -TL / 2 + 0.048), "BankVault_Paint")
ct.cylinder((-TW / 2 + 0.03, deck_y + 0.75, -TL / 2 + 0.03), "x", 0.02, TW - 0.06, "BankVault_Steel", segs=12)
cs.box((-TW / 2 + 0.03, deck_y + 0.73, -TL / 2 + 0.01), (TW / 2 - 0.03, deck_y + 0.77, -TL / 2 + 0.05), "BankVault_Steel")
# Four casters: a fork block and a rubber wheel on an x axle.
for x in (-TW / 2 + 0.08, TW / 2 - 0.08):
    for z in (-TL / 2 + 0.1, TL / 2 - 0.1):
        ct.box((x - 0.035, deck_y - 0.13, z - 0.03), (x + 0.035, deck_y - 0.06, z + 0.03), "BankVault_Steel")
        ct.cylinder((x - 0.025, 0.085, z), "x", 0.085, 0.05, "BankVault_Dark", segs=16)
        cs.box((x - 0.025, 0, z - 0.08), (x + 0.025, deck_y - 0.06, z + 0.08), "BankVault_Dark")


def brick(prt, lo, hi):
    # A banknote brick: the top and bottom show the note, the sides the stack; the band runs across.
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    q = lambda pts, n, uvs: prt.face([B(*p) for p in pts], "BankVault_Cash", n, uvs)
    q([(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)], B(0, 1, 0), [(0, 0), (0.5, 0), (0.5, 1), (0, 1)])
    q([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], B(0, -1, 0), [(0, 0), (0, 1), (0.5, 1), (0.5, 0)])
    q([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], B(0, 0, 1), [(0.5, 0), (0.5, 1), (1, 1), (1, 0)])
    q([(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)], B(0, 0, -1), [(0.5, 0), (1, 0), (1, 1), (0.5, 1)])
    q([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], B(-1, 0, 0), [(0.5, 0), (1, 0), (1, 1), (0.5, 1)])
    q([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], B(1, 0, 0), [(0.5, 0), (0.5, 1), (1, 1), (1, 0)])


bl, bw, bh = 0.16, 0.075, 0.05  # one brick: 160 x 75 mm, 50 mm of notes
nx, nz, ny = 6, 5, 7
x_start = -nx * bw / 2
z_start = -TL / 2 + 0.1
for i in range(nx):
    for k in range(nz):
        layers = ny - (1 if (i + k) % 4 == 0 else 0) - (1 if (i * 3 + k) % 7 == 0 else 0)
        for j in range(layers):
            lo = (x_start + i * bw + 0.002, deck_y + j * bh, z_start + k * bl + 0.002)
            hi = (x_start + (i + 1) * bw - 0.002, deck_y + (j + 1) * bh - 0.001, z_start + (k + 1) * bl - 0.002)
            brick(ct, lo, hi)
brick(cs, (x_start, deck_y, z_start), (x_start + nx * bw, deck_y + ny * bh, z_start + nz * bl))
ct_obj = ct.finish()
cs_obj = cs.finish()
cfg = kit["colliders"]["VaultCashTrolley"]
model("VaultCashTrolley", lod_set("VaultCashTrolley", [ct_obj], [cs_obj]), [
    collider("VaultCashTrolley", 0, (-TW / 2 - 0.015, 0, -TL / 2 - 0.015), (TW / 2 + 0.015, deck_y, TL / 2 + 0.015), cfg),
    collider("VaultCashTrolley", 1, (x_start, deck_y, z_start), (x_start + nx * bw, deck_y + ny * bh, z_start + nz * bl), cfg),
    collider("VaultCashTrolley", 2, (-TW / 2, deck_y, -TL / 2), (TW / 2, deck_y + 0.78, -TL / 2 + 0.06), cfg),
])

# ================================================================ the hinge bone, export, renders
def bounds(o):
    co = np.array([v.co[:] for v in o.data.vertices])
    return co.min(axis=0), co.max(axis=0)


for name, m in MODELS.items():
    lods = m["lods"]
    export = list(lods) + list(m["colliders"])
    info = {"lods": [{"name": o.name, "tris": tris(o)} for o in lods], "colliders": [c.name for c in m["colliders"]],
            "materials": [s.material.name for s in lods[0].material_slots]}
    lo, hi = bounds(lods[0])
    info["bounds"] = {"min": b2e(lo), "max": b2e(hi)}
    if "bone" in m["extra"]:
        # One bone, "door", at the model origin: the hinge pivot. Every door vertex follows it, so the
        # anim stage swings the door about Enfusion y through this bone (tools/assets/bank-vault/kit.json).
        arm_data = bpy.data.armatures.new("Armature")
        arm = bpy.data.objects.new("Armature", arm_data)
        bpy.context.scene.collection.objects.link(arm)
        activate(arm)
        bpy.ops.object.mode_set(mode="EDIT")
        bone = arm_data.edit_bones.new("door")
        bone.head = Vector((0, 0, 0))
        bone.tail = Vector((0, 0.02, 0))  # along +Y with roll 0: the rest rotation is identity
        bone.roll = 0
        bpy.ops.object.mode_set(mode="OBJECT")
        for o in lods:
            g = o.vertex_groups.new(name="door")
            g.add(list(range(len(o.data.vertices))), 1.0, "REPLACE")
            o.parent = arm
            o.modifiers.new("Armature", "ARMATURE").object = arm
        export.append(arm)
        info["bones"] = [{"name": "door", "pos": [0, 0, 0]}]
        info["pivot_in_front"] = report["pivot"]
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action="DESELECT")
    for o in export:
        o.select_set(True)
    bpy.context.view_layer.objects.active = lods[0]
    fbx = os.path.join(out, name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=fbx, check_existing=False, use_selection=True,
                             object_types={"MESH", "OTHER", "EMPTY", "ARMATURE"},
                             use_custom_props=True, add_leaf_bones=False, bake_anim=False)
    info["fbx"] = fbx
    report["models"][name] = info

# Renders: the whole kit assembled as it stands in the prefab, then each piece.
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "TEXTURE"
scene.display.shading.show_cavity = True
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.32, 0.33, 0.35)
for o in bpy.data.objects:
    if o.type == "MESH":
        o.hide_render = not o.name.endswith("_LOD0")
cam_data = bpy.data.cameras.new("cam")
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam


def place(name, at, yaw=0.0):
    o = bpy.data.objects[name + "_LOD0"]
    root = o.parent if o.parent else o
    root.matrix_world = Matrix.Translation(B(*at)) @ Matrix.Rotation(math.radians(yaw), 4, "Z")


def shot(file, target, direction, dist, ortho=None, res=(1200, 900), lens=24):
    d = B(*direction).normalized()  # direction in Enfusion axes
    cam.location = target + d * dist
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    cam_data.type = "ORTHO" if ortho else "PERSP"
    if ortho:
        cam_data.ortho_scale = ortho
    cam_data.lens = lens
    cam_data.clip_start, cam_data.clip_end = 0.01, 200
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.filepath = file
    bpy.ops.render.render(write_still=True)


rv = os.path.join(out, "review")
place("VaultDoor", pivot)
# The deposit wall stands against the room's left wall, its face toward +x: -90 degrees about up.
dep = bpy.data.objects["VaultDepositWall_LOD0"]
dep.matrix_world = Matrix.Translation(B(-rw / 2, floor_y, z_front - rd / 2)) @ Matrix.Rotation(math.radians(-90), 4, "Z")
trol = bpy.data.objects["VaultCashTrolley_LOD0"]
trol.matrix_world = Matrix.Translation(B(0.6, floor_y, z_back + 0.9))
shot(os.path.join(rv, "kit_front.png"), B(0, 1.4, 0), (0.45, 0.25, 1), 7.5)
shot(os.path.join(rv, "front_ortho.png"), B(0, 1.6, 0), (0, 0, 1), 10, ortho=3.8)
shot(os.path.join(rv, "side_ortho.png"), B(0, 1.6, -1.8), (1, 0, 0), 10, ortho=5.2)
shot(os.path.join(rv, "top_ortho.png"), B(0, 1.6, -1.8), (0, 1, 0.0001), 10, ortho=5.2)
# Swing the door open 100 degrees about its pivot to judge the hinge: it must clear the frame.
arm = bpy.data.objects["VaultDoor_LOD0"].parent
arm.matrix_world = Matrix.Translation(B(*pivot)) @ Matrix.Rotation(math.radians(100), 4, "Z")
shot(os.path.join(rv, "door_open.png"), B(0.4, 1.4, 0.3), (0.2, 0.35, 1), 7.0)
# Inside the room, from just inside the tunnel, looking at the back wall.
shot(os.path.join(rv, "inside.png"), B(-0.3, 1.2, -2.6), (0.35, 0.3, 1), 1.9, lens=14)
arm.matrix_world = Matrix.Translation(B(*pivot))
for name in ("VaultDepositWall", "VaultCashTrolley", "VaultDoor"):
    for o in bpy.data.objects:
        if o.type == "MESH":
            o.hide_render = o.name != name + "_LOD0"
    o = bpy.data.objects[name + "_LOD0"]
    lo, hi = bounds(o)
    c = o.matrix_world @ Vector(((lo + hi) / 2).tolist())
    span = float(max(hi - lo))
    shot(os.path.join(rv, name + ".png"), c, (0.6, 0.55, 1), span * 2.0, lens=35)
    last = bpy.data.objects[name + "_LOD%d" % len(kit["lod_ratios"])]
    last.matrix_world = o.matrix_world
    o.hide_render = True
    last.hide_render = False
    shot(os.path.join(rv, name + "_lodlast.png"), c, (0.6, 0.55, 1), span * 2.0, lens=35)
    last.hide_render = True

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "bank-vault.blend"))
with open(os.path.join(out, "report.json"), "w") as f:
    json.dump(report, f, indent=1, default=float)
print("LF_KIT_BUILD ok", json.dumps({k: [l["tris"] for l in v["lods"]] for k, v in report["models"].items()}))
