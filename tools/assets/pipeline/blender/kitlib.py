# Shared helpers for the box-and-cylinder kit build scripts (tools/assets/<slug>/build.py, `tools\cli asset kit`).
# The atm-wall kit carries its own copy of the texture, geometry and export code; the kits after it (shop counter,
# smelter furnace, apple press, seed packet, cabbage) import this module instead:
#   sys.path.insert(0, job["blender_lib"]); import kitlib as K; K.init(job)
#
# Space: models are written in Enfusion axes (x right, y up, z forward). B(x, y, z) maps a point to Blender axes;
# the FBX export lands Blender (X, Y, Z) on Enfusion (-x, z, y), so x is negated to keep left and right.
import bpy, bmesh, json, os, math
import numpy as np
from mathutils import Vector

from tiff import write_tiff

OUT = None
KIT = None
REPORT = {"models": {}, "materials": {}, "notes": []}
TEX = {}  # material name -> {"base_png": path, "tile_m": metres per texture repeat}
rng = np.random.default_rng(1989)


def init(job):
    global OUT, KIT
    KIT, OUT = job["kit"], job["out"]
    os.makedirs(os.path.join(OUT, "tex"), exist_ok=True)
    os.makedirs(os.path.join(OUT, "review"), exist_ok=True)
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)
    return KIT, OUT


def B(x, y, z):
    return Vector((-x, z, y))


def b2e(v):
    return [round(-float(v[0]), 4), round(float(v[2]), 4), round(float(v[1]), 4)]


# ================================================================ textures
def noise(h, w, sx, sy=None):
    # Periodic (tileable) gaussian noise, zero mean, unit deviation; sx, sy are feature sizes in pixels.
    sy = sx if sy is None else sy
    g0 = rng.standard_normal((h, w))
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    g = np.exp(-2 * (np.pi ** 2) * ((fx * sx) ** 2 + (fy * sy) ** 2))
    n = np.real(np.fft.ifft2(np.fft.fft2(g0) * g))
    return (n - n.mean()) / (n.std() + 1e-9)


def blur(a, s):
    h, w = a.shape
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    return np.real(np.fft.ifft2(np.fft.fft2(a) * np.exp(-2 * (np.pi ** 2) * (s ** 2) * (fx ** 2 + fy ** 2))))


def normal_from_height(hf, strength):
    dx = (np.roll(hf, -1, 1) - np.roll(hf, 1, 1)) * 0.5 * strength
    dy = (np.roll(hf, 1, 0) - np.roll(hf, -1, 0)) * 0.5 * strength
    n = np.dstack([-dx, -dy, np.ones_like(hf)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    return n * 0.5 + 0.5


def to_linear(c):
    # Workbench's colour map import treats the TIFF as linear and encodes it to sRGB (atm-wall build.py).
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def save_material(name, base, rough, metal, normal, tile_m, em=None):
    # base sRGB (h, w, 3); rough, metal (h, w); normal from normal_from_height; tile_m metres per repeat
    # (None: an atlas mapped by explicit UVs). em: emitted colour per pixel, written as <name>_EM.tif.
    h, w = base.shape[:2]
    rough = np.broadcast_to(rough, (h, w))
    metal = np.broadcast_to(metal, (h, w))
    bcr = np.dstack([to_linear(base[:, :, :3]), rough])
    nmo = np.dstack([normal[:, :, 0], 1.0 - normal[:, :, 1], metal, np.ones((h, w))])
    write_tiff(os.path.join(OUT, "tex", name + "_BCR.tif"), (np.clip(bcr, 0, 1) * 255 + 0.5).astype(np.uint8))
    write_tiff(os.path.join(OUT, "tex", name + "_NMO.tif"), (np.clip(nmo, 0, 1) * 255 + 0.5).astype(np.uint8))
    if em is not None:
        emr = np.dstack([to_linear(em), np.ones((h, w))])
        write_tiff(os.path.join(OUT, "tex", name + "_EM.tif"), (np.clip(emr, 0, 1) * 255 + 0.5).astype(np.uint8))
    png = os.path.join(OUT, "tex", name + "_base.png")
    img = bpy.data.images.new(name + "_base", w, h)
    img.pixels.foreach_set(np.dstack([np.clip(base[:, :, :3], 0, 1), np.ones((h, w))])[::-1].astype(np.float32).ravel())
    img.filepath_raw = png
    img.file_format = "PNG"
    img.save()
    TEX[name] = {"base_png": png, "tile_m": tile_m, "px": (w, h)}
    REPORT["materials"][name] = {"px": [w, h], "tile_m": tile_m,
                                 "mean_base": [round(float(v), 3) for v in base[:, :, :3].mean(axis=(0, 1))],
                                 "mean_rough": round(float(np.mean(rough)), 3), "mean_metal": round(float(np.mean(metal)), 3)}


def wood(px, rgb, grain_px=6, ring=0.10, along="v", rough=0.55, varnish=0.0, seed_rows=None):
    # A tiling wood texture: long streaky grain along v (rows) or u, darker rings, a little figure.
    # Returns (base, rough, height). varnish lowers roughness and adds a faint sheen variation.
    sx, sy = (grain_px, px * 0.8) if along == "v" else (px * 0.8, grain_px)
    g = noise(px, px, sx, sy)
    fine = noise(px, px, max(1, grain_px * 0.25), px * 0.3 if along == "v" else 1)
    if along != "v":
        fine = noise(px, px, px * 0.3, max(1, grain_px * 0.25))
    rings = np.sin(g * 3.2) * 0.5 + 0.5
    tone = 1 + (rings - 0.5) * ring * 2 + fine * 0.035 + noise(px, px, px / 6) * 0.04
    base = np.ones((px, px, 3)) * np.array(rgb) * tone[:, :, None]
    r = np.full((px, px), rough) - varnish * 0.3 + fine * 0.03
    return np.clip(base, 0, 1), np.clip(r, 0.05, 1), g * 0.15 + fine * 0.1


def plain(px, rgb, rough, metal=0.0, grain=0.02, bump=0.1, feature=3):
    base = np.ones((px, px, 3)) * np.array(rgb) * (1 + noise(px, px, feature * 8) * grain + noise(px, px, feature) * grain * 0.5)[:, :, None]
    return np.clip(base, 0, 1), np.full((px, px), rough), np.full((px, px), metal), noise(px, px, feature) * bump


# ---------------------------------------------------------------- a 5x7 pixel font
FONT = {
    "A": "01110 10001 10001 11111 10001 10001 10001", "B": "11110 10001 10001 11110 10001 10001 11110",
    "C": "01110 10001 10000 10000 10000 10001 01110", "D": "11110 10001 10001 10001 10001 10001 11110",
    "E": "11111 10000 10000 11110 10000 10000 11111", "F": "11111 10000 10000 11110 10000 10000 10000",
    "G": "01110 10001 10000 10111 10001 10001 01111", "H": "10001 10001 10001 11111 10001 10001 10001",
    "I": "01110 00100 00100 00100 00100 00100 01110", "J": "00111 00010 00010 00010 00010 10010 01100",
    "K": "10001 10010 10100 11000 10100 10010 10001", "L": "10000 10000 10000 10000 10000 10000 11111",
    "M": "10001 11011 10101 10101 10001 10001 10001", "N": "10001 11001 10101 10011 10001 10001 10001",
    "O": "01110 10001 10001 10001 10001 10001 01110", "P": "11110 10001 10001 11110 10000 10000 10000",
    "Q": "01110 10001 10001 10001 10101 10010 01101", "R": "11110 10001 10001 11110 10100 10010 10001",
    "S": "01111 10000 10000 01110 00001 00001 11110", "T": "11111 00100 00100 00100 00100 00100 00100",
    "U": "10001 10001 10001 10001 10001 10001 01110", "V": "10001 10001 10001 10001 10001 01010 00100",
    "W": "10001 10001 10001 10101 10101 11011 10001", "X": "10001 10001 01010 00100 01010 10001 10001",
    "Y": "10001 10001 01010 00100 00100 00100 00100", "Z": "11111 00001 00010 00100 01000 10000 11111",
    "0": "01110 10001 10011 10101 11001 10001 01110", "1": "00100 01100 00100 00100 00100 00100 01110",
    "2": "01110 10001 00001 00010 00100 01000 11111", "3": "11110 00001 00001 01110 00001 00001 11110",
    "4": "00010 00110 01010 10010 11111 00010 00010", "5": "11111 10000 11110 00001 00001 10001 01110",
    "6": "00110 01000 10000 11110 10001 10001 01110", "7": "11111 00001 00010 00100 01000 01000 01000",
    "8": "01110 10001 10001 01110 10001 10001 01110", "9": "01110 10001 10001 01111 00001 00010 01100",
    "-": "00000 00000 00000 11111 00000 00000 00000", ".": "00000 00000 00000 00000 00000 01100 01100",
    ",": "00000 00000 00000 00000 01100 00100 01000", "+": "00000 00100 00100 11111 00100 00100 00000",
    "*": "00000 00100 10101 01110 10101 00100 00000", " ": "00000 00000 00000 00000 00000 00000 00000",
    "/": "00001 00010 00010 00100 01000 01000 10000", "'": "00100 00100 01000 00000 00000 00000 00000",
    "%": "11001 11010 00010 00100 01000 01011 10011", "=": "00000 00000 11111 00000 11111 00000 00000",
}
GLYPH = {k: np.array([[int(c) for c in row] for row in v.split()], bool) for k, v in FONT.items()}


def text_mask(s, scale, gap=1):
    cols = []
    for ch in s:
        cols.append(np.kron(GLYPH[ch], np.ones((scale, scale), bool)))
        cols.append(np.zeros((7 * scale, gap * scale), bool))
    return np.hstack(cols[:-1])


class Canvas:
    """An atlas: base colour, roughness, metal and a height field, drawn with rectangles, circles and text."""

    def __init__(self, w, h, base_rgb, rough=0.55):
        self.w, self.h = w, h
        self.base = np.ones((h, w, 3)) * np.array(base_rgb, float)
        self.rough = np.full((h, w), rough)
        self.metal = np.zeros((h, w))
        self.height = np.zeros((h, w))
        self.em = np.zeros((h, w, 3))

    def rect(self, x0, y0, x1, y1, rgb=None, rough=None, metal=None, height=None):
        sub = (slice(int(y0), int(y1)), slice(int(x0), int(x1)))
        if rgb is not None:
            self.base[sub] = rgb
        if rough is not None:
            self.rough[sub] = rough
        if metal is not None:
            self.metal[sub] = metal
        if height is not None:
            self.height[sub] = height

    def paste(self, x0, y0, base, rough=None, metal=None, height=None):
        h, w = base.shape[:2]
        sub = (slice(int(y0), int(y0) + h), slice(int(x0), int(x0) + w))
        self.base[sub] = base[:, :, :3]
        if rough is not None:
            self.rough[sub] = rough
        if metal is not None:
            self.metal[sub] = metal
        if height is not None:
            self.height[sub] = height

    def text(self, s, x, y, scale, rgb, centre_in=None, height=None, gap=1, em=None):
        m = text_mask(s, scale, gap)
        th, tw = m.shape
        if centre_in:
            x = int((centre_in[0] + centre_in[1] - tw) / 2)
        x, y = int(x), int(y)
        sub = (slice(y, y + th), slice(x, x + tw))
        self.base[sub][m] = rgb
        if height is not None:
            self.height[sub][m] = height
        if em is not None:
            self.em[sub][m] = em
        return (x, y, tw, th)

    def circle(self, cx, cy, r, rgb=None, ring=None, height=None, rough=None, metal=None):
        yy, xx = np.mgrid[0:self.h, 0:self.w]
        d = np.hypot(xx - cx, yy - cy)
        mask = d <= r if ring is None else (d <= r) & (d >= r - ring)
        if rgb is not None:
            self.base[mask] = rgb
        if height is not None:
            self.height[mask] = height
        if rough is not None:
            self.rough[mask] = rough
        if metal is not None:
            self.metal[mask] = metal
        return mask

    def save(self, name, strength=1.0, em=False):
        save_material(name, np.clip(self.base, 0, 1), self.rough, self.metal,
                      normal_from_height(blur(self.height, 0.7), strength), None, em=np.clip(self.em, 0, 1) if em else None)


# ================================================================ geometry
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


class Part:
    """A bmesh with a material per face and box-projected UVs (metres / the material's tile)."""

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
            by_vert = dict(zip(vs, uvs))
            for loop in f.loops:
                loop[self.uv].uv = by_vert[loop.vert]
        else:
            f[self.auto] = 1
        return f

    def box(self, lo, hi, mat, mats=None, skip=(), regions=None):
        # lo, hi in Enfusion axes. mats maps a side ("+x", "-x", "+y", "-y", "+z", "-z") to a material;
        # regions maps a side to (material, (x, y, w, h) atlas pixels): that face is atlas-mapped, its
        # image upright as seen from outside (for +y/-y the image top is toward -z).
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        c = {
            # corners bl, br, tr, tl seen from outside the face
            "+x": ([(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)], (1, 0, 0)),
            "-x": ([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], (-1, 0, 0)),
            "+y": ([(x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)], (0, 1, 0)),
            "-y": ([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0, -1, 0)),
            "+z": ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1)),
            "-z": ([(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], (0, 0, -1)),
        }
        for side, (pts, n) in c.items():
            if side in skip:
                continue
            if regions and side in regions:
                rm, reg = regions[side]
                self.quad(rm, *pts, n, reg)
            else:
                self.face([B(*p) for p in pts], (mats or {}).get(side, mat), want=B(*n))

    def prism(self, profile, x0, x1, mat, skip_edges=(), skip_caps=False, edge_mats=None):
        # A convex profile of (z, y) points extruded along x from x0 to x1. Edge i joins point i to i+1.
        n = len(profile)
        centre = B((x0 + x1) / 2, sum(p[1] for p in profile) / n, sum(p[0] for p in profile) / n)
        for i in range(n):
            if i in skip_edges:
                continue
            (za, ya), (zb, yb) = profile[i], profile[(i + 1) % n]
            pts = [B(x0, ya, za), B(x1, ya, za), B(x1, yb, zb), B(x0, yb, zb)]
            mid = sum(pts, Vector()) / 4
            self.face(pts, (edge_mats or {}).get(i, mat), want=mid - centre)
        if not skip_caps:
            for x in (x0, x1):
                pts = [B(x, y, z) for z, y in profile]
                mid = sum(pts, Vector()) / n
                self.face(pts, mat, want=mid - centre)

    def cyl(self, c, r, length, axis, mat, segs=12, caps=(True, True), r2=None, cap_mat=None, cap_region=None):
        # A cylinder (or a cone frustum with r2) from c along +axis ("x", "y" or "z") for `length` metres.
        r2 = r if r2 is None else r2
        ai = "xyz".index(axis)
        u, v = [k for k in range(3) if k != ai]
        ring0, ring1 = [], []
        for i in range(segs):
            a = 2 * math.pi * i / segs
            p0, p1 = list(c), list(c)
            p0[u] += math.cos(a) * r
            p0[v] += math.sin(a) * r
            p1[u] += math.cos(a) * r2
            p1[v] += math.sin(a) * r2
            p1[ai] += length
            ring0.append(tuple(p0))
            ring1.append(tuple(p1))
        mid = list(c)
        mid[ai] += length / 2
        centre = B(*mid)
        for i in range(segs):
            j = (i + 1) % segs
            pts = [B(*ring0[i]), B(*ring0[j]), B(*ring1[j]), B(*ring1[i])]
            m = sum(pts, Vector()) / 4
            self.face(pts, mat, want=m - centre)
        for k, ring in enumerate((ring0, ring1)):
            if not caps[k]:
                continue
            cm = cap_mat or mat
            pts = [B(*p) for p in ring]
            n = Vector((0, 0, 0))
            n_e = [0, 0, 0]
            n_e[ai] = -1 if k == 0 else 1
            if cap_region:
                rx, ry, rw, rh = cap_region
                aw, ah = TEX[cm]["px"]
                uvs = []
                for i in range(segs):
                    a = 2 * math.pi * i / segs
                    uvs.append(((rx + rw * (0.5 + 0.5 * math.cos(a))) / aw, 1 - (ry + rh * (0.5 + 0.5 * math.sin(a))) / ah))
                self.face(pts, cm, want=B(*n_e), uvs=uvs)
            else:
                self.face(pts, cm, want=B(*n_e))

    def quad(self, mat, bl, br, tr, tl, normal, region, size=None):
        # An atlas-mapped quad: corners and normal in Enfusion axes, region (x, y, w, h) in atlas pixels.
        rx, ry, rw, rh = region
        aw, ah = size or TEX[mat]["px"]
        uvs = [(rx / aw, 1 - (ry + rh) / ah), ((rx + rw) / aw, 1 - (ry + rh) / ah), ((rx + rw) / aw, 1 - ry / ah), (rx / aw, 1 - ry / ah)]
        self.face([B(*p) for p in (bl, br, tr, tl)], mat, want=B(*normal), uvs=uvs)

    def finish(self):
        for f in self.bm.faces:
            if not f[self.auto]:
                continue
            tile = TEX[self.mats[f.material_index]]["tile_m"] or 1.0
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


def tris(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


def bounds(o):
    co = np.array([v.co[:] for v in o.data.vertices])
    return co.min(axis=0), co.max(axis=0)


def collider(model, i, lo, hi, preset):
    # One UBX_ box (Enfusion axes lo, hi), its origin at its own centre.
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
    o["usage"] = preset
    o.display_type = "WIRE"
    o.hide_render = True
    return o


def export_model(name, lods, boxes):
    # lods: [LOD0, LOD1, ...] objects from Part.finish(); boxes: [(lo, hi), ...] collider boxes in Enfusion axes.
    for i, o in enumerate(lods):
        o.name = o.data.name = "%s_LOD%d" % (name, i)
    preset = KIT["colliders"][name]["layerPreset"]
    cols = [collider(name, i + 1, lo, hi, preset) for i, (lo, hi) in enumerate(boxes)]
    info = {"lods": [{"name": o.name, "tris": tris(o)} for o in lods], "colliders": [c.name for c in cols],
            "materials": [s.material.name for s in lods[0].material_slots]}
    lo, hi = bounds(lods[0])
    e0, e1 = b2e(lo), b2e(hi)
    info["bounds"] = {"min": [min(a, b) for a, b in zip(e0, e1)], "max": [max(a, b) for a, b in zip(e0, e1)]}
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action="DESELECT")
    for o in list(lods) + cols:
        o.select_set(True)
    bpy.context.view_layer.objects.active = lods[0]
    fbx = os.path.join(OUT, name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=fbx, check_existing=False, use_selection=True,
                             object_types={"MESH", "OTHER", "EMPTY"}, use_custom_props=True, add_leaf_bones=False, bake_anim=False)
    info["fbx"] = fbx
    REPORT["models"][name] = info
    return info


# ================================================================ review renders
_cam = None


def review_setup(ground=True):
    global _cam
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.display.shading.show_cavity = True
    scene.world = bpy.data.worlds.new("w")
    scene.world.color = (0.32, 0.33, 0.35)
    if ground:
        g = bpy.data.objects.new("ground", bpy.data.meshes.new("ground"))
        gbm = bmesh.new()
        bmesh.ops.create_grid(gbm, x_segments=1, y_segments=1, size=4)
        gbm.to_mesh(g.data)
        gbm.free()
        g.data.materials.append(bpy.data.materials.new("groundmat"))
        g.data.materials[0].diffuse_color = (0.45, 0.44, 0.42, 1)
        scene.collection.objects.link(g)
    cam_data = bpy.data.cameras.new("cam")
    _cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(_cam)
    scene.camera = _cam


def show_only(objs):
    keep = set(o.name for o in objs)
    for o in bpy.data.objects:
        if o.type == "MESH":
            o.hide_render = not (o.name in keep or o.name == "ground")


def shot(file, target, direction, dist, ortho=None, res=(1000, 900), lens=35):
    scene = bpy.context.scene
    d = B(*direction).normalized()
    _cam.location = B(*target) + d * dist
    _cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    _cam.data.type = "ORTHO" if ortho else "PERSP"
    if ortho:
        _cam.data.ortho_scale = ortho
    _cam.data.lens = lens
    _cam.data.clip_start, _cam.data.clip_end = 0.01, 50
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.filepath = os.path.join(OUT, "review", file)
    bpy.ops.render.render(write_still=True)


def finish(slug):
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, slug + ".blend"))
    with open(os.path.join(OUT, "report.json"), "w") as f:
        json.dump(REPORT, f, indent=1, default=float)
    print("LF_KIT_BUILD ok", json.dumps({k: [l["tris"] for l in v["lods"]] for k, v in REPORT["models"].items()}))
