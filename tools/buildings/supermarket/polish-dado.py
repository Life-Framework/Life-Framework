# Hand polish of the supermarket: a 1.5 m tiled dado on the sales hall wall finish (its cream
# paint), the edit tools/buildings/supermarket/polished.blend carries. Kept so it can be redone on
# a regenerated building (docs/building-pipeline/polish.md "Changing settings later"):
#   blender --background tmp/buildings/supermarket/building.blend --python polish-dado.py -- <out.blend>
#   tools\cli building adopt supermarket <out.blend>
import bmesh
import bpy
import sys

out = sys.argv[sys.argv.index("--") + 1]
o = next(x for x in bpy.data.objects if x.get("ab_part") == "lining")
me = o.data
names = [m.name if m else "" for m in me.materials]
cream = names.index("AB_Paint_Cream")
if "AB_Tiles_Wall" not in names:
    me.materials.append(bpy.data.materials["AB_Tiles_Wall"])
    names.append("AB_Tiles_Wall")
tiles = names.index("AB_Tiles_Wall")
mw = o.matrix_world
z = 1.5 - mw.translation.z
bm = bmesh.new()
bm.from_mesh(me)
geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, z), plane_no=(0, 0, 1))
n = 0
for f in bm.faces:
    if f.material_index == cream and f.calc_center_median().z < z:
        f.material_index = tiles
        n += 1
bm.to_mesh(me)
bm.free()
me.update()
print("LF_POLISH dado faces", n)
bpy.ops.wm.save_as_mainfile(filepath=out, compress=True)
