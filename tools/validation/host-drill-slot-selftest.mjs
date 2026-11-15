// tools/validation/host-drill-slot-selftest.mjs
// inputs: tools/host/drill.mjs, tools/host/commands/drill.mjs, tools/remote/pool.mjs, tools/lib/pool-queue.mjs
//
// Proves the drill hands its pool lane to the host it boots (#1637). `host drill` takes a lane from
// the pool, then boots the scratch profile with `host start`; that start took server-2001 for itself
// and failed "lock 'server-2001' is held" whenever a session or test held 2001. Now:
//   1. a local pool slot carries a token in its lock file, so LF_SERVER_SLOT_HELD=<lock>:<token>
//      names it and pool.heldSlotFromEnv accepts it;
//   2. runDrillHere, given that value, puts LF_SERVER_SLOT_HELD in the environment of `host start`
//      and `host stop`, and without it leaves the variable out.
// red-proof: 2026-11-15, before the change tryLocal's slot had no token ("local pool slot has no
// token") and runDrillHere ignored slotEnv ("host start did not see LF_SERVER_SLOT_HELD").

import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const load = (rel) => import(pathToFileURL(join(ROOT, rel)).href);
const pool = await load("tools/remote/pool.mjs");
const drill = await load("tools/host/drill.mjs");
const { safeRemoveTree } = await load("tools/link-safety.mjs");

let failed = 0;
const check = (cond, msg) => {
  if (!cond) {
    console.log(`FAIL  ${msg}`);
    failed++;
  }
};

const tmp = mkdtempSync(join(tmpdir(), "lf-drill-slot-"));
const savedEnv = process.env[pool.SLOT_ENV];
try {
  // 1. A local slot is nameable through LF_SERVER_SLOT_HELD.
  const root = join(tmp, "root");
  mkdirSync(root, { recursive: true });
  spawnSync("git", ["init", "-q", root], { windowsHide: true });
  mkdirSync(join(root, "tmp", "wt", "locks"), { recursive: true });
  const local = { name: "local", via: "local", slots: 1, lanes: null };
  const slot = pool.tryLocal(root, local, { lock: "server-test-1", index: 1, port: 2002 }, { slug: "drill", purpose: "drill", root }, { freeMb: 9000, runs: [] });
  check(!!slot, "tryLocal gave no slot on an empty lane");
  if (slot) {
    check(typeof slot.token === "string" && slot.token.length >= 8, "local pool slot has no token");
    const value = `${slot.lockName}:${slot.token}`;
    process.env[pool.SLOT_ENV] = value;
    let held = null;
    try {
      held = pool.heldSlotFromEnv(root);
    } catch (e) {
      check(false, `heldSlotFromEnv refused the drill's own slot: ${e.message}`);
    }
    check(held && held.lockName === "server-test-1" && held.port === 2002, `heldSlotFromEnv named the wrong lane: ${JSON.stringify(held && { lock: held.lockName, port: held.port })}`);
    delete process.env[pool.SLOT_ENV];
    slot.release();
  }

  // 2. runDrillHere passes the lane to `host start` and `host stop`.
  const fake = join(tmp, "fakeroot");
  mkdirSync(join(fake, "tools"), { recursive: true });
  writeFileSync(
    join(fake, "tools", "cli.mjs"),
    `import { appendFileSync } from "node:fs";\nappendFileSync(process.env.LF_FAKE_SEEN, process.argv[3] + "=" + (process.env.LF_SERVER_SLOT_HELD ?? "none") + "\\n");\n`,
  );
  const seenFile = join(tmp, "seen.txt");
  process.env.LF_FAKE_SEEN = seenFile;
  delete process.env[pool.SLOT_ENV];
  drill.runDrillHere({ root: fake, scratchDir: join(tmp, "scratch1"), timeoutSec: 5, slotEnv: "server-test-1:abc123", log: () => {} });
  const seen = existsSync(seenFile) ? readFileSync(seenFile, "utf8").trim().split("\n") : [];
  check(seen.includes("start=server-test-1:abc123"), `host start did not see LF_SERVER_SLOT_HELD (saw ${seen.join(", ") || "nothing"})`);
  check(seen.includes("stop=server-test-1:abc123"), `host stop did not see LF_SERVER_SLOT_HELD (saw ${seen.join(", ") || "nothing"})`);

  writeFileSync(seenFile, "");
  drill.runDrillHere({ root: fake, scratchDir: join(tmp, "scratch2"), timeoutSec: 5, log: () => {} });
  const seen2 = readFileSync(seenFile, "utf8").trim().split("\n");
  check(seen2.every((l) => l.endsWith("=none")), `a drill with no pool lane set LF_SERVER_SLOT_HELD (saw ${seen2.join(", ")})`);
} finally {
  if (savedEnv === undefined) delete process.env[pool.SLOT_ENV];
  else process.env[pool.SLOT_ENV] = savedEnv;
  delete process.env.LF_FAKE_SEEN;
  safeRemoveTree(tmp);
}

if (failed) {
  console.log(`SUMMARY host-drill-slot-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY host-drill-slot-selftest ok");
