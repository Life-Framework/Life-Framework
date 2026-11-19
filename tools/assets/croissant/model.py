# A butter croissant, modelled and textured in Blender (spec "source": "blender"), no generation credits.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# A crescent lying on Z: one fat tapered tube swept round an arc, cut into layered bands that alternate gold and
# darker gold. No text, no brand. Material ids: 0.0 light gold, 0.3 darker gold, 0.6 pale butter tips.
import math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fishing-lib"))
import geo
from geo import piece, sweep, finish
from mathutils import Vector

LIGHT, DARK, TIP = 0.0, 0.3, 0.6

ARC = 0.052          # radius of the crescent's centre line
SPAN = math.radians(150)
Z0 = 0.020


def point(a):
    return Vector((ARC * math.sin(a), ARC * (1.0 - math.cos(a)), Z0))


def radius(t):
    # 0 at the ends, 1 in the middle: a fat belly and pointed horns.
    return 0.0045 + 0.0165 * math.sin(math.pi * t) ** 0.8


N = 24
path, radii = [], []
for k in range(N + 1):
    t = k / N
    path.append(point(-SPAN / 2 + SPAN * t))
    radii.append(radius(t))
sweep(piece(LIGHT), path, 0.01, seg=10, radii=radii)

# Layer folds: thin darker rings a touch fatter than the body, and pale butter on the two horn tips.
for t0 in (0.22, 0.34, 0.46, 0.58, 0.70, 0.82):
    ring_path, ring_r = [], []
    for k in range(3):
        t = t0 + (k - 1) * 0.018
        ring_path.append(point(-SPAN / 2 + SPAN * t))
        ring_r.append(radius(t) * 1.05)
    sweep(piece(DARK), ring_path, 0.01, seg=10, radii=ring_r)
for t0, t1 in ((0.0, 0.10), (0.90, 1.0)):
    tip_path, tip_r = [], []
    for k in range(4):
        t = t0 + (t1 - t0) * k / 3.0
        tip_path.append(point(-SPAN / 2 + SPAN * t))
        tip_r.append(radius(t) * 1.04)
    sweep(piece(TIP), tip_path, 0.01, seg=10, radii=tip_r)

finish("Croissant", geo.job(), [
    (LIGHT, (0.80, 0.52, 0.20), 0.65, 0.0),
    (DARK, (0.64, 0.36, 0.11), 0.65, 0.0),
    (TIP, (0.88, 0.66, 0.34), 0.7, 0.0),
], 0.3, center=True, tile=0.2)
