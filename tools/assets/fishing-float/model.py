# The fishing float, modelled and textured in Blender (spec "source": "blender"), no generation credits.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# A classic red-and-white round bobber on a thin quill: the white lower half sits in the water, the red top half
# and the quill show, so a dip is easy to see. Stands on Z. Material ids: 0.0 red, 0.45 white, 0.8 quill.
import math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fishing-lib"))
import geo
from geo import piece, lathe_z, finish

RED, WHITE, QUILL = 0.0, 0.45, 0.8
R = 0.022          # ball radius
ZB = 0.03          # ball centre height (the stem below it, the quill above)


def ball_profile(z0, z1, steps):
    pts = []
    for i in range(steps + 1):
        a = math.radians(z0 + (z1 - z0) * i / steps)   # polar angle 0 (top) .. 180 (bottom)
        pts.append((ZB + R * math.cos(a), R * math.sin(a)))
    return pts


# lathe_z wants (z, radius) from low z up; the lower half is white, the upper red
lower = piece(WHITE)
lathe_z(lower, 0, 0, [(0.0, 0.0035), (0.006, 0.004)] + list(reversed(ball_profile(90, 155, 5))), seg=16)
upper = piece(RED)
lathe_z(upper, 0, 0, list(reversed(ball_profile(5, 90, 6))), seg=16)
quill = piece(QUILL)
lathe_z(quill, 0, 0, [(ZB + R - 0.003, 0.0045), (ZB + R + 0.012, 0.0035), (ZB + R + 0.042, 0.0025), (ZB + R + 0.043, 0.0)], seg=10)

finish("FishingFloat", geo.job(), [
    (RED, (0.62, 0.05, 0.04), 0.4, 0.0),
    (WHITE, (0.82, 0.8, 0.74), 0.4, 0.0),
    (QUILL, (0.55, 0.42, 0.22), 0.6, 0.0),
], 0.3, center=True, tile=0.1)
