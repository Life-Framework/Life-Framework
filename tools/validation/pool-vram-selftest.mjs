// pool-vram-selftest: the pool counts free GPU memory before it starts a client session (#2629;
// tools/lib/pool-vram.mjs, tools/lib/pool-queue.mjs, docs/worktrees.md "Session profiles"). Stubbed
// nvidia-smi output: no game, no GPU, no ssh.
// inputs: tools/lib/pool-vram.mjs, tools/lib/pool-queue.mjs, tools/validation/pool-vram-selftest.mjs
//
// red-proof 2026-11-17: without tools/lib/pool-vram.mjs the import below fails and the selftest exits 1; with
// parseVramFreeMb always returning a number, "unparsable output is unknown" fails; with rankMachines
// ignoring vramFreeMb, "a machine without VRAM goes after one with room" fails.

import * as v from "../lib/pool-vram.mjs";
import * as q from "../lib/pool-queue.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

const settings = { minFreeMb: 1500, minFreeVramMb: 256, sessionReserveClientVramMb: { lowspec: 700, full: 1800 } };
const stub = (stdout, status = 0, extra = {}) => () => ({ status, stdout, ...extra });

// --- parsing and the read (a stubbed nvidia-smi)
check(v.parseVramFreeMb("6000\n") === 6000, "one number is the free MB");
check(v.parseVramFreeMb("6000\r\n") === 6000, "CRLF output parses");
check(v.parseVramFreeMb("1200\n7400\n") === 7400, "with two cards the one with the most free memory counts");
check(v.parseVramFreeMb("") === null && v.parseVramFreeMb(undefined) === null, "empty output is unknown");
check(v.parseVramFreeMb("[N/A]") === null, "[N/A] is unknown");
check(v.parseVramFreeMb("NVIDIA-SMI has failed because it couldn't communicate with the NVIDIA driver.") === null, "an error message is unknown");
check(v.readVramFreeMb({ exec: stub("6000\n") }) === 6000, "a good read returns the number");
check(v.readVramFreeMb({ exec: stub("garbage") }) === null, "an unparsable reading is unknown");
check(v.readVramFreeMb({ exec: stub("", 1) }) === null, "a failed nvidia-smi (exit 1) is unknown");
check(v.readVramFreeMb({ exec: stub("", null, { error: new Error("spawn nvidia-smi ENOENT") }) }) === null, "no nvidia-smi is unknown");
check(v.readVramFreeMb({ exec: () => { throw new Error("boom"); } }) === null, "a throwing executor is unknown, never an exception");
let seen = null;
v.readVramFreeMb({ exec: (cmd, args, o) => { seen = { cmd, args, o }; return { status: 0, stdout: "100" }; }, timeoutMs: 1234 });
check(seen?.cmd === "nvidia-smi" && seen.args.join(" ") === "--query-gpu=memory.free --format=csv,noheader,nounits" && seen.o.timeout === 1234, "the read is nvidia-smi --query-gpu=memory.free, bounded by a timeout");
check(v.parseVramLine("lf-pool: ram 1 2\nlf-pool: vram 5800\n") === 5800 && v.parseVramLine("lf-pool: ram 1 2\n") === null, "the remote state line parses, missing is unknown");

// --- reserves per profile
check(q.DEFAULTS.sessionReserveClientVramMb.lowspec === 700 && q.DEFAULTS.sessionReserveClientVramMb.full === 1800, "defaults: lowspec 700, full 1800 per client");
check(v.sessionVramReserveMb(settings, 2, "full") === 3600 && v.sessionVramReserveMb(settings, 2, "lowspec") === 1400, "a session reserves per client by profile");
check(v.sessionVramReserveMb(settings, 0, "full") === 0, "a server-only session reserves no VRAM");
check(v.sessionVramReserveMb({}, 1, "full") === 1800, "no settings falls back to the code default");
check(v.sessionVramReserveMb({ sessionReserveClientVramMb: { full: 2000 } }, 1, "lowspec") === 2000, "a profile missing from settings uses the full number");

// --- the gate: 6000 MB free fits one full client and not four
const gate = (clients, o = {}) => q.vramGate(settings, { vramFreeMb: 6000, clients, profile: "full", ...o });
check(gate(1).ok === true, "6000 MB free fits one full client");
check(gate(3).ok === true, "6000 MB free fits three full clients (5400 + 256 floor)");
const four = gate(4);
check(four.ok === false && four.needMb === 7200 + 256 && /VRAM/.test(four.reason), "6000 MB free does not fit four full clients, and the reason says VRAM");
check(q.vramGate(settings, { vramFreeMb: 6000, clients: 4, profile: "lowspec" }).ok === true, "four lowspec clients (2800) fit the same card");
check(gate(1, { pendingMb: 4200 }).ok === false, "clients still starting count against the free VRAM");
check(q.vramGate(settings, { vramFreeMb: 2000, clients: 1, profile: "full" }).ok === false, "2000 MB free leaves under the 256 MB floor after a full client");

// --- unknown falls back to RAM only
for (const bad of [null, undefined, NaN]) {
  const r = gate(4, { vramFreeMb: bad });
  check(r.ok === true && r.unknown === true, `unknown VRAM (${bad}) skips the rule, never a refusal`);
}

// --- a test-lane job (no client) ignores VRAM, even with the card full
check(q.vramGate(settings, { vramFreeMb: 0, lane: "test", clients: 4 }).ok === true, "a test-lane job ignores VRAM");
check(q.ticketVramReserveMb({ lane: "test", clients: 2, profile: "full" }, settings) === 0, "a test ticket reserves no VRAM");
check(q.ticketVramReserveMb({ lane: "session", clients: 2, profile: "full" }, settings) === 3600, "a session ticket reserves its clients' VRAM");
check(q.ticketVramReserveMb({ clients: 1 }, {}) === 1800, "a ticket without a lane is a session (the older CLI), full profile, default numbers");

// --- booting clients count until free VRAM shows them
const now = Date.parse("2026-11-17T12:00:00Z");
const infos = [
  { purpose: "session", at: "2026-11-17T11:59:00Z", clients: 2, profile: "full" },
  { purpose: "session", at: "2026-11-17T11:50:00Z", clients: 2, profile: "full" },
  { purpose: "test", at: "2026-11-17T11:59:30Z" },
  { purpose: "session", at: "2026-11-17T11:59:30Z", clients: 1, profile: "lowspec", alive: false },
];
check(v.bootingVramMb(infos, settings, now) === 3600, "only a live session younger than 3 minutes counts as starting");

// --- machine choice: a machine without VRAM goes after one with room while another has room
const cands = [
  { name: "laptop", freeMb: 30000, vramFreeMb: 1000, multiplier: 2 },
  { name: "pc", freeMb: 9000, vramFreeMb: 6000, multiplier: 1 },
];
const withVram = q.rankMachines(cands, { reserveMb: 4000, vramReserveMb: 3600 });
check(withVram[0].name === "pc" && withVram[0].room === true && withVram[1].name === "laptop" && withVram[1].room === false, "a machine without enough VRAM goes after one with room (it has more RAM)");
const ramOnly = q.rankMachines(cands, { reserveMb: 4000 });
check(ramOnly.every((r) => r.room === true), "a job with no client reserve ignores VRAM (test lane)");
const unknown = q.rankMachines([{ name: "laptop", freeMb: 30000, vramFreeMb: null, multiplier: 2 }, { name: "pc", freeMb: 9000, vramFreeMb: 6000, multiplier: 1 }], { reserveMb: 4000, vramReserveMb: 3600 });
check(unknown.every((r) => r.room === true), "unknown VRAM leaves the RAM verdict alone");
const allFull = q.rankMachines([{ name: "a", freeMb: 30000, vramFreeMb: 500 }, { name: "b", freeMb: 30000, vramFreeMb: 900 }], { vramReserveMb: 1800 });
check(allFull.every((r) => r.room === false), "no machine with VRAM room: all queue");
const noRam = q.rankMachines([{ name: "a", freeMb: null, vramFreeMb: 500 }], { vramReserveMb: 1800 });
check(noRam[0].room === false, "unread RAM with known too-little VRAM is no room");

// --- logged once
const lines = [];
v.resetVramUnknownLog();
v.noteVramUnknown("lf-testbox", (s) => lines.push(s));
v.noteVramUnknown("lf-testbox", (s) => lines.push(s));
v.noteVramUnknown("lf-cloud", (s) => lines.push(s));
check(lines.length === 2 && /lf-testbox/.test(lines[0]) && /RAM only/.test(lines[0]), "unknown VRAM is logged once per machine");
check(v.vramStatusText(6000) === " VRAM 5.9 GB free" && v.vramStatusText(null) === "", "the status text shows VRAM only when known");

if (failures.length) {
  for (const f of failures) console.log(`pool-vram-selftest: FAIL ${f}`);
  console.log("pool-vram-selftest: FAIL");
  process.exit(1);
}
console.log("pool-vram-selftest: OK (nvidia-smi parsing, unknown falls back to RAM-only, per-profile client reserve, 6000 MB fits one full client and not four, test lane ignores VRAM, machine choice)");
