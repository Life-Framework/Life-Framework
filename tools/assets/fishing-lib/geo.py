# Shared by the fishing items' model.py (rod, bait tin, fish): small mesh builders, the join/UV step and a
# palette bake. Run inside Blender (blender --background --python <item>/model.py -- <job.json>).
# Vertex colour "part" carries a material id in 0..1; PALETTE rows are (id, colour, roughness, metallic).
import bpy, bmesh, json, math, os, sys
from mathutils import Vector

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pipeline", "blender"))
import texbake

parts = []   # (bmesh, part id)


def job():
    return json.load(open(sys.argv[sys.argv.index("--") + 1]))


def piece(pid):
    bm = bmesh.new()
    parts.append((bm, pid))
    return bm


def block(bm, x0, x1, y0, y1, z0, z1, bevel=0.001, seg=2):
    t = bmesh.ops.create_cube(bm, size=1.0)
    for v in t["verts"]:
        v.co = Vector((x0 if v.co.x < 0 else x1, y0 if v.co.y < 0 else y1, z0 if v.co.z < 0 else z1))
    keep = set(t["verts"])
    bm.edges.ensure_lookup_table()
    edges = [e for e in bm.edges if e.verts[0] in keep and e.verts[1] in keep]
    if bevel > 0:
        bmesh.ops.bevel(bm, geom=edges, offset=bevel, segments=seg, profile=0.6, affect="EDGES", clamp_overlap=True)


def lathe_y(bm, x, z, profile, seg=14):
    """A turned part along Y: profile = [(y, radius)]; a radius of 0 closes that end."""
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
    live = [r for r in rings if r is not None]
    bm.faces.new(list(reversed(live[0])))
    bm.faces.new(live[-1])


def lathe_z(bm, x, y, profile, seg=16):
    """A turned part along Z (a tin, a reel): profile = [(z, radius)]."""
    rings = []
    for z, r in profile:
        rings.append([bm.verts.new((x + r * math.cos(2 * math.pi * s / seg), y + r * math.sin(2 * math.pi * s / seg), z)) for s in range(seg)])
    for a in range(len(profile) - 1):
        for s in range(seg):
            bm.faces.new((rings[a][s], rings[a][(s + 1) % seg], rings[a + 1][(s + 1) % seg], rings[a + 1][s]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])


def prism(bm, pts, axis, c0, c1):
    """A thin extruded polygon. axis "x": pts are (y, z) and it spans x c0..c1; axis "z": pts are (x, y), spans z c0..c1."""
    ring = []
    for c in (c0, c1):
        if axis == "x":
            ring.append([bm.verts.new((c, p[0], p[1])) for p in pts])
        else:
            ring.append([bm.verts.new((p[0], p[1], c)) for p in pts])
    n = len(pts)
    for i in range(n):
        bm.faces.new((ring[0][i], ring[0][(i + 1) % n], ring[1][(i + 1) % n], ring[1][i]))
    bm.faces.new(list(reversed(ring[0])))
    bm.faces.new(ring[1])


def loft_y(bm, sections, seg=14, pid_of=None):
    """A body along Y: sections = [(y, half_width_x, half_height_z, z_centre)], first and last close to a point.
    pid_of(sin_angle) gives a per-vertex material id (a fish's dark back and pale belly)."""
    layer = bm.verts.layers.float.new("pid") if pid_of else None
    rings = []
    for y, a, b, zc in sections:
        ring = []
        for s in range(seg):
            ang = 2 * math.pi * s / seg
            v = bm.verts.new((a * math.cos(ang), y, zc + b * math.sin(ang)))
            if layer:
                v[layer] = pid_of(math.sin(ang))
            ring.append(v)
        rings.append(ring)
    for i in range(len(rings) - 1):
        for s in range(seg):
            bm.faces.new((rings[i][s], rings[i][(s + 1) % seg], rings[i + 1][(s + 1) % seg], rings[i + 1][s]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])


def sweep(bm, path, r, seg=8, radii=None):
    """A round tube along a 3D path, capped at both ends. radii: one radius per path point (a taper)."""
    rings = []
    up = Vector((1, 0, 0))
    for i, p in enumerate(path):
        t = (path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]).normalized()
        ref = up if abs(t.dot(up)) < 0.95 else Vector((0, 0, 1))
        side = t.cross(ref).normalized()
        up2 = side.cross(t).normalized()
        rr = radii[i] if radii else r
        rings.append([bm.verts.new(p + rr * (math.cos(2 * math.pi * s / seg) * side + math.sin(2 * math.pi * s / seg) * up2)) for s in range(seg)])
    for i in range(len(path) - 1):
        for s in range(seg):
            bm.faces.new((rings[i][s], rings[i + 1][s], rings[i + 1][(s + 1) % seg], rings[i][(s + 1) % seg]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])


def finish(name, j, palette, review_distance, center=True, tile=0.3):
    """Join, centre, UV, bake the palette, export model.glb and a review picture."""
    spec, raw, px = j["spec"], j["raw"], j["px"]
    os.makedirs(raw, exist_ok=True)
    verts, faces, cols = [], [], []
    for bm, pid in parts:
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        idx = {}
        vp = bm.verts.layers.float.get("pid")
        for v in bm.verts:
            idx[v] = len(verts)
            verts.append(tuple(v.co))
            cols.append(v[vp] if vp else pid)
        for f in bm.faces:
            faces.append([idx[v] for v in f.verts])
    lo = Vector([min(v[k] for v in verts) for k in range(3)])
    hi = Vector([max(v[k] for v in verts) for k in range(3)])
    mid = (lo + hi) / 2
    if not center:
        mid = Vector((mid[0], mid[1], lo[2]))
    verts = [tuple(Vector(v) - mid) for v in verts]
    print("LF extent", [round(float(hi[k] - lo[k]), 4) for k in range(3)])
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    attr = me.color_attributes.new("part", "FLOAT_COLOR", "POINT")
    for i, c in enumerate(cols):
        attr.data[i].color = (c, c, c, 1.0)
    obj = bpy.data.objects.new(name, me)
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

    sets = texbake.Sets(j["src"])
    texbake.cycles(16)
    g = texbake.Graph(name + "_set", sets, tile=tile)
    vc = g.node("ShaderNodeVertexColor", layer_name="part")
    sep = g.node("ShaderNodeSeparateColor")
    g.link(vc.outputs["Color"], sep.inputs["Color"])
    pid = sep.outputs[0]
    wear = g.pointiness(0.5, 0.64)
    ao = g.ao(0.004)
    grain = g.noise(300, detail=4)
    pits = g.noise(80, detail=5, w=1.0)

    def band(p):
        return g.math("MULTIPLY", g.ramp(pid, p - 0.07, p - 0.04), g.math("SUBTRACT", 1.0, g.ramp(pid, p + 0.04, p + 0.07)))

    first = palette[0]
    base, rough, metal = first[1], first[2], first[3]
    for p, colour, r, m in palette[1:]:
        mask = band(p)
        base = g.mix(base, colour, mask)
        rough = g.mix(rough, r, mask, kind="FLOAT")
        metal = g.mix(metal, m, mask, kind="FLOAT")
    # Wear: edges rubbed paler, folds dark, a little mottling.
    base = g.mix(base, (0.35, 0.34, 0.32), g.math("MULTIPLY", wear, 0.18))
    base = g.mix(base, (0.02, 0.02, 0.02), g.math("MULTIPLY", g.ramp(g.math("SUBTRACT", 1.0, ao), 0.3, 0.7), 0.5))
    rough = g.math("ADD", rough, g.math("MULTIPLY", pits, 0.15), clamp=True)
    recipe = {"base": base, "rough": rough, "metal": metal,
              "height": g.math("MULTIPLY", grain, 0.3), "bump_strength": 0.5, "bump_distance": 0.0004,
              "ao": g.math("ADD", 0.35, g.math("MULTIPLY", ao, 0.65))}
    files = texbake.bake(obj, g, recipe, px, raw)
    obj.data.materials.clear()
    obj.data.materials.append(texbake.preview_material(name, files))
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(raw, "model.glb"), use_selection=True, export_format="GLB")
    obj = texbake.fresh_scene_from(os.path.join(raw, "model.glb"))
    texbake.review_scene(Vector((0, 0, 0)), review_distance)
    texbake.render(os.path.join(raw, "review.png"))
    print("LF_ASSET_MODEL ok")
