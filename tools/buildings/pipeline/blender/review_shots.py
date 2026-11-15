# Review renders of a building straight from its .blend (tools\cli building review <slug> --shots).
#
#   blender --background --factory-startup <building.blend> --python review_shots.py -- <job.json>
#
# job.json: {"out": "<dir>", "albedo": {"exterior": k, "interior": k, "outside_share": s}}. Writes the
# shots and <out>/shots.json, exits 0 (3 on an exception). It needs no add-on: the .blend is read as
# plain meshes. Where it points the camera comes from the building's own bounds, its entrance gap and
# the rooms found in a floor-plan slice, never from numbers kept per building.
#
# The street front is Blender -Y (ArchBuilder builds that way), which is the prefab's -Z.
import bpy
import json
import math
import os
import sys
import time
from collections import deque

from mathutils import Vector
from mathutils.bvhtree import BVHTree

job = json.load(open(sys.argv[sys.argv.index("--") + 1], encoding="utf-8"))
out = job["out"]
os.makedirs(out, exist_ok=True)
W, H = 1280, 720
EYE = 1.7
t0 = time.time()

sc = bpy.context.scene
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
if not meshes:
    raise RuntimeError("the .blend holds no meshes")


def part(o):
    return o.get("ab_part") or ""


def bounds(objs):
    lo, hi = Vector((1e9, 1e9, 1e9)), Vector((-1e9, -1e9, -1e9))
    for o in objs:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo, hi = Vector(map(min, lo, w)), Vector(map(max, hi, w))
    return lo, hi


lo, hi = bounds(meshes)                                    # everything: canopy, roof units
walls = [o for o in meshes if o.get("ab_is_wall")]
wlo, whi = bounds(walls) if walls else (lo, hi)            # the building body
c, bw, bd = (wlo + whi) / 2, whi.x - wlo.x, whi.y - wlo.y
FRONT_Y = wlo.y


# ------------------------------------------------------------------------ colours
# The game's albedo is k x the Blender colour (tools/buildings/pipeline/materials.mjs), one k for a
# material that faces outside and one for a material only inside. Here the share of area facing out
# is read from the part a face is on: the shell and roof are outside, rooms and what is in them inside,
# and a wall face in an interior finish is inside.
OUTSIDE_PARTS = {"walls", "roof", "trim", "detail", "frames", "glass"}
INSIDE_FINISH = ("AB_Plaster_Interior", "AB_Paint", "AB_Tiles")
area = {}
for o in meshes:
    for p in o.data.polygons:
        if p.material_index >= len(o.material_slots) or o.material_slots[p.material_index].material is None:
            continue
        m = o.material_slots[p.material_index].material
        outside = part(o) in OUTSIDE_PARTS and not (part(o) == "walls" and m.name.startswith(INSIDE_FINISH))
        e = area.setdefault(m, [0.0, 0.0])
        e[0] += p.area
        if outside:
            e[1] += p.area
a_cfg = job["albedo"]
for m, (total, outside) in area.items():
    k = a_cfg["exterior"] if total and outside / total >= a_cfg["outside_share"] else a_cfg["interior"]
    col = m.diffuse_color
    m.diffuse_color = (col[0] * k, col[1] * k, col[2] * k, col[3])


# ------------------------------------------------------------------------ rooms
# A floor-plan slice at 1.2 m: the distance from each cell to the nearest wall, partition or door
# frame. Cells within a closing distance of one are solid, which also shuts door gaps up to twice that wide; what is
# left in pieces is the rooms. The piece touching the border is the outside, the biggest is the hall.
CELL, SLICE_Z = 0.12, 1.2
solid_objs = [o for o in meshes if part(o) in ("walls", "interior", "idoors", "frames", "glass")]
verts, tris = [], []
for o in solid_objs:
    base = len(verts)
    verts.extend(o.matrix_world @ v.co for v in o.data.vertices)
    o.data.calc_loop_triangles()
    tris.extend(tuple(base + i for i in t.vertices) for t in o.data.loop_triangles)
rooms, hall = [], None
if tris:
    bvh = BVHTree.FromPolygons(verts, tris)
    x0, y0 = wlo.x - 3.0, wlo.y - 3.0
    nx, ny = int((bw + 6.0) / CELL) + 1, int((bd + 6.0) / CELL) + 1
    depth = [0.0] * (nx * ny)
    for j in range(ny):
        for i in range(nx):
            hit = bvh.find_nearest(Vector((x0 + (i + 0.5) * CELL, y0 + (j + 0.5) * CELL, SLICE_Z)), 3.0)
            depth[j * nx + i] = hit[3] if hit[3] is not None else 3.0
    cell = lambda i, j: j * nx + i

    def pieces(close):
        label, found = [-1] * (nx * ny), []
        for j0 in range(ny):
            for i0 in range(nx):
                if label[cell(i0, j0)] != -1 or depth[cell(i0, j0)] <= close:
                    continue
                n = len(found)
                q, cells = deque([(i0, j0)]), []
                label[cell(i0, j0)] = n
                while q:
                    i, j = q.popleft()
                    cells.append((i, j))
                    for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        a, b = i + di, j + dj
                        if 0 <= a < nx and 0 <= b < ny and label[cell(a, b)] == -1 and depth[cell(a, b)] > close:
                            label[cell(a, b)] = n
                            q.append((a, b))
                found.append(cells)
        return label, found

    # Door gaps differ from building to building: take the closing distance that splits the inside
    # into the most rooms (the smallest one wins a tie, so rooms keep their size)
    best = None
    for close in (0.5, 0.65, 0.8, 0.95, 1.1):
        label, sizes = pieces(close)
        count = sum(1 for n, cells in enumerate(sizes) if n != label[cell(0, 0)] and len(cells) * CELL * CELL >= 0.5)
        if best is None or count > best[0]:
            best = (count, close, label, sizes)
    _, CLOSE, label, sizes = best
    outside_id = label[cell(0, 0)]
    # each piece grows back out to the walls (cells not on a wall surface), nearest piece first
    owner = list(label)
    frontier = deque((i, j) for n, cells in enumerate(sizes) for i, j in cells)
    for _ in range(int(CLOSE / CELL) + 2):
        nxt = deque()
        while frontier:
            i, j = frontier.popleft()
            for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                a, b = i + di, j + dj
                if 0 <= a < nx and 0 <= b < ny and owner[cell(a, b)] == -1 and depth[cell(a, b)] > CELL * 0.9:
                    owner[cell(a, b)] = owner[cell(i, j)]
                    nxt.append((a, b))
        frontier = nxt
    by_room = {}
    for j in range(ny):
        for i in range(nx):
            n = owner[cell(i, j)]
            if n >= 0 and n != outside_id:
                by_room.setdefault(n, []).append((i, j))
    for n, cells in by_room.items():
        if len(cells) * CELL * CELL < 3.0:
            continue
        xs, ys = [i for i, _ in cells], [j for _, j in cells]
        deep = max(cells, key=lambda p: depth[cell(*p)])
        rooms.append({
            "cells": cells, "area": len(cells) * CELL * CELL,
            "min": Vector((x0 + min(xs) * CELL, y0 + min(ys) * CELL, 0)),
            "max": Vector((x0 + (max(xs) + 1) * CELL, y0 + (max(ys) + 1) * CELL, 0)),
            "mid": Vector((x0 + (deep[0] + 0.5) * CELL, y0 + (deep[1] + 0.5) * CELL, 0)),
        })
    rooms.sort(key=lambda r: -r["area"])
    if rooms:
        hall = rooms.pop(0)
        rooms.sort(key=lambda r: r["mid"].x)


def where(room):
    # name by place in the building as seen from the street: front/back, left/middle/right
    mid = (room["min"] + room["max"]) / 2
    fb = "back" if mid.y > (hall["min"].y + hall["max"].y) / 2 else "front"
    lr = "left" if mid.x < c.x - bw / 6 else ("right" if mid.x > c.x + bw / 6 else "middle")
    return fb + "-" + lr


# ------------------------------------------------------------------------ the entrance
# A gap in the front wall at 0.5 m (below any window sill) wide enough for a door. Several: the one
# nearest the middle of the front. None: the middle.
entrance = {"x": c.x, "w": 0.0, "found": False}
if walls:
    wv, wt = [], []
    for o in walls:
        base = len(wv)
        wv.extend(o.matrix_world @ v.co for v in o.data.vertices)
        o.data.calc_loop_triangles()
        wt.extend(tuple(base + i for i in t.vertices) for t in o.data.loop_triangles)
    wb = BVHTree.FromPolygons(wv, wt)
    step, gaps, run = 0.1, [], None
    x = wlo.x + 0.3
    while x < whi.x - 0.3:
        hit = wb.ray_cast(Vector((x, FRONT_Y - 3.0, 0.5)), Vector((0, 1, 0)), 3.0 + 1.0)[0]
        if hit is None:
            run = [x, x] if run is None else [run[0], x]
        elif run is not None:
            gaps.append(run)
            run = None
        x += step
    if run is not None:
        gaps.append(run)
    doors = [g for g in gaps if 0.8 <= g[1] - g[0] + step <= 3.6]
    if doors:
        g = min(doors, key=lambda g: abs((g[0] + g[1]) / 2 - c.x))
        entrance = {"x": (g[0] + g[1]) / 2, "w": round(g[1] - g[0] + step, 2), "found": True}

# ------------------------------------------------------------------------ the scene
sc.render.engine = "BLENDER_WORKBENCH"
sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage = W, H, 100
sc.render.film_transparent = False
sc.view_settings.view_transform = "Standard"
sc.view_settings.exposure = job.get("exterior_ev", 1.3)
sc.view_settings.look = "None"
sh = sc.display.shading
sh.light, sh.color_type = "STUDIO", "MATERIAL"
sh.show_cavity, sh.cavity_type = True, "WORLD"
sh.show_shadows = True
sc.display.render_aa = "8"
sc.world = sc.world or bpy.data.worlds.new("World")
sc.world.color = (0.16, 0.28, 0.50)

gmesh = bpy.data.meshes.new("lf_shots_ground")
s = max(hi.x - lo.x, hi.y - lo.y) * 2.2
cx, cy = (lo.x + hi.x) / 2, (lo.y + hi.y) / 2
gmesh.from_pydata([(cx - s, cy - s, 0), (cx + s, cy - s, 0), (cx + s, cy + s, 0), (cx - s, cy + s, 0)], [], [(0, 1, 2, 3)])
ground = bpy.data.objects.new("lf_shots_ground", gmesh)
gmat = bpy.data.materials.new("lf_shots_ground")
gmat.diffuse_color = (0.035, 0.06, 0.02, 1)
gmesh.materials.append(gmat)
sc.collection.objects.link(ground)

cam_data = bpy.data.cameras.new("lf_shots_cam")
cam = bpy.data.objects.new("lf_shots_cam", cam_data)
sc.collection.objects.link(cam)
sc.camera = cam
cam_data.clip_start, cam_data.clip_end = 0.05, max(bw, bd) * 12

taken = []


def render(name, text, at=None, look=None, lens=24):
    cam_data.type, cam_data.lens = "PERSP", lens
    cam.location = Vector(at)
    cam.rotation_euler = (Vector(look) - Vector(at)).to_track_quat("-Z", "Y").to_euler()
    path = os.path.join(out, name + ".png")
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    # prefab space is (x, z, y): Blender -Y is the prefab's -Z
    taken.append({"name": name, "label": text, "file": path, "lens": lens,
                  "cam_blender": [round(v, 2) for v in at], "target_blender": [round(v, 2) for v in look],
                  "cam_prefab": [round(at[0], 2), round(at[2], 2), round(at[1], 2)],
                  "target_prefab": [round(look[0], 2), round(look[2], 2), round(look[1], 2)]})


# street shots: outside, with shadows. Distances are in building widths, so they scale.
tan_h = lambda lens: 18.0 / lens                    # tan of half the horizontal field of view
dist_front = (bw / 2 * 1.08) / tan_h(26) + (FRONT_Y - lo.y)
render("street_front", "street front", (c.x, FRONT_Y - dist_front, EYE), (c.x, FRONT_Y, 2.3), 26)
d3 = max(bw, bd) * 1.15 + (FRONT_Y - lo.y)
render("street_3q", "three-quarter", (c.x - d3 * 0.62, FRONT_Y - d3 * 0.78, EYE), (c.x + bw * 0.08, FRONT_Y + bd * 0.1, 1.9), 26)
d_door = max(5.5, (entrance["w"] or 2.7) * 1.7) + (FRONT_Y - lo.y)
render("entrance", "entrance (%.1f m wide)" % entrance["w"] if entrance["found"] else "entrance (not found: front middle)",
       (entrance["x"], FRONT_Y - d_door, EYE), (entrance["x"], FRONT_Y, 1.9), 28)

# inside: no sun shadows (the roof would shade the whole hall) and no ground
sh.show_shadows = False
sh.light = job.get("interior_light", "STUDIO")
sc.view_settings.exposure = job.get("interior_ev", 0.8)
ground.hide_render = True
if hall:
    near_y = max(hall["min"].y + 0.7, FRONT_Y + 0.7)
    far_y = hall["max"].y - 0.5
    render("hall", "hall from the entrance", (entrance["x"], near_y, EYE), (hall["mid"].x, far_y, 1.5), 20)
    render("hall_back", "hall from the back", (hall["mid"].x, far_y - 0.3, EYE), (entrance["x"], near_y, 1.6), 20)
    for n, r in enumerate(rooms, 1):
        # stand at the far end of the room from the hall, look back at where one comes in
        door = min(r["cells"], key=lambda p: (Vector((hall["mid"].x, hall["mid"].y)) - Vector((x0 + p[0] * CELL, y0 + p[1] * CELL))).length)
        dp = Vector((x0 + (door[0] + 0.5) * CELL, y0 + (door[1] + 0.5) * CELL, 0))
        far = max((p for p in r["cells"] if depth[cell(*p)] > 0.35), key=lambda p: (dp - Vector((x0 + p[0] * CELL, y0 + p[1] * CELL, 0))).length, default=None)
        fp = Vector((x0 + (far[0] + 0.5) * CELL, y0 + (far[1] + 0.5) * CELL, 0)) if far else r["mid"]
        r["name"] = "room%d" % n
        r["label"] = "room %d, %s, %.0f x %.0f m" % (n, where(r), r["max"].x - r["min"].x, r["max"].y - r["min"].y)
        render(r["name"], r["label"], (fp.x, fp.y, EYE), (dp.x, dp.y, 1.3), 18)

# the plan: a section 2.2 m above the floor, seen from above, front at the bottom, rooms labelled
ground.hide_render = False
cam_data.type = "ORTHO"
cam_data.ortho_scale = max(bw + 4, (bd + 4) * W / H)
top = hi.z + 5
cam.location = Vector((c.x, c.y, top))
cam.rotation_euler = (0, 0, 0)
cam_data.clip_start, cam_data.clip_end = top - 2.2, top + 10


def text(body, at, size=0.55, colour=(1, 0.9, 0.15, 1)):
    cu = bpy.data.curves.new("lf_shots_text", "FONT")
    cu.body, cu.size, cu.align_x, cu.align_y = body, size, "CENTER", "CENTER"
    ob = bpy.data.objects.new("lf_shots_text", cu)
    mat = bpy.data.materials.new("lf_shots_text")
    mat.diffuse_color = colour
    cu.materials.append(mat)
    ob.location = at
    ob.show_in_front = True
    sc.collection.objects.link(ob)


if hall:
    text("hall %.0f x %.0f m" % (hall["max"].x - hall["min"].x, hall["max"].y - hall["min"].y), (hall["mid"].x, hall["mid"].y, 2.1), 0.7)
    for r in rooms:
        text(r["label"].replace(", ", "\n"), (r["mid"].x, r["mid"].y, 2.1), 0.4)
if entrance["found"]:
    text("entrance", (entrance["x"], FRONT_Y - 1.4, 2.1), 0.45, (0.3, 1, 0.4, 1))
path = os.path.join(out, "plan.png")
sc.render.filepath = path
bpy.ops.render.render(write_still=True)
taken.append({"name": "plan", "label": "plan, section at 2.2 m", "file": path, "lens": 0,
              "cam_blender": [round(c.x, 2), round(c.y, 2), round(top, 2)], "target_blender": [round(c.x, 2), round(c.y, 2), 0.0],
              "cam_prefab": [], "target_prefab": []})

report = {
    "secs": round(time.time() - t0, 1),
    "building_m": [round(bw, 2), round(bd, 2), round(hi.z - max(lo.z, 0.0), 2)],
    "entrance": entrance,
    "hall": {"area": round(hall["area"], 1), "mid": [round(v, 2) for v in hall["mid"][:2]]} if hall else None,
    "rooms": [{"name": r["name"], "label": r["label"], "area": round(r["area"], 1), "mid": [round(v, 2) for v in r["mid"][:2]]} for r in rooms],
    "shots": taken,
}
with open(os.path.join(out, "shots.json"), "w", encoding="utf-8") as fh:
    json.dump(report, fh, indent=1)
sys.stdout.flush()
os._exit(0)
