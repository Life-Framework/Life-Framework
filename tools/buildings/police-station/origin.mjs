// Where the villa stands in its grid claim (Grid_police_station_kit, c3r1 2x2): building space maps to the world by this.
// The kit itself holds no world coordinates; `town fit` replaces this with the real building's anchors. The number lives in
// spec.json check_layer.origin, so `tools\cli building check police-station` judges the layer rows in the same frame.
import { readFileSync } from "node:fs";
const spec = JSON.parse(readFileSync(new URL("./spec.json", import.meta.url), "utf8"));
const [x, z] = spec.check_layer.origin;
export const ORIGIN = { x, z };
