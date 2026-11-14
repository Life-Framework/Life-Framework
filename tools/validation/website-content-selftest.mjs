// Proves the website content check: the shipped repo is clean, a missing node_modules skips with a hint,
// and a failing content step is reported with the missing path. red-proof: with build-content.mjs pointing
// its core scenario at the moved LF_PlaytestTown.conf.meta the real run printed
// "FAIL website-content: ... ENOENT ... LF_PlaytestTown.conf.meta"; with EL_DebugTest.conf it is OK.
import { mkdirSync, mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { websiteContent } from "./validate-website-content.mjs";

const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };

const root = mkdtempSync(join(tmpdir(), "lf-web-"));
expect(websiteContent(root).skipped, "a root with no website skips");
mkdirSync(join(root, "website", "scripts"), { recursive: true });
writeFileSync(join(root, "website", "scripts", "build-content.mjs"), "");
const noMods = websiteContent(root);
expect(/node_modules/.test(noMods.skipped || "") && /npm ci/.test(noMods.skipped || ""), `missing node_modules skips with a hint: ${JSON.stringify(noMods)}`);

mkdirSync(join(root, "website", "node_modules"));
const stderr = "Error: ENOENT: no such file or directory, open 'addons/LifeFramework/Missions/LF_PlaytestTown.conf.meta'";
const bad = websiteContent(root, () => ({ status: 1, stderr: `node:fs:441\n${stderr}\n at build-content.mjs`, stdout: "" }));
expect(bad.problems?.length === 1 && /LF_PlaytestTown\.conf\.meta/.test(bad.problems[0]), `a failing step names the path: ${JSON.stringify(bad)}`);
const good = websiteContent(root, () => ({ status: 0, stderr: "", stdout: "" }));
expect(good.problems?.length === 0, "a clean step has no problems");
let sawOffline = false;
websiteContent(root, (_n, _a, o) => { sawOffline = o.env.LF_SITE_OFFLINE === "1"; return { status: 0 }; });
expect(sawOffline, "the step runs offline");

// The real repo: only when website/node_modules exists (otherwise the skip is the proof above).
const repo = resolve(import.meta.dirname, "..", "..");
const real = websiteContent(repo);
if (!real.skipped) expect(real.problems.length === 0, `the shipped repo fails the content step: ${real.problems?.join(" | ")}`);

for (const f of fails) console.log(`FAIL website-content-selftest: ${f}`);
if (fails.length) process.exit(1);
console.log("OK website-content-selftest");
