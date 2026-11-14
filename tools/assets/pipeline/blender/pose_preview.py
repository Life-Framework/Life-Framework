# Preview of a solved character pose (tools/assets/pipeline/pose.mjs): a stick figure with bone
# volumes, the props it holds, a floor and an optional wall. An agent reads the pictures before the
# clip goes near Workbench (docs/asset-pipeline/animation.md).
#   blender --background --factory-startup --python pose_preview.py -- <job.json>
# job.json = { frames: [{ label, joints: { bone: [x, y, z] }, props: [{ fbx, pos, yaw, pitch }] }],
#              parents: { bone: parent }, wall_z, views, out }
# Positions are Enfusion model space (x right, y up, z forward).
import bpy, sys, json, os, math
from mathutils import Vector, Matrix

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
os.makedirs(job["out"], exist_ok=True)


def e2b(p):
    return Vector((p[0], p[2], p[1]))


bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "MATERIAL"
scene.display.shading.show_cavity = True
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.32, 0.33, 0.35)


def material(name, rgba):
    m = bpy.data.materials.new(name)
    m.diffuse_color = rgba
    return m


SKIN = material("skin", (0.75, 0.6, 0.5, 1))
BODY = material("body", (0.35, 0.42, 0.5, 1))
LEFT = material("left", (0.25, 0.55, 0.9, 1))
RIGHT = material("right", (0.9, 0.4, 0.25, 1))
FLOOR = material("floor", (0.45, 0.45, 0.42, 1))
WALL = material("wall", (0.55, 0.52, 0.48, 1))
PROP = material("prop", (0.3, 0.38, 0.25, 1))
SKIP = ("Volume", "Twist", "Prop", "IK", "Cloth", "Pectoral", "Blade", "Stomach", "Camera", "Collision", "Inbetween", "Entity")


def limb(a, b, radius, mat, into):
    d = b - a
    if d.length < 1e-4:
        return
    bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=d.length, location=(a + b) / 2, vertices=12)
    o = bpy.context.view_layer.objects.active
    o.rotation_euler = d.to_track_quat("Z", "Y").to_euler()
    o.data.materials.append(mat)
    into.append(o)


def figure(joints):
    made = []
    j = {k: e2b(v) for k, v in joints.items()}
    # Spine, neck and the fingers follow the bone chain; the limbs are joined through their twist bones.
    for bone, parent in job["parents"].items():
        if bone not in j or parent not in j or any(k in bone for k in SKIP):
            continue
        if "Hand" in parent and "Hand" in bone:
            mat = LEFT if parent.startswith("Left") else RIGHT
            limb(j[parent], j[bone], 0.009, mat, made)
        elif parent.startswith(("Spine", "Hips")) and bone.startswith(("Spine", "Neck")):
            limb(j[parent], j[bone], 0.11, BODY, made)
        elif parent.startswith("Neck") and bone.startswith(("Neck", "Head")):
            limb(j[parent], j[bone], 0.05, SKIN, made)
    for side, mat in (("Left", LEFT), ("Right", RIGHT)):
        for a, b, r in (("Spine5", side + "Arm", 0.05), (side + "Arm", side + "ForeArm", 0.042), (side + "ForeArm", side + "Hand", 0.034),
                        ("Hips", side + "Leg", 0.08), (side + "Leg", side + "Knee", 0.07), (side + "Knee", side + "Foot", 0.05),
                        (side + "Foot", side + "Toe", 0.04)):
            if a in j and b in j:
                limb(j[a], j[b], r, mat, made)
    if "Head" in j:
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.1, location=j["Head"] + Vector((0, 0.02, 0.09)), segments=16, ring_count=10)
        o = bpy.context.view_layer.objects.active
        o.data.materials.append(SKIN)
        made.append(o)
        # A nose, so the facing reads.
        bpy.ops.mesh.primitive_cone_add(radius1=0.025, depth=0.06, location=j["Head"] + Vector((0, 0.13, 0.08)), rotation=(-math.pi / 2, 0, 0))
        o = bpy.context.view_layer.objects.active
        o.data.materials.append(SKIN)
        made.append(o)
    return made


loaded = {}


def prop(p, made):
    if p["fbx"] not in loaded:
        before = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=p["fbx"])
        new = [o for o in bpy.data.objects if o not in before]
        keep = [o for o in new if o.type == "MESH" and o.name.endswith("_LOD0")]
        for o in new:
            if o not in keep:
                o.hide_render = True
        for o in keep:
            o.parent = None
            o.modifiers.clear()
            if not o.data.materials or o.data.materials[0] is None:
                o.data.materials.append(PROP)
        loaded[p["fbx"]] = keep
    for o in loaded[p["fbx"]]:
        # yaw turns the prop about up (0 points its forward along +z), pitch tips its nose up.
        o.matrix_world = (Matrix.Translation(e2b(p["pos"])) @ Matrix.Rotation(-math.radians(p.get("yaw", 0)), 4, "Z")
                          @ Matrix.Rotation(math.radians(p.get("pitch", 0)), 4, "X") @ Matrix.Rotation(math.radians(p.get("roll", 0)), 4, "Y"))


bpy.ops.mesh.primitive_plane_add(size=4, location=(0, 0, 0))
bpy.context.view_layer.objects.active.data.materials.append(FLOOR)
if job.get("wall_z") is not None:
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, job["wall_z"] + 0.1, 1.1))
    w = bpy.context.view_layer.objects.active
    w.scale = (2.4, 0.2, 2.2)
    w.data.materials.append(WALL)

cam_data = bpy.data.cameras.new("cam")
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
cam_data.type = "ORTHO"
cam_data.ortho_scale = job.get("span", 2.3)
scene.render.resolution_x, scene.render.resolution_y = 640, 640
centre = Vector((0, job.get("centre_z", 0.2), 0.95))
VIEWS = {"side": (-1, 0, 0.05), "front": (0, 1, 0.1), "quarter": (-0.75, 0.8, 0.45), "back": (0.5, -1, 0.35)}

for frame in job["frames"]:
    made = figure(frame["joints"])
    for p in frame.get("props", []):
        prop(p, made)
    for view in job.get("views", ["side", "front", "quarter"]):
        d = Vector(VIEWS[view]).normalized()
        cam.location = centre + d * 6
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = os.path.join(job["out"], "%s_%s.png" % (frame["label"], view))
        bpy.ops.render.render(write_still=True)
    for o in made:
        bpy.data.objects.remove(o, do_unlink=True)
print("LF_POSE_PREVIEW ok", job["out"])
