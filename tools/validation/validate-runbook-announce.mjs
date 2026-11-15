// The session runbook tells the operator what `host announce` needs and leaves behind (#1632).
// inputs: docs/ops/session-runbook.md, docs/ops/session-cheatsheet.md, tools/host/commands/announce.mjs
//
// `tools\cli host announce --at <iso>` on a fresh host stops with "no build: pass --build <version>"
// (no host save to read it from) and, when it works, rewrites website/content/sessions.json in the
// checkout even without --send. A runbook step that leaves out either one fails on its first real
// run. This reads the command's two behaviours from its source, then requires the runbook's "Before"
// section and the cheatsheet to say the matching thing: the `--build` flag with where the number
// comes from, and the sessions.json edit with what to do about the dirty checkout.
//
// `--selftest` runs the same check on a page that says neither and a page that says both.
//
// red-proof: 2026-11-15, on the runbook before #1632 `SUMMARY validate-runbook-announce fail 4` with
// "runbook Before step 1 never names --build" (4 findings) and exit 1.
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

// The text of the runbook's "## Before" section, up to the next "## " heading.
export function beforeSection(md) {
  const m = /^## Before\s*$([\s\S]*?)(?=^## )/m.exec(md);
  return m ? m[1] : "";
}

// What the announce source does, read from the code so a changed command changes the demand.
export function announceBehaviour(src) {
  return {
    needsBuild: /pass --build <version>/.test(src),
    writesSessions: /sessions\.json/.test(src) && /writeFileSync\(file/.test(src),
  };
}

// Findings for one page: strings naming what the page is missing.
export function checkAnnounceText(text, behaviour, where) {
  const out = [];
  const announce = /host announce/.test(text);
  if (!announce) return [`${where} never mentions host announce`];
  if (behaviour.needsBuild) {
    if (!/--build/.test(text)) out.push(`${where} never names --build`);
    else if (!/(installed game build|file version|FileVersion|ArmaReforgerServer\.exe|Steam)/i.test(text)) out.push(`${where} names --build but not where the build number comes from`);
  }
  if (behaviour.writesSessions) {
    if (!/sessions\.json/.test(text)) out.push(`${where} never says announce edits website/content/sessions.json`);
    else if (!/(git (checkout|restore)|revert|commit)/i.test(text)) out.push(`${where} says announce edits sessions.json but not to revert or commit it`);
  }
  return out;
}

function selftest() {
  const behaviour = { needsBuild: true, writesSessions: true };
  let failed = 0;
  const fail = (m) => {
    console.log(`FAIL  ${m}`);
    failed++;
  };
  const bad = "Run `tools\\cli host announce --at <time>` and post it.";
  const good =
    "Run `tools\\cli host announce --at <time> --build <build>`. On a fresh host the build is the file version of " +
    "ArmaReforgerServer.exe. It writes website/content/sessions.json: `git checkout` it or commit it.";
  if (checkAnnounceText(bad, behaviour, "bad").length !== 2) fail("a page that says neither --build nor sessions.json should give 2 findings");
  if (checkAnnounceText(good, behaviour, "good").length) fail(`a page that says both was flagged: ${checkAnnounceText(good, behaviour, "good").join("; ")}`);
  if (checkAnnounceText("no command here", behaviour, "none").length !== 1) fail("a page without announce should give 1 finding");
  if (!announceBehaviour('"no build: pass --build <version>" writeFileSync(file, x) sessions.json').needsBuild) fail("announceBehaviour misses --build");
  if (!beforeSection("## Before\n\nbody\n\n## Start\n").includes("body")) fail("beforeSection misses the section");
  console.log(failed ? `SUMMARY validate-runbook-announce selftest fail ${failed}` : "SUMMARY validate-runbook-announce selftest ok");
  process.exit(failed ? 1 : 0);
}

if (process.argv.includes("--selftest")) selftest();

const read = (p) => (existsSync(join(ROOT, p)) ? readFileSync(join(ROOT, p), "utf8") : "");
const behaviour = announceBehaviour(read("tools/host/commands/announce.mjs"));
const findings = [
  ...checkAnnounceText(beforeSection(read("docs/ops/session-runbook.md")), behaviour, "runbook Before step 1"),
  ...checkAnnounceText(read("docs/ops/session-cheatsheet.md"), behaviour, "cheatsheet"),
];
for (const f of findings) console.log(`FAIL  ${f}`);
console.log(findings.length ? `SUMMARY validate-runbook-announce fail ${findings.length}` : "SUMMARY validate-runbook-announce ok");
process.exit(findings.length ? 1 : 0);
