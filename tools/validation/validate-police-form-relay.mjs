// A filed police application goes through LF_PoliceApplicationRelay.File (issue #1575, slice #1289).
// inputs: addons/LifeFramework/Scripts/Game/Feature/Police/LF_PoliceApplicationForm.c
//
// The form's RPC handler is the only caller that files a real application. A direct
// LF_PoliceApplications.Submit/SubmitGated call there files the application silently: the staff
// channel never hears of it. The Relay files through SubmitGated and sends the staff line.
//
// red-proof: 2026-11-15, on the unwired form this printed
// `FAIL ...LF_PoliceApplicationForm.c:187 LF_PoliceApplications.Submit(` and exited 1; wired it prints OK.
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const rel = "addons/LifeFramework/Scripts/Game/Feature/Police/LF_PoliceApplicationForm.c";
const file = join(root, rel);
if (!existsSync(file)) {
  console.log(`FAIL ${rel} is missing`);
  process.exit(1);
}

const lines = readFileSync(file, "utf8").split("\n");
const bad = [];
let relayCalls = 0;
lines.forEach((line, i) => {
  if (/^\s*\/\//.test(line)) return;
  if (/LF_PoliceApplications\.Submit(Gated)?\s*\(/.test(line)) bad.push(`${rel}:${i + 1} ${line.trim().match(/LF_PoliceApplications\.Submit(Gated)?\(/)[0]}`);
  if (/LF_PoliceApplicationRelay\.File\s*\(/.test(line)) relayCalls++;
});
if (relayCalls === 0) bad.push(`${rel} never calls LF_PoliceApplicationRelay.File(`);

if (bad.length) {
  for (const b of bad) console.log(`FAIL ${b}`);
  process.exit(1);
}
console.log("OK police application form files through LF_PoliceApplicationRelay.File");
