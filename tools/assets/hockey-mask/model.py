# The 1989 cracked hockey mask, modelled in Blender from the base body's own head (spec "mesh.build"): the moulded
# fibreglass face of a goalie's practice mask gone yellow, with narrow eye slits, drilled vent holes, a faded blue
# stripe, and cracks. No generation credits, no downloaded model, no film markings: a rink-bag mask from a thrift
# store. Run by the clothing fit stage:
#   blender --background --factory-startup --python model.py -- <job.json>
# job = { template_blend, out }. Writes <out>/hockeymask.blend (objects HockeyMask and HockeyMask_item),
# HockeyMask_BCR.tif, HockeyMask_NMO.tif and review_0..2.png. The shared shell code is
# tools/assets/pipeline/blender/mask_shell.py.
import json, math, os, sys

argv = sys.argv
here = os.path.dirname(os.path.abspath(argv[argv.index("--python") + 1]))
sys.path.insert(0, os.path.join(here, "..", "pipeline", "blender"))
import numpy as np
import mask_shell as M

job = json.load(open(argv[argv.index("--") + 1]))


def inside(c):
    # a squarer shell than a doll's: straight sides, a flat brow line, and it reaches a little further round the jaw
    if c.y > 0.0:
        return False
    nx, nz = c.x / 0.083, (c.z - 1.677) / 0.106
    return abs(nx) ** 5.0 + abs(nz) ** 5.0 < 1.0


def hole(x, z):
    # a narrow slit a little wider than each eye
    for ex, ez in M.EYES:
        nx, nz = (x - ex) / 0.025, (z - ez) / 0.0075
        if abs(nx) ** 3.5 + abs(nz) ** 2.5 < 1.0:
            return True
    return False


def crack(rng, start, end, branches=2, jitter=0.004, steps=9):
    """A wandering polyline from start to end (x, z in metres) plus a few short side cracks."""
    lines = []
    pts = [start]
    for k in range(1, steps):
        t = k / steps
        pts.append((start[0] + (end[0] - start[0]) * t + rng.normal(0, jitter), start[1] + (end[1] - start[1]) * t + rng.normal(0, jitter)))
    pts.append(end)
    lines.append(pts)
    for _ in range(branches):
        i = int(rng.integers(2, steps - 2))
        ang = rng.uniform(-1.2, 1.2) + math.atan2(end[1] - start[1], end[0] - start[0])
        ln = rng.uniform(0.008, 0.02)
        b = [pts[i], (pts[i][0] + ln * 0.5 * math.cos(ang) + rng.normal(0, 0.002), pts[i][1] + ln * 0.5 * math.sin(ang) + rng.normal(0, 0.002)),
             (pts[i][0] + ln * math.cos(ang), pts[i][1] + ln * math.sin(ang))]
        lines.append(b)
    return lines


def paint():
    cv = M.Canvas(base_rgb=(0.26, 0.235, 0.15), rough=0.34)   # yellowed white fibreglass, a little glossy
    n = cv.n
    grain = M.value_noise(n, 8, 11) - 0.5
    patch = M.value_noise(n, 80, 12) - 0.5
    stain = M.value_noise(n, 200, 13) - 0.5
    cv.rgb *= (1.0 + grain * 0.04 + patch * 0.16)[..., None]
    cv.rgb[..., 2] *= 1.0 - 0.18 * np.clip(stain + 0.3, 0, 1)    # age: yellower in blotches
    cv.rough += patch * 0.2
    black, blue = (0.010, 0.010, 0.011), (0.02, 0.045, 0.13)
    rng = np.random.default_rng(1989)
    # the moulding seam down the middle, a hair lighter
    cv.over(cv.stroke([(0.0, 1.570), (0.0, 1.785)], 0.0016, soft=0.0008), (0.62, 0.60, 0.50), 0.5)
    # faded blue stripes across the brow, and a thin one down the nose bridge
    for z, h in ((1.748, 0.0065), (1.733, 0.0032)):
        cv.over(cv.rect(0.0, z, 0.058, h, r=0.0012) * (0.75 + 0.25 * np.clip(patch + 0.5, 0, 1)), blue, 0.5)
    cv.over(cv.rect(0.0, 1.698, 0.0035, 0.024, r=0.001) * 0.7, blue, 0.5)
    # drilled vent holes: three columns of three on each cheek, two on the chin
    for s in (1, -1):
        for cx in (0.041, 0.053, 0.065):
            for cz in (1.640, 1.625, 1.610):
                cv.over(cv.ellipse(s * cx, cz, 0.0034, 0.0034, soft=0.0006), black, 0.5)
    for x in (-0.009, 0.009):
        cv.over(cv.ellipse(x, 1.590, 0.0032, 0.0032, soft=0.0006), black, 0.5)
    # a black band round each slit
    for ex, ez in M.EYES:
        d = (np.abs((cv.X - ex) / 0.025) ** 3.5 + np.abs((cv.Z - ez) / 0.0075) ** 2.5) ** 0.3
        cv.over(1 - M.smooth(1.0, 1.9, d), black, 0.5)
    # grime running down from each slit
    for ex, ez in M.EYES:
        cv.over(cv.stroke([(ex - 0.004, ez - 0.008), (ex - 0.006, ez - 0.032)], 0.006, soft=0.004) * 0.35, (0.14, 0.11, 0.07), 0.7)
    # cracks: a long one from the brow into the left slit, one from the right cheek edge, one under the chin line
    crack_alpha = np.zeros((n, n))
    for start, end in (((-0.030, 1.776), (-0.018, 1.706)), ((0.075, 1.640), (0.046, 1.596)), ((0.012, 1.585), (0.030, 1.574)), ((0.050, 1.742), (0.066, 1.724))):
        for line in crack(rng, start, end):
            crack_alpha = np.maximum(crack_alpha, cv.stroke(line, 0.0013, soft=0.0005, taper=(1.0, 0.5)))
    cv.over(crack_alpha, black, 0.5, height=-1.0)
    # chips: a few paint losses showing darker fibreglass
    for _ in range(14):
        cx, cz = rng.uniform(-0.07, 0.07), rng.uniform(1.58, 1.77)
        cv.over(cv.ellipse(cx, cz, rng.uniform(0.002, 0.005), rng.uniform(0.002, 0.004), soft=0.0008) * 0.8, (0.22, 0.20, 0.14), 0.7)
    edge = np.clip(1 - ((np.abs(cv.X / 0.083) ** 5.0 + np.abs((cv.Z - 1.677) / 0.106) ** 5.0) ** (1 / 5.0)), 0, 1)
    cv.over(1 - M.smooth(0.0, 0.05, edge), (0.20, 0.18, 0.13), 0.55)
    # the elastic strap patch, near black
    px = int(M.STRAP_UV[0] * n)
    cv.rgb[:, : px + 24] = (0.020, 0.020, 0.022)
    cv.rough[:, : px + 24] = 0.85
    cv.height[:] = M.blur(cv.height, 1)
    return cv


M.build(job, {
    "name": "HockeyMask",
    "blend": "hockeymask.blend",
    "material": "HockeyMask",
    "item_material": "HockeyMask_item",
    "inside": inside,
    "hole": hole,
    "smooth_iter": 8,
    "tris": 1500,
    "offset": 0.007,
    "gaps": (0.0055, 0.002),
    "smooth_after": 8,
    "thick": 0.0032,
    "strap": {"z": 1.722, "half": 0.0105, "y_start": -0.016, "offset": 0.0105, "thick": 0.002},
    "paint": paint,
})
