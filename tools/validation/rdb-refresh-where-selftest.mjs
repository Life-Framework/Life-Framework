// Offline proof of where a stale resourceDatabase.rdb is refreshed (chooseRefresh in
// tools/lib/wb-gate.mjs, built on chooseGate; used by refreshOnPool in tools/cli.mjs): this PC when it
// has a free Workbench slot and the RAM, a pool machine with a free slot AND RAM when this PC is busy,
// short of RAM or its build lock is held, and this PC again when no pool machine has both (a remote
// it could only queue on is no better than waiting here).
// inputs: tools/lib/wb-gate.mjs, tools/lib/pool-queue.mjs, tools/validation/rdb-refresh-where-selftest.mjs
// red-proof: 2026-11-14, chooseRefresh ignoring local.buildLocked failed "build lock held: goes remote";
// chooseRefresh taking a remote chooseGate only queued on (the `!c.queue` test dropped) failed "remote
// slots full: refresh here" and "cap 2 full: refresh here"; ramOk always true failed "free slot but
// this PC short of RAM: goes remote".

import { chooseRefresh, WB_RESERVE_MB } from "../lib/wb-gate.mjs";

const failures = [];
const check = (name, ok) => {
  if (!ok) failures.push(name);
};

const pc = (freeMb, busy = 0, queued = 0, buildLocked = false) => ({ busy, queued, limit: 1, freeMb, multiplier: 1, buildLocked });
const box = (name, freeMb, { wbAlive = 0, wbLimit = 1, multiplier = 2 } = {}) => ({ name, caps: ["server", "workbench"], multiplier, state: { freeMb, wbAlive, wbLimit } });
const down = (name) => ({ name, caps: ["workbench"], multiplier: 2, state: null });
const where = (arg) => chooseRefresh(arg).where;

check("free PC with RAM refreshes here", where({ local: pc(12000), remotes: [box("lf-cloud", 20000)] }) === "local");
check("busy PC: remote", where({ local: pc(12000, 1), remotes: [box("lf-cloud", 20000)] }) === "lf-cloud");
check("queued PC: remote", where({ local: pc(12000, 0, 2), remotes: [box("lf-cloud", 20000)] }) === "lf-cloud");
check("build lock held: goes remote", where({ local: pc(12000, 0, 0, true), remotes: [box("lf-cloud", 20000)] }) === "lf-cloud");
check("free slot but this PC short of RAM: goes remote", where({ local: pc(1500), remotes: [box("lf-cloud", 20000)] }) === "lf-cloud");
check("just under reserve + floor is short", where({ local: pc(WB_RESERVE_MB + 1500 - 1), remotes: [box("lf-cloud", 20000)] }) === "lf-cloud");
check("unknown local RAM does not count as short", where({ local: pc(null), remotes: [box("lf-cloud", 20000)] }) === "local");
check("short of RAM and no remote has room: refresh here (the only option)", where({ local: pc(1500), remotes: [box("lf-cloud", 20000, { wbAlive: 1 })] }) === "local");
check("remote slots full: refresh here", where({ local: pc(12000, 1), remotes: [box("lf-cloud", 20000, { wbAlive: 1 })] }) === "local");
check("cap 2 full: refresh here", where({ local: pc(12000, 1), remotes: [box("lf-cloud", 20000, { wbAlive: 2, wbLimit: 2 })] }) === "local");
check("cap 2 with one Workbench running still has a slot", where({ local: pc(12000, 1), remotes: [box("lf-cloud", 20000, { wbAlive: 1, wbLimit: 2 })] }) === "lf-cloud");
check("no remote RAM: refresh here", where({ local: pc(12000, 1), remotes: [box("lf-testbox", 900)] }) === "local");
check("remote with a slot but no RAM is skipped", where({ local: pc(12000, 1), remotes: [box("lf-testbox", 900), box("lf-cloud", 8000)] }) === "lf-cloud");
check("remote down: refresh here", where({ local: pc(12000, 1), remotes: [down("lf-cloud")] }) === "local");
check("no remote machines: refresh here", where({ local: pc(12000, 1), remotes: [] }) === "local");
check("a full box is skipped for the next one", where({ local: pc(12000, 1), remotes: [box("lf-cloud", 20000, { wbAlive: 2, wbLimit: 2 }), box("lf-testbox", 6000)] }) === "lf-testbox");
check("mode local forces here", where({ mode: "local", local: pc(12000, 1), remotes: [box("lf-cloud", 20000)] }) === "local");
check("mode pool goes remote while this PC is free", where({ mode: "pool", local: pc(12000), remotes: [box("lf-cloud", 20000)] }) === "lf-cloud");
check("mode pool with nobody answering falls back", where({ mode: "pool", local: pc(12000), remotes: [down("lf-cloud")] }) === "local");
const why = chooseRefresh({ local: pc(12000, 1), remotes: [box("lf-cloud", 20000)] }).why;
check("the reason names why this PC was passed over and the machine's RAM", /this PC is busy/.test(why) && /lf-cloud/.test(why));
check("the build-lock reason says so", /build lock is held/.test(chooseRefresh({ local: pc(12000, 0, 0, true), remotes: [box("lf-cloud", 20000)] }).why));

if (failures.length) {
  console.log(`rdb-refresh-where-selftest: FAILED\n  ${failures.join("\n  ")}`);
  process.exit(1);
}
console.log("rdb-refresh-where-selftest: OK (local when free, remote when busy or short of RAM, local when no remote has a slot and RAM)");
