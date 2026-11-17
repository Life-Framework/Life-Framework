// tools/validation/starter-packs-selftest.mjs
// inputs: tools/**, agent-box/**, .githooks/**, addons/**, docs/**, website/**, video/**
//
// Keeps the starter packs (docs/starter-packs/, printed by `tools\cli brief <kind>`) short and complete: every pack is one
// page (at most MAX_LINES), has a "Known traps" list and the standard environment notes (no Workbench here, never pin,
// foreground or t3_background_wait, never --force a worktree), and `brief` resolves every kind and refuses an unknown one.
// red-proof: with the "## Known traps" heading removed from docs/starter-packs/grid.md this printed
// "FAIL grid: has a Known traps list" and exited 1 (2026-11-16); restored, it passes.
import { kinds, main, pack, resolveKind } from "../commands/brief.mjs";

const MAX_LINES = 80;
const KINDS = ["building", "grid", "map", "slice", "town"];
let failed = 0;
const ok = (cond, what) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${what}`);
  }
};

ok(KINDS.every((k) => kinds().includes(k)), `the packs are ${KINDS.join(", ")}`);
for (const k of KINDS) {
  const text = pack(k);
  const lines = text.split("\n").length;
  ok(text.startsWith("# Starter pack:"), `${k}: starts with its title`);
  ok(lines <= MAX_LINES, `${k}: is one page (${lines} lines, at most ${MAX_LINES})`);
  ok(/^## Known traps$/m.test(text), `${k}: has a Known traps list`);
  const env = text.split(/^## Environment$/m)[1] || "";
  ok(/No Workbench on this PC/.test(env) && /never pin/i.test(env), `${k}: environment says no Workbench here and never pin`);
  ok(/foreground/.test(env) && /t3_background_wait/.test(env), `${k}: environment says foreground or t3_background_wait`);
  ok(/--force/.test(env) && /wt prune/.test(env), `${k}: environment says never --force or rm -rf a worktree`);
  ok(!/\r/.test(text), `${k}: LF line endings`);
  ok(resolveKind(k) === k, `${k}: brief resolves it`);
}
ok(resolveKind("fitout") === "building" && resolveKind("everon") === "town" && resolveKind("--map") === "map", "aliases and --kind resolve");
ok(resolveKind("nope") === null, "an unknown kind resolves to nothing");

const out = [];
const log = console.log;
console.log = (...a) => out.push(a.join(" "));
const w = process.stdout.write.bind(process.stdout);
let printed = "";
process.stdout.write = (s) => { printed += s; return true; };
let rc = [];
try {
  rc = [await main(["slice"]), await main(["nope"]), await main([])];
} finally {
  process.stdout.write = w;
  console.log = log;
}
ok(rc[0] === 0 && printed.startsWith("# Starter pack: slice card"), "brief slice prints the slice pack");
ok(rc[1] === 2 && out.some((l) => /no starter pack 'nope'/.test(l)), "brief nope fails and lists the kinds");
ok(rc[2] === 0 && KINDS.every((k) => out.some((l) => l.includes(k))), "brief alone lists every kind");

if (failed) {
  console.log(`SUMMARY starter-packs-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY starter-packs-selftest ok");
