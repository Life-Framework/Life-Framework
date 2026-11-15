// Proves `tools\cli look` on the pool (tools/visual/look.mjs routeLook) offline: when it routes and when it
// stays here, the argv a pool machine runs, the pictures' index rewritten to this PC, a starved Workbench
// counted as the machine's fault, the look's RAM need fed to the shared Workbench ranking (a PC with
// 2 GB free goes to a roomy pool machine, everyone short queues), and a test run on a PC under its disk
// floor going to the pool machines instead of being refused.
// inputs: tools/visual/look.mjs, tools/lib/wb-gate.mjs, tools/lib/pool-queue.mjs, tools/remote/pool.mjs, tools/validation/look-route-selftest.mjs
// red-proof: 2026-11-14, routeMode returning "local" for every look (the old behaviour) failed "a plain
// look routes"; diskRoute rethrowing every refusal failed "disk-short PC drops out of an any-machine run".

import { LOOK_RESERVE_MB, parseArgs, rehomeIndex, remoteLookArgs, routeMode, starvedRun } from "../visual/look.mjs";
import { chooseGate } from "../lib/wb-gate.mjs";
import { diskRoute } from "../remote/pool.mjs";

const failures = [];
const check = (name, ok) => {
  if (!ok) failures.push(name);
};
const mode = (argv, o = {}) => routeMode(parseArgs(argv), o);

// When it routes
check("a plain look routes", mode(["bank"]) === "route");
check("look item routes", mode(["item", "ShelfShop_01"]) === "route");
check("--local stays here", mode(["bank", "--local"]) === "local");
check("--force stays here", mode(["bank", "--force"]) === "local");
check("--serve stays here", mode(["bank", "--serve"]) === "local");
check("a live look server here is used", mode(["bank"], { serverUp: () => true }) === "local");
check("--fresh skips the live server", mode(["bank", "--fresh"], { serverUp: () => true }) === "route");
check("--machine routes even with a server up", mode(["bank", "--machine", "lf-cloud"], { serverUp: () => true }) === "route");
check("LF_TESTBOX_AUTO=0 stays here", mode(["bank"], { env: { LF_TESTBOX_AUTO: "0" } }) === "local");
check("--machine takes a value", parseArgs(["bank", "--machine", "lf-cloud"]).flags.machine === "lf-cloud");

// The remote argv
const ra = remoteLookArgs(["bank", "hospital", "--out", "x/y", "--machine", "lf-cloud", "--hour", "19", "--local"], "tmp/look-routed/abc");
check("remote argv pins the box and writes to its own folder", ra.join(" ") === "look bank hospital --hour 19 --local --out tmp/look-routed/abc");

// The index moved home
const idx = rehomeIndex({ area: "bank", contact: "C:\\LF\\rt\\tmp\\look-routed\\abc\\bank\\contact.jpg", shots: [{ kind: "wide", file: "C:\\LF\\rt\\tmp\\look-routed\\abc\\bank\\wide.png" }, { kind: "x", file: null }] }, "D:\\wt\\tmp\\look\\bank");
check("contact path rewritten", /^D:\\wt\\tmp\\look\\bank[\\/]contact\.jpg$/.test(idx.contact));
check("shot paths rewritten, missing kept null", /look[\\/]bank[\\/]wide\.png$/.test(idx.shots[0].file) && idx.shots[1].file === null);

// A starved Workbench (2026-11-14: --force on 2 GB free, TIMEOUT after 100 s, 0/6 shots)
check("starved: timeout, no shot, short", !!starvedRun({ timedOut: true, written: 0, freeMb: 2017 }));
check("not starved with a shot written", starvedRun({ timedOut: true, written: 1, freeMb: 2017 }) === null);
check("not starved with the RAM there", starvedRun({ timedOut: true, written: 0, freeMb: 8000 }) === null);
check("not starved without a timeout", starvedRun({ timedOut: false, written: 0, freeMb: 100 }) === null);

// The look's reserve in the shared ranking (the 2026-11-14 machines)
const pc = (freeMb, busy = 0) => ({ busy, queued: 0, limit: 1, freeMb, multiplier: 1 });
const box = (name, freeMb, multiplier = 2, wbLimit = 1) => ({ name, caps: ["workbench"], multiplier, state: { freeMb, wbAlive: 0, wbLimit } });
const at = (local, remotes) => chooseGate({ local, remotes, reserveMb: LOOK_RESERVE_MB, minFreeMb: 1500 });
check("2 GB free here goes to the roomy cloud box", at(pc(2017), [box("lf-cloud", 26000, 2, 2)]).where === "lf-cloud");
check("roomy PC keeps the look", at(pc(12000), [box("lf-testbox", 4000)]).where === "local");
check("everyone short queues", at(pc(1500), [box("lf-testbox", 2500)]).where === "wait");
check("no pool machine answers and PC roomy: here", at(pc(9000), [{ name: "lf-cloud", caps: ["workbench"], multiplier: 2, state: null }]).where === "local");
check("a tried machine is skipped", chooseGate({ local: pc(9000), remotes: [box("lf-cloud", 26000)], reserveMb: LOOK_RESERVE_MB, minFreeMb: 1500, exclude: ["local"] }).where === "lf-cloud");

// The disk floor: route instead of refusing
const refusal = Object.assign(new Error("free disk 3.1 GB is under the 5 GB floor (pool minFreeDiskGb); clean up"), { refusal: true });
const ms = [{ name: "local", via: "local" }, { name: "lf-cloud", via: "ssh" }];
let kept = null;
try {
  kept = diskRoute(ms, refusal, { pinned: false });
} catch {}
check("disk-short PC drops out of an any-machine run", kept?.length === 1 && kept[0].name === "lf-cloud");
let threw = false;
try {
  diskRoute(ms, refusal, { pinned: true });
} catch {
  threw = true;
}
check("a run pinned here keeps the refusal", threw);
threw = false;
try {
  diskRoute([ms[0]], refusal);
} catch {
  threw = true;
}
check("no other machine keeps the refusal", threw);

if (failures.length) {
  for (const f of failures) console.error(`FAIL  look-route-selftest: ${f}`);
  process.exit(1);
}
console.log("look-route-selftest: OK (route or stay, remote argv, index rehomed, starved Workbench, look RAM ranking, disk floor routes)");
