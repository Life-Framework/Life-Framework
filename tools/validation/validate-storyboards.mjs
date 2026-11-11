// Every video/*.json that declares a storyboard format must validate, and the storyboard module's own selftest must
// pass (a bad storyboard names its field). video/storyboard.schema.json must parse. Post-production boards (no
// `format`, a `clips` list) are checked by film-make itself.
//
// red-proof: 2026-11-11, a scratch `"duration": 0` in video/trailer-571.json printed
// `FAIL video/trailer-571.json: storyboard shots[0].duration: must be a number of seconds, at least 1` and exited 1.
import { readFileSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { selftestStoryboard, validateStoryboard } from "../film/storyboard.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const dir = join(root, "video");
const bad = selftestStoryboard().map((f) => `FAIL storyboard selftest: ${f}`);
try {
  JSON.parse(readFileSync(join(dir, "storyboard.schema.json"), "utf8"));
} catch (e) {
  bad.push(`FAIL video/storyboard.schema.json: ${e.message}`);
}
for (const f of readdirSync(dir).filter((n) => n.endsWith(".json") && n !== "storyboard.schema.json")) {
  try {
    const board = JSON.parse(readFileSync(join(dir, f), "utf8"));
    if (board.format) validateStoryboard(board);
  } catch (e) {
    bad.push(`FAIL video/${f}: ${e.message}`);
  }
}
if (bad.length) {
  console.error(bad.join("\n"));
  process.exit(1);
}
console.log("storyboards ok");
