// Fixture beat for opening-fixes-selftest: starts a long-lived process, records its pid the way a real beat does, and exits
// WITHOUT stopping it, as B2's `host watch` did (#1692). The runner has to stop it. The pid is written to <host>/leaked.pid.
import { writeFileSync } from "node:fs";
import { spawn } from "node:child_process";
import { join } from "node:path";
import { recordPid } from "../../../../playtest/opening/_scratch.mjs";

const host = process.argv[process.argv.indexOf("--host") + 1];
const child = spawn(process.execPath, ["-e", "setTimeout(() => {}, 120000)"], { detached: true, stdio: "ignore", windowsHide: true });
child.unref();
recordPid(host, "fixture watcher", child.pid);
writeFileSync(join(host, "leaked.pid"), String(child.pid));
console.log("BEAT B2 started PASS a long-lived process is up and this script exits without stopping it");
