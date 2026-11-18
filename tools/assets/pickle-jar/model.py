# A jar of pickled gherkins, modelled and textured in Blender (spec "source": "blender"), no generation credits.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# A squat glass jar: pickle-green body, one plain cream paper label, a gold screw lid. No text, no brand.
# Stands on Z, base at the bottom. Material ids: 0.0 pickle green, 0.3 label, 0.9 gold lid.
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fishing-lib"))
import geo
from geo import piece, lathe_z, finish

GREEN, LABEL, LID = 0.0, 0.3, 0.9

body = piece(GREEN)
lathe_z(body, 0, 0, [(0.0, 0.0), (0.0, 0.038), (0.004, 0.043), (0.118, 0.043), (0.128, 0.036), (0.131, 0.0335), (0.131, 0.0)], seg=22)
label = piece(LABEL)
lathe_z(label, 0, 0, [(0.034, 0.0), (0.034, 0.0436), (0.094, 0.0436), (0.094, 0.0)], seg=22)
lid = piece(LID)
lathe_z(lid, 0, 0, [(0.128, 0.0), (0.128, 0.0372), (0.1425, 0.0378), (0.1445, 0.034), (0.1445, 0.0)], seg=22)

finish("PickleJar", geo.job(), [
    (GREEN, (0.20, 0.30, 0.06), 0.15, 0.0),
    (LABEL, (0.86, 0.80, 0.62), 0.65, 0.0),
    (LID, (0.78, 0.60, 0.14), 0.4, 0.8),
], 0.4, center=True, tile=0.15)
