# The 1989 smiling doll mask, modelled in Blender from the base body's own head (spec "mesh.build"): a cheap moulded
# white plastic face with a painted smile, rosy cheeks, painted brows and lashes. No generation credits, no downloaded
# model, nothing from any film: a doll's face from a toy shop. Run by the clothing fit stage:
#   blender --background --factory-startup --python model.py -- <job.json>
# job = { template_blend, out }. Writes <out>/doll.blend (objects DollMask and DollMask_item), DollMask_BCR.tif,
# DollMask_NMO.tif and review_0..2.png. The shared shell code is tools/assets/pipeline/blender/mask_shell.py.
import json, math, os, sys

argv = sys.argv
here = os.path.dirname(os.path.abspath(argv[argv.index("--python") + 1]))
sys.path.insert(0, os.path.join(here, "..", "pipeline", "blender"))
import numpy as np
import mask_shell as M

job = json.load(open(argv[argv.index("--") + 1]))


def inside(c):
    # a rounded oval from the chin to the hairline, ending in front of the ears
    if c.y > -0.004:
        return False
    nx, nz = c.x / 0.082, (c.z - 1.676) / 0.104
    return abs(nx) ** 2.8 + abs(nz) ** 2.8 < 1.0


def hole(x, z):
    # an almond opening a little bigger than each eye
    for ex, ez in M.EYES:
        nx, nz = (x - ex) / 0.021, (z - ez) / 0.0125
        if abs(nx) ** 2.4 + abs(nz) ** 2.0 < 1.0:
            return True
    return False


def paint():
    cv = M.Canvas(base_rgb=(0.28, 0.265, 0.23), rough=0.22)   # painted ivory plastic, glossy
    # moulding grain and the dull patches of a cheap paint job
    n = cv.n
    grain = M.value_noise(n, 8, 3) - 0.5
    patch = M.value_noise(n, 96, 5) - 0.5
    cv.rgb *= (1.0 + grain * 0.05 + patch * 0.10)[..., None]
    cv.rough += patch * 0.25 + grain * 0.05
    black, red, pink = (0.012, 0.011, 0.012), (0.30, 0.012, 0.02), (0.60, 0.17, 0.16)
    # rosy cheeks
    for s in (1, -1):
        cv.over(cv.ellipse(s * 0.046, 1.622, 0.019, 0.016, soft=0.007) * 0.85, pink)
    # a painted hairline of black curls over the forehead
    for k, x in enumerate((-0.055, -0.03, -0.005, 0.02, 0.045)):
        pts = [(x - 0.014, 1.757 + 0.002 * (k % 2)), (x, 1.769 + 0.003 * (k % 2)), (x + 0.014, 1.757 + 0.002 * (k % 2))]
        cv.over(cv.stroke(pts, 0.0035, taper=(0.6, 1.0)), black, 0.4)
    # eyebrows: high thin arches
    for s in (1, -1):
        pts = [(s * (0.012 + 0.040 * t), 1.722 + 0.012 * math.sin(math.pi * t) - 0.006 * t) for t in np.linspace(0, 1, 14)]
        cv.over(cv.stroke(pts, 0.0034, taper=(1.0, 0.45)), black, 0.35)
    # a black rim round each eye hole and three lashes flicking outwards
    for ex, ez in M.EYES:
        s = 1 if ex > 0 else -1
        d = np.sqrt(((cv.X - ex) / 0.021) ** 2 + ((cv.Z - ez) / 0.0125) ** 2)
        cv.over(1 - M.smooth(1.0, 1.18, d), black, 0.4)
        for k in range(3):
            a = math.radians(20 + 22 * k)
            base = (ex + s * 0.020 * math.cos(math.radians(20 + 22 * k) * 0.3), ez + 0.010 * math.sin(a))
            tip = (base[0] + s * 0.012 * math.cos(a * 0.6), base[1] + 0.010 * math.sin(a) + 0.002)
            cv.over(cv.stroke([base, tip], 0.0028, taper=(1.0, 0.3)), black, 0.4)
    # the smile: a wide red curve with upturned corners, and a dimple tick at each end
    pts = [(-0.030 + 0.060 * t, 1.607 - 0.017 * math.sin(math.pi * t)) for t in np.linspace(0, 1, 24)]
    cv.over(cv.stroke(pts, 0.0048, taper=(0.5, 0.5)), red, 0.18)
    upper = [(-0.026 + 0.052 * t, 1.609 - 0.010 * math.sin(math.pi * t)) for t in np.linspace(0, 1, 18)]
    cv.over(cv.stroke(upper, 0.0026, taper=(0.5, 0.5)), red, 0.18)
    for s in (1, -1):
        cv.over(cv.stroke([(s * 0.031, 1.6085), (s * 0.036, 1.612)], 0.0026), black, 0.4)
    # nose: a small pink tip and two nostril dots
    cv.over(cv.ellipse(0.0, 1.647, 0.007, 0.005, soft=0.002) * 0.9, pink, 0.2)
    for s in (1, -1):
        cv.over(cv.ellipse(s * 0.0045, 1.640, 0.0016, 0.0012, soft=0.0008), black, 0.4)
    # wear: scuffs on the nose and brow, a darker edge round the whole oval
    scuff = np.clip((M.value_noise(n, 12, 9) - 0.62) * 4, 0, 1)
    ridge = cv.ellipse(0.0, 1.655, 0.020, 0.040, soft=0.02) + cv.ellipse(0.0, 1.735, 0.07, 0.012, soft=0.01)
    cv.over(scuff * np.clip(ridge, 0, 1) * 0.5, (0.62, 0.60, 0.55), 0.6)
    edge = np.clip(1 - ((np.abs(cv.X / 0.082) ** 2.8 + np.abs((cv.Z - 1.676) / 0.104) ** 2.8) ** (1 / 2.8)), 0, 1)
    cv.over(1 - M.smooth(0.0, 0.05, edge), (0.30, 0.28, 0.24), 0.5)
    # the elastic strap patch, near black
    px = int(M.STRAP_UV[0] * n)
    cv.rgb[:, : px + 24] = (0.020, 0.020, 0.022)
    cv.rough[:, : px + 24] = 0.85
    cv.height[:] = M.blur(cv.height, 1)
    return cv


M.build(job, {
    "name": "DollMask",
    "blend": "dollmask.blend",
    "material": "DollMask",
    "item_material": "DollMask_item",
    "inside": inside,
    "hole": hole,
    "smooth_iter": 8,
    "tris": 1500,
    "offset": 0.007,
    "gaps": (0.0055, 0.002),
    "smooth_after": 8,
    "thick": 0.0028,
    "strap": {"z": 1.722, "half": 0.0105, "y_start": -0.016, "offset": 0.0105, "thick": 0.002},
    "paint": paint,
})
