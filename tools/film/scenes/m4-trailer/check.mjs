// Dry run of the M4 trailer storyboard (#1292): parse it with the film tool's own validator, then walk every shot the way
// `film-make` would (selection, skip reason, harness camera command) without a session. Exit 1 on the first problem.
//   node tools/film/scenes/m4-trailer/check.mjs
// red-proof: 2026-11-14, run before m4-trailer.json existed: `FAIL ENOENT ... m4-trailer.json`, exit 1.
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { cameraCommands, selectShots, skipReason, validateStoryboard } from "../../storyboard.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const WANT = ["village", "heist", "chase", "ambulance", "quiz", "moderator", "recruits", "crowd"];
try {
  const b = JSON.parse(readFileSync(join(here, "m4-trailer.json"), "utf8"));
  validateStoryboard(b);
  const shots = selectShots(b).filter((s) => s.kind !== "title");
  const have = shots.map((s) => s.group);
  if (shots.length !== 8 || WANT.some((g, i) => have[i] !== g)) throw new Error(`want 8 shots in order ${WANT.join(", ")}; have ${have.join(", ")}`);
  for (const s of shots) {
    const why = skipReason(s);
    console.log(`${s.id} ${s.group.padEnd(9)} ${String(s.duration).padStart(2)}s  ${why ? `SKIP ${why}` : cameraCommands(s).join(" | ") || "(player view)"}`);
  }
  console.log(`m4-trailer ok: ${shots.length} shots, ${shots.filter((s) => !skipReason(s)).length} shootable now`);
} catch (e) {
  console.error(`FAIL ${e.message}`);
  process.exit(1);
}
