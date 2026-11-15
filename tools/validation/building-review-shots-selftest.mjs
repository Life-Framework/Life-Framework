// Offline proof of tools\cli building review --shots (no Blender needed): which .blend it reads, the
// order of the contact sheet, and the colour scale it hands to Blender (the export's own numbers).
// The Blender side (tools/buildings/pipeline/blender/review_shots.py) is proven by running the command.
// red-proof: 2026-11-15, one line broken, restored after its FAIL was seen:
//   sheetTiles putting the plan first                    -> FAIL sheet: the plan comes last
//   albedoJob returning the interior scale for exterior  -> FAIL albedo: the same scales as the export
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { pathToFileURL } from "node:url";
import { albedoJob, reviewBlend, reviewShots, sheetCols, sheetTiles } from "../buildings/pipeline/review-shots.mjs";
import { ALBEDO_SCALE, OUTSIDE_SHARE } from "../buildings/pipeline/materials.mjs";

export async function main() {
  let failed = 0;
  const ok = (cond, what) => {
    if (cond) return;
    failed++;
    console.log(`FAIL ${what}`);
  };
  const tmp = mkdtempSync(join(tmpdir(), "lf-review-shots-"));
  try {
    const names = ["plan", "room2", "hall", "street_3q", "room1", "entrance", "street_front", "hall_back"];
    const tiles = sheetTiles({ shots: names.map((name) => ({ name, label: name, file: `${name}.png` })) });
    ok(tiles.map((t) => t.label).join() === "street_front,street_3q,entrance,hall,hall_back,room1,room2,plan", `sheet: outside, entrance, hall, rooms, then the plan last (${tiles.map((t) => t.label).join()})`);
    ok(tiles[tiles.length - 1].label === "plan", "sheet: the plan comes last");
    ok(sheetCols(5) === 3 && sheetCols(9) === 4, "sheet: three columns up to six tiles, four beyond");

    const a = albedoJob();
    ok(a.exterior === ALBEDO_SCALE.exterior && a.interior === ALBEDO_SCALE.interior && a.outside_share === OUTSIDE_SHARE && a.exterior > a.interior, "albedo: the same scales as the export gives its materials");

    mkdirSync(join(tmp, "tools", "buildings", "shop"), { recursive: true });
    ok(reviewBlend(tmp, "shop").endsWith(join("tmp", "buildings", "shop", "building.blend")), "blend: the generated building without a hand polish");
    writeFileSync(join(tmp, "tools", "buildings", "shop", "polished.blend"), "x");
    ok(reviewBlend(tmp, "shop").endsWith(join("tools", "buildings", "shop", "polished.blend")), "blend: the hand-polished scene wins");

    const lines = [];
    const code = await reviewShots(tmp, "nothing", { say: (l) => lines.push(l), setup: { exe: join(tmp, "no-blender.exe") } });
    ok(code === 1 && /no .*building\.blend/.test(lines.join("\n")), "missing blend: fails and says to generate first");
    const code2 = await reviewShots(tmp, "shop", { say: (l) => lines.push(l), setup: { exe: join(tmp, "no-blender.exe") } });
    ok(code2 === 0 && lines.some((l) => /^SKIP/.test(l)), "no Blender: skips, does not fail (CI)");
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }
  console.log(failed ? `building-review-shots-selftest: ${failed} FAILED` : "building-review-shots-selftest: OK");
  return failed ? 1 : 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
