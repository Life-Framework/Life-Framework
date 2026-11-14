# The fit step of a route (b) or (c) garment (docs/asset-pipeline/clothing.md "Routes b and c").
#   blender --background --factory-startup --disable-autoexec --python clothing_fit.py -- <job.json>
#
# Opens Bohemia's Character_Weights_Template.blend (Arma Reforger Samples, APL) for Body_LOD0, the base
# body skinned with the game's weights, and lets Bohemia's Enfusion Blender Tools (shipped with Arma
# Reforger Tools) rebuild the vanilla skeleton from the Samples' Character_Template.fbx: 224 bone
# Empties and the 6 locators every worn model carries. Then:
#   1. brings in the garment's worn mesh (job.source + job.object) and its ground mesh (job.item_object);
#   2. transfers skin weights onto the worn mesh from job.weights_from: "body" (Body_LOD0) or the name
#      of a skinned object in job.source (a similar garment); keeps the 4 strongest per vertex
#      (Enfusion's limit) and normalises them;
#   3. pushes any garment vertex that sits inside the body out along the body normal by job.clearance_m
#      (the body pokes through where it is not hidden);
#   4. builds LODs <Name>_LOD0..n by job.lod_ratios (decimate keeps the weights), and a rigid FireGeo
#      collider per body region, parented to its bone (UTM_<Name>_<Bone>), the way the sample vest does;
#   5. exports <out>/<Name>.fbx through EBT's patched FBX writer (bones as LimbNodes with a bind pose,
#      LODs skinned to them, colliders, locators) and <out>/<Name>_item.fbx (the ground model with LODs
#      and a UBX_ box collider, resting on its origin);
#   6. writes <out>/fit.json: triangles per LOD, influences, bones used, how far it moved vertices,
#      and, when job.compare_to names a skinned object, how close the transferred weights are to it.
import json, math, os, sys
import bpy
import bmesh
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree


def log(*a):
    print("LF_CLOTHING_FIT", *a)


def bring_in(path, names):
    """Append objects `names` from a .blend, or import a .fbx/.glb and keep those names."""
    ext = os.path.splitext(path)[1].lower()
    before = set(bpy.data.objects)
    if ext == ".blend":
        with bpy.data.libraries.load(path, link=False) as (src, dst):
            dst.objects = [n for n in src.objects if n in names]
        for o in dst.objects:
            if o:
                bpy.context.scene.collection.objects.link(o)
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=path)
    elif ext in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=path)
    else:
        raise SystemExit("unsupported garment source " + path)
    # A just-linked object reports an identity matrix_world until the view layer updates (seen 2026-11-14:
    # the sample vest came in 0.97 m too low and took the hip weights).
    bpy.context.view_layer.update()
    new = [o for o in bpy.data.objects if o not in before]
    found = {}
    for n in names:
        # Only what this import brought: the item file of the sample vest names its mesh Object_LOD0 too.
        o = next((x for x in new if x.name == n), None) or next((x for x in new if x.name.split(".")[0] == n), None)
        if not o:
            raise SystemExit("the source has no object %s (it has %s)" % (n, [x.name for x in new]))
        found[n] = o
    # Anything else the import brought (its own armature, lights) is dropped.
    for o in new:
        if o not in found.values() and o.type != "MESH":
            bpy.data.objects.remove(o, do_unlink=True)
    return found


def mesh_copy(o, name):
    c = o.copy()
    c.data = o.data.copy()
    c.name = name
    c.data.name = name
    bpy.context.scene.collection.objects.link(c)
    bpy.context.view_layer.update()
    return c


def apply_transforms(o):
    o.parent = None
    mw = o.matrix_world.copy()
    o.data.transform(mw)
    o.matrix_world.identity()


def clear_weights(o):
    for m in list(o.modifiers):
        o.modifiers.remove(m)
    o.vertex_groups.clear()


def transfer(o, src, bones):
    mod = o.modifiers.new("lf_transfer", "DATA_TRANSFER")
    mod.object = src
    mod.use_vert_data = True
    mod.data_types_verts = {"VGROUP_WEIGHTS"}
    mod.vert_mapping = "POLYINTERP_NEAREST"
    mod.layers_vgroup_select_src = "ALL"
    mod.layers_vgroup_select_dst = "NAME"
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.datalayout_transfer(modifier=mod.name)
    bpy.ops.object.modifier_apply(modifier=mod.name)
    for g in list(o.vertex_groups):
        if g.name not in bones:
            o.vertex_groups.remove(g)


def garment_bones(arm, keep_head=False):
    """The bones a garment may follow: none of the face (everything under Head), the fingers (under the
    hands) or the toes. A hat keeps Head itself."""
    out = set()
    def walk(b, blocked):
        name = b.name
        here_blocked = blocked or (name.endswith("Toe") or name.endswith("Toe_End"))
        if not here_blocked:
            out.add(name)
        under = blocked or name in ("Head", "LeftHand", "RightHand")
        for c in b.children:
            walk(c, under)
    for b in arm.data.bones:
        if b.parent is None:
            walk(b, False)
    if not keep_head:
        out.discard("Head")
    return out


def fold_into_allowed(o, arm, allowed):
    """Move the weight of every bone outside `allowed` onto its nearest allowed ancestor (Jaw -> Neck3)."""
    moved = {}
    for g in list(o.vertex_groups):
        if g.name in allowed:
            continue
        b = arm.data.bones.get(g.name)
        while b is not None and b.name not in allowed:
            b = b.parent
        target = b.name if b is not None else "Hips"
        tg = o.vertex_groups.get(target) or o.vertex_groups.new(name=target)
        for v in o.data.vertices:
            for vg in v.groups:
                if vg.group == g.index and vg.weight > 1e-6:
                    tg.add([v.index], vg.weight, "ADD")
        moved[g.name] = target
        o.vertex_groups.remove(g)
    return moved


def smooth_weights(o, repeat, factor=0.5):
    """Average each vertex's weights with its edge neighbours', `repeat` times (a stiff garment should not
    follow every muscle helper of the body under it)."""
    if repeat <= 0:
        return
    me = o.data
    n = len(me.vertices)
    nbr = [[] for _ in range(n)]
    for e in me.edges:
        a, b = e.vertices
        nbr[a].append(b)
        nbr[b].append(a)
    names = [g.name for g in o.vertex_groups]
    w = [dict() for _ in range(n)]
    for v in me.vertices:
        for g in v.groups:
            if g.weight > 1e-6:
                w[v.index][g.group] = g.weight
    for _ in range(repeat):
        nw = []
        for i in range(n):
            if not nbr[i]:
                nw.append(w[i])
                continue
            avg = {}
            for j in nbr[i]:
                for k, x in w[j].items():
                    avg[k] = avg.get(k, 0) + x / len(nbr[i])
            keys = set(w[i]) | set(avg)
            nw.append({k: (1 - factor) * w[i].get(k, 0) + factor * avg.get(k, 0) for k in keys})
        w = nw
    for gi, name in enumerate(names):
        g = o.vertex_groups[name]
        g.remove(list(range(n)))
    for i in range(n):
        for k, x in w[i].items():
            if x > 1e-5:
                o.vertex_groups[k].add([i], x, "REPLACE")


def limit_and_normalise(o, limit):
    me = o.data
    worst = 0
    names = {g.index: g.name for g in o.vertex_groups}
    for v in me.vertices:
        ws = sorted(((g.weight, g.group) for g in v.groups if g.weight > 1e-5), reverse=True)
        worst = max(worst, len(ws))
        keep = ws[:limit]
        total = sum(w for w, _ in keep) or 1.0
        # The group indices first: removing from a vertex invalidates its v.groups entries.
        for gi in [g.group for g in v.groups]:
            o.vertex_groups[names[gi]].remove([v.index])
        for w, gi in keep:
            o.vertex_groups[names[gi]].add([v.index], w / total, "REPLACE")
    return worst


def used_bones(o):
    used = set()
    for v in o.data.vertices:
        for g in v.groups:
            if g.weight > 1e-5:
                used.add(o.vertex_groups[g.group].name)
    return sorted(used)


def push_out(o, body, clearance):
    """Move garment vertices that are inside the body (or closer than `clearance`) out along the body normal."""
    dg = bpy.context.evaluated_depsgraph_get()
    bm = bmesh.new()
    bm.from_object(body, dg)
    bm.transform(body.matrix_world)
    tree = BVHTree.FromBMesh(bm)
    moved, most = 0, 0.0
    for v in o.data.vertices:
        loc, normal, _, dist = tree.find_nearest(v.co)
        if loc is None:
            continue
        side = (v.co - loc).dot(normal)
        if side < clearance:
            delta = clearance - side
            v.co = v.co + normal * delta
            moved += 1
            most = max(most, delta)
    bm.free()
    return moved, most


def tris(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


def decimate(o, ratio):
    mod = o.modifiers.new("lf_lod", "DECIMATE")
    mod.ratio = ratio
    mod.use_collapse_triangulate = True
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.modifier_apply(modifier=mod.name)


def skin(o, arm):
    o.parent = None
    mod = o.modifiers.new("Armature", "ARMATURE")
    mod.object = arm


def region_colliders(name, worn, bone_by_name, gamemat_name):
    """One rigid UTM_ piece per dominant bone region, parented to that bone's Empty (the sample vest parents
    its pieces to the bones; with EBT the bones are Empties, and the armature itself may have no children)."""
    by_bone = {}
    for p in worn.data.polygons:
        acc = {}
        for vi in p.vertices:
            for g in worn.data.vertices[vi].groups:
                acc[g.group] = acc.get(g.group, 0) + g.weight
        if acc:
            gi = max(acc, key=acc.get)
            by_bone.setdefault(worn.vertex_groups[gi].name, []).append(p.index)
    out = []
    for bone, faces in by_bone.items():
        if len(faces) < 8 or bone not in bone_by_name:
            continue
        c = mesh_copy(worn, "UTM_%s_%s" % (name, bone))
        clear_weights(c)
        bm = bmesh.new()
        bm.from_mesh(c.data)
        keep = set(faces)
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in keep], context="FACES")
        bm.to_mesh(c.data)
        bm.free()
        if tris(c) > 300:
            decimate(c, 300 / tris(c))
        c.data.materials.clear()
        mat = bpy.data.materials.get(gamemat_name) or bpy.data.materials.new(gamemat_name)
        c.data.materials.append(mat)
        c["usage"] = "FireGeo"
        world = c.matrix_world.copy()
        c.parent = bone_by_name[bone]
        c.matrix_world = world
        c["lf_bone"] = bone
        out.append(c)
    return out


def export(path, objects):
    """Export exactly `objects`. The template keeps its Armature in a hidden collection, and a hidden object
    cannot be selected, so the first export left the bones out (2026-11-14): everything exported is linked
    to the scene's own collection and made visible first, and the selection is checked."""
    scene = bpy.context.scene.collection
    for o in objects:
        if o.name not in scene.objects:
            scene.objects.link(o)
        o.hide_viewport = False
        o.hide_select = False
        o.hide_set(False)
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    missed = [o.name for o in objects if not o.select_get()]
    if missed:
        raise SystemExit("could not select for export: %s" % missed)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH", "EMPTY"},
                             add_leaf_bones=False, use_custom_props=True, mesh_smooth_type="FACE",
                             apply_unit_scale=True, bake_anim=False, use_armature_deform_only=False)


def weight_agreement(a, b):
    """How close two skinnings of the same mesh are: mean |dw| over the bones, dominant-bone agreement."""
    names_a = {g.index: g.name for g in a.vertex_groups}
    names_b = {g.index: g.name for g in b.vertex_groups}
    diff, same, n = 0.0, 0, 0
    for va, vb in zip(a.data.vertices, b.data.vertices):
        wa = {names_a[g.group]: g.weight for g in va.groups if g.weight > 1e-5}
        wb = {names_b[g.group]: g.weight for g in vb.groups if g.weight > 1e-5}
        ta, tb = sum(wa.values()) or 1, sum(wb.values()) or 1
        keys = set(wa) | set(wb)
        diff += sum(abs(wa.get(k, 0) / ta - wb.get(k, 0) / tb) for k in keys) / 2
        if wa and wb and max(wa, key=wa.get) == max(wb, key=wb.get):
            same += 1
        n += 1
    return {"mean_weight_difference": round(diff / max(n, 1), 4), "dominant_bone_agreement": round(same / max(n, 1), 4), "vertices": n}


EBT_ADDONS = "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Blender/scripts/addons"


def enable_ebt(path):
    """Bohemia's Enfusion Blender Tools, as Arma Reforger Tools installs them (GPL-3.0). EBT_TEST (set by the
    caller) skips its PySide6 install and Qt window; the modules a headless run cannot load (the viewport
    drawing, the Qt validation UI) fail on their own and are not needed here."""
    import addon_utils
    if path not in sys.path:
        sys.path.insert(0, path)
    sd = bpy.context.preferences.filepaths.script_directories.new()
    sd.directory = path.rsplit("/addons", 1)[0]
    addon_utils.modules_refresh()
    addon_utils.enable("EnfusionBlenderTools", default_set=True)
    from EnfusionBlenderTools.core.fbx import fbx_io, skeleton_import, utils as fbx_utils
    from EnfusionBlenderTools.core import skeleton_utils
    return {"fbx_io": fbx_io, "skeleton_import": skeleton_import, "fbx_utils": fbx_utils, "skeleton_utils": skeleton_utils}


def ebt_skeleton(ebt, body_fbx):
    """Import Character_Template.fbx and let EBT rebuild its skeleton the Enfusion way: one Empty per bone
    (tagged FBX type Bone), the locators (EntityPosition and the IK targets) and an armature driven by them."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=body_fbx)
    import pathlib
    ebt["skeleton_import"].rebuild_skeletons_from_fbx(before, pathlib.Path(body_fbx))
    new = [o for o in bpy.data.objects if o not in before]
    bone_type = ebt["skeleton_utils"].FBX_TYPE_BONE
    bones = [o for o in new if o.type == "EMPTY" and o.ebt_fbx_type == bone_type]
    locators = [o for o in new if o.type == "EMPTY" and o.ebt_fbx_type != bone_type]
    arms = [o for o in new if o.type == "ARMATURE"]
    if len(arms) != 1 or not bones:
        raise SystemExit("EBT rebuilt %d armature(s) and %d bone(s) from %s" % (len(arms), len(bones), body_fbx))
    return arms[0], bones, locators


def export_ebt(ebt, path, objects, name):
    """Export through EBT's patched FBX writer: bone Empties become LimbNodes with a bind pose and the mesh
    skin clusters point at them. A stock Blender export of an armature-skinned mesh loses every mesh in
    today's Workbench (2026-11-14: the .txo held the 231 nodes and no mesh, with Bohemia's own 2024 sample
    vest .fbx too), so the worn model goes this way."""
    coll = bpy.data.collections.new("lf_export_" + name)
    bpy.context.scene.collection.children.link(coll)
    for o in objects:
        coll.objects.link(o)
    kwargs = ebt["fbx_utils"].FBXExportOptions.create_kwargs(path, coll.name, 1.0)
    ebt["fbx_io"]._export_fbx_with_ebt_info("None", **kwargs)


def main():
    job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
    bpy.ops.wm.open_mainfile(filepath=job["template_blend"], load_ui=False, use_scripts=False)
    arm = bpy.data.objects.get("Armature")
    body = bpy.data.objects.get("Body_LOD0")
    if not arm or not body:
        raise SystemExit("the template has no Armature or Body_LOD0: is it Character_Weights_Template.blend?")
    bones = {b.name for b in arm.data.bones}
    exporter = job.get("exporter", "ebt")
    for o in [o for o in bpy.data.objects if o.name.startswith(("UCS_", "UCX_"))]:
        bpy.data.objects.remove(o, do_unlink=True)
    if exporter == "ebt":
        # The weights template's own helpers go: EBT brings the skeleton the export uses, and their names
        # must stay free (an EntityPosition.001 would not be the node the engine looks for).
        for o in [o for o in bpy.data.objects if o.type == "EMPTY"]:
            bpy.data.objects.remove(o, do_unlink=True)
        arm.name = "lf_weights_armature"
        ebt = enable_ebt(job.get("ebt_addons") or EBT_ADDONS)
        skeleton, bone_empties, helpers = ebt_skeleton(ebt, job["body_fbx"])
    else:
        # Stock Blender export, the way the weapon kind's clean.py does it: the template's armature, the
        # meshes its children as well as skinned to it, the colliders parented to its bones.
        ebt, skeleton, bone_empties = None, arm, []
        helpers = [o for o in bpy.data.objects if o.type == "EMPTY"]
    bone_by_name = {o.name: o for o in bone_empties}
    # The template's own colliders and meshes stay out of the export.
    item_apart = bool(job.get("item_source"))
    names = [job["object"]] + ([job["item_object"]] if job.get("item_object") and not item_apart else []) + ([job["weights_from"]] if job.get("weights_from") not in (None, "body") else []) + ([job["compare_to"]] if job.get("compare_to") else [])
    got = bring_in(job["source"], list(dict.fromkeys(names)))
    if item_apart:
        # The ground model in a file of its own (the sample vest's _item.fbx): kept under a key of its own.
        for o in got.values():
            o.name = "lf_src_" + o.name
        got["__item__"] = bring_in(job["item_source"], [job["item_object"]])[job["item_object"]]
    else:
        got["__item__"] = got.get(job.get("item_object"))
    name = job["name"]
    src_worn = got[job["object"]]
    reference = mesh_copy(src_worn, "lf_reference") if job.get("compare_to") == job["object"] else (got.get(job.get("compare_to")) if job.get("compare_to") else None)
    worn = mesh_copy(src_worn, name + "_LOD0")
    apply_transforms(worn)
    clear_weights(worn)
    weights_src = body if job.get("weights_from", "body") == "body" else got[job["weights_from"]]
    transfer(worn, weights_src, bones)
    allowed = garment_bones(arm, keep_head=bool(job.get("keep_head")))
    folded = fold_into_allowed(worn, arm, allowed)
    smooth_weights(worn, int(job.get("smooth", 2)))
    before = limit_and_normalise(worn, job.get("max_influences", 4))
    moved, most = (0, 0.0)
    if job.get("clearance_m"):
        moved, most = push_out(worn, body, float(job["clearance_m"]))
    report = {"name": name, "source": job["source"], "weights_from": job.get("weights_from", "body"), "influences_before_limit": before,
              "pushed_out": {"vertices": moved, "most_m": round(most, 4)}, "lods": [], "bones_used": used_bones(worn),
              "folded_bones": len(folded), "smooth": int(job.get("smooth", 2))}
    if reference is not None:
        report["weights_vs_reference"] = weight_agreement(worn, reference)
    lods = [worn]
    for i, r in enumerate(job.get("lod_ratios", [0.5, 0.25, 0.12])):
        l = mesh_copy(worn, "%s_LOD%d" % (name, i + 1))
        decimate(l, r)
        # A collapse merges the weights of the vertices it joins: LOD1 came out with 6 influences, LOD2
        # with 7 (2026-11-14), over the 4 Enfusion skins with. Limit every LOD again.
        limit_and_normalise(l, job.get("max_influences", 4))
        lods.append(l)
    for l in lods:
        l.parent = None
        if ebt:
            ebt["skeleton_utils"].bind_mesh_to_armature(l, skeleton)
        else:
            l.parent = arm
            l.modifiers.new("Armature", "ARMATURE").object = arm
        report["lods"].append({"name": l.name, "tris": tris(l), "max_influences": max((sum(1 for g in v.groups if g.weight > 1e-5) for v in l.data.vertices), default=0)})
    colliders = region_colliders(name, worn, bone_by_name if ebt else {b.name: arm for b in arm.data.bones}, job.get("collider_material", "fabric"))
    if not ebt:
        for c in colliders:
            world = c.matrix_world.copy()
            c.parent = arm
            c.parent_type = "BONE"
            c.parent_bone = c["lf_bone"]
            c.matrix_world = world
    report["colliders"] = [c.name for c in colliders]
    out = job["out"]
    os.makedirs(out, exist_ok=True)
    if ebt:
        export_ebt(ebt, os.path.join(out, name + ".fbx"), [skeleton] + bone_empties + helpers + lods + colliders, name)
    else:
        export(os.path.join(out, name + ".fbx"), [arm] + helpers + lods + colliders)
    report["exporter"] = exporter
    if got.get("__item__") is not None:
        item = mesh_copy(got["__item__"], name + "_item_LOD0")
        apply_transforms(item)
        clear_weights(item)
        # Rest on the origin: the ground model's lowest point at y = 0 (Blender z), centred in x and y.
        zs = [v.co.z for v in item.data.vertices]
        xs = [v.co.x for v in item.data.vertices]
        ys = [v.co.y for v in item.data.vertices]
        item.data.transform(Matrix.Translation((-(min(xs) + max(xs)) / 2, -(min(ys) + max(ys)) / 2, -min(zs))))
        ilods = [item]
        for i, r in enumerate(job.get("item_lod_ratios", [0.5, 0.25])):
            l = mesh_copy(item, "%s_item_LOD%d" % (name, i + 1))
            decimate(l, r)
            ilods.append(l)
        # A box around the folded garment, the way vanilla ground models have it (UBX_Jacket_Suit_01_Item).
        # A convex hull of the whole mesh kept its interior faces and Workbench refused it (2026-11-14).
        me = bpy.data.meshes.new("UBX_%s_item" % name)
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        lo = Vector((min(xs) - (min(xs) + max(xs)) / 2, min(ys) - (min(ys) + max(ys)) / 2, 0.0))
        hi = Vector((max(xs) - (min(xs) + max(xs)) / 2, max(ys) - (min(ys) + max(ys)) / 2, max(zs) - min(zs)))
        for v in bm.verts:
            v.co = Vector(tuple(lo[i] if v.co[i] < 0 else hi[i] for i in range(3)))
        bm.to_mesh(me)
        bm.free()
        hull = bpy.data.objects.new(me.name, me)
        bpy.context.scene.collection.objects.link(hull)
        hull["usage"] = "ItemFireView"
        report["item"] = {"lods": [{"name": l.name, "tris": tris(l)} for l in ilods], "collider": hull.name, "collider_tris": tris(hull),
                          "size_m": [round(max(xs) - min(xs), 3), round(max(ys) - min(ys), 3), round(max(zs) - min(zs), 3)]}
        export(os.path.join(out, name + "_item.fbx"), ilods + [hull])
    with open(os.path.join(out, "fit.json"), "w") as f:
        json.dump(report, f, indent=1)
    log("ok", json.dumps({k: report[k] for k in ("lods", "pushed_out")}), json.dumps(report.get("weights_vs_reference", {})))


if __name__ == "__main__":
    main()
