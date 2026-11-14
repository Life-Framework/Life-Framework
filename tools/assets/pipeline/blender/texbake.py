# Shared helpers for a model or texture script that bakes its own PBR maps (spec "source": "blender",
# docs/asset-pipeline/README.md "Modelled in Blender"). A script builds a node recipe from downloaded
# CC0 texture sets and procedural masks, and bake() writes the four maps the clean stage reads:
#   tex_base_color.png (sRGB), tex_roughness.png, tex_metallic.png, tex_normal.png (OpenGL, tangent space)
# The material step (materials.py) remaps roughness and metal by class afterwards; a recipe only says
# where each material is.
import bpy, glob, math, os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


class Sets:
    """CC0 sets unpacked by the generate stage under <src>/<AssetId>/: <AssetId>_1K-JPG_<Map>.jpg."""

    def __init__(self, src):
        self.src = src

    def file(self, asset, channel):
        hits = glob.glob(os.path.join(self.src, asset, "%s_*_%s.jpg" % (asset, channel)))
        if not hits:
            raise SystemExit("texture %s %s not found under %s; rerun the generate stage" % (asset, channel, self.src))
        return hits[0]


def cycles(samples=16):
    s = bpy.context.scene
    s.render.engine = "CYCLES"
    s.cycles.device = "CPU"
    s.cycles.samples = samples
    s.render.bake.margin = 8
    s.render.bake.use_clear = True
    return s


class Graph:
    """Builds one material's node tree. Coordinates are object space in metres divided by `tile`:
    a 1K set covers `tile` metres, so texel density is the same on every face of every object."""

    def __init__(self, name, sets, tile=0.25, coords="Object"):
        self.mat = bpy.data.materials.new(name)
        self.mat.use_nodes = True
        self.nt = self.mat.node_tree
        self.nt.nodes.clear()
        self.sets = sets
        tc = self.node("ShaderNodeTexCoord")
        mp = self.node("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (1 / tile, 1 / tile, 1 / tile)
        self.link(tc.outputs[coords], mp.inputs["Vector"])
        self.vec = mp.outputs["Vector"]
        self.raw = tc.outputs[coords]
        self.cache = {}

    def node(self, kind, **props):
        n = self.nt.nodes.new(kind)
        for k, v in props.items():
            setattr(n, k, v)
        return n

    def link(self, a, b):
        self.nt.links.new(a, b)

    def img(self, asset, channel, data=True, vec=None):
        key = (asset, channel, id(vec))
        if key in self.cache:
            return self.cache[key]
        n = self.node("ShaderNodeTexImage")
        n.image = bpy.data.images.load(self.sets.file(asset, channel), check_existing=True)
        n.image.colorspace_settings.name = "Non-Color" if data else "sRGB"
        if vec is None:
            n.projection = "BOX"
            n.projection_blend = 0.3
        self.link(vec or self.vec, n.inputs["Vector"])
        out = n.outputs["Color"]
        if channel != "Color":
            sep = self.node("ShaderNodeSeparateColor")
            self.link(out, sep.inputs["Color"])
            out = sep.outputs[0]
        self.cache[key] = out
        return out

    def value(self, v):
        n = self.node("ShaderNodeValue")
        n.outputs[0].default_value = v
        return n.outputs[0]

    def rgb(self, r, g, b):
        n = self.node("ShaderNodeRGB")
        n.outputs[0].default_value = (r, g, b, 1)
        return n.outputs[0]

    def _sock(self, x, kind):
        if isinstance(x, (int, float)):
            return self.value(float(x)) if kind == "FLOAT" else self.rgb(x, x, x)
        if isinstance(x, tuple):
            return self.rgb(*x)
        return x

    def mix(self, a, b, fac, kind="RGBA", blend="MIX"):
        n = self.node("ShaderNodeMix", data_type=kind)
        if kind == "RGBA":
            n.blend_type = blend
        n.clamp_factor = True
        ins = [s for s in n.inputs if s.name in ("A", "B") and s.type == ("RGBA" if kind == "RGBA" else "VALUE")]
        self.link(self._sock(fac, "FLOAT"), n.inputs["Factor"])
        self.link(self._sock(a, kind), ins[0])
        self.link(self._sock(b, kind), ins[1])
        return [s for s in n.outputs if s.type == ("RGBA" if kind == "RGBA" else "VALUE")][0]

    def math(self, op, a, b=0.0, clamp=False):
        n = self.node("ShaderNodeMath", operation=op, use_clamp=clamp)
        self.link(self._sock(a, "FLOAT"), n.inputs[0])
        self.link(self._sock(b, "FLOAT"), n.inputs[1])
        return n.outputs[0]

    def ramp(self, v, lo, hi):
        """0 below lo, 1 above hi, smooth between: a coverage mask from a noise value."""
        n = self.node("ShaderNodeMapRange", interpolation_type="SMOOTHSTEP")
        self.link(self._sock(v, "FLOAT"), n.inputs["Value"])
        n.inputs["From Min"].default_value = lo
        n.inputs["From Max"].default_value = hi
        return n.outputs["Result"]

    def noise(self, scale, detail=4.0, rough=0.55, vec=None, w=None, distortion=0.0):
        n = self.node("ShaderNodeTexNoise", noise_dimensions="4D" if w is not None else "3D")
        n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail
        n.inputs["Roughness"].default_value = rough
        n.inputs["Distortion"].default_value = distortion
        if w is not None:
            self.link(self._sock(w, "FLOAT"), n.inputs["W"])
        self.link(vec or self.raw, n.inputs["Vector"])
        return n.outputs["Fac"]

    def voronoi(self, scale, vec=None, randomness=1.0, w=None):
        n = self.node("ShaderNodeTexVoronoi", feature="F1", voronoi_dimensions="4D" if w is not None else "3D")
        if w is not None:
            self.link(self._sock(w, "FLOAT"), n.inputs["W"])
        n.inputs["Scale"].default_value = scale
        n.inputs["Randomness"].default_value = randomness
        self.link(vec or self.raw, n.inputs["Vector"])
        return n.outputs["Distance"]

    def torus(self, radius):
        """A 4D point on a torus over the UV square: noise fed with it (noise(..., vec=v, w=w)) repeats
        exactly once per UV tile, so a texture baked on a plane tiles without a seam."""
        uv = self.node("ShaderNodeTexCoord").outputs["UV"]
        sep = self.node("ShaderNodeSeparateXYZ")
        self.link(uv, sep.inputs[0])
        ang = [self.math("MULTIPLY", sep.outputs[i], 2 * math.pi) for i in (0, 1)]
        trig = [self.math("MULTIPLY", self.math(op, a), radius) for a in ang for op in ("COSINE", "SINE")]
        comb = self.node("ShaderNodeCombineXYZ")
        for i in range(3):
            self.link(trig[i], comb.inputs[i])
        return comb.outputs[0], trig[3]

    def uv_tiles(self, k):
        """UV times a whole number: a tiling set sampled this way still tiles on the UV square."""
        uv = self.node("ShaderNodeTexCoord").outputs["UV"]
        mp = self.node("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (k, k, 1)
        self.link(uv, mp.inputs["Vector"])
        return mp.outputs["Vector"]

    def ridge(self, v, width):
        """Thin bright lines where a noise value crosses 0.5: veins."""
        d = self.math("ABSOLUTE", self.math("SUBTRACT", v, 0.5))
        return self.ramp(self.math("SUBTRACT", width, d), 0.0, width * 0.6)

    def scale_color(self, c, k):
        return self.mix(c, (k[0], k[1], k[2]) if isinstance(k, tuple) else (k, k, k), 1.0, blend="MULTIPLY")

    def pointiness(self, lo, hi):
        g = self.node("ShaderNodeNewGeometry")
        return self.ramp(g.outputs["Pointiness"], lo, hi)

    def ao(self, distance):
        n = self.node("ShaderNodeAmbientOcclusion", samples=16)
        n.inputs["Distance"].default_value = distance
        return n.outputs["AO"]


def _emit_out(g, sock):
    for n in [n for n in g.nt.nodes if n.type in ("OUTPUT_MATERIAL", "EMISSION", "BSDF_PRINCIPLED", "BUMP")]:
        g.nt.nodes.remove(n)
    out = g.node("ShaderNodeOutputMaterial")
    em = g.node("ShaderNodeEmission")
    g.link(sock, em.inputs["Color"])
    g.link(em.outputs[0], out.inputs["Surface"])


def _normal_out(g, height, strength, distance):
    for n in [n for n in g.nt.nodes if n.type in ("OUTPUT_MATERIAL", "EMISSION", "BSDF_PRINCIPLED", "BUMP")]:
        g.nt.nodes.remove(n)
    out = g.node("ShaderNodeOutputMaterial")
    bsdf = g.node("ShaderNodeBsdfPrincipled")
    bump = g.node("ShaderNodeBump")
    bump.inputs["Strength"].default_value = strength
    bump.inputs["Distance"].default_value = distance
    g.link(g._sock(height, "FLOAT"), bump.inputs["Height"])
    g.link(bump.outputs["Normal"], bsdf.inputs["Normal"])
    g.link(bsdf.outputs[0], out.inputs["Surface"])


def bake(obj, g, recipe, px, outdir):
    """recipe: {base, rough, metal, height, bump_strength, bump_distance, ao?}. Writes the four maps, and tex_ao.png when the recipe has an ao."""
    os.makedirs(outdir, exist_ok=True)
    obj.data.materials.clear()
    obj.data.materials.append(g.mat)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    target = g.node("ShaderNodeTexImage")
    files = {}
    keys = [("base_color", False), ("roughness", True), ("metallic", True), ("normal", True)]
    if recipe.get("ao") is not None:
        keys.append(("ao", True))  # the NMO alpha: occlusion in the recesses (clean.py packs it)
    for key, data in keys:
        im = bpy.data.images.new("%s_%s" % (g.mat.name, key), px, px, alpha=False, is_data=data)
        target.image = im
        g.nt.nodes.active = target
        target.select = True
        if key == "normal":
            _normal_out(g, recipe["height"], recipe.get("bump_strength", 1.0), recipe.get("bump_distance", 0.002))
            bpy.ops.object.bake(type="NORMAL", normal_space="TANGENT", use_clear=True, margin=8)
        else:
            sock = {"base_color": recipe["base"], "roughness": recipe["rough"], "metallic": recipe["metal"], "ao": recipe.get("ao")}[key]
            if key != "base_color":
                comb = g.node("ShaderNodeCombineColor")
                for i in range(3):
                    g.link(g._sock(sock, "FLOAT"), comb.inputs[i])
                sock = comb.outputs[0]
            _emit_out(g, g._sock(sock, "RGBA"))
            bpy.ops.object.bake(type="EMIT", use_clear=True, margin=8)
        path = os.path.join(outdir, "tex_%s.png" % key)
        im.filepath_raw = path
        im.file_format = "PNG"
        im.save()
        files[key] = path
    return files


def preview_material(name, files):
    """A plain Principled material on the baked maps, for the review renders."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    def tex(key, data):
        n = nt.nodes.new("ShaderNodeTexImage")
        n.image = bpy.data.images.load(files[key], check_existing=False)
        n.image.colorspace_settings.name = "Non-Color" if data else "sRGB"
        return n
    nt.links.new(tex("base_color", False).outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(tex("roughness", True).outputs["Color"], bsdf.inputs["Roughness"])
    nt.links.new(tex("metallic", True).outputs["Color"], bsdf.inputs["Metallic"])
    nm = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(tex("normal", True).outputs["Color"], nm.inputs["Color"])
    nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def review_scene(target, distance, samples=48, res=512):
    """Camera, sun and a grey sky aimed at `target` (a Vector), for preview renders."""
    s = cycles(samples)
    s.render.resolution_x = s.render.resolution_y = res
    s.render.film_transparent = False
    s.render.image_settings.file_format = "PNG"
    s.render.image_settings.color_mode = "RGB"
    s.render.image_settings.color_depth = "8"
    w = bpy.data.worlds.new("w")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs["Color"].default_value = (0.42, 0.45, 0.5, 1)
    w.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.8
    s.world = w
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(-35))
    s.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    s.collection.objects.link(cam)
    s.camera = cam
    from mathutils import Vector
    d = Vector((-0.8, -1.0, 0.75)).normalized()
    cam.location = target + d * distance
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 70
    return s


def fresh_scene_from(glb):
    """An empty scene holding only the exported model: the review shows what the clean stage imports."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=glb)
    return [o for o in bpy.data.objects if o.type == "MESH"][0]


def render(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def _save(path, arr, data):
    """Write a top-down (px, px, 3) float array as a PNG."""
    px = arr.shape[0]
    rgba = np.ones((px, px, 4), np.float32)
    rgba[:, :, :3] = arr[:, :, :3]
    im = bpy.data.images.new(os.path.basename(path), px, px, alpha=False, is_data=data)
    im.pixels.foreach_set(rgba[::-1].ravel())
    im.filepath_raw = path
    im.file_format = "PNG"
    im.save()
    bpy.data.images.remove(im)
    return path


def remapped_material(name, folder, plan, px):
    """A Principled material on the maps after the material step: what the game will get. The
    remapped maps are written to <folder>/remapped/ and loaded back like any baked map."""
    import variants
    bcr, nmo, _ = variants.pack(folder, plan, px)
    nx, ny = nmo[:, :, 0] * 2 - 1, (1 - nmo[:, :, 1]) * 2 - 1
    nz = np.sqrt(np.clip(1 - nx * nx - ny * ny, 0, 1))
    grey = lambda a: np.dstack([a, a, a])
    out = os.path.join(folder, "remapped")
    os.makedirs(out, exist_ok=True)
    files = {
        "base_color": _save(os.path.join(out, "tex_base_color.png"), bcr[:, :, :3], False),
        "roughness": _save(os.path.join(out, "tex_roughness.png"), grey(bcr[:, :, 3]), True),
        "metallic": _save(os.path.join(out, "tex_metallic.png"), grey(nmo[:, :, 2]), True),
        "normal": _save(os.path.join(out, "tex_normal.png"), np.dstack([nmo[:, :, 0], 1 - nmo[:, :, 1], nz * 0.5 + 0.5]), True),
    }
    return preview_material(name, files)
