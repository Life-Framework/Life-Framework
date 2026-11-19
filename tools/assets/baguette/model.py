# A French baguette, modelled and textured in Blender (spec "source": "blender"), no generation credits.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# A long, slightly flattened loaf along Y with a golden crust and five diagonal pale scores. No text, no brand.
# Material ids: 0.0 crust, 0.3 score (pale crumb showing through the cuts), 0.6 darker baked tips.
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fishing-lib"))
import geo
from geo import piece, loft_y, prism, finish

CRUST, SCORE, TIP = 0.0, 0.3, 0.6

# (y, half width, half height, z centre): rounded blunt ends, 62 cm long, the middle the fattest.
sections = [
    (-0.310, 0.0020, 0.0020, 0.0),
    (-0.300, 0.0120, 0.0100, 0.0),
    (-0.270, 0.0190, 0.0160, 0.0),
    (-0.200, 0.0240, 0.0200, 0.0),
    (0.000, 0.0260, 0.0220, 0.0),
    (0.200, 0.0240, 0.0200, 0.0),
    (0.270, 0.0190, 0.0160, 0.0),
    (0.300, 0.0120, 0.0100, 0.0),
    (0.310, 0.0020, 0.0020, 0.0),
]
body = piece(CRUST)
loft_y(body, sections, seg=12)

# Five slanted scores on the top, each a raised pale parallelogram.
for k in range(5):
    y = -0.16 + 0.08 * k
    score = piece(SCORE)
    prism(score, [(-0.011, y - 0.016), (0.011, y + 0.004), (0.011, y + 0.016), (-0.011, y - 0.004)], "z", 0.0166, 0.0232)

geo_job = geo.job()
finish("Baguette", geo_job, [
    (CRUST, (0.62, 0.38, 0.14), 0.7, 0.0),
    (SCORE, (0.86, 0.70, 0.42), 0.8, 0.0),
    (TIP, (0.46, 0.26, 0.09), 0.7, 0.0),
], 0.5, center=True, tile=0.25)
