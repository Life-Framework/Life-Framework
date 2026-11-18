# The fishing rod, modelled and textured in Blender (spec "source": "blender"), no generation credits.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# A 1989 angler's rod: a dark green fibreglass blank with red thread wraps and wire guides, a cork handle,
# a steel reel seat and a fixed-spool reel hung under the handle. Lies long on Y, butt at -Y, tip at +Y.
# Material ids: 0.0 cork, 0.2 blank, 0.4 thread, 0.6 steel, 0.8 reel enamel.
import math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fishing-lib"))
import geo
from geo import piece, block, lathe_y, sweep, finish
from mathutils import Vector

L = 1.2
CORK, BLANK, THREAD, STEEL, ENAMEL = 0.0, 0.2, 0.4, 0.6, 0.8


def bow(y):
    t = max(0.0, (y - 0.3) / (L - 0.3))
    return 0.05 * t * t          # the tip bends up under its own weight


cork = piece(CORK)
lathe_y(cork, 0, 0, [(0.0, 0.0), (0.004, 0.0105), (0.03, 0.0125), (0.16, 0.0135), (0.2, 0.0125), (0.205, 0.0)], seg=12)
steel = piece(STEEL)
lathe_y(steel, 0, 0, [(0.2, 0.0), (0.2, 0.0145), (0.27, 0.0145), (0.272, 0.0125), (0.272, 0.0)], seg=12)       # reel seat
lathe_y(steel, 0, 0, [(-0.002, 0.0), (-0.002, 0.0112), (0.0, 0.0112), (0.0, 0.0)], seg=12)                      # butt cap

blank = piece(BLANK)
ys = [0.272 + (L - 0.272) * i / 14 for i in range(15)]
path = [Vector((0, y, bow(y))) for y in ys]
radii = [0.0105 - (0.0105 - 0.0022) * (i / 14) ** 0.8 for i in range(15)]
sweep(blank, path, 0, seg=10, radii=radii)

thread = piece(THREAD)
guide = piece(STEEL)
for gy in (0.42, 0.56, 0.7, 0.82, 0.92, 1.0, 1.08, 1.15):
    i = min(14, max(0, int((gy - 0.272) / (L - 0.272) * 14)))
    r = radii[i]
    z = bow(gy)
    lathe_y(thread, 0, z, [(gy - 0.007, 0.0), (gy - 0.007, r + 0.0007), (gy + 0.007, r + 0.0007), (gy + 0.007, 0.0)], seg=10)
    ring_r = 0.0055 + 0.0035 * (1 - (gy - 0.42) / 0.8)
    ring = [Vector((ring_r * math.sin(a), gy, z + r + ring_r + ring_r * math.cos(a))) for a in [2 * math.pi * k / 8 for k in range(9)]]
    sweep(guide, ring, 0.0007, seg=4)
tip = piece(STEEL)
lathe_y(tip, 0, bow(L), [(L - 0.004, 0.0), (L - 0.004, 0.0034), (L + 0.003, 0.0034), (L + 0.006, 0.0)], seg=8)

reel = piece(ENAMEL)
RZ = -0.047
lathe_y(reel, 0, RZ, [(0.2, 0.0), (0.2, 0.021), (0.245, 0.021), (0.26, 0.024), (0.292, 0.024), (0.295, 0.0)], seg=16)   # spool
geo.block(reel, -0.012, 0.012, 0.215, 0.255, RZ + 0.018, -0.018, bevel=0.002)                                          # body
foot = piece(STEEL)
block(foot, -0.004, 0.004, 0.218, 0.25, RZ + 0.02, -0.012, bevel=0.0008)                                               # reel foot
block(foot, -0.005, 0.005, 0.215, 0.255, -0.014, -0.0105, bevel=0.0008)
crank = piece(STEEL)
block(crank, 0.021, 0.07, 0.205, 0.213, RZ - 0.003, RZ + 0.003, bevel=0.0008)
knob = piece(ENAMEL)
lathe_y(knob, 0.07, RZ, [(0.195, 0.0), (0.195, 0.007), (0.222, 0.007), (0.222, 0.0)], seg=10)

finish("FishingRod", geo.job(), [
    (CORK, (0.5, 0.36, 0.2), 0.85, 0.0),
    (BLANK, (0.035, 0.09, 0.05), 0.3, 0.0),
    (THREAD, (0.45, 0.04, 0.03), 0.7, 0.0),
    (STEEL, (0.3, 0.3, 0.32), 0.4, 0.9),
    (ENAMEL, (0.03, 0.04, 0.05), 0.4, 0.4),
], 1.8, center=True, tile=0.5)
