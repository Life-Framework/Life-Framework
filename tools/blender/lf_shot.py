# Headless pictures: blender --background [file.blend] --python lf_shot.py -- <job.json>
# job.json = [ { "out": "x.png", "view": "iso", "size": [w, h], "target": ["Name_LOD0"] }, ... ]
# Uses the same render code as the live bridge (lf_bridge/shot.py), with the Workbench engine.
import importlib.util
import json
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("lf_bridge_shot", os.path.join(here, "lf_bridge", "shot.py"))
shot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(shot)

jobs = json.load(open(sys.argv[sys.argv.index("--") + 1]))
for req in jobs:
    res = shot.run(req)
    if not res.get("ok"):
        raise SystemExit("shot %s: %s" % (req.get("out"), res.get("error")))
    print("LF_SHOT %s %s %d bytes" % (res["view"], res["out"], res["bytes"]))
