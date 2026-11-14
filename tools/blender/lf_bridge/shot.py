"""`shot`: render a picture of the scene to a PNG.

Live (a Blender with a window): the active 3D viewport as the user has it (no `view`), or a preset
view, through the viewport renderer (bpy.ops.render.opengl). Headless (`blender --background`, no GL
context): the Workbench engine renders the same presets. Either way the user's scene is put back as
it was: camera, render settings, visibility and the temporary camera.

Requests: {"op": "shot", "out": "C:/x.png", "view": "front|back|left|right|top|bottom|iso" (omit for
the viewport), "camera": "<object name>", "size": [w, h], "target": ["<object name>", ...]}.
`target` shows only those objects (and frames them); without it, every visible object is framed.
"""
import math
import os

import bpy
from mathutils import Vector

# Blender's own convention: "front" looks along +Y from -Y; "right" from +X.
VIEWS = {
    "front": (0, -1, 0),
    "back": (0, 1, 0),
    "right": (1, 0, 0),
    "left": (-1, 0, 0),
    "top": (0, 0, 1),
    "bottom": (0, 0, -1),
    "iso": (1, -1, 0.8),
}
DEFAULT_SIZE = (1280, 720)


def _bounds(objs):
    lo = Vector((1e18, 1e18, 1e18))
    hi = Vector((-1e18, -1e18, -1e18))
    seen = False
    for o in objs:
        if o.type not in {"MESH", "CURVE", "SURFACE", "META", "FONT", "ARMATURE", "LATTICE"} or not getattr(o, "bound_box", None):
            continue
        for c in o.bound_box:
            p = o.matrix_world @ Vector(c)
            lo = Vector((min(lo.x, p.x), min(lo.y, p.y), min(lo.z, p.z)))
            hi = Vector((max(hi.x, p.x), max(hi.y, p.y), max(hi.z, p.z)))
            seen = True
    return (lo, hi) if seen else None


def _find_view3d():
    wm = bpy.context.window_manager
    for window in wm.windows:
        for area in window.screen.areas:
            if area.type == "VIEW_3D":
                region = next((r for r in area.regions if r.type == "WINDOW"), None)
                if region:
                    return window, area, region
    return None


def _make_camera(scene, view, framed, aspect):
    """`aspect` is height / width of the picture; the tighter axis decides how far back the camera goes."""
    lo, hi = framed
    centre = (lo + hi) / 2
    dims = hi - lo
    radius = max(dims.length / 2, 0.001)
    d = Vector(VIEWS[view]).normalized()
    data = bpy.data.cameras.new("lf_bridge_cam")
    cam = bpy.data.objects.new("lf_bridge_cam", data)
    scene.collection.objects.link(cam)
    if view == "iso":
        data.type = "PERSP"
        data.lens = 50
        data.sensor_width = 36
        dist = radius * 1.05 / math.sin(math.atan(18 * min(1.0, aspect) / 50))
    else:
        data.type = "ORTHO"
        data.ortho_scale = max(dims.length, 0.001) * 0.95 / min(1.0, aspect)
        dist = radius * 3 + 1
    data.clip_start = max(radius * 0.01, 0.0005)
    data.clip_end = dist + radius * 4
    cam.location = centre + d * dist
    if view == "top":
        cam.rotation_euler = (0, 0, 0)
    elif view == "bottom":
        cam.rotation_euler = (math.pi, 0, 0)
    else:
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    return cam


def run(req):
    out = req.get("out")
    if not isinstance(out, str) or not out:
        return {"ok": False, "error": "shot needs 'out', the PNG path to write"}
    out = os.path.abspath(out)
    if not out.lower().endswith(".png"):
        out += ".png"
    os.makedirs(os.path.dirname(out), exist_ok=True)
    size = req.get("size") or DEFAULT_SIZE
    width, height = int(size[0]), int(size[1])
    view, cam_name, targets = req.get("view"), req.get("camera"), req.get("target")
    if view is not None and view != "viewport" and view not in VIEWS:
        return {"ok": False, "error": "view must be one of %s, or omitted for the active viewport" % ", ".join(sorted(VIEWS))}

    scene = bpy.context.scene
    background = bool(bpy.app.background)
    window_area = None if background else _find_view3d()

    if targets:
        wanted = [bpy.data.objects.get(n) for n in targets]
        missing = [n for n, o in zip(targets, wanted) if o is None]
        if missing:
            return {"ok": False, "error": "no such object: %s" % ", ".join(missing)}
    else:
        wanted = [o for o in scene.objects if o.visible_get() or background]
    framed = _bounds(wanted)

    use_viewport = view in (None, "viewport") and not cam_name and not background and window_area is not None and not targets
    if view in (None, "viewport") and not cam_name and not use_viewport:
        view = "iso"  # nothing to read a viewport from: a preset instead
    camera_obj = None
    if cam_name:
        camera_obj = bpy.data.objects.get(cam_name)
        if camera_obj is None or camera_obj.type != "CAMERA":
            return {"ok": False, "error": "no camera object named %r" % cam_name}
    elif not use_viewport:
        if framed is None:
            return {"ok": False, "error": "nothing to frame: the scene holds no visible geometry"}

    r, shading = scene.render, scene.display.shading
    saved = {
        "camera": scene.camera, "engine": r.engine, "path": r.filepath, "rx": r.resolution_x, "ry": r.resolution_y,
        "pct": r.resolution_percentage, "fmt": r.image_settings.file_format, "alpha": r.film_transparent,
        "light": shading.light, "color": shading.color_type,
    }
    hidden = {}
    temp_cam = None
    try:
        r.resolution_x, r.resolution_y, r.resolution_percentage = width, height, 100
        r.image_settings.file_format = "PNG"
        r.film_transparent = False
        r.filepath = out
        shading.light, shading.color_type = "STUDIO", "TEXTURE"
        if targets:
            keep = {o.name for o in wanted}
            for o in scene.objects:
                hidden[o.name] = (o.hide_viewport, o.hide_render, o.hide_get())
                show = o.name in keep
                o.hide_viewport = not show
                o.hide_render = not show
                o.hide_set(not show)
        if camera_obj is not None:
            scene.camera = camera_obj
        elif not use_viewport:
            temp_cam = _make_camera(scene, view, framed, height / width)
            scene.camera = temp_cam
        bpy.context.view_layer.update()
        if background:
            r.engine = "BLENDER_WORKBENCH"
            bpy.ops.render.render(write_still=True)
        elif use_viewport:
            window, area, region = window_area
            with bpy.context.temp_override(window=window, area=area, region=region, screen=window.screen):
                bpy.ops.render.opengl(write_still=True, view_context=True)
        else:
            if window_area:
                window, area, region = window_area
                with bpy.context.temp_override(window=window, area=area, region=region, screen=window.screen):
                    bpy.ops.render.opengl(write_still=True, view_context=False)
            else:
                bpy.ops.render.opengl(write_still=True, view_context=False)
    finally:
        for name, (hv, hr, hs) in hidden.items():
            o = bpy.data.objects.get(name)
            if o is not None:
                o.hide_viewport, o.hide_render = hv, hr
                o.hide_set(hs)
        scene.camera = saved["camera"]
        if temp_cam is not None:
            data = temp_cam.data
            bpy.data.objects.remove(temp_cam)
            bpy.data.cameras.remove(data)
        r.engine, r.filepath, r.resolution_x, r.resolution_y = saved["engine"], saved["path"], saved["rx"], saved["ry"]
        r.resolution_percentage, r.image_settings.file_format, r.film_transparent = saved["pct"], saved["fmt"], saved["alpha"]
        shading.light, shading.color_type = saved["light"], saved["color"]

    if not os.path.isfile(out):
        return {"ok": False, "error": "the render wrote no file at %s" % out}
    label = "camera %s" % cam_name if cam_name else ("viewport" if use_viewport else view)
    return {"ok": True, "out": out, "bytes": os.path.getsize(out), "size": [width, height], "view": label}
