// Offline proof of the Workbench limit (tools/lib/wb-lock.mjs): the limit setting, the FIFO rule and
// a real second process queuing behind a held lock and taking it when the holder dies. Where `ci`
// runs its Workbench steps is proved by tools/validation/wb-gate-selftest.mjs.
// red-proof: 2026-11-14, mayTake returning ok for any ticket while a lock was free failed
// "FIFO: the second ticket waits". 2026-11-15: with the holder on a 1500 ms exit timer again, 8 runs beside 100 busy
// processes failed 8/8 ("the waiter queued with one status line", "held lock shows busy"); on a stdin release, 8/8 pass.

import { spawn, spawnSync } from "node:child_process";
import { mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { pathToFileURL } from "node:url";
import * as wb from "../lib/wb-lock.mjs";

const failures = [];
const check = (name, ok) => {
  if (!ok) failures.push(name);
};

// 1. limit: env > lf.local.json > pool.json > 1
check("limit default 1", wb.pickLimit({}) === 1);
check("limit from pool.json", wb.pickLimit({ base: 2 }) === 2);
check("limit lf.local.json wins", wb.pickLimit({ base: 2, local: 3 }) === 3);
check("limit env wins", wb.pickLimit({ env: "4", local: 3, base: 2 }) === 4);
check("limit ignores junk", wb.pickLimit({ env: "x", local: 0, base: -1 }) === 1);

// 2. FIFO: a ticket takes a lock only while fewer older tickets wait than locks are free
const t = [{ id: "a" }, { id: "b" }, { id: "c" }];
check("FIFO: the oldest ticket takes the free lock", wb.mayTake(t, "a", 1).ok);
check("FIFO: the second ticket waits", !wb.mayTake(t, "b", 1).ok && wb.mayTake(t, "b", 1).ahead === 1);
check("FIFO: two free locks take two tickets", wb.mayTake(t, "b", 2).ok && !wb.mayTake(t, "c", 2).ok);
check("FIFO: nothing free, nobody takes", !wb.mayTake(t, "a", 0).ok);

// 3. real locks: a child holds the only lock; this process queues, and takes it when the child dies.
// No wall-clock sleeps: the child holds until this process closes its stdin, and this process closes it
// only after the waiter printed its "queued" line (the old 1500 ms holder timer let a loaded machine
// finish the holder before the waiter started: no queued line, "held lock shows busy" false).
const dir = mkdtempSync(join(tmpdir(), "lf-wb-lock-"));
spawnSync("git", ["init", "-q", dir], { windowsHide: true });
writeFileSync(join(dir, "lf.local.json"), JSON.stringify({ pool: { workbenchLimit: 1 } }));
const lib = pathToFileURL(join(import.meta.dirname, "..", "lib", "wb-lock.mjs")).href;
const holderJs = `const w = await import(${JSON.stringify(lib)}); await w.acquireWorkbench(${JSON.stringify(dir)}, { purpose: "selftest-holder", log: () => {} }); console.log("HELD"); process.stdin.resume(); process.stdin.on("end", () => process.exit(0));`;
const child = spawn(process.execPath, ["--input-type=module", "-e", holderJs], { windowsHide: true, stdio: ["pipe", "pipe", "inherit"], env: { ...process.env, LF_WB_LIMIT: "" } });
await new Promise((res) => child.stdout.on("data", (d) => String(d).includes("HELD") && res()));
check("held lock shows busy", wb.holders(dir, 1).filter((h) => h.alive).length === 1);
const lines = [];
let released = false;
let acquiredAfterRelease = false;
const waiter = wb.acquireWorkbench(dir, { purpose: "selftest-waiter", pollMs: 50, waitMs: 120000, log: (l) => lines.push(l) }).then((lock) => {
  acquiredAfterRelease = released;
  return lock;
});
while (!lines.some((l) => l.startsWith("workbench: queued"))) await new Promise((res) => setTimeout(res, 20));
check("the waiter is still queued while the holder lives", lines.length === 1 && wb.liveTickets(dir).length === 1);
released = true;
child.stdin.end();
const h = await waiter;
check("the waiter queued with one status line", lines.filter((l) => l.startsWith("workbench: queued")).length === 1);
check("the waiter got the lock only after the holder died", acquiredAfterRelease && h.name === "workbench-1");
check("status line shows it", wb.statusLine(dir).startsWith("workbench (this PC): 1/1 busy"));
h.release();
check("released", wb.holders(dir, 1).filter((x) => x.alive).length === 0 && wb.liveTickets(dir).length === 0);

if (failures.length) {
  console.log(`wb-lock-selftest: FAILED\n  ${failures.join("\n  ")}`);
  process.exit(1);
}
console.log("wb-lock-selftest: OK (limit, FIFO, a real queued handover)");
