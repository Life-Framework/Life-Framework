# The rooms of an ArchBuilder building, from its geometry (tools\cli building, export stage).
#
# ArchBuilder keeps no room list in the scene: the ceiling is one slab over the whole storey and the
# rooms are the spaces its partitions leave. So the rooms are found the way a person reads a plan:
#   1. every wall and partition face that stands at head height is a barrier on a 0.2 m grid;
#   2. the free cells inside a ceiling slab, shrunk by 0.55 m, fall apart at the doors (a door gap is
#      narrower than 1.1 m) into one blob per room;
#   3. a blob's box, grown back by 0.55 m, is the room.
# Returns [{"x0", "x1", "y0", "y1", "z", "area"}] in the building's own frame (Blender metres; the
# prefab's x and z are Blender's x and y), `z` the ceiling height, `area` the free floor in m2.
# tools/buildings/pipeline/lighting.mjs turns them into ceiling lights. Loaded by archbuilder_job.py.
import math
from collections import deque


def find_rooms(anchor, dg, cell=0.2, erode=0.55, min_area=2.5):
    inv = anchor.matrix_world.inverted()
    ceilings = []  # (polygon [(x, y)], z)
    faces = []     # (xy points, z low, z high) of the standing faces of walls and partitions
    for o in anchor.children_recursive:
        part = o.get("ab_part")
        if o.type != "MESH" or o.get("ab_cutter") or o.hide_render or part not in ("slabs", "walls", "interior"):
            continue
        ev = o.evaluated_get(dg)
        me = ev.to_mesh()
        m = inv @ o.matrix_world
        nm = m.to_3x3().inverted().transposed()
        for p in me.polygons:
            n = nm @ p.normal
            vs = [m @ me.vertices[i].co for i in p.vertices]
            zs = [v.z for v in vs]
            if part == "slabs":
                if n.z < -0.9 and min(zs) > 1.8:
                    ceilings.append(([(v.x, v.y) for v in vs], zs[0]))
            elif abs(n.z) < 0.3:
                faces.append(([(v.x, v.y) for v in vs], min(zs), max(zs)))
        ev.to_mesh_clear()

    def inside(px, py, poly):
        c = False
        for i in range(len(poly)):
            (ax, ay), (bx, by) = poly[i], poly[i - 1]
            if (ay > py) != (by > py) and px < (bx - ax) * (py - ay) / (by - ay) + ax:
                c = not c
        return c

    rooms = []
    r = int(round(erode / cell))
    for poly, zc in ceilings:
        x0 = min(p[0] for p in poly)
        y0 = min(p[1] for p in poly)
        nx = int(math.ceil((max(p[0] for p in poly) - x0) / cell))
        ny = int(math.ceil((max(p[1] for p in poly) - y0) / cell))
        blocked = [[False] * ny for _ in range(nx)]
        head = zc - 2.0
        for pts, lo, hi in faces:
            if not (lo - 0.01 <= head <= hi + 0.01):
                continue
            for i in range(len(pts)):
                (ax, ay), (bx, by) = pts[i], pts[i - 1]
                steps = max(1, int(math.hypot(bx - ax, by - ay) / (cell / 4)))
                for s in range(steps + 1):
                    cx = int((ax + (bx - ax) * s / steps - x0) / cell)
                    cy = int((ay + (by - ay) * s / steps - y0) / cell)
                    for dx in (-1, 0, 1):
                        for dy in (-1, 0, 1):
                            if 0 <= cx + dx < nx and 0 <= cy + dy < ny:
                                blocked[cx + dx][cy + dy] = True
        free = [[(not blocked[i][j]) and inside(x0 + (i + 0.5) * cell, y0 + (j + 0.5) * cell, poly) for j in range(ny)] for i in range(nx)]
        core = [[False] * ny for _ in range(nx)]
        for i in range(nx):
            for j in range(ny):
                if free[i][j] and all(0 <= i + a < nx and 0 <= j + b < ny and free[i + a][j + b]
                                      for a in range(-r, r + 1) for b in range(-r, r + 1)):
                    core[i][j] = True
        seen = [[False] * ny for _ in range(nx)]
        for i in range(nx):
            for j in range(ny):
                if not core[i][j] or seen[i][j]:
                    continue
                cells = []
                q = deque([(i, j)])
                seen[i][j] = True
                while q:
                    a, b = q.popleft()
                    cells.append((a, b))
                    for da, db in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        u, v = a + da, b + db
                        if 0 <= u < nx and 0 <= v < ny and core[u][v] and not seen[u][v]:
                            seen[u][v] = True
                            q.append((u, v))
                rx0 = x0 + min(c[0] for c in cells) * cell - erode
                rx1 = x0 + (max(c[0] for c in cells) + 1) * cell + erode
                ry0 = y0 + min(c[1] for c in cells) * cell - erode
                ry1 = y0 + (max(c[1] for c in cells) + 1) * cell + erode
                area = (rx1 - rx0) * (ry1 - ry0)
                if area >= min_area:
                    rooms.append({"x0": round(rx0, 2), "x1": round(rx1, 2), "y0": round(ry0, 2), "y1": round(ry1, 2),
                                  "z": round(zc, 3), "area": round(area, 1)})
    rooms.sort(key=lambda q: (q["z"], q["y0"], q["x0"]))
    return rooms
