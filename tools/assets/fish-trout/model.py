# The first fish, a brown trout, modelled and textured in Blender (spec "source": "blender"), no credits.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# One mesh for all three sizes: the prefabs scale it. Lies long on Y, nose at -Y, tail at +Y, belly down.
# Material ids: 0.0 olive-brown back, 0.4 pale flank and belly, 0.7 fins, 1.0 eye.
import math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fishing-lib"))
import geo
from geo import piece, loft_y, prism, finish

BACK, FLANK, FIN, EYE = 0.0, 0.4, 0.7, 1.0
L = 0.30
# (t along the body, height as a fraction of the max): nose, head, shoulder, belly, thin tail stalk
PROFILE = [(0.0, 0.0), (0.02, 0.32), (0.08, 0.62), (0.2, 0.9), (0.38, 1.0), (0.58, 0.82), (0.78, 0.46), (0.92, 0.28), (1.0, 0.22)]
HMAX = 0.036


def h(t):
    for (t0, a), (t1, b) in zip(PROFILE, PROFILE[1:]):
        if t <= t1:
            return a + (b - a) * (t - t0) / (t1 - t0)
    return PROFILE[-1][1]


secs = []
for i in range(17):
    t = i / 16.0
    hh = HMAX * h(t)
    secs.append((t * L, hh * 0.62, hh, 0.0))
secs[0] = (0.0, 0.0, 0.0, 0.0)
secs[-1] = (L, 0.0, 0.0, 0.0)
# keep the end rings tiny but real so the cap faces exist
secs[0] = (-0.004, 0.003, 0.004, -0.004)
secs[-1] = (L + 0.002, 0.002, 0.008, 0.0)

body = piece(BACK)
loft_y(body, secs, seg=14, pid_of=lambda s: BACK if s > 0.2 else FLANK)

fins = piece(FIN)
prism(fins, [(L - 0.03, 0.01), (L + 0.062, 0.05), (L + 0.045, 0.0), (L + 0.062, -0.05), (L - 0.03, -0.01)], "x", -0.0015, 0.0015)    # tail
prism(fins, [(0.11, 0.034), (0.15, 0.062), (0.2, 0.06), (0.215, 0.026)], "x", -0.0013, 0.0013)                                   # dorsal
prism(fins, [(0.205, 0.008), (0.215, 0.026), (0.2, 0.03)], "x", -0.0013, 0.0013)                                                  # adipose stub
prism(fins, [(0.2, -0.026), (0.225, -0.05), (0.255, -0.04), (0.248, -0.016)], "x", -0.0013, 0.0013)                              # anal
for sx in (-1, 1):
    prism(fins, [(sx * 0.02, 0.075), (sx * 0.048, 0.092), (sx * 0.036, 0.1), (sx * 0.014, 0.088)], "z", -0.017, -0.0155)        # pectoral
    prism(fins, [(sx * 0.016, 0.135), (sx * 0.042, 0.152), (sx * 0.03, 0.158), (sx * 0.012, 0.146)], "z", -0.024, -0.0225)      # pelvic
eye = piece(EYE)
for sx in (-1, 1):
    geo.lathe_y(eye, sx * 0.0125, 0.012, [(0.026, 0.0), (0.026, 0.0035), (0.0325, 0.0035), (0.0325, 0.0)], seg=8)

finish("FishTrout", geo.job(), [
    (BACK, (0.13, 0.12, 0.05), 0.5, 0.0),
    (FLANK, (0.62, 0.6, 0.45), 0.4, 0.0),
    (FIN, (0.45, 0.17, 0.08), 0.55, 0.0),
    (EYE, (0.01, 0.01, 0.01), 0.2, 0.0),
], 0.7, center=True, tile=0.3)
