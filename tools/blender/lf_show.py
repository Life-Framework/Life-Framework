# Runs inside the user's live Blender (sent by `tools\cli blender show` over the bridge).
# args[0] = JSON {"path": "<model file>" | null, "clear": bool, "colliders": bool}
# Puts what it loads in the LF_Show collection, selects it and frames it in every 3D view.
# Sets `result` = {loaded, objects, meshes, tris, dims_m, collection_total, framed_views}.
import json
import os
import re

import bpy
from mathutils import Vector

job = json.loads(args[0])  # noqa: F821 (args is provided by the bridge's exec namespace)
NAME = "LF_Show"


def show_collection():
    col = bpy.data.collections.get(NAME)
    if col is None:
        col = bpy.data.collections.new(NAME)
    if col.name not in bpy.context.scene.collection.children:
        bpy.context.scene.collection.children.link(col)
    return col


def layer_collection(layer, col):
    if layer.collection == col:
        return layer
    for child in layer.children:
        hit = layer_collection(child, col)
        if hit:
            return hit
    return None


def clear(col):
    """Remove LF_Show's objects, then the object data that nothing else uses."""
    gone = list(col.objects)
    kinds = ("meshes", "armatures", "curves", "cameras", "lights")
    owned = []  # (collection name, data name): looked up again afterwards, the references die with the objects
    for o in gone:
        if o.data is not None:
            for kind in kinds:
                if o.data.name in getattr(bpy.data, kind) and getattr(bpy.data, kind)[o.data.name] == o.data:
                    owned.append((kind, o.data.name))
    for o in gone:
        bpy.data.objects.remove(o, do_unlink=True)
    for kind, name in set(owned):
        d = getattr(bpy.data, kind).get(name)
        if d is not None and d.users == 0:
            getattr(bpy.data, kind).remove(d)
    return len(gone)


col = show_collection()
removed = clear(col) if job.get("clear") else 0
new_objs = []
path = job.get("path")
if path:
    if bpy.context.object is not None and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    before = set(bpy.data.objects.keys())
    ext = os.path.splitext(path)[1].lower()
    lc = layer_collection(bpy.context.view_layer.layer_collection, col)
    if lc is not None:
        bpy.context.view_layer.active_layer_collection = lc
    if ext == ".blend":
        if bpy.data.filepath and os.path.normcase(os.path.abspath(bpy.data.filepath)) == os.path.normcase(os.path.abspath(path)):
            raise SystemExit("that .blend is the file open in Blender; its objects are already in the scene")
        with bpy.data.libraries.load(path, link=False) as (src, dst):
            dst.objects = list(src.objects)
        for o in dst.objects:
            if o is not None:
                col.objects.link(o)
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=path)
    elif ext in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=path)
    elif ext == ".obj":
        bpy.ops.wm.obj_import(filepath=path)
    else:
        raise SystemExit("cannot show a %s file in Blender (.fbx .glb .gltf .obj .blend only)" % ext)
    new_objs = [bpy.data.objects[n] for n in bpy.data.objects.keys() if n not in before]
    for o in new_objs:  # importers link to the active collection; make sure, and keep them out of others
        for c in list(o.users_collection):
            if c != col:
                c.objects.unlink(o)
        if o.name not in col.objects:
            col.objects.link(o)

# Enfusion collision shapes (UBX_ box, UCX_ convex, UTM_ trimesh ...) cover the model: hidden unless asked for.
COLLIDER = re.compile(r"^(UBX|UCX|UTM|USP|UCS|UCL)_")
colliders = [o for o in new_objs if COLLIDER.match(o.name)]
if not job.get("colliders"):
    for o in colliders:
        o.hide_set(True)
    new_objs = [o for o in new_objs if o not in colliders]

bpy.ops.object.select_all(action="DESELECT")
for o in new_objs:
    if o.name in bpy.context.view_layer.objects:
        o.select_set(True)
meshes = [o for o in new_objs if o.type == "MESH"]
active = meshes[0] if meshes else (new_objs[0] if new_objs else None)
if active is not None:
    bpy.context.view_layer.objects.active = active

lo = Vector((1e18,) * 3)
hi = Vector((-1e18,) * 3)
tris = 0
for o in new_objs:
    if o.type == "MESH":
        o.data.calc_loop_triangles()
        tris += len(o.data.loop_triangles)
    if o.type in {"MESH", "ARMATURE", "CURVE", "EMPTY"}:
        corners = [o.matrix_world @ Vector(c) for c in o.bound_box] if o.type != "EMPTY" else [o.matrix_world.translation]
        for p in corners:
            lo = Vector((min(lo.x, p.x), min(lo.y, p.y), min(lo.z, p.z)))
            hi = Vector((max(hi.x, p.x), max(hi.y, p.y), max(hi.z, p.z)))
scale = bpy.context.scene.unit_settings.scale_length
dims = [round((hi[i] - lo[i]) * scale, 4) for i in range(3)] if new_objs and hi.x >= lo.x else None

framed = 0
if new_objs:
    biggest = max(dims) if dims else 1.0
    for window in bpy.context.window_manager.windows:
        for area in window.screen.areas:
            if area.type != "VIEW_3D":
                continue
            region = next((r for r in area.regions if r.type == "WINDOW"), None)
            if region is None:
                continue
            space = area.spaces.active
            if biggest < 1.0:  # a 15 cm prop would sit inside the default 1 cm near clip
                space.clip_start = min(space.clip_start, max(biggest * 0.01, 0.0001))
            with bpy.context.temp_override(window=window, area=area, region=region):
                bpy.ops.view3d.view_selected()
            framed += 1

result = {
    "loaded": os.path.basename(path) if path else None,
    "removed": removed,
    "objects": [o.name for o in new_objs],
    "meshes": len(meshes),
    "tris": tris,
    "dims_m": dims,
    "collection_total": len(col.objects),
    "framed_views": framed,
    "colliders_hidden": 0 if job.get("colliders") else len(colliders),
}
print("LF_Show: %d object(s), %d mesh(es), %d tris" % (len(new_objs), len(meshes), tris))
