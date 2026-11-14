// The stable-id restart proof of `tools\cli wt e2e <slug> --content-ids` (#965).
//
// Three boots of one test, LF_Test_E2E_ContentIds, on the suite's mission header:
// 1. save boot: a carried inventory with 4 apples under a renamed id and a canteen whose
//    content was removed; ends by the host stop file so the engine writes the SHUTDOWN save;
// 2. reload with the rename alias: the apples load, the canteen is logged and held, the
//    inventory is captured again; ends by the host stop file (a second SHUTDOWN save);
// 3. reload with the content back: the canteen returns inside the backpack.
// Each save boot must pass checkShutdownSave, so the next boot loads the shutdown save.

import { readFileSync } from "node:fs";
import { checkShutdownSave, newestTestConsole, E2E_MISSION, HOST_STOP_DEFINE } from "./e2e-shutdown.mjs";

export const TEST = "LF_Test_E2E_ContentIds";
const BASE = "LF_E2E_CONTENT_IDS";

// The boots, in order. `marker` is the console line the host stop waits for.
export const PASSES = [
  { name: "save boot (old ids)", defines: [BASE, HOST_STOP_DEFINE], keepProfile: false, hostStop: true, marker: "e2e content ids seeded" },
  { name: "reload with the rename alias", defines: [BASE, HOST_STOP_DEFINE, "LF_E2E_LOAD_BOOT", "LF_E2E_IDS_RENAMED"], keepProfile: true, hostStop: true, marker: "e2e content ids held" },
  { name: "reload with the content back", defines: [BASE, "LF_E2E_LOAD_BOOT", "LF_E2E_IDS_RENAMED", "LF_E2E_IDS_RESTORED"], keepProfile: true, hostStop: false, marker: "e2e content ids restored" },
];

// The lines of a boot's log that show what the stable-id load did.
const EVIDENCE = /save alias used|save content missing|save ids:|holds \d+ item\(s\) whose content is missing|e2e content ids/;

export function evidenceLines(text) {
  return String(text ?? "").split(/\r?\n/).filter((l) => EVIDENCE.test(l)).map((l) => l.trim());
}

// runPass(opts) -> exit code; opts as runE2ePass takes them. root is the worktree.
export function runContentIds(root, runPass, log = console.log) {
  for (let i = 0; i < PASSES.length; i++) {
    const p = PASSES[i];
    const label = `e2e content-ids: pass ${i + 1}/${PASSES.length}`;
    log(`${label} - ${p.name} (${p.defines.join(",")})`);
    let code = runPass({
      mission: E2E_MISSION,
      only: TEST,
      keepProfile: p.keepProfile,
      loadLatestSave: p.keepProfile,
      hostStop: p.hostStop,
      hostStopAfter: p.hostStop ? p.marker : undefined,
      extraDefine: p.defines.join(","),
    });
    const file = newestTestConsole(root);
    const text = file ? readFileSync(file, "utf8") : "";
    for (const line of evidenceLines(text)) log(`${label} ${line}`);
    if (code === 0 && !text.includes(p.marker)) {
      log(`${label} FAILED: no "${p.marker}" line`);
      code = 1;
    }
    if (code === 0 && p.hostStop) {
      const check = checkShutdownSave(text);
      if (!check.ok) {
        log(`${label} FAILED (shutdown save): ${check.reason}`);
        code = 1;
      }
    }
    if (code !== 0) {
      log(`${label} FAILED (exit ${code}); log ${file ?? "(none)"}`);
      return code;
    }
  }
  log("e2e content-ids: OK (renamed id loaded, removed content held across a save, restored content back)");
  return 0;
}

// Fixture check for tools/validation/e2e-content-ids-selftest.mjs; returns failure strings.
export function selftestContentIds() {
  const failures = [];
  if (PASSES[0].defines.includes("LF_E2E_LOAD_BOOT")) failures.push("the save boot must not be a load boot");
  if (!PASSES[1].defines.includes("LF_E2E_IDS_RENAMED") || PASSES[1].defines.includes("LF_E2E_IDS_RESTORED")) failures.push("boot 2 has the rename alias only");
  if (!PASSES[2].defines.includes("LF_E2E_IDS_RESTORED") || !PASSES[2].defines.includes("LF_E2E_IDS_RENAMED")) failures.push("boot 3 keeps the rename and adds the restore");
  if (!PASSES[1].hostStop) failures.push("boot 2 must end by the host stop so its capture is saved");
  const lines = evidenceLines([
    "10:00:00.000 SCRIPT       : [LFDebug:Persistence] save alias used: prefab {0E2EA11A5000000A}Prefabs/Items/Food/Apple_1988.et -> {C9D661E5B0714711}Prefabs/Items/Food/Apple.et",
    "10:00:00.001 SCRIPT       : [LFDebug:Persistence] applied 3 carried snapshot(s)",
  ].join("\n"));
  if (lines.length !== 1) failures.push(`evidenceLines kept ${lines.length} line(s), expected 1`);
  return failures;
}
