# Headless clean-up of a generated model for Enfusion. Run by tools\cli asset (stage "clean"):
#   blender --background --factory-startup --python clean.py -- <job.json>
# job.json = { spec, kind, glb, raw, out }. Writes into <out>:
#   <Name>.fbx                    LODs, collider, and for a weapon the armature and socket empties
#   <Name>_BCR.tif, <Name>_NMO.tif  Enfusion channel packing (BCR: base colour + roughness in alpha;
#                                 NMO: normal X, normal Y flipped to DirectX, metallic, AO in alpha)
#   review/*.png, icon.png        renders an agent reads (docs/asset-pipeline/review-clean.md)
#   report.json                   every number tools/assets/pipeline/checks.mjs judges
#
# Space: Enfusion is x right, y up, z forward. Blender is X right, Y forward, Z up, and the default
# FBX export (forward -Z, up Y) lands Blender (X, Y, Z) on Enfusion (x, z, y). e2b/b2e do that swap.
import bpy, bmesh, sys, json, os, math, struct
import numpy as np
from mathutils import Vector, Matrix

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
spec, kind, out = job["spec"], job["kind"], job["out"]
name = spec["name"]
fit = spec.get("fit", {})
report = {"name": name, "notes": []}
os.makedirs(os.path.join(out, "review"), exist_ok=True)


def e2b(p):
    return Vector((p[0], p[2], p[1]))


def b2e(v):
    return [round(float(v[0]), 5), round(float(v[2]), 5), round(float(v[1]), 5)]


def activate(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def bounds(obj):
    co = np.empty(len(obj.data.vertices) * 3, np.float32)
    obj.data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    return co, co.min(axis=0), co.max(axis=0)


def tri_count(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


# ---------------------------------------------------------------- import and join
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=job["glb"])
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
if not meshes:
    raise SystemExit("the GLB holds no mesh")
source_images = {}
for m in bpy.data.materials:
    if not m.use_nodes:
        continue
    for n in m.node_tree.nodes:
        if n.type == "BSDF_PRINCIPLED":
            for sock, key in (("Base Color", "base_color"), ("Normal", "normal"), ("Roughness", "roughness"), ("Metallic", "metallic")):
                link = n.inputs[sock].links[0] if n.inputs[sock].links else None
                node = link.from_node if link else None
                while node and node.type != "TEX_IMAGE":
                    ins = [i for i in node.inputs if i.links]
                    node = ins[0].links[0].from_node if ins else None
                if node and node.image:
                    source_images.setdefault(key, node.image)
bpy.ops.object.select_all(action="DESELECT")
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
obj = bpy.context.view_layer.objects.active
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for o in list(bpy.data.objects):
    if o is not obj:
        bpy.data.objects.remove(o, do_unlink=True)
report["source"] = {"meshes": len(meshes), "tris": tri_count(obj), "verts": len(obj.data.vertices)}

# ---------------------------------------------------------------- geometry clean-up
bm = bmesh.new()
bm.from_mesh(obj.data)
before = len(bm.verts)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
loose = [v for v in bm.verts if not v.link_faces]
bmesh.ops.delete(bm, geom=loose, context="VERTS")
# Floaters: islands of a few faces that the generator left hanging in the air.
seen, islands = set(), []
for f in bm.faces:
    if f in seen:
        continue
    stack, isl = [f], []
    seen.add(f)
    while stack:
        g = stack.pop()
        isl.append(g)
        for e in g.edges:
            for h in e.link_faces:
                if h not in seen:
                    seen.add(h)
                    stack.append(h)
    islands.append(isl)
small = [f for isl in islands if len(isl) < 8 for f in isl]
bmesh.ops.delete(bm, geom=small, context="FACES")
bmesh.ops.triangulate(bm, faces=bm.faces)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
report["cleanup"] = {"merged_verts": before - len(bm.verts) - len(loose), "loose_verts": len(loose),
                     "islands": len(islands), "floater_faces": len(small),
                     "non_manifold_edges": sum(1 for e in bm.edges if not e.is_manifold)}
bm.to_mesh(obj.data)
bm.free()

# ---------------------------------------------------------------- orientation, scale, origin
co, lo, hi = bounds(obj)
ext = hi - lo
size = spec["size_m"]
# Generators hand the model over upright, so up stays up unless the spec says which source axis is
# up (fit.up_axis: "x" | "y" | "z"). Of the two horizontal axes the wider one is the length.
up = {"x": 0, "y": 1, "z": 2}[fit.get("up_axis", "z")]
flat = sorted([a for a in (0, 1, 2) if a != up], key=lambda a: -ext[a])
have = [flat[1], flat[0], up]  # source axis for width (X), length (Y), height (Z)
R = Matrix(((0, 0, 0), (0, 0, 0), (0, 0, 0)))
for target, source in enumerate(have):
    R[target][int(source)] = 1.0
if R.determinant() < 0:
    R[0] = -R[0]  # keep it a rotation; the width axis is the one that may mirror
obj.data.transform(R.to_4x4())
orient = {"axis_map": [int(s) for s in have], "flip_up": False, "flip_forward": False}

if kind["fit"] == "pistol":
    # The barrel half is longer than the grip half, and the grip sits at the rear.
    co, lo, hi = bounds(obj)
    mid = (lo[2] + hi[2]) / 2
    top, bottom = co[co[:, 2] > mid], co[co[:, 2] <= mid]
    if np.ptp(bottom[:, 1]) > np.ptp(top[:, 1]):
        orient["flip_up"] = True
    if fit.get("flip_up"):
        orient["flip_up"] = not orient["flip_up"]
    if orient["flip_up"]:
        obj.data.transform(Matrix.Rotation(math.pi, 4, "Y"))
    co, lo, hi = bounds(obj)
    grip = co[co[:, 2] < lo[2] + 0.35 * (hi[2] - lo[2])]
    if grip[:, 1].mean() > (lo[1] + hi[1]) / 2:
        orient["flip_forward"] = True
    if fit.get("flip_forward"):
        orient["flip_forward"] = not orient["flip_forward"]
    if orient["flip_forward"]:
        obj.data.transform(Matrix.Rotation(math.pi, 4, "Z"))
else:
    if fit.get("flip_up"):
        orient["flip_up"] = True
        obj.data.transform(Matrix.Rotation(math.pi, 4, "Y"))
    if fit.get("flip_forward"):
        orient["flip_forward"] = True
        obj.data.transform(Matrix.Rotation(math.pi, 4, "Z"))
if fit.get("pitch_deg"):
    obj.data.transform(Matrix.Rotation(math.radians(fit["pitch_deg"]), 4, "X"))
report["orientation"] = orient

co, lo, hi = bounds(obj)
ext = hi - lo
# A deployable can be made of one generated link repeated along its length (fit.repeat): the
# generator makes the link, this stage tiles it, so size_m stays the size of the whole thing.
repeat = max(int(fit.get("repeat", 1)), 1)
link_length = size["length"] / repeat
scale = (size["height"] / ext[2]) if kind["fit"] == "pistol" else (link_length / ext[1])
scale *= fit.get("scale", 1.0)
if fit.get("stretch") and kind["fit"] != "pistol":
    # Fit each axis to size_m on its own: right for a flat prop whose height the generator guesses.
    obj.data.transform(Matrix.Diagonal((size["width"] / ext[0], link_length / ext[1], size["height"] / ext[2], 1.0)))
    report["stretch"] = [float(size["width"] / ext[0] / scale), 1.0, float(size["height"] / ext[2] / scale)]
else:
    obj.data.transform(Matrix.Scale(scale, 4))
co, lo, hi = bounds(obj)
if kind["fit"] == "pistol":
    ref = kind["reference"]["bounds"]
    move = Vector((-(lo[0] + hi[0]) / 2, ref["min"][2] - lo[1], ref["min"][1] - lo[2]))
elif kind["fit"] == "ground":
    move = Vector((-(lo[0] + hi[0]) / 2, -(lo[1] + hi[1]) / 2, -lo[2]))
else:
    move = Vector((-(lo[0] + hi[0]) / 2, -(lo[1] + hi[1]) / 2, -(lo[2] + hi[2]) / 2))
move += e2b(fit.get("offset", [0, 0, 0]))
obj.data.transform(Matrix.Translation(move))
co, lo, hi = bounds(obj)
report["scale"] = float(scale)
report["size"] = [float(hi[1] - lo[1]), float(hi[2] - lo[2]), float(hi[0] - lo[0])]  # length, height, width
report["bounds"] = {"min": b2e(lo), "max": b2e(hi)}

# The fitted, cleaned, undecimated mesh: the source of the high-to-low normal bake below.
high = obj.copy()
high.data = obj.data.copy()
high.name = "lf_high"
bpy.context.collection.objects.link(high)
report["source"]["tris_after_cleanup"] = tri_count(high)

# ---------------------------------------------------------------- UVs and the one material
uvs = obj.data.uv_layers
while len(uvs) > 1:
    uvs.remove(uvs[-1])
if not uvs:
    raise SystemExit("the generated mesh has no UV map; regenerate with textures")
uvs[0].name = "UVMap"
uv = np.empty(len(obj.data.loops) * 2, np.float32)
uvs[0].data.foreach_get("uv", uv)
report["uv"] = {"min": float(uv.min()), "max": float(uv.max())}


def find_map(key):
    path = os.path.join(job["raw"], "tex_%s.png" % key)
    if os.path.exists(path):
        return bpy.data.images.load(path)
    return source_images.get(key)


def pixels(img, px, fallback):
    if img is None:
        return np.full((px, px, 4), fallback, np.float32)
    img = img.copy()
    img.scale(px, px)
    arr = np.empty(px * px * 4, np.float32)
    img.pixels.foreach_get(arr)
    return arr.reshape(px, px, 4)[::-1]  # Blender rows run bottom-up; TIFF runs top-down


def write_tiff(path, rgba):
    # Baseline uncompressed 8-bit RGBA TIFF with unassociated alpha: what Workbench imports.
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


px = spec["budget"]["texture_px"]
maps = {k: find_map(k) for k in ("base_color", "roughness", "metallic", "normal")}
report["textures"] = {"px": px, "found": [k for k, v in maps.items() if v is not None]}
if maps["base_color"] is None:
    raise SystemExit("no base colour map: neither raw/tex_base_color.png nor an image in the GLB")
base = pixels(maps["base_color"], px, (0.5, 0.5, 0.5, 1))
rough = pixels(maps["roughness"], px, (0.6, 0.6, 0.6, 1))
metal = pixels(maps["metallic"], px, (0, 0, 0, 1))
normal = pixels(maps["normal"], px, (0.5, 0.5, 1, 1))


def height_mask(above_m):
    # Faces whose centre is above this height (Blender Z, metres, after the fit), rasterised through
    # their UV triangles. Rows are flipped like pixels(): Blender UV v runs up, the TIFF runs down.
    me = obj.data
    me.calc_loop_triangles()
    uv = np.empty(len(me.loops) * 2, np.float32)
    me.uv_layers[0].data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2).astype(np.float64)
    mask = np.zeros((px, px), dtype=bool)
    for t in me.loop_triangles:
        if me.polygons[t.polygon_index].center.z <= above_m:
            continue
        p = uv[list(t.loops)] * px
        x0, x1 = int(max(0, np.floor(p[:, 0].min()))), int(min(px - 1, np.ceil(p[:, 0].max())))
        y0, y1 = int(max(0, np.floor(p[:, 1].min()))), int(min(px - 1, np.ceil(p[:, 1].max())))
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        a, b, c = p
        d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(d) < 1e-12:
            continue
        w1 = ((b[1] - c[1]) * (xs - c[0]) + (c[0] - b[0]) * (ys - c[1])) / d
        w2 = ((c[1] - a[1]) * (xs - c[0]) + (a[0] - c[0]) * (ys - c[1])) / d
        mask[y0:y1 + 1, x0:x1 + 1] |= (w1 >= -0.02) & (w2 >= -0.02) & (1 - w1 - w2 >= -0.02)
    return mask[::-1]


# Material classes (tools/assets/pipeline/materials.mjs): Meshy's roughness and metallic remapped per
# declared region, so nothing reaches the game as polished chrome.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import materials
plan = job["materials"]
masks = materials.region_masks(plan, metal[:, :, 0], height_mask)
base3, rough1, metal1 = materials.apply(plan, base[:, :, :3], rough[:, :, 0], metal[:, :, 0], masks)
report["materials"] = {"default": plan["default"], "regions": [{"class": g["class"], "share": float(m.mean())} for g, m in zip(plan["regions"], masks)],
                       "before": {"rough": float(rough[:, :, 0].mean()), "metal": float(metal[:, :, 0].mean())},
                       "after": {"rough": float(rough1.mean()), "metal": float(metal1.mean())}}
base[:, :, :3], rough[:, :, 0], metal[:, :, 0] = base3, rough1, metal1
bcr = np.dstack([base[:, :, :3], rough[:, :, 0]])
nmo = np.dstack([normal[:, :, 0], 1.0 - normal[:, :, 1], metal[:, :, 0], np.ones((px, px), np.float32)])
write_tiff(os.path.join(out, name + "_BCR.tif"), (np.clip(bcr, 0, 1) * 255 + 0.5).astype(np.uint8))
write_tiff(os.path.join(out, name + "_NMO.tif"), (np.clip(nmo, 0, 1) * 255 + 0.5).astype(np.uint8))
report["textures"]["mean_base"] = [float(v) for v in base[:, :, :3].mean(axis=(0, 1))]
report["textures"]["mean_roughness"] = float(rough[:, :, 0].mean())
report["textures"]["mean_metallic"] = float(metal[:, :, 0].mean())

obj.data.materials.clear()
mat = bpy.data.materials.new(name)  # this name is the FBX SourceMaterial the .xob.meta maps to the .emat
mat.use_nodes = True
tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
tex.image = maps["base_color"]
mat.node_tree.links.new(tex.outputs["Color"], mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
mat.node_tree.nodes.active = tex
obj.data.materials.append(mat)
for p in obj.data.polygons:
    p.material_index = 0

# ---------------------------------------------------------------- LODs
# With fit.repeat the budget is for the whole strip, so each link gets its share.
budget = spec["budget"]["lod0_tris"] / repeat


def decimate(o, ratio):
    if ratio >= 1:
        return
    activate(o)
    mod = o.modifiers.new("d", "DECIMATE")
    mod.ratio = ratio
    mod.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier="d")


decimate(obj, budget / max(tri_count(obj), 1))
obj.name = name + "_LOD0"
obj.data.name = obj.name
for p in obj.data.polygons:
    p.use_smooth = True


def bake_normal(high, low, px, normal_img, lod_mat, tex_node):
    """Tangent-space normal map of `high` on `low`'s UVs (what a normal map is for). Returns an
    (px, px, 3) array in the orientation pixels() uses, or None when the bake failed."""
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 1
    img = bpy.data.images.new("lf_bake_normal", px, px, alpha=False, float_buffer=False)
    img.colorspace_settings.name = "Non-Color"
    # The low mesh bakes into an image texture node that is active in its material.
    node = lod_mat.node_tree.nodes.new("ShaderNodeTexImage")
    node.image = img
    lod_mat.node_tree.nodes.active = node
    # The high mesh carries the generator's own normal map, so its fine detail lands in the bake too.
    hm = bpy.data.materials.new("lf_high_mat")
    hm.use_nodes = True
    if normal_img is not None:
        nm = hm.node_tree.nodes.new("ShaderNodeNormalMap")
        tn = hm.node_tree.nodes.new("ShaderNodeTexImage")
        tn.image = normal_img
        normal_img.colorspace_settings.name = "Non-Color"
        hm.node_tree.links.new(tn.outputs["Color"], nm.inputs["Color"])
        hm.node_tree.links.new(nm.outputs["Normal"], hm.node_tree.nodes["Principled BSDF"].inputs["Normal"])
    high.data.materials.clear()
    high.data.materials.append(hm)
    co, lo, hi = bounds(low)
    diag = float(np.linalg.norm(hi - lo))
    bpy.ops.object.select_all(action="DESELECT")
    high.select_set(True)
    low.select_set(True)
    bpy.context.view_layer.objects.active = low
    try:
        bpy.ops.object.bake(type="NORMAL", use_selected_to_active=True, normal_space="TANGENT",
                            cage_extrusion=0.01 * diag, max_ray_distance=0.03 * diag, margin=16, margin_type="EXTEND")
    except Exception as e:  # a failed bake keeps the generator's own map
        report["notes"].append("normal bake failed: %s" % e)
        lod_mat.node_tree.nodes.remove(node)
        return None
    arr = np.empty(px * px * 4, np.float32)
    img.pixels.foreach_get(arr)
    arr = arr.reshape(px, px, 4)[::-1, :, :3].copy()
    lod_mat.node_tree.nodes.remove(node)
    # Tilt statistics: how strong the map is (0 degrees is a flat map).
    v = arr * 2 - 1
    z = np.clip(v[:, :, 2] / np.maximum(np.linalg.norm(v, axis=2), 1e-6), -1, 1)
    ang = np.degrees(np.arccos(z))
    empty = float((arr.sum(axis=2) < 0.02).mean())
    report["bake"] = {"ok": empty < 0.5, "source_tris": tri_count(high), "low_tris": tri_count(low),
                      "mean_tilt_deg": float(ang.mean()), "p95_tilt_deg": float(np.percentile(ang, 95)), "empty_share": empty,
                      "cage_extrusion_m": 0.01 * diag, "max_ray_distance_m": 0.03 * diag}
    return arr if empty < 0.5 else None


# A remeshed model arrives at LOD0 already: there is nothing to bake from. A collapse from a bigger
# source (a model that skipped the remesh stage) gets its normals baked, unless fit.bake is false:
# on a flat plate studded with spikes (the spike strip) the bake turns the collapse error into dark
# dents around every spike, and the generator's own map reads better.
if fit.get("bake", True) is False:
    report["textures"]["normal"] = "the generator's own map (fit.bake is false)"
elif px >= 256 and tri_count(high) > 1.15 * tri_count(obj):
    baked = bake_normal(high, obj, px, maps["normal"], mat, None)
    if baked is not None:
        normal = baked
        nmo = np.dstack([normal[:, :, 0], 1.0 - normal[:, :, 1], metal[:, :, 0], np.ones((px, px), np.float32)])
        write_tiff(os.path.join(out, name + "_NMO.tif"), (np.clip(nmo, 0, 1) * 255 + 0.5).astype(np.uint8))
        report["textures"]["normal"] = "baked from %d source triangles" % tri_count(high)
    else:
        report["textures"]["normal"] = "the generator's own map"
bpy.data.objects.remove(high, do_unlink=True)
mat.node_tree.nodes.active = tex
bpy.context.scene.render.engine = "BLENDER_WORKBENCH"
report["textures"]["source_normal_mean_tilt_deg"] = float(np.degrees(np.arccos(np.clip(
    (lambda v: v[:, :, 2] / np.maximum(np.linalg.norm(v, axis=2), 1e-6))(pixels(maps["normal"], 256, (0.5, 0.5, 1, 1))[:, :, :3] * 2 - 1), -1, 1))).mean())

lods = [obj]
for i, ratio in enumerate(spec["budget"].get("lod_ratios", []), start=1):
    activate(obj)
    bpy.ops.object.duplicate()
    lod = bpy.context.view_layer.objects.active
    lod.name = "%s_LOD%d" % (name, i)
    lod.data.name = lod.name
    decimate(lod, ratio)
    lods.append(lod)


def tile(o, count, axis):
    # Repeat the mesh `count` times end to end along a Blender axis (1 = length, 2 = height).
    if count <= 1:
        return
    activate(o)
    mod = o.modifiers.new("tile", "ARRAY")
    mod.count = count
    mod.use_relative_offset = True
    mod.relative_offset_displace = [1.0 if k == axis else 0.0 for k in range(3)]
    bpy.ops.object.modifier_apply(modifier="tile")


folded = []
if repeat > 1:
    if fit.get("fold"):
        # The folded state: the same links stacked flat on each other, as a strip is carried.
        for i, lod in enumerate(lods):
            activate(lod)
            bpy.ops.object.duplicate()
            f = bpy.context.view_layer.objects.active
            f.name = "%s_Folded_LOD%d" % (name, i)
            f.data.name = f.name
            tile(f, repeat, 2)
            folded.append(f)
    link = float(bounds(obj)[2][1] - bounds(obj)[1][1])
    for lod in lods:
        tile(lod, repeat, 1)
        # Centre the strip on the origin along its length again.
        lod.data.transform(Matrix.Translation(Vector((0, -link * (repeat - 1) / 2, 0))))
    co, lo, hi = bounds(obj)
    report["size"] = [float(hi[1] - lo[1]), float(hi[2] - lo[2]), float(hi[0] - lo[0])]
    report["bounds"] = {"min": b2e(lo), "max": b2e(hi)}
    report["repeat"] = {"count": repeat, "link_m": link}
report["lods"] = [{"name": o.name, "tris": tri_count(o), "verts": len(o.data.vertices)} for o in lods]
co, lo, hi = bounds(obj)

# ---------------------------------------------------------------- collider
col_cfg = kind["collider"]


def box_into(cm, lo, hi):
    bmesh.ops.create_cube(cm, size=1)
    for v in cm.verts:
        v.co = Vector(((lo[0] + hi[0]) / 2 + v.co.x * (hi[0] - lo[0]), (lo[1] + hi[1]) / 2 + v.co.y * (hi[1] - lo[1]),
                       (lo[2] + hi[2]) / 2 + v.co.z * (hi[2] - lo[2])))


cm = bmesh.new()
if col_cfg["prefix"] == "UCX_":
    # Support points along 42 fixed directions: the hull of a subset of hull points is convex.
    ico = bmesh.new()
    bmesh.ops.create_icosphere(ico, subdivisions=2, radius=1)
    picks = sorted({int(np.argmax(co @ np.array(v.co, np.float32))) for v in ico.verts})
    ico.free()

    def sliver(face):
        # Thinnest height of a triangle: Workbench rejects a collider with a near-zero one
        # ("Face is degenerated", then "Collider is not valid").
        longest = max(e.calc_length() for e in face.edges)
        return 2 * face.calc_area() / longest if longest > 0 else 0

    # Merge the support points further apart until no hull face is a sliver.
    for merge in (0.02, 0.032, 0.05):
        cm.clear()
        for i in picks:
            cm.verts.new(co[i].tolist())
        bmesh.ops.remove_doubles(cm, verts=cm.verts, dist=merge)
        if len(cm.verts) < 4:
            break
        hull = bmesh.ops.convex_hull(cm, input=cm.verts)
        bmesh.ops.delete(cm, geom=[g for g in hull["geom_interior"] if isinstance(g, bmesh.types.BMVert)], context="VERTS")
        bmesh.ops.triangulate(cm, faces=cm.faces)
        if cm.faces and min(sliver(f) for f in cm.faces) > 0.002:
            break
    else:
        cm.clear()
    if len(cm.faces) < 4:
        # Last resort: the bounding box as a convex shape.
        report["notes"].append("collider fell back to the bounding box; the hull kept sliver faces")
        cm.clear()
        bmesh.ops.create_cube(cm, size=1)
        for v in cm.verts:
            v.co = Vector(((lo[0] + hi[0]) / 2 + v.co.x * (hi[0] - lo[0]), (lo[1] + hi[1]) / 2 + v.co.y * (hi[1] - lo[1]),
                           (lo[2] + hi[2]) / 2 + v.co.z * (hi[2] - lo[2])))
        bmesh.ops.triangulate(cm, faces=cm.faces)
    bmesh.ops.recalc_face_normals(cm, faces=cm.faces)
else:
    top = hi[2]
    if col_cfg.get("maxHeight"):
        # A thin box over the base: a strip a wheel rolls over, not a kerb it climbs.
        top = min(hi[2], lo[2] + col_cfg["maxHeight"])
    box_into(cm, lo, Vector((hi[0], hi[1], top)))


def collider_object(cm, cname):
    cmesh = bpy.data.meshes.new(cname)
    cm.to_mesh(cmesh)
    info = {"name": cmesh.name, "verts": len(cm.verts), "faces": len(cm.faces), "usage": col_cfg["layerPreset"]}
    cm.free()
    o = bpy.data.objects.new(cmesh.name, cmesh)
    bpy.context.scene.collection.objects.link(o)
    o["usage"] = col_cfg["layerPreset"]
    o.display_type = "WIRE"
    o.hide_render = True
    if col_cfg["prefix"] == "UBX_":
        # A box collider's own origin is its centre (wiki: FBX Import).
        activate(o)
        bpy.ops.object.origin_set(type="ORIGIN_GEOMETRY", center="BOUNDS")
    return o, info


collider, report["collider"] = collider_object(cm, col_cfg["prefix"] + name)

# ---------------------------------------------------------------- skeleton, skin, sockets
export = lods + [collider]
sockets = []
report["parts"] = []
report["bones"] = []
nodes = kind.get("skeleton") or []
if nodes:
    arm_data = bpy.data.armatures.new("Armature")
    arm = bpy.data.objects.new("Armature", arm_data)
    bpy.context.scene.collection.objects.link(arm)
    activate(arm)
    bpy.ops.object.mode_set(mode="EDIT")
    world = {}
    for n in nodes:
        world[n["name"]] = (world[n["parent"]] if n["parent"] else Vector((0, 0, 0))) + e2b(n["pos"])
    for n in nodes:
        if not n.get("bone"):
            continue
        b = arm_data.edit_bones.new(n["name"])
        b.head = world[n["name"]]
        b.tail = b.head + Vector((0, 0.02, 0))  # along +Y with roll 0: the rest rotation is identity
        b.roll = 0
        if n["parent"]:
            b.parent = arm_data.edit_bones[n["parent"]]
    bpy.ops.object.mode_set(mode="OBJECT")
    report["bones"] = [{"name": n["name"], "pos": b2e(world[n["name"]])} for n in nodes if n.get("bone")]

    for lod in lods:
        bm = bmesh.new()
        bm.from_mesh(lod.data)
        owner = {}
        for part in spec.get("parts", []):
            a, b = e2b(part["box"][0:3]), e2b(part["box"][3:6])
            blo = Vector((min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)))
            bhi = Vector((max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)))
            faces = [f for f in bm.faces if all(blo[k] <= f.calc_center_median()[k] <= bhi[k] for k in range(3))]
            if faces:
                # Split the part off so it moves rigidly instead of stretching the body.
                split = bmesh.ops.split(bm, geom=faces)
                faces = [g for g in split["geom"] if isinstance(g, bmesh.types.BMFace)]
            for f in faces:
                for v in f.verts:
                    owner[v] = part["bone"]
            if lod is lods[0]:
                report["parts"].append({"name": part["name"], "bone": part["bone"], "faces": len(faces)})
        bm.verts.index_update()
        groups = {v.index: owner.get(v, nodes[0]["name"]) for v in bm.verts}
        bm.to_mesh(lod.data)
        bm.free()
        for n in nodes:
            if n.get("bone"):
                lod.vertex_groups.new(name=n["name"])
        by_bone = {}
        for index, bone in groups.items():
            by_bone.setdefault(bone, []).append(index)
        for bone, indices in by_bone.items():
            lod.vertex_groups[bone].add(indices, 1.0, "REPLACE")
        lod.parent = arm
        lod.modifiers.new("Armature", "ARMATURE").object = arm

    co, lo, hi = bounds(lods[0])
    for n in nodes:
        if n.get("bone"):
            continue
        p = world[n["name"]].copy()
        if n.get("fit") == "muzzle":
            # On the front face, at the height of whatever is there (the dart ports, the bore).
            front = co[co[:, 1] > hi[1] - 0.004]
            p = Vector((0, float(hi[1]), float(front[:, 2].mean())))
        if n["name"] in spec.get("sockets", {}):
            p = e2b(spec["sockets"][n["name"]])
        e = bpy.data.objects.new(n["name"], None)
        e.empty_display_size = 0.01
        bpy.context.scene.collection.objects.link(e)
        e.parent = arm
        e.parent_type = "BONE"
        e.parent_bone = n["parent"]
        bpy.context.view_layer.update()
        e.matrix_world = Matrix.Translation(p)
        sockets.append({"name": n["name"], "pos": b2e(p)})
        export.append(e)
    export.append(arm)
report["sockets"] = sockets

# ---------------------------------------------------------------- export
bpy.context.view_layer.update()
bpy.ops.object.select_all(action="DESELECT")
for o in export:
    o.select_set(True)
bpy.context.view_layer.objects.active = lods[0]
fbx = os.path.join(out, name + ".fbx")
# The same options Enfusion Blender Tools passes; everything else is the exporter's default.
bpy.ops.export_scene.fbx(filepath=fbx, check_existing=False, use_selection=True,
                         object_types={"MESH", "OTHER", "EMPTY", "ARMATURE"},
                         use_custom_props=True, add_leaf_bones=False, bake_anim=False)
report["fbx"] = {"file": fbx, "bytes": os.path.getsize(fbx), "objects": sorted(o.name for o in export)}

# The folded state is a second model with the same material: its own LODs and box collider.
report["variants"] = []
if folded:
    fname = name + "_Folded"
    fco, flo, fhi = bounds(folded[0])
    fcm = bmesh.new()
    box_into(fcm, flo, fhi)
    fcollider, finfo = collider_object(fcm, col_cfg["prefix"] + fname)
    fexport = folded + [fcollider]
    bpy.ops.object.select_all(action="DESELECT")
    for o in fexport:
        o.select_set(True)
    bpy.context.view_layer.objects.active = folded[0]
    ffbx = os.path.join(out, fname + ".fbx")
    bpy.ops.export_scene.fbx(filepath=ffbx, check_existing=False, use_selection=True,
                             object_types={"MESH", "OTHER", "EMPTY"}, use_custom_props=True,
                             add_leaf_bones=False, bake_anim=False)
    report["variants"].append({
        "name": fname, "fbx": ffbx, "collider": finfo,
        "lods": [{"name": o.name, "tris": tri_count(o), "verts": len(o.data.vertices)} for o in folded],
        "size": [float(fhi[1] - flo[1]), float(fhi[2] - flo[2]), float(fhi[0] - flo[0])],
        "bounds": {"min": b2e(flo), "max": b2e(fhi)},
    })
    for o in folded:
        o.hide_render = True

# ---------------------------------------------------------------- review renders
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "TEXTURE"
scene.display.shading.show_cavity = True
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.32, 0.33, 0.35)
for lod in lods[1:]:
    lod.hide_render = True

markers = []


def marker(p, rgba, radius=0.0035):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=radius, location=p, segments=12, ring_count=8)
    o = bpy.context.view_layer.objects.active
    m = bpy.data.materials.new("marker")
    m.diffuse_color = rgba
    o.data.materials.append(m)
    markers.append(o)


COLOURS = {"eye": (0.1, 0.5, 1, 1), "barrel_muzzle": (1, 0.1, 0.1, 1), "barrel_chamber": (1, 0.6, 0, 1), "slot_magazine": (0.1, 0.9, 0.2, 1)}
for s in sockets:
    marker(e2b(s["pos"]), COLOURS.get(s["name"], (1, 0, 1, 1)))
for b in report["bones"]:
    marker(e2b(b["pos"]), (1, 1, 1, 1), 0.002)
if kind.get("reference"):
    # The vanilla weapon's bounding box, as a thin frame, to judge the grip fit against.
    rb = kind["reference"]["bounds"]
    a, b = e2b(rb["min"]), e2b(rb["max"])
    bpy.ops.mesh.primitive_cube_add(size=1, location=(a + b) / 2)
    frame = bpy.context.view_layer.objects.active
    frame.scale = (abs(b.x - a.x), abs(b.y - a.y), abs(b.z - a.z))
    bpy.ops.object.transform_apply(scale=True)
    w = frame.modifiers.new("w", "WIREFRAME")
    w.thickness = 0.0012
    m = bpy.data.materials.new("frame")
    m.diffuse_color = (1, 0.9, 0.1, 1)
    frame.data.materials.append(m)
    markers.append(frame)

cam_data = bpy.data.cameras.new("cam")
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
co, lo, hi = bounds(lods[0])
centre = Vector(((lo + hi) / 2).tolist())
span = float(max(hi - lo))


def shot(file, direction, ortho=True, res=(1100, 800), transparent=False):
    d = Vector(direction).normalized()
    cam.location = centre + d * span * 3
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    cam_data.type = "ORTHO" if ortho else "PERSP"
    cam_data.ortho_scale = span * 1.5
    cam_data.lens = 85
    if not ortho:
        cam.location = centre + d * span * 4.2
    cam_data.clip_start, cam_data.clip_end = 0.01, 100
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.film_transparent = transparent
    scene.render.filepath = file
    bpy.ops.render.render(write_still=True)


review = os.path.join(out, "review")
shot(os.path.join(review, "side_left.png"), (-1, 0, 0))
shot(os.path.join(review, "side_right.png"), (1, 0, 0))
shot(os.path.join(review, "top.png"), (0, 0, 1))
shot(os.path.join(review, "front.png"), (0, 1, 0))
shot(os.path.join(review, "quarter.png"), (-0.8, 0.9, 0.5), ortho=False)
for o in markers:
    o.hide_render = True
shot(os.path.join(out, "icon.png"), (-0.8, 0.9, 0.5), ortho=False, res=(512, 512), transparent=True)
shot(os.path.join(review, "clean_side.png"), (-1, 0, 0))
# The lowest LOD on its own: a broken silhouette here means the ratio is too low.
lods[0].hide_render = True
lods[-1].hide_render = False
shot(os.path.join(review, "lod_last.png"), (-0.8, 0.9, 0.5), ortho=False)
if folded:
    lods[-1].hide_render = True
    folded[0].hide_render = False
    co, lo, hi = bounds(folded[0])
    centre = Vector(((lo + hi) / 2).tolist())
    span = float(max(hi - lo))
    shot(os.path.join(review, "folded.png"), (-0.8, 0.9, 0.5), ortho=False)

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, name + ".blend"))
with open(os.path.join(out, "report.json"), "w") as f:
    json.dump(report, f, indent=1, default=float)
print("LF_ASSET_CLEAN ok", fbx)
