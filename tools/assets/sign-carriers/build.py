# Writes the sign carrier kit (#3307): one-mesh quads at six aspects, each ONE mesh object with ONE material
# and UV 0..1, facing +X like Assets/Signs/LF_BoardFace.fbx (the proven 16:9 carrier, #3039). A second mesh object
# or a second material draws white in the bake, so there is never one.
#
# Run: blender -b --factory-startup -P build.py -- <board-face.fbx> <out-dir> <report.json>
# The 16:9 source quad is re-shaped, not rebuilt, so the axes, normal and UV order are the proven ones: the long
# axis is the width, the other the height (1 m), and only the width is scaled. tools/assets/sign-carriers/build.mjs
# runs this and writes the .meta and prefab files.
import bpy, sys, json, os

SRC, OUT, REPORT = sys.argv[-3], sys.argv[-2], sys.argv[-1]
MATERIAL = "LF_SignFace"

# id -> width over height. The 16:9 row is LF_BoardFace itself and is not rebuilt.
KIT = {
    "1x1": 1.0,
    "4x3": 4.0 / 3.0,
    "3x2": 3.0 / 2.0,
    "2x1": 2.0,
    "4x1": 4.0,
    "1x2": 0.5,
    "5x8": 5.0 / 8.0,
}


def load_source():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=SRC)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    assert len(meshes) == 1, "the source must be one mesh object, got %d" % len(meshes)
    return meshes[0]


def extents(obj):
    xs = [v.co for v in obj.data.vertices]
    return [max(c[i] for c in xs) - min(c[i] for c in xs) for i in range(3)]


def main():
    os.makedirs(OUT, exist_ok=True)
    report = {}
    for key, aspect in KIT.items():
        obj = load_source()
        ext = extents(obj)
        axes = sorted(range(3), key=lambda i: -ext[i])
        wide, high = axes[0], axes[1]
        assert ext[axes[2]] < 1e-6, "the source quad is not flat"
        base_h = ext[high]
        factor = (aspect * base_h) / ext[wide]
        centre = sum(v.co[wide] for v in obj.data.vertices) / len(obj.data.vertices)
        for v in obj.data.vertices:
            v.co[wide] = centre + (v.co[wide] - centre) * factor
        obj.data.update()
        assert len(obj.material_slots) == 1, "one material slot"
        obj.material_slots[0].material.name = MATERIAL
        obj.name = "LF_SignQuad_" + key
        obj.data.name = obj.name
        dst = os.path.join(OUT, "LF_SignQuad_%s.fbx" % key)
        for o in bpy.data.objects:
            o.select_set(o is obj)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.export_scene.fbx(filepath=dst, object_types={"MESH"}, use_selection=True,
                                 mesh_smooth_type="FACE", add_leaf_bones=False)
        ext = extents(obj)
        uv = obj.data.uv_layers.active.data
        us = [d.uv[0] for d in uv]
        vs = [d.uv[1] for d in uv]
        report[key] = {
            "fbx": dst,
            "aspect": aspect,
            "width": ext[wide],
            "height": ext[high],
            "uv": [min(us), min(vs), max(us), max(vs)],
            "polys": len(obj.data.polygons),
            "materials": [s.material.name for s in obj.material_slots],
        }
        print("CARRIER", key, "w", round(ext[wide], 4), "h", round(ext[high], 4), "uv", report[key]["uv"])
    with open(REPORT, "w") as f:
        json.dump(report, f, indent=1)


main()
