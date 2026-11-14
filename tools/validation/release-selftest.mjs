// The playtest download: zip writer, addon file list (no generated files),
// inputs: tools/**, agent-box/**, .githooks/**, addons/**, docs/**, website/**, video/**
// launcher contents, and the community-server file (tools/release/).
import { selftestPack } from "../release/pack-playtest.mjs";
import { selftestServer } from "../release/playtest-server.mjs";
import { selftestRelease } from "../release/release-playtest.mjs";
import { readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const failures = [...selftestPack(), ...selftestServer(), ...selftestRelease()];
try {
  const live = JSON.parse(readFileSync(join(root, "website", "public", "playtest-server.json"), "utf8"));
  if (!("address" in live)) failures.push("website/public/playtest-server.json has no address field");
} catch (e) {
  failures.push(`website/public/playtest-server.json: ${e.message}`);
}
for (const f of failures) console.log(`  FAIL  ${f}`);
if (failures.length) {
  console.log("release-selftest: FAIL");
  process.exit(1);
}
console.log("release-selftest: OK (playtest zip, launcher, server file)");
