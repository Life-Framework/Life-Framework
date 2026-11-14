// Offline proof of where a stale resourceDatabase.rdb is refreshed (chooseRefresh in
// tools/lib/wb-gate.mjs, used by refreshOnPool in tools/cli.mjs): this PC when its Workbench slot is
// free, a pool machine with a free slot and RAM when this PC is busy, this PC again when no pool
// machine has room or answers, and a held build lock counts as a busy PC.
// red-proof: 2026-11-14, chooseRefresh ignoring local.buildLocked failed "build lock held: goes remote";
// chooseRefresh returning a remote while no pool machine had room failed "no remote slot: refresh here".

import { chooseRefresh, parseWbState } from "../lib/wb-gate.mjs";

const failures = [];
const check = (name, ok) => {
  if (!ok) failures.push(name);
};

const free = { busy: 0, queued: 0, limit: 1 };
const busy = { busy: 1, queued: 0, limit: 1 };
const box = (name, freeMb, wbAlive = 0, wbLimit = 1) => ({ name, caps: ["server", "workbench"], state: { freeMb, wbAlive, wbLimit } });
const where = (arg) => chooseRefresh(arg).where;

check("free PC refreshes here", where({ local: free, remotes: [box("lf-cloud", 20000)] }) === "local");
check("busy PC: remote", where({ local: busy, remotes: [box("lf-cloud", 20000)] }) === "lf-cloud");
check("queued PC: remote", where({ local: { ...free, queued: 2 }, remotes: [box("lf-cloud", 20000)] }) === "lf-cloud");
check("build lock held: goes remote", where({ local: { ...free, buildLocked: true }, remotes: [box("lf-cloud", 20000)] }) === "lf-cloud");
check("free slot but this PC short of RAM: goes remote", where({ local: { ...free, freeMb: 1500 }, remotes: [box("lf-cloud", 20000)] }) === "lf-cloud");
check("this PC with RAM and a free slot stays local", where({ local: { ...free, freeMb: 6000 }, remotes: [box("lf-cloud", 20000)] }) === "local");
check("short of RAM and no remote has room: refresh here (the only option)", where({ local: { ...free, freeMb: 1500 }, remotes: [box("lf-cloud", 20000, 1)] }) === "local");
check("remote with a slot but no RAM is skipped", where({ local: busy, remotes: [box("lf-testbox", 900), box("lf-cloud", 8000)] }) === "lf-cloud");
check("no remote slot: refresh here", where({ local: busy, remotes: [box("lf-cloud", 20000, 1)] }) === "local");
check("no remote RAM: refresh here", where({ local: busy, remotes: [box("lf-testbox", 900)] }) === "local");
check("remote down: refresh here", where({ local: busy, remotes: [{ name: "lf-cloud", caps: ["workbench"], state: null }] }) === "local");
check("no remote machines: refresh here", where({ local: busy, remotes: [] }) === "local");
check("cap 2 with one Workbench running still has a slot", where({ local: busy, remotes: [box("lf-cloud", 20000, 1, 2)] }) === "lf-cloud");
check("cap 2 full: refresh here", where({ local: busy, remotes: [box("lf-cloud", 20000, 2, 2)] }) === "local");
check("most free RAM wins", where({ local: busy, remotes: [box("lf-testbox", 5000), box("lf-cloud", 20000)] }) === "lf-cloud");
check("a full box is skipped for the next one", where({ local: busy, remotes: [box("lf-cloud", 20000, 2, 2), box("lf-testbox", 5000)] }) === "lf-testbox");
check("mode local forces here", where({ mode: "local", local: busy, remotes: [box("lf-cloud", 20000)] }) === "local");
check("mode pool goes remote while this PC is free", where({ mode: "pool", local: free, remotes: [box("lf-cloud", 20000)] }) === "lf-cloud");
check("mode pool with nobody answering falls back", where({ mode: "pool", local: free, remotes: [{ name: "lf-cloud", caps: ["workbench"], state: null }] }) === "local");
check("the reason names the machine and its RAM", /lf-cloud/.test(chooseRefresh({ local: busy, remotes: [box("lf-cloud", 20000)] }).why));
check("box state parses", JSON.stringify(parseWbState("lf-wb: 0 21000\n", 2)) === JSON.stringify({ wbAlive: 0, freeMb: 21000, wbLimit: 2 }));

if (failures.length) {
  console.log(`rdb-refresh-where-selftest: FAILED\n  ${failures.join("\n  ")}`);
  process.exit(1);
}
console.log("rdb-refresh-where-selftest: OK (local when free, remote when busy, local when no remote slot)");
