# Shared helpers for the printed-paper items (fine ticket, receipt, hospital bill, sentence slip, ID card).
# Each slug's model.py lays its print out on a Canvas (millimetres, y up) and calls finish(): the canvas
# is rendered flat with Blender's Workbench engine, composed over a paper (or laminate) texture with
# numpy, and written as the four maps the clean stage reads (tex_base_color, tex_roughness,
# tex_metallic, tex_normal). No generation credits, no downloaded textures, no real logos or names:
# every word printed on the paper is fictional (the Republic of Everon is an invented island).
#
# Fonts are the two Blender ships (Inter, DejaVu Sans Mono), both under open licences.
import bpy, bmesh, math, os, sys
import numpy as np

FONT_DIR = os.path.join(os.path.dirname(bpy.app.binary_path), str(bpy.app.version[0]) + "." + str(bpy.app.version[1]), "datafiles", "fonts")
_fonts = {}


def font(name):
    if name not in _fonts:
        f = {"sans": "Inter.woff2", "mono": "DejaVuSansMono.woff2"}[name]
        _fonts[name] = bpy.data.fonts.load(os.path.join(FONT_DIR, f))
    return _fonts[name]


def lin(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


class Canvas:
    """A flat 2D drawing in millimetres rendered at tex_w x tex_h pixels (the pixel aspect stretches it
    to fill the texture, which is how a 148 x 210 mm sheet fills a square map)."""

    def __init__(self, w_mm, h_mm, tex_w, tex_h):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        _fonts.clear()
        self.w, self.h, self.tw, self.th = w_mm, h_mm, tex_w, tex_h
        self.z = 0.0
        self.layers = {}
        self.mats = {}
        sc = bpy.context.scene
        self.sc = sc
        sc.render.engine = "BLENDER_WORKBENCH"
        sc.render.resolution_x, sc.render.resolution_y = tex_w, tex_h
        sc.render.resolution_percentage = 100
        ratio = (tex_w * h_mm) / (tex_h * w_mm)  # pixel aspect y/x that makes the frame the sheet's shape
        sc.render.pixel_aspect_x, sc.render.pixel_aspect_y = (1.0, ratio) if ratio >= 1 else (1.0 / ratio, 1.0)
        sc.render.film_transparent = False
        sc.display.shading.light = "FLAT"
        sc.display.shading.color_type = "MATERIAL"
        sc.display.shading.show_object_outline = False
        sc.display.shading.show_cavity = False
        sc.display.render_aa = "32"
        sc.view_settings.view_transform = "Standard"
        sc.view_settings.look = "None"
        sc.display_settings.display_device = "sRGB"
        world = bpy.data.worlds.new("w")
        world.use_nodes = False
        world.color = (1, 1, 1)
        sc.world = world
        cam = bpy.data.cameras.new("c")
        cam.type = "ORTHO"
        cam.ortho_scale = max(w_mm, h_mm)
        cam.clip_start, cam.clip_end = 0.1, 100
        co = bpy.data.objects.new("c", cam)
        co.location = (w_mm / 2, h_mm / 2, 20)
        sc.collection.objects.link(co)
        sc.camera = co
        self.layer_name = "print"
        self.xf = None

    # ---------------------------------------------------------------- drawing
    def tilt(self, cx=0, cy=0, deg=0):
        """Everything drawn after this is turned by deg about (cx, cy): a stamp that was not put down square."""
        self.xf = (cx, cy, math.radians(deg)) if deg else None

    def _p(self, x, y):
        if not self.xf:
            return x, y
        cx, cy, a = self.xf
        return (cx + (x - cx) * math.cos(a) - (y - cy) * math.sin(a), cy + (x - cx) * math.sin(a) + (y - cy) * math.cos(a))

    def layer(self, name):
        self.layer_name = name

    def _mat(self, rgb):
        key = tuple(rgb)
        if key not in self.mats:
            m = bpy.data.materials.new("m%d" % len(self.mats))
            m.diffuse_color = (lin(rgb[0]), lin(rgb[1]), lin(rgb[2]), 1)
            self.mats[key] = m
        return self.mats[key]

    def _add(self, ob, rgb):
        self.z += 0.0005
        ob.location.z = self.z
        ob.data.materials.append(self._mat(rgb))
        ob["layer"] = self.layer_name
        self.sc.collection.objects.link(ob)
        return ob

    def poly(self, pts, rgb):
        me = bpy.data.meshes.new("p")
        bm = bmesh.new()
        vs = [bm.verts.new((*self._p(x, y), 0)) for x, y in pts]
        bm.faces.new(vs)
        bmesh.ops.triangulate(bm, faces=bm.faces)
        bm.to_mesh(me)
        bm.free()
        return self._add(bpy.data.objects.new("p", me), rgb)

    def rect(self, x0, y0, x1, y1, rgb):
        return self.poly([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], rgb)

    def box(self, x0, y0, x1, y1, rgb, t=0.25):
        """An outlined rectangle, line width t."""
        self.rect(x0, y0, x1, y0 + t, rgb)
        self.rect(x0, y1 - t, x1, y1, rgb)
        self.rect(x0, y0, x0 + t, y1, rgb)
        self.rect(x1 - t, y0, x1, y1, rgb)

    def hline(self, x0, x1, y, rgb, t=0.2):
        self.rect(x0, y - t / 2, x1, y + t / 2, rgb)

    def vline(self, x, y0, y1, rgb, t=0.2):
        self.rect(x - t / 2, y0, x + t / 2, y1, rgb)

    def dots(self, x0, x1, y, rgb, step=1.4, r=0.22):
        x = x0
        while x < x1:
            self.rect(x, y - r, x + r * 2, y + r, rgb)
            x += step

    def ring(self, cx, cy, r_out, r_in, rgb, a0=0, a1=360, seg=72):
        pts_o = [(cx + r_out * math.cos(math.radians(a0 + (a1 - a0) * i / seg)), cy + r_out * math.sin(math.radians(a0 + (a1 - a0) * i / seg))) for i in range(seg + 1)]
        pts_i = [(cx + r_in * math.cos(math.radians(a0 + (a1 - a0) * i / seg)), cy + r_in * math.sin(math.radians(a0 + (a1 - a0) * i / seg))) for i in range(seg + 1)]
        me = bpy.data.meshes.new("r")
        bm = bmesh.new()
        vo = [bm.verts.new((*self._p(x, y), 0)) for x, y in pts_o]
        vi = [bm.verts.new((*self._p(x, y), 0)) for x, y in pts_i]
        for i in range(seg):
            bm.faces.new((vo[i], vo[i + 1], vi[i + 1], vi[i]))
        bm.to_mesh(me)
        bm.free()
        return self._add(bpy.data.objects.new("r", me), rgb)

    def disc(self, cx, cy, r, rgb, seg=48):
        return self.poly([(cx + r * math.cos(2 * math.pi * i / seg), cy + r * math.sin(2 * math.pi * i / seg)) for i in range(seg)], rgb)

    def text(self, x, y, s, size, rgb, face="sans", align="LEFT", bold=0.0, rot=0.0, spacing=1.0, fit=None):
        """Text with its baseline at y. size is the em height in mm. fit: squeeze or stretch to this width (mm)."""
        cu = bpy.data.curves.new("t", "FONT")
        cu.body = s
        cu.size = size
        cu.font = font(face)
        cu.align_x = align
        cu.align_y = "BOTTOM_BASELINE"
        cu.space_character = spacing
        cu.offset = 0.0
        cu.extrude = 0.0
        ob = bpy.data.objects.new("t", cu)
        self._add(ob, rgb)
        px_, py_ = self._p(x, y)
        ob.rotation_euler.z = math.radians(rot) + (self.xf[2] if self.xf else 0)
        ob.location.x, ob.location.y = px_, py_
        if fit:
            bpy.context.view_layer.update()
            width = ob.dimensions.x
            if width > 0:
                ob.scale.x = fit / width
        return ob

    def width_of(self, s, size, face="sans", spacing=1.0, bold=0.0):
        ob = self.text(-500, -500, s, size, (0, 0, 0), face, spacing=spacing, bold=bold)
        bpy.context.view_layer.update()
        w = ob.dimensions.x
        bpy.data.objects.remove(ob, do_unlink=True)
        return w

    def star(self, cx, cy, r, rgb, rot=90, points=5, inner=0.4):
        pts = []
        for i in range(points * 2):
            rr = r if i % 2 == 0 else r * inner
            a = math.radians(rot + 180 * i / points)
            pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
        return self.poly(pts, rgb)

    # ---------------------------------------------------------------- render
    def render(self, which, path):
        for ob in self.sc.objects:
            if ob.type == "CAMERA":
                continue
            ob.hide_render = ob.get("layer") not in which
        self.sc.render.filepath = path
        self.sc.render.image_settings.file_format = "PNG"
        self.sc.render.image_settings.color_mode = "RGB"
        self.sc.render.image_settings.color_depth = "8"
        bpy.ops.render.render(write_still=True)
        img = bpy.data.images.load(path)
        img.colorspace_settings.name = "Non-Color"
        arr = np.empty(self.tw * self.th * 4, np.float32)
        img.pixels.foreach_get(arr)
        bpy.data.images.remove(img)
        return arr.reshape(self.th, self.tw, 4)[:, :, :3]


# ---------------------------------------------------------------- noise and paper
def coloured(h, w, beta, seed, sx=1.0, sy=1.0):
    rng = np.random.default_rng(seed)
    f = np.fft.fft2(rng.standard_normal((h, w)))
    fy = np.fft.fftfreq(h)[:, None] * sy
    fx = np.fft.fftfreq(w)[None, :] * sx
    r = np.sqrt(fx ** 2 + fy ** 2)
    r[0, 0] = 1
    f = f / (r ** (beta / 2))
    f[0, 0] = 0
    a = np.fft.ifft2(f).real
    return ((a - a.mean()) / a.std()).astype(np.float32)


def blur(a, k):
    """Cheap separable box blur, k pixels."""
    k = int(k)
    if k < 1:
        return a
    out = a.astype(np.float64)
    for axis in (0, 1):
        m = np.moveaxis(out, axis, 0)
        p = np.pad(m, [(k + 1, k)] + [(0, 0)] * (m.ndim - 1), mode="edge")
        c = np.cumsum(p, axis=0)
        n = 2 * k + 1
        m = (c[n:] - c[:-n]) / n
        out = np.moveaxis(m, 0, axis)
    return out.astype(np.float32)


class Surface:
    """What one face of the item is made of: base colour (0..255), fibre, roughness, gloss."""

    def __init__(self, base, fibre=0.035, blotch=0.03, rough=0.9, rough_var=0.05, grain=0.5, seed=1, ink_rough=-0.12):
        self.base, self.fibre, self.blotch, self.rough, self.rough_var, self.grain, self.seed, self.ink_rough = base, fibre, blotch, rough, rough_var, grain, seed, ink_rough


def compose(surface, ink, extra_height=None, wear=0.18, crease=None):
    """ink: float array (h, w, 3) of multiplied print layers (1 = bare paper). Returns the four maps."""
    h, w = ink.shape[:2]
    s = surface
    blotch = blur(coloured(h, w, 2.4, s.seed), 2) * s.blotch
    fib = (coloured(h, w, 1.0, s.seed + 1, 1.0, 0.12) + coloured(h, w, 1.0, s.seed + 2, 0.12, 1.0)) * 0.5
    fine = coloured(h, w, 0.2, s.seed + 3)
    paper = np.array(s.base, np.float32) / 255.0
    col = paper[None, None, :] * (1.0 + blotch[:, :, None] + (fib * s.fibre)[:, :, None])
    # Worn ink: it never covers the paper fully.
    gaps = np.clip(coloured(h, w, 1.5, s.seed + 4) * 0.5 + 0.2, 0, 1)
    ink_w = 1.0 - (1.0 - ink) * (1.0 - wear * gaps[:, :, None])
    col = col * ink_w
    # Edge darkening: sheets handled for a while go grey and soft at the rim.
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    edge = np.minimum(np.minimum(xx, w - 1 - xx) / w, np.minimum(yy, h - 1 - yy) / h)
    col = col * (1.0 - 0.07 * np.clip(1.0 - edge / 0.03, 0, 1))[:, :, None]
    ink_amt = np.clip(1.0 - ink.mean(axis=2), 0, 1)
    height = 0.35 * fine + 0.6 * fib * s.grain + blur(ink_amt, 1) * -0.5
    if extra_height is not None:
        height = height + extra_height
    if crease is not None:
        col = col * (1.0 + crease[0])[:, :, None]
        height = height + crease[1]
    k = 0.55
    gx = np.gradient(height, axis=1)
    gy = np.gradient(height, axis=0)
    nrm = np.dstack([-gx * k, -gy * k, np.ones_like(gx)])
    nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
    rough = s.rough + s.rough_var * fine + s.ink_rough * ink_amt
    return {
        "base_color": np.clip(col, 0, 1),
        "roughness": np.clip(rough, 0.05, 1)[:, :, None].repeat(3, 2),
        "metallic": np.zeros((h, w, 3), np.float32),
        "normal": nrm * 0.5 + 0.5,
    }


def write_png(path, rgb):
    h, w = rgb.shape[:2]
    img = bpy.data.images.new(os.path.basename(path), w, h, alpha=False, float_buffer=False)
    img.colorspace_settings.name = "Non-Color"
    rgba = np.ones((h, w, 4), np.float32)
    rgba[:, :, :3] = np.clip(rgb, 0, 1)
    img.pixels.foreach_set(rgba.ravel())
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def write_maps(raw, maps):
    os.makedirs(raw, exist_ok=True)
    for k, v in maps.items():
        write_png(os.path.join(raw, "tex_%s.png" % k), v)


# ---------------------------------------------------------------- geometry (metres, Z up, print face up)
def sheet(w, length, t, bow_x=0.0, bow_y=0.0, nx=10, ny=14, corner=None):
    """A flat sheet lying on XY, print face up (+Z): a grid so a gentle bow can lift the ends. Top faces
    map to the whole texture, the underside to the same print mirrored (it shows through the back), the
    edge faces to a texel of plain paper in the corner."""
    me = bpy.data.meshes.new("Sheet")
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")

    def zat(u, v):
        return bow_x * ((u - 0.5) * 2) ** 2 + bow_y * ((v - 0.5) * 2) ** 2

    top, bot = {}, {}
    for j in range(ny + 1):
        for i in range(nx + 1):
            u, v = i / nx, j / ny
            x, y = (u - 0.5) * w, (v - 0.5) * length
            z = zat(u, v)
            top[i, j] = bm.verts.new((x, y, z + t / 2))
            bot[i, j] = bm.verts.new((x, y, z - t / 2))
    edge_uv = (0.004, 0.004)
    for j in range(ny):
        for i in range(nx):
            a, b, c, d = top[i, j], top[i + 1, j], top[i + 1, j + 1], top[i, j + 1]
            f = bm.faces.new((a, b, c, d))
            for lp, (uu, vv) in zip(f.loops, ((i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1))):
                lp[uv].uv = (uu / nx, vv / ny)
            a, b, c, d = bot[i, j], bot[i, j + 1], bot[i + 1, j + 1], bot[i + 1, j]
            f = bm.faces.new((a, b, c, d))
            for lp, (uu, vv) in zip(f.loops, ((i, j), (i, j + 1), (i + 1, j + 1), (i + 1, j))):
                lp[uv].uv = (1 - uu / nx, vv / ny)
    ring = [(i, 0) for i in range(nx)] + [(nx, j) for j in range(ny)] + [(i, ny) for i in range(nx, 0, -1)] + [(0, j) for j in range(ny, 0, -1)]
    for n in range(len(ring)):
        p, q = ring[n], ring[(n + 1) % len(ring)]
        f = bm.faces.new((top[p], bot[p], bot[q], top[q]))
        for lp in f.loops:
            lp[uv].uv = edge_uv
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    return me


def card(w, h, t, radius, seg=7):
    """A rounded-corner card on XY, front face up. Front maps to the top half of the atlas, back (mirrored)
    to the bottom half, the edge to a corner texel of the front."""
    me = bpy.data.meshes.new("Card")
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    out = []
    centres = [(w / 2 - radius, h / 2 - radius, 0), (-w / 2 + radius, h / 2 - radius, 90), (-w / 2 + radius, -h / 2 + radius, 180), (w / 2 - radius, -h / 2 + radius, 270)]
    for cx, cy, a0 in centres:
        for k in range(seg + 1):
            a = math.radians(a0 + 90 * k / seg)
            out.append((cx + radius * math.cos(a), cy + radius * math.sin(a)))
    tv = [bm.verts.new((x, y, t / 2)) for x, y in out]
    bv = [bm.verts.new((x, y, -t / 2)) for x, y in out]
    ftop = bm.faces.new(tv)
    for lp in ftop.loops:
        x, y = lp.vert.co.x, lp.vert.co.y
        lp[uv].uv = (x / w + 0.5, 0.5 + 0.5 * (y / h + 0.5))
    fbot = bm.faces.new(list(reversed(bv)))
    for lp in fbot.loops:
        x, y = lp.vert.co.x, lp.vert.co.y
        lp[uv].uv = (1 - (x / w + 0.5), 0.5 * (y / h + 0.5))
    n = len(out)
    for i in range(n):
        j = (i + 1) % n
        f = bm.faces.new((tv[i], bv[i], bv[j], tv[j]))
        for lp in f.loops:
            lp[uv].uv = (0.5, 0.97)  # the card's own edge colour is the front's top margin
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    return me


def export_glb(me, name, raw, base_png):
    sc = bpy.context.scene
    ob = bpy.data.objects.new(name, me)
    sc.collection.objects.link(ob)
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(base_png)
    mat.node_tree.links.new(tex.outputs["Color"], mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
    me.materials.append(mat)
    for p in me.polygons:
        p.use_smooth = False
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.gltf(filepath=os.path.join(raw, "model.glb"), use_selection=True, export_format="GLB")
    return ob


def review(path, ob, dist=0.5, res=(640, 640), angle=(55, 0, 20)):
    """A picture of the textured model from above, for the agent that reads it."""
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "TEXTURE"
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.pixel_aspect_x = sc.render.pixel_aspect_y = 1.0
    cam = bpy.data.cameras.new("rc")
    cam.lens = 70
    co = bpy.data.objects.new("rc", cam)
    sc.collection.objects.link(co)
    sc.camera = co
    co.location = (0, -dist * math.sin(math.radians(angle[0])) * 0.0, dist)
    co.rotation_euler = (math.radians(angle[1]), 0, 0)
    sc.render.filepath = path
    sc.render.image_settings.file_format = "PNG"
    bpy.ops.render.render(write_still=True)


# ---------------------------------------------------------------- finishing touches
def worn(img, amount, seed):
    """Print that has not taken evenly: lifts the ink toward the paper in patches (1 = bare paper)."""
    h, w = img.shape[:2]
    gaps = np.clip(coloured(h, w, 1.2, seed) * 0.6 + 0.1, 0, 1) * amount
    return 1.0 - (1.0 - img) * (1.0 - gaps[:, :, None])


def fold(h, w, axis, pos, width_px=10, strength=0.05, bump=0.8):
    """A crease at fraction `pos` across the sheet: (shade multiplier offset, height) for compose(crease=...)."""
    if axis == "h":
        d = (np.arange(h, dtype=np.float32)[:, None] - pos * h) * np.ones((1, w), np.float32)
    else:
        d = (np.arange(w, dtype=np.float32)[None, :] - pos * w) * np.ones((h, 1), np.float32)
    g = np.exp(-(d / width_px) ** 2)
    side = np.tanh(d / (width_px * 1.5))
    return (-strength * g + strength * 0.5 * g * side, -bump * g + 0.35 * bump * side * g)
