// No escaped quote inside a string-table Target (agent playtest round 4, 2026-11-27).
// inputs: addons/**/Language/*.st
//
// The engine's string table cuts a Target at an escaped quote: ten outpost toasts written as
// "\"Take it. ...\" You paid %1." showed a bare `"` on screen, with the whole sentence gone. A
// spoken line takes no quote marks (the toast title names the speaker), so a Target holds none.
//
// red-proof: 2026-11-27, the ten outpost rows as they stood before the fix printed
// `FAIL addons/LifeFramework/Language/everonlife_localization.st:12722 ... LF-OutpostBuyer_Full` and
// exited 1; after the quotes were dropped it prints OK.
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const files = execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard", "addons"], {
  cwd: root,
  encoding: "utf8",
  maxBuffer: 1 << 28,
}).split("\0").filter((f) => /\/Language\/[^/]*\.st$/.test(f));

const bad = [];
for (const f of files) {
  if (!existsSync(join(root, f))) continue;
  let id = "";
  readFileSync(join(root, f), "utf8").split("\n").forEach((line, i) => {
    const idMatch = /^\s*Id "([^"]+)"/.exec(line);
    if (idMatch) id = idMatch[1];
    if (/^\s*Target_\w+ ".*\\"/.test(line)) bad.push(`FAIL ${f}:${i + 1} ${id}`);
  });
}

if (bad.length) {
  console.log(bad.slice(0, 40).join("\n"));
  console.log(`validate-no-escaped-quote-targets: ${bad.length} Target(s) with an escaped quote. Drop the quote marks (the engine cuts the text at the first one).`);
  process.exit(1);
}
console.log(`validate-no-escaped-quote-targets: OK (${files.length} string table(s))`);
