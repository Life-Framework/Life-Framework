// tools/validation/validate-meta-name-path.mjs
// inputs: addons/**/*.meta
//
// A resource meta's Name is "{GUID}<path in the addon>", e.g. "{BF263B38BFBB5166}UI/Textures/Map/LF_MapHatch_Cross.edds".
// A Name with nothing between the GUID and the extension ("{B4C1A2A68D627B9F}.edds") gives the resource no path,
// so no "{GUID}path" reference can name it (AGENTS.md golden rule 3). The pack hatch and icon textures shipped
// that way (#2231) and could not be referenced until the map proof pass fixed them.
// Fails a tracked addons/**/*.meta whose Name is "{GUID}.<ext>". A wrong folder or file case is not checked here.
// red-proof: meta-name-path-selftest.mjs.

import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";

export const BARE_NAME_RE = /^\s*Name\s+"\{[0-9A-Fa-f]{16}\}\.[A-Za-z0-9]+"/m;

export function isBareName(text) {
  return BARE_NAME_RE.test(text);
}

function main() {
  const files = execFileSync("git", ["ls-files", "-z", "addons/*.meta"], { encoding: "utf8" })
    .split("\0")
    .filter(Boolean);
  const bad = files.filter((f) => isBareName(readFileSync(f, "utf8")));
  for (const f of bad) console.log(`FAIL  ${f}:2 meta Name has no path; write "{GUID}<path in the addon>"`);
  console.log(bad.length ? "FAIL  validate-meta-name-path" : `PASS  validate-meta-name-path (${files.length} metas)`);
  process.exit(bad.length ? 1 : 0);
}

if (process.argv[1] && process.argv[1].endsWith("validate-meta-name-path.mjs")) main();
