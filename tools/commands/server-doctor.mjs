// tools\cli server-doctor <profile-or-log>   read a server console.log: mods, script errors with the fix, success lines, PASS/FAIL (#1271)
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { diagnoseTarget, findLog, renderReport } from "../lib/server-doctor.mjs";

export const help = "server-doctor <profile-or-log>   which mods loaded, script errors with the triage fix, first-boot lines, PASS or FAIL (no game needed)";

export async function main(args, { root }) {
  const target = args.find((a) => !a.startsWith("--"));
  if (!target || args.includes("--help")) {
    console.log("usage: tools\cli server-doctor <profile-folder|console.log>");
    return target ? 0 : 2;
  }
  const log = findLog(target);
  if (!log) {
    console.log(`FAIL  no console.log at ${target}`);
    console.log("SUMMARY server-doctor fail stage=input");
    return 1;
  }
  let triage = "";
  try { triage = readFileSync(join(root, "docs", "debug-log-triage.md"), "utf8"); } catch { /* the table is advice; the verdict does not need it */ }
  const r = diagnoseTarget(target, { triage });
  console.log(renderReport(r));
  console.log(`SUMMARY server-doctor ${r.verdict === "PASS" ? "ok" : "fail"} stage=${r.stage} mods=${r.mods.length} errors=${r.errors.length} log=${log}`);
  return r.verdict === "PASS" ? 0 : 1;
}
