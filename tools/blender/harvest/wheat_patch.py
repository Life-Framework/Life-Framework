# Writes the wheat patch models (#3290): LF_WheatPatch_01 (a ripe 3 x 3 m patch of crossed polyplanes) and
# LF_WheatStubble_01 (the same footprint after the cut: short straw stalks). Both are cut from the base game's own
# atlas, Assets/Vegetation/_Polyplanes/polyplane_wheat_01_{A,BCR,NTC}.edds (1024 x 512): the three standing clusters
# fill u 0.36..0.995; the stubble is their lower third. No texture of ours.
#
# Run: blender -b --factory-startup -P wheat_patch.py -- <out-dir> <report.json>
# tools/blender/harvest/build.mjs runs this and writes the .meta, .emat and prefab files.
#
# Rules the models keep so patches tile with no seam and nothing floats:
#   - the origin is the middle of the patch on the ground; a plane's foot sits 4 cm below it, so a plane on a slope
#     never shows its edge;
#   - the plane centres sit on a grid whose pitch divides the 3 m cell, so the next cell's first row is one pitch
#     past this cell's last row and the spacing is even across the border;
#   - the planes are alpha-cut cards of ONE mesh object and ONE material per model (a second material draws white
#     in the bake); LODs are `<Name>_LOD0`, `<Name>_LOD1` objects in the same FBX;
#   - no collider: wheat is walked through.
import bpy, sys, os, json, math, random, mathutils

OUT, REPORT = sys.argv[-2], sys.argv[-1]

CELL = 3.0           # metres; LF_WheatFieldsConfig.m_fCellSize
FOOT = -0.04         # a plane's bottom edge, below the ground

# Atlas windows (u0, v0, u1, v1), v up like Blender's UV editor.
STANDING = [(0.360, 0.0, 0.680, 1.0), (0.675, 0.0, 0.995, 1.0)]
# Stubble is the foot of the same standing clusters, cut level at 30% of the atlas height: dense straw stems that all
# end at one height, which is what a combine leaves. (The atlas's own short-stalk strip at the top left is a few thin
# sticks and vanishes at ten metres.)
STUBBLE = [(0.360, 0.0, 0.680, 0.30), (0.675, 0.0, 0.995, 0.30)]


def build_mesh(name, material_name, planes):
    """planes: dicts of x, y (centre), yaw (rad), width, height, lean (m), uv (u0, v0, u1, v1), flip, bend."""
    verts, faces, uvs = [], [], []
    for p in planes:
        c, s = math.cos(p["yaw"]), math.sin(p["yaw"])
        ax = (c, s, 0.0)                 # along the plane
        nx = (-s, c, 0.0)                # across the plane (the lean)
        hw = p["width"] / 2.0
        h = p["height"]
        u0, v0, u1, v1 = p["uv"]
        if p["flip"]:
            u0, u1 = u1, u0
        rows = [(0.0, 0.0, 0.0)]         # (height fraction, lean fraction, along shift)
        if p["bend"]:
            rows.append((0.5, 0.0, 0.0))
        rows.append((1.0, 1.0, p.get("sway", 0.0)))
        base = len(verts)
        for fr, lean, sway in rows:
            z = FOOT + (h - FOOT) * fr if fr > 0 else FOOT
            for side in (-1.0, 1.0):
                off = side * hw + sway * lean
                verts.append((p["x"] + ax[0] * off + nx[0] * p["lean"] * lean,
                              p["y"] + ax[1] * off + nx[1] * p["lean"] * lean, z))
            vv = v0 + (v1 - v0) * (0.0 if fr == 0 else fr)
            uvs.append((u0, vv))
            uvs.append((u1, vv))
        for r in range(len(rows) - 1):
            a = base + r * 2
            faces.append((a, a + 1, a + 3, a + 2))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    uv = mesh.uv_layers.new(name="UVMap")
    n = 0
    for poly in mesh.polygons:
        for li, vi in zip(poly.loop_indices, poly.vertices):
            uv.data[li].uv = uvs[vi]
            n += 1
    # Every normal points up: a card lit from its own face goes dark on the far side, and a field is lit like the
    # ground under it. The material's normal map still shades the stalks.
    mesh.normals_split_custom_set([(0.0, 0.0, 1.0)] * len(mesh.loops))
    mat = bpy.data.materials.get(material_name) or bpy.data.materials.new(material_name)
    mesh.materials.append(mat)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def grid_planes(rng, nx, per, width, height, h_jitter, lean, uv_windows, bend, sway, jitter=0.18):
    """nx x nx cells of the 3 m square, `per` planes in each at even yaw steps from a random start."""
    pitch = CELL / nx
    out = []
    for ix in range(nx):
        for iy in range(nx):
            cx = (ix + 0.5) * pitch - CELL / 2.0 + rng.uniform(-jitter, jitter) * pitch
            cy = (iy + 0.5) * pitch - CELL / 2.0 + rng.uniform(-jitter, jitter) * pitch
            yaw0 = rng.uniform(0.0, math.pi)
            for k in range(per):
                win = rng.choice(uv_windows)
                aspect = (win[2] - win[0]) * 1024.0 / ((win[3] - win[1]) * 512.0)
                hh = height * rng.uniform(1.0 - h_jitter, 1.0 + h_jitter)
                out.append({
                    "x": cx, "y": cy, "yaw": yaw0 + k * math.pi / per,
                    "width": hh * aspect, "height": hh,
                    "lean": rng.uniform(-lean, lean), "sway": rng.uniform(-sway, sway),
                    "uv": win, "flip": rng.random() < 0.5, "bend": bend,
                })
    return out


def tris(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


def bounds(obj):
    xs = [v.co for v in obj.data.vertices]
    return ([min(c[i] for c in xs) for i in range(3)], [max(c[i] for c in xs) for i in range(3)])


def export(model, lods, material):
    for o in bpy.data.objects:
        o.select_set(False)
    for o in lods:
        o.select_set(True)
    bpy.context.view_layer.objects.active = lods[0]
    dst = os.path.join(OUT, model + ".fbx")
    # The options Enfusion Blender Tools passes; everything else is the exporter's default.
    bpy.ops.export_scene.fbx(filepath=dst, check_existing=False, use_selection=True,
                             object_types={"MESH", "OTHER", "EMPTY"}, use_custom_props=True,
                             add_leaf_bones=False, bake_anim=False)
    info = {"fbx": dst, "material": material, "lods": []}
    for o in lods:
        lo, hi = bounds(o)
        info["lods"].append({"name": o.name, "tris": tris(o), "verts": len(o.data.vertices),
                             "materials": [s.material.name for s in o.material_slots],
                             "min": lo, "max": hi})
    return info


def main():
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    report = {}

    # Ripe wheat: 5 x 5 positions, a star of three planes at each, every plane bent at half height:
    # 75 planes x 4 triangles = 300. Wheat in 1989 stood about a metre.
    rng = random.Random(3290)
    lod0 = build_mesh("LF_WheatPatch_01_LOD0", "LF_WheatPatch",
                      grid_planes(rng, 5, 3, 0, 1.0, 0.10, 0.10, STANDING, True, 0.05))
    # LOD1: one plane per position of a 6 x 6 grid, no bend: 36 planes x 2 = 72.
    lod1 = build_mesh("LF_WheatPatch_01_LOD1", "LF_WheatPatch",
                      grid_planes(rng, 6, 1, 0, 1.0, 0.10, 0.0, STANDING, False, 0.0))
    report["LF_WheatPatch_01"] = export("LF_WheatPatch_01", [lod0, lod1], "LF_WheatPatch")

    # Stubble: 6 x 6 positions, three planes each, 30 cm of cut straw (a plane is 0.69 m wide at that height):
    # 108 planes x 2 triangles = 216; LOD1 36 x 2 = 72.
    rng = random.Random(32901)
    s0 = build_mesh("LF_WheatStubble_01_LOD0", "LF_WheatStubble",
                    grid_planes(rng, 6, 3, 0, 0.30, 0.1, 0.0, STUBBLE, False, 0.0))
    s1 = build_mesh("LF_WheatStubble_01_LOD1", "LF_WheatStubble",
                    grid_planes(rng, 6, 1, 0, 0.32, 0.05, 0.0, STUBBLE, False, 0.0))
    report["LF_WheatStubble_01"] = export("LF_WheatStubble_01", [s0, s1], "LF_WheatStubble")

    with open(REPORT, "w") as f:
        json.dump(report, f, indent=1)
    for k, v in report.items():
        print("WHEAT", k, [(l["name"], l["tris"]) for l in v["lods"]])


main()
