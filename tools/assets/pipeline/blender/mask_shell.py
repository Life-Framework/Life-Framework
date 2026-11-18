# Shared builder for the 1989 face masks (tools/assets/doll-mask, tools/assets/hockey-mask; #1997's balaclava is the
# same idea: the base body's own head, cut, pushed out and thickened, so it sits on Bohemia's head to the millimetre).
# A per-mask model.py (spec "mesh.build") imports this, describes its outline, eye holes and paint, and calls build().
#
# Axes (Blender, as the Samples' template): z up, the face at -y, x to the character's left. Head: z 1.526 (neck) to
# 1.794 (crown), nose tip z 1.65 at y -0.104, eyes centred x +-0.030 z 1.689, chin underside z 1.575.
#
# The texture is one front projection: u and v are the mask's x and z in metres times US, so a painted feature keeps
# its real size on the face. The strap's faces all sit on one small black patch at the texture's left edge.
import bpy, bmesh, math, os, sys
import numpy as np
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

HEAD_C = Vector((0.0, 0.01, 1.66))
NECK_Z = 1.525
SKIN_SLOTS = ("Basebody_01_Male_Head", "Neck", "Head_White_02")
EYES = ((0.030, 1.689), (-0.030, 1.689))   # eye centres (x, z)
NOSE = (0.0, 1.650)
CHIN_Z = 1.575
Z0, US, V0, UC = 1.56, 3.75, 0.05, 0.55     # texture mapping: v = V0 + (z - Z0) * US, u = UC + x * US
STRAP_UV = (0.03, 0.5)
PX = 1024


def uv_of(x, z):
    return (UC + x * US, V0 + (z - Z0) * US)


def grid(n=PX):
    """Metre coordinates (x, z) of every texture pixel; row 0 is the bottom (Blender's own order)."""
    j = (np.arange(n) + 0.5) / n
    i = (np.arange(n) + 0.5) / n
    X = ((j - UC) / US)[None, :].repeat(n, 0)
    Z = Z0 + ((i - V0) / US)[:, None].repeat(n, 1)
    return X, Z


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


class Canvas:
    """Painting on a float RGB (linear), roughness and height; every shape is an alpha field over the metre grid."""

    def __init__(self, base_rgb, rough, n=PX):
        self.n = n
        self.X, self.Z = grid(n)
        self.rgb = np.zeros((n, n, 3)) + np.array(base_rgb)
        self.rough = np.zeros((n, n)) + rough
        self.height = np.zeros((n, n))

    def over(self, alpha, rgb, rough=None, height=None):
        a = np.clip(alpha, 0, 1)
        self.rgb = self.rgb * (1 - a[..., None]) + np.array(rgb)[None, None, :] * a[..., None]
        if rough is not None:
            self.rough = self.rough * (1 - a) + rough * a
        if height is not None:
            self.height = self.height * (1 - a) + height * a

    def ellipse(self, cx, cz, rx, rz, soft=0.0006):
        d = np.sqrt(((self.X - cx) / rx) ** 2 + ((self.Z - cz) / rz) ** 2)
        return 1 - smooth(1 - soft / min(rx, rz), 1 + soft / min(rx, rz), d)

    def stroke(self, pts, width, soft=0.0004, taper=None):
        """A polyline of (x, z) points drawn `width` metres thick; taper (a, b) scales the width from the first to the last point."""
        best = np.full(self.X.shape, 9.0)
        wscale = np.ones(self.X.shape)
        total = sum(math.hypot(pts[k + 1][0] - pts[k][0], pts[k + 1][1] - pts[k][1]) for k in range(len(pts) - 1)) or 1.0
        run = 0.0
        for k in range(len(pts) - 1):
            (ax, az), (bx, bz) = pts[k], pts[k + 1]
            dx, dz = bx - ax, bz - az
            l2 = dx * dx + dz * dz or 1e-12
            t = np.clip(((self.X - ax) * dx + (self.Z - az) * dz) / l2, 0, 1)
            d = np.hypot(self.X - (ax + t * dx), self.Z - (az + t * dz))
            seg = math.sqrt(l2)
            frac = (run + t * seg) / total
            w = 1.0
            if taper:
                w = taper[0] + (taper[1] - taper[0]) * frac
            # distance relative to the local half width
            rel = d / (0.5 * width * w)
            better = rel < best
            best = np.where(better, rel, best)
            run += seg
        edge = soft / (0.5 * width)
        return 1 - smooth(1 - edge, 1 + edge, best)

    def rect(self, cx, cz, hx, hz, r=0.002, soft=0.0005):
        qx = np.abs(self.X - cx) - (hx - r)
        qz = np.abs(self.Z - cz) - (hz - r)
        d = np.hypot(np.maximum(qx, 0), np.maximum(qz, 0)) + np.minimum(np.maximum(qx, qz), 0) - r
        return 1 - smooth(-soft, soft, d)


def blur(a, k=3):
    out = np.zeros_like(a)
    cnt = 0
    for dx in range(-k, k + 1):
        for dy in range(-k, k + 1):
            out += np.roll(np.roll(a, dx, 0), dy, 1)
            cnt += 1
    return out / cnt


def value_noise(n, cell, seed):
    rng = np.random.default_rng(seed)
    g = rng.random((n // cell + 2, n // cell + 2))
    ys = (np.arange(n) / cell)
    xs = (np.arange(n) / cell)
    y0, x0 = ys.astype(int), xs.astype(int)
    fy, fx = (ys - y0)[:, None], (xs - x0)[None, :]
    fy, fx = fy * fy * (3 - 2 * fy), fx * fx * (3 - 2 * fx)
    a = g[y0][:, x0]
    b = g[y0][:, x0 + 1]
    c = g[y0 + 1][:, x0]
    d = g[y0 + 1][:, x0 + 1]
    return a * (1 - fx) * (1 - fy) + b * fx * (1 - fy) + c * (1 - fx) * fy + d * fx * fy


def srgb(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - 0.055)


def save_maps(cv, out, prefix):
    """BCR (sRGB colour, roughness in alpha) and NMO (normal x, y, metallic, AO) from a painted canvas."""
    n = cv.n
    h = blur(cv.height, 1)
    gy, gx = np.gradient(h)
    nx, ny = -gx * 400.0, -gy * 400.0
    nz = np.ones_like(h)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    nx, ny = nx / ln, ny / ln
    ao = np.clip(1.0 + cv.height * 0.6, 0, 1)
    bcr = np.dstack([srgb(cv.rgb[..., k]) for k in range(3)] + [np.clip(cv.rough, 0, 1)])
    nmo = np.dstack([nx * 0.5 + 0.5, ny * 0.5 + 0.5, np.zeros_like(h), ao])
    prev = bpy.data.images.new(prefix + "_preview", n, n, alpha=False, float_buffer=False)
    prev.pixels = np.dstack([bcr[..., :3], np.ones((n, n))]).astype(np.float32).ravel()
    prev.filepath_raw = os.path.join(out, prefix + "_preview.png")
    prev.file_format = "PNG"
    prev.save()
    for name, arr, cs in ((prefix + "_BCR", bcr, "sRGB"), (prefix + "_NMO", nmo, "Non-Color")):
        img = bpy.data.images.new(name, n, n, alpha=True, float_buffer=False)
        img.colorspace_settings.name = cs
        img.pixels = arr.astype(np.float32).ravel()
        img.alpha_mode = "CHANNEL_PACKED"
        img.filepath_raw = os.path.join(out, name + ".tif")
        img.file_format = "TIFF"
        img.save()


def _largest_island(bm):
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
    for isl in islands[:5]:
        xs = [v.co.x for f in isl for v in f.verts]
        ys = [v.co.y for f in isl for v in f.verts]
        zs = [v.co.z for f in isl for v in f.verts]
        print("LF_MASK island", len(isl), "x", round(min(xs), 3), round(max(xs), 3), "y", round(min(ys), 3), round(max(ys), 3), "z", round(min(zs), 3), round(max(zs), 3))
    for isl in islands[1:]:
        bmesh.ops.delete(bm, geom=isl, context="FACES_ONLY")
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    return [len(i) for i in islands[:5]]


def _head_bmesh(body, keep_slots, drop_test):
    bm = bmesh.new()
    bm.from_mesh(body.data)
    # the template's head is split along its UV seams: weld it, or the island filter below keeps one fragment
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0002)
    bm.faces.ensure_lookup_table()
    drop = []
    for f in bm.faces:
        c = f.calc_center_median()
        if f.material_index not in keep_slots or min(v.co.z for v in f.verts) < NECK_Z or max((v.co - HEAD_C).length for v in f.verts) > 0.165 or drop_test(c):
            drop.append(f)
    bmesh.ops.delete(bm, geom=drop, context="FACES")
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    return bm


def _apply(ob, kind, **kw):
    mod = ob.modifiers.new(kind.lower(), kind)
    for k, v in kw.items():
        setattr(mod, k, v)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier=mod.name)


def _new_object(name, me):
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.view_layer.objects.active = ob
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    ob.select_set(True)
    return ob


def _outward_normals(me, toward):
    """Flip the shell if the vertex nearest `toward` (an extreme point of the outer surface) has its normal pointing in."""
    ext = max(me.vertices, key=lambda v: v.co.dot(toward))
    if ext.normal.dot(toward) < 0:
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.mesh.flip_normals()
        bpy.ops.object.mode_set(mode="OBJECT")


def build(job, P):
    """P: name, inside(c) -> bool for a face centre of the front shell, hole(x, z) -> bool, smooth_iter, tris, offset, thick,
    strap {z, half, y_start, offset, thick}, paint(Canvas-module) -> Canvas, material, item_material, review_name."""
    out = job["out"]
    os.makedirs(out, exist_ok=True)
    name = P["name"]
    bpy.ops.wm.open_mainfile(filepath=job["template_blend"], load_ui=False, use_scripts=False)
    body = bpy.data.objects["Body_LOD0"]
    slot_names = [(s.material.name.replace("\\", "/").split("/")[-1] if s.material else "") for s in body.material_slots]
    keep_slots = {i for i, n in enumerate(slot_names) if n in SKIN_SLOTS}

    # 1. The front shell: head skin where the outline is, minus the eye holes.
    bm = _head_bmesh(body, keep_slots, lambda c: (not P["inside"](c)) or P["hole"](c.x, c.z))
    isl = _largest_island(bm)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.materials.clear()
    shell = _new_object(name, me)
    print("LF_MASK shell faces", len(me.polygons), "islands", isl)

    # 2. Soften the skin's relief (lips and eyelids), keep nose and brow, thin it out, push out, thicken.
    _apply(shell, "SMOOTH", factor=0.5, iterations=P["smooth_iter"])
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    if tris > P["tris"]:
        _apply(shell, "DECIMATE", ratio=P["tris"] / tris)
    _apply(shell, "SMOOTH", factor=0.5, iterations=3)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bm.normal_update()
    for v in bm.verts:
        v.co += v.normal * P["offset"]
    bm.to_mesh(me)
    bm.free()
    # smoothing pulls the nose and brow below the skin: every vertex is pushed back out to a clear gap above the real head
    skin = _head_bmesh(body, keep_slots, lambda c: False)
    bvh = BVHTree.FromBMesh(skin)
    gaps = P.get("gaps", (0.003, 0.003))
    for rnd in range(2):
        poked = 0
        for v in me.vertices:
            loc, nrm, _i, dist = bvh.find_nearest(v.co)
            if loc is not None and (v.co - loc).dot(nrm) < gaps[rnd]:
                v.co = loc + nrm * gaps[rnd]
                poked += 1
        print("LF_MASK clearance pass", rnd, "pushed", poked, "of", len(me.vertices))
        if rnd == 0:
            _apply(shell, "SMOOTH", factor=0.5, iterations=P.get("smooth_after", 2))
    skin.free()
    _outward_normals(me, Vector((0, -1, 0)))
    _apply(shell, "SOLIDIFY", thickness=P["thick"], offset=1.0, use_even_offset=False)
    bpy.ops.object.shade_smooth()
    # front projection UVs, by hand: the painted features keep their size in metres
    while me.uv_layers:       # the head's own UV layers came along with the cut: only ours may remain
        me.uv_layers.remove(me.uv_layers[0])
    uvl = me.uv_layers.new(name="UVMap")
    for p in me.polygons:
        for li in p.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            uvl.data[li].uv = uv_of(co.x, co.z)
    shell_tris = sum(len(p.vertices) - 2 for p in me.polygons)
    print("LF_MASK shell tris", shell_tris)

    # 3. The ground model: just the shell, lying face up.
    item = shell.copy()
    item.data = shell.data.copy()
    item.name = item.data.name = name + "_item"
    bpy.context.scene.collection.objects.link(item)
    item.rotation_euler = (math.radians(-90), 0, 0)
    bpy.context.view_layer.update()
    item.data.transform(item.matrix_world)
    item.matrix_world.identity()
    xs = [v.co.x for v in item.data.vertices]
    ys = [v.co.y for v in item.data.vertices]
    zs = [v.co.z for v in item.data.vertices]
    item.data.transform(Matrix.Translation((-(min(xs) + max(xs)) / 2, -(min(ys) + max(ys)) / 2, -min(zs))))
    print("LF_MASK item size", round(max(xs) - min(xs), 3), round(max(ys) - min(ys), 3), round(max(zs) - min(zs), 3))

    # 4. The strap: a thin band of the head's own surface above the ears, from one side of the shell round the back to the other.
    S = P["strap"]
    bm = _head_bmesh(body, keep_slots, lambda c: abs(c.z - S["z"]) > S["half"] or c.y < S["y_start"])
    _largest_island(bm)
    sme = bpy.data.meshes.new(name + "_strap")
    bm.to_mesh(sme)
    bm.free()
    strap = _new_object(name + "_strap", sme)
    bm = bmesh.new()
    bm.from_mesh(sme)
    bm.normal_update()
    for v in bm.verts:
        if v.is_boundary and abs(v.co.z - S["z"]) > S["half"] * 0.3:
            v.co.z = S["z"] + math.copysign(S["half"], v.co.z - S["z"])    # straight band edges, not the head mesh's zigzag
        v.co += v.normal * S["offset"]
    bm.to_mesh(sme)
    bm.free()
    _outward_normals(sme, Vector((0, 1, 0)))
    _apply(strap, "SOLIDIFY", thickness=S["thick"], offset=1.0, use_even_offset=False)
    while sme.uv_layers:
        sme.uv_layers.remove(sme.uv_layers[0])
    suv = sme.uv_layers.new(name="UVMap")
    for li in range(len(sme.loops)):
        suv.data[li].uv = STRAP_UV
    # decimate the strap: a band needs few triangles
    stris = sum(len(p.vertices) - 2 for p in sme.polygons)
    if stris > 700:
        _apply(strap, "DECIMATE", ratio=700 / stris)
    bpy.ops.object.shade_smooth()
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    shell.select_set(True)
    strap.select_set(True)
    bpy.context.view_layer.objects.active = shell
    bpy.ops.object.join()
    worn = bpy.context.view_layer.objects.active
    worn.name = worn.data.name = name
    print("LF_MASK worn tris", sum(len(p.vertices) - 2 for p in worn.data.polygons), "strap tris", stris)

    # 5. The paint.
    cv = P["paint"]()
    save_maps(cv, out, name)

    # 6. Materials (the names the import matches), then the .blend the fit stage reads.
    for o, mname in ((worn, P["material"]), (item, P["item_material"])):
        o.data.materials.clear()
        o.data.materials.append(bpy.data.materials.new(mname))
    keep = {name, name + "_item"}
    for o in [o for o in bpy.data.objects if o.name not in keep]:
        bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, P["blend"]), compress=True)

    # 7. A picture for the review: the textured mask on the base body's head, front, three quarters and side.
    if job.get("review", True):
        bpy.ops.wm.open_mainfile(filepath=job["template_blend"], load_ui=False, use_scripts=False)
        with bpy.data.libraries.load(os.path.join(out, P["blend"]), link=False) as (src, dst):
            dst.objects = [name]
        b = dst.objects[0]
        bpy.context.scene.collection.objects.link(b)
        mat = bpy.data.materials.new("review")
        mat.use_nodes = True
        tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
        tex.image = bpy.data.images.load(os.path.join(out, name + "_BCR.tif"))
        tex.image.alpha_mode = "NONE"      # the alpha channel is roughness, not transparency
        mat.node_tree.nodes.active = tex
        b.data.materials.clear()
        b.data.materials.append(mat)
        for o in bpy.data.objects:
            if o.type == "MESH" and o.name not in (name, "Body_LOD0"):
                o.hide_render = True
        bpy.data.objects["Body_LOD0"].hide_render = False
        bpy.data.objects["Body_LOD0"].color = (0.8, 0.6, 0.5, 1)
        sc = bpy.context.scene
        sc.render.engine = "BLENDER_WORKBENCH"
        sc.render.resolution_x, sc.render.resolution_y = 900, 900
        sc.display.shading.light = "STUDIO"
        sc.display.shading.color_type = "TEXTURE"
        cam_d = bpy.data.cameras.new("cam")
        cam_d.type = "ORTHO"
        cam_d.ortho_scale = 0.40
        cam = bpy.data.objects.new("cam", cam_d)
        sc.collection.objects.link(cam)
        sc.camera = cam
        views = (((0, -2, 1.66), (math.pi / 2, 0, 0)), ((1.4, -1.4, 1.66), (math.pi / 2, 0, math.pi / 4)), ((2, 0, 1.66), (math.pi / 2, 0, math.pi / 2)))
        for i, (loc, rot) in enumerate(views):
            cam.location, cam.rotation_euler = loc, rot
            sc.render.filepath = os.path.join(out, "review_%d.png" % i)
            bpy.ops.render.render(write_still=True)
    print("LF_MASK ok")
