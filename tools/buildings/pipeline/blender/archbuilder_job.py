# Headless driver for the ArchBuilder Blender add-on (tools\cli building).
#
#   blender --background [building.blend] --python archbuilder_job.py -- <job.json>
#
# job.json: {"mode": "generate"|"review"|"polish"|"export"|"materials", "spec": {...}, "out": "<dir>", ...}. The script
# writes <out>/<mode>.json and exits 0, or exits 3 (--python-exit-code) on an exception, or 4 when
# ArchBuilder is not installed, enabled or licensed (the stage then skips, it does not fail).
#
# It uses only the add-on's public surface: its operators (ab.generate_building, ab.landmark,
# ab.rf_export), its property groups (scene.ab_gen, object.ab_bld, scene.ab_rf_*) and license.is_full,
# plus a read-only look at the report its reforger.export.run returns (export() below).
# None of the add-on's own code is copied here. Findings: docs/architecture/building-pipeline-spike.md.
import bpy
import json
import math
import os
import sys
import time

from mathutils import Vector

job = json.load(open(sys.argv[sys.argv.index("--") + 1], encoding="utf-8"))
spec = job["spec"]
out = job["out"]
os.makedirs(out, exist_ok=True)
mode = job["mode"]


def done(report, code=0):
    with open(os.path.join(out, mode + ".json"), "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=1)
    sys.stdout.flush()
    os._exit(code)  # skip Blender's exit handlers: the extension's timers have nothing to flush


def archbuilder():
    pkg = next((m for m in bpy.context.preferences.addons.keys() if m.split(".")[-1] == "arch_builder"), None)
    if pkg is None or not hasattr(bpy.types.Scene, "ab_gen"):
        done({"skip": "ArchBuilder is not enabled in this Blender's preferences"}, 4)
    lic = sys.modules.get(pkg + ".license")
    if lic is None or not lic.is_full(bpy.context):
        done({"skip": "ArchBuilder is installed but not licensed (its operators are locked)"}, 4)
    mod = sys.modules.get(pkg)
    return pkg, job.get("version") or getattr(mod, "bl_info", {}).get("version")


def anchor():
    for o in bpy.data.objects:
        if o.get("ab_anchor"):
            return o
    raise RuntimeError("the .blend holds no ArchBuilder building (no object with ab_anchor)")


def select_only(obj):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


# ------------------------------------------------------------------------------ generate
def generate():
    pkg, version = archbuilder()
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    sc = bpy.context.scene
    g = sc.ab_gen
    ab = dict(spec["archbuilder"])
    landmark = ab.pop("landmark", None)
    unknown = []
    for k, v in ab.items():
        if not hasattr(g, k):
            unknown.append(k)
            continue
        setattr(g, k, v)
    if unknown:
        raise RuntimeError("scene.ab_gen has no setting(s) %s" % ", ".join(unknown))
    # The game's character only fits through vanilla-sized doors (docs/building-pipeline/README.md).
    g.proportions = "REFORGER"
    g.vanilla_openings = True
    sc.cursor.location = (0, 0, 0)
    t = time.time()
    if landmark:
        g.landmark = landmark
        res = bpy.ops.ab.landmark()
    else:
        res = bpy.ops.ab.generate_building()
    if "FINISHED" not in res:
        raise RuntimeError("ArchBuilder returned %s" % (res,))
    a = anchor()
    secs = time.time() - t
    blend = os.path.join(out, "building.blend")
    bpy.ops.wm.save_as_mainfile(filepath=blend, compress=True)
    resolved = json.loads(a.get("ab_resolved") or "{}")
    done({"anchor": a.name, "secs": round(secs, 2), "blend": blend, "version": str(version),
          "settings": {k: getattr(a.ab_bld, k) for k in ("archetype", "region", "footprint", "storeys", "seed",
                                                          "width", "depth", "proportions", "vanilla_openings")},
          "resolved": {k: resolved.get(k) for k in ("wall", "roof_style", "roof_mat", "win_style", "storey_h", "base_h")}})


# ------------------------------------------------------------------------------ review
def tris_of(obj, depsgraph):
    ev = obj.evaluated_get(depsgraph)
    me = ev.to_mesh()
    n = sum(len(p.vertices) - 2 for p in me.polygons)
    ev.to_mesh_clear()
    return n


def review():
    archbuilder()
    a = anchor()
    dg = bpy.context.evaluated_depsgraph_get()
    parts = {}
    lo, hi = Vector((1e9, 1e9, 1e9)), Vector((-1e9, -1e9, -1e9))
    wlo, whi = Vector((1e9, 1e9, 1e9)), Vector((-1e9, -1e9, -1e9))
    for o in a.children_recursive:
        if o.type != "MESH":
            continue
        parts[o.get("ab_part") or o.name] = tris_of(o, dg)
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo, hi = Vector(map(min, lo, w)), Vector(map(max, hi, w))
            if o.get("ab_is_wall"):
                wlo, whi = Vector(map(min, wlo, w)), Vector(map(max, whi, w))
    if not parts:
        raise RuntimeError("the building has no meshes")
    if whi.x < wlo.x:
        wlo, whi = lo, hi
    stats = {
        "tris": sum(parts.values()), "parts": parts, "storeys": a.ab_bld.storeys,
        "footprint_m": [round(whi.x - wlo.x, 2), round(whi.y - wlo.y, 2)],
        "bounds_m": [round(hi.x - lo.x, 2), round(hi.y - lo.y, 2), round(hi.z - lo.z, 2)],
        "height_m": round(hi.z - max(lo.z, 0.0), 2),
    }
    # Renders: the workbench engine (no GPU path tracing, works headless) with material colours.
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sh = sc.display.shading
    sh.light, sh.color_type = "STUDIO", "MATERIAL"
    sh.show_cavity, sh.cavity_type = True, "WORLD"
    sh.show_shadows = True
    sc.render.film_transparent = False
    sc.world = sc.world or bpy.data.worlds.new("World")
    sc.world.color = (0.62, 0.66, 0.70)
    ground = bpy.data.meshes.new("ground")
    s = max(hi.x - lo.x, hi.y - lo.y) * 1.6
    cx, cy = (lo.x + hi.x) / 2, (lo.y + hi.y) / 2
    ground.from_pydata([(cx - s, cy - s, 0), (cx + s, cy - s, 0), (cx + s, cy + s, 0), (cx - s, cy + s, 0)], [], [(0, 1, 2, 3)])
    gobj = bpy.data.objects.new("lf_review_ground", ground)
    gm = bpy.data.materials.new("lf_review_ground")
    gm.diffuse_color = (0.33, 0.38, 0.27, 1)
    ground.materials.append(gm)
    sc.collection.objects.link(gobj)
    cam_data = bpy.data.cameras.new("lf_review_cam")
    cam = bpy.data.objects.new("lf_review_cam", cam_data)
    sc.collection.objects.link(cam)
    sc.camera = cam
    centre = (lo + hi) / 2
    span = max(hi - lo)
    review_dir = os.path.join(out, mode)
    os.makedirs(review_dir, exist_ok=True)
    shots = []

    def shot(name, direction, ortho=True):
        d = Vector(direction).normalized()
        cam_data.type = "ORTHO" if ortho else "PERSP"
        # ortho_scale covers the image width; 1280x900 needs it wider for the whole height to fit.
        cam_data.ortho_scale = span * 1.15 * 1280 / 900
        cam_data.lens = 35
        cam.location = centre + d * span * (3.0 if ortho else 1.9)
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        cam_data.clip_start, cam_data.clip_end = 0.1, span * 10
        sc.render.resolution_x, sc.render.resolution_y = 1280, 900
        f = os.path.join(review_dir, name + ".png")
        sc.render.filepath = f
        bpy.ops.render.render(write_still=True)
        shots.append(f)

    # ArchBuilder builds with the street front on -Y (Blender), which is the prefab's -Z after export.
    if mode == "review":
        shot("front", (0, -1, 0.0001))
        shot("side", (1, 0, 0.0001))
        shot("top", (0, 0, 1))
        shot("iso", (-0.9, -1.2, 0.8), ortho=False)
        done({"stats": stats, "shots": shots})
    # polish: what a player sees (docs/building-pipeline/polish.md): each side and the street corner
    # from eye height, and the sales hall from inside the entrance facing the back wall.
    # the plan: a section 2.2 m above the floor seen from above (the near clip cuts roof and ceiling)
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = span * 1.15 * 1280 / 900
    cam.location = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, hi.z + 5))
    cam.rotation_euler = (0, 0, 0)
    cam_data.clip_start, cam_data.clip_end = hi.z + 5 - 2.2, hi.z + 10
    sc.render.resolution_x, sc.render.resolution_y = 1280, 900
    sc.render.filepath = os.path.join(review_dir, "plan.png")
    bpy.ops.render.render(write_still=True)
    shots.append(sc.render.filepath)
    cam_data.clip_start, cam_data.clip_end = 0.1, span * 10
    sc.render.resolution_x, sc.render.resolution_y = 1280, 720
    cam_data.type, cam_data.lens = "PERSP", 24

    def eye(name, at, look):
        cam.location = Vector(at)
        cam.rotation_euler = (Vector(look) - Vector(at)).to_track_quat("-Z", "Y").to_euler()
        f = os.path.join(review_dir, name + ".png")
        sc.render.filepath = f
        bpy.ops.render.render(write_still=True)
        shots.append(f)
    c, w, d = (wlo + whi) / 2, whi.x - wlo.x, whi.y - wlo.y
    for name, (dx, dy) in (("front", (0, -1)), ("back", (0, 1)), ("left", (-1, 0)), ("right", (1, 0))):
        far = (d if dx == 0 else w) / 2 + max(w, d) * 0.9
        eye(name, (c.x + dx * far, c.y + dy * far, 1.7), (c.x, c.y, 2.2))
    eye("street", (c.x - w * 0.55, wlo.y - 9.0, 1.7), (c.x, wlo.y, 2.6))
    sh.show_shadows = False                     # the roof would shade the whole hall
    gobj.hide_render = True
    eye("inside", (c.x, wlo.y + 1.2, 1.7), (c.x, whi.y, 1.4))
    done({"stats": stats, "shots": shots,
          "settings": {k: getattr(a.ab_bld, k) for k in spec["archbuilder"] if hasattr(a.ab_bld, k)}})


# ------------------------------------------------------------------------------ export
def export():
    archbuilder()
    a = anchor()
    sc = bpy.context.scene
    e = spec.get("export", {})
    sc.ab_rf_addon = job["staging"]
    sc.ab_rf_name = spec["name"]
    sc.ab_rf_wb_import = False             # the pipeline compiles in its own hidden Workbench
    sc.ab_rf_light_system = False          # it writes an Enforce script with ArchBuilder's class names
    sc.ab_rf_furniture = bool(e.get("furniture", True))
    sc.ab_rf_lamps = bool(e.get("lamps", True))
    sc.ab_rf_vanilla_props = bool(e.get("vanilla_props", True))
    sc.ab_rf_lights_on = bool(e.get("lights_on", False))
    sc.ab_rf_lods = bool(e.get("lods", True))
    sc.ab_rf_bsp = bool(e.get("bsp", False))
    sc.ab_rf_probes = e.get("probes", "ONE")
    sc.ab_rf_invert = bool(e.get("invert_swing", False))
    select_only(a)
    # The operator reports only a one-line summary; the export's own report (warnings, vanilla
    # props placed per kind, furniture records it found no place or prefab for) is the return value
    # of the add-on's reforger.export.run, which the operator calls by module attribute. Keep a copy
    # of it so the pipeline can fail on anything the export skipped instead of shipping a bare shell.
    pkg = next(m for m in bpy.context.preferences.addons.keys() if m.split(".")[-1] == "arch_builder")
    import importlib
    try:
        exp = importlib.import_module(pkg + ".reforger.export")  # the operator imports it lazily; same module object
    except ImportError:
        exp = None
    if exp is None or not hasattr(exp, "run"):
        raise RuntimeError("%s.reforger.export.run not found: this ArchBuilder version is not the one the "
                           "pipeline knows (docs/building-pipeline/README.md)" % pkg)
    reports = []
    inner = exp.run

    def keep(*args, **kw):
        r = inner(*args, **kw)
        reports.append(r)
        return r
    exp.run = keep
    t = time.time()
    print("LF_BUILDING export start")
    try:
        # a hand-polished .blend ships what was edited in its scene (docs/building-pipeline/polish.md)
        res = bpy.ops.ab.rf_export_scene() if job.get("scene") else bpy.ops.ab.rf_export()
    finally:
        exp.run = inner
    print("LF_BUILDING export end")
    if "FINISHED" not in res:
        raise RuntimeError("ab.rf_export returned %s" % (res,))
    r = reports[-1] if reports else None
    keys = ("warnings", "props", "props_vanilla", "props_mesh", "probes", "vanilla_windows", "vanilla_doors",
            "vanilla_glass", "doors", "materials", "colliders", "lods", "faces", "prefab", "base_prefab")
    done({"secs": round(time.time() - t, 2), "staging": job["staging"],
          "report": {k: r.get(k) for k in keys} if r else None})


# ------------------------------------------------------------------------------ materials
# What the game material needs to look like Blender: for every material on the building, the colour
# ArchBuilder shows for it in Blender (its viewport colour) and how much of its area faces the
# outside; and the linear mean albedo of each base-game texture handed in as a .dds
# (job["textures"]: {name: path}). tools/buildings/pipeline/materials.mjs turns them into Color.
# Outside is measured, not read from ArchBuilder's part names: one "walls" part holds both faces of
# an outer wall, the beige render and the interior plaster. A face is outside when a ray from it
# along its normal leaves the building.
def materials():
    archbuilder()
    a = anchor()
    dg = bpy.context.evaluated_depsgraph_get()
    sc = bpy.context.scene
    mats = {}
    for o in a.children_recursive:
        if o.type != "MESH" or o.get("ab_cutter") or o.hide_render:
            continue
        ev = o.evaluated_get(dg)
        me = ev.to_mesh()
        mw = o.matrix_world
        nm = mw.to_3x3().inverted().transposed()
        for poly in me.polygons:
            if poly.material_index >= len(o.material_slots):
                continue
            m = o.material_slots[poly.material_index].material
            if m is None or not m.name.startswith("AB_"):
                continue
            e = mats.setdefault(m.name, {"colour": [round(c, 4) for c in m.diffuse_color[:3]], "area": 0.0, "outside": 0.0})
            n = (nm @ poly.normal).normalized()
            c = mw @ poly.center
            area = poly.area
            e["area"] += area
            # the centre and points part-way to each corner: one big face (a flat roof) can have
            # its centre under a rooftop unit and still be mostly open sky
            pts = [c] + [c + (mw @ me.vertices[v].co - c) * f for v in poly.vertices for f in (0.4, 0.8)]
            free = sum(1 for q in pts if not sc.ray_cast(dg, q + n * 0.03, n, distance=200.0)[0])
            e["outside"] += area * free / len(pts)
        ev.to_mesh_clear()
    for e in mats.values():
        e["outside_share"] = round(e["outside"] / e["area"], 3) if e["area"] else 0.0
        e["area"] = round(e["area"], 2)
        e["outside"] = round(e["outside"], 2)

    def lin(c):
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    means = {}
    for name, path in job.get("textures", {}).items():
        im = bpy.data.images.load(path, check_existing=False)
        im.colorspace_settings.name = "sRGB"
        px = im.pixels[:]
        n = len(px) // 4
        means[name] = [round(sum(lin(px[i * 4 + c]) for i in range(n)) / n, 4) for c in range(3)]
        bpy.data.images.remove(im)
    done({"materials": mats, "means": means})


try:
    {"generate": generate, "review": review, "polish": review, "export": export, "materials": materials}[mode]()
except SystemExit:
    raise
except Exception:
    import traceback
    traceback.print_exc()
    sys.stdout.flush()
    os._exit(3)
