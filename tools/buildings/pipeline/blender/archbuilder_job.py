# Headless driver for the ArchBuilder Blender add-on (tools\cli building).
#
#   blender --background [building.blend] --python archbuilder_job.py -- <job.json>
#
# job.json: {"mode": "generate"|"review"|"export", "spec": {...}, "out": "<dir>", ...}. The script
# writes <out>/<mode>.json and exits 0, or exits 3 (--python-exit-code) on an exception, or 4 when
# ArchBuilder is not installed, enabled or licensed (the stage then skips, it does not fail).
#
# It uses only the add-on's public surface: its operators (ab.generate_building, ab.landmark,
# ab.rf_export), its property groups (scene.ab_gen, object.ab_bld, scene.ab_rf_*) and license.is_full.
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
    review_dir = os.path.join(out, "review")
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
    shot("front", (0, -1, 0.0001))
    shot("side", (1, 0, 0.0001))
    shot("top", (0, 0, 1))
    shot("iso", (-0.9, -1.2, 0.8), ortho=False)
    done({"stats": stats, "shots": shots})


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
    t = time.time()
    print("LF_BUILDING export start")
    res = bpy.ops.ab.rf_export()
    print("LF_BUILDING export end")
    if "FINISHED" not in res:
        raise RuntimeError("ab.rf_export returned %s" % (res,))
    done({"secs": round(time.time() - t, 2), "staging": job["staging"]})


try:
    {"generate": generate, "review": review, "export": export}[mode]()
except SystemExit:
    raise
except Exception:
    import traceback
    traceback.print_exc()
    sys.stdout.flush()
    os._exit(3)
