// Proves two overlapping `look --serve` requests from different worktrees cannot clobber each other's shots
// (tools/visual/look-serve.mjs claimAnswer, docs/visual-testing.md "Look server").
// On 2026-11-27 the villages sweep got 70 of 70 shots missing twice: look released the build lock and then collected
// its PNGs from the server's shared raw folder, and the next request cleared that folder (sendRequest removes every
// .png, a restart empties it). This runs two requests against one fake server folder: A answers, releases the lock and
// collects slowly; B takes the lock in between, restarts the server (empties the raw folder) and answers. Each must
// collect exactly its own shots, moved into its worktree on another drive (a rename there fails with EXDEV).
// inputs: tools/visual/look-serve.mjs, tools/visual/look.mjs
// red-proof: with moveFile rethrowing EXDEV (the old renameSync) it printed "FAIL A: 0 of 35 shots moved into the worktree
//   across drives"; with claimAnswer returning the shared raw folder (the old collect) the replay printed "FAIL A: 0 of 35
//   shots collected"; with serveLook's `raw: own` put back to `raw: res.raw` it printed "FAIL serveLook returns the claimed answer folder".
import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, statSync, utimesSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { scratchDir } from "../lib/scratch.mjs";
import * as serve from "../visual/look-serve.mjs";
import { moveFile } from "../visual/look.mjs";

const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg.startsWith("FAIL") ? msg : `FAIL ${msg}`); };
// rename across drives (the viewer on C:, worktrees on D:) fails with EXDEV, as on PC1 on 2026-11-27.
const crossDrive = () => { const e = new Error("EXDEV: cross-device link not permitted"); e.code = "EXDEV"; throw e; };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const base = scratchDir("lf-look-overlap-");
const profile = join(base, "visual-serve");
const raw = serve.rawOf(profile);
const answers = serve.answersOf(profile);
mkdirSync(raw, { recursive: true });
writeFileSync(join(raw, "serve.txt"), "1\n");
writeFileSync(join(raw, "alive.txt"), "1\n");

// The build lock, as one promise chain: one request at a time, like wt.acquireLock("build").
let chain = Promise.resolve();
const withLock = (fn) => { const run = chain.then(fn); chain = run.catch(() => {}); return run; };

// What the server does for a request: the inbox cleared the way sendRequest clears it, or the whole folder emptied
// the way startServer does on a restart, then the shots, status and done line written.
function answer(id, shots, { restart = false } = {}) {
  for (const f of readdirSync(raw)) if (restart ? true : serve.REQUEST_FILES.test(f)) rmSync(join(raw, f), { force: true });
  if (restart) writeFileSync(join(raw, "serve.txt"), "1\n");
  writeFileSync(join(raw, "plan.txt"), `plan ${id}\n`);
  for (let i = 0; i < shots; i++) writeFileSync(join(raw, `${id}-shot${i}.png`), `png ${id} ${i}`);
  writeFileSync(join(raw, "dump-area.txt"), `dump ${id}\n`);
  writeFileSync(join(raw, "status.txt"), `request ${id}\n`);
  writeFileSync(join(raw, "done.txt"), `${id}\n`);
}

// One look: under the lock the server answers and the answer is claimed (what serveLook does before it returns); the
// lock is released; then the slow collect reads every shot from the folder it was handed.
async function look(id, shots, { restart = false, collectDelayMs = 0, startDelayMs = 0 } = {}) {
  await sleep(startDelayMs);
  const got = await withLock(async () => {
    answer(id, shots, { restart });
    return serve.claimAnswer(raw, answers, id);
  });
  await sleep(collectDelayMs);
  let collected = 0;
  let foreign = 0;
  for (let i = 0; i < shots; i++) {
    const f = join(got, `${id}-shot${i}.png`);
    if (existsSync(f) && readFileSync(f, "utf8") === `png ${id} ${i}`) collected++;
  }
  for (const f of readdirSync(got)) if (/\.png$/.test(f) && !f.startsWith(`${id}-`)) foreign++;
  // The collect moves each shot into the caller's worktree, on another drive than the viewer: rename throws EXDEV.
  const out = join(base, `out-${id}`);
  mkdirSync(out, { recursive: true });
  let moved = 0;
  for (let i = 0; i < shots; i++) {
    const from = join(got, `${id}-shot${i}.png`);
    try {
      moveFile(from, join(out, `shot${i}.png`), { rename: crossDrive });
      if (readFileSync(join(out, `shot${i}.png`), "utf8") === `png ${id} ${i}` && !existsSync(from)) moved++;
    } catch {}
  }
  const status = existsSync(join(got, "status.txt")) ? readFileSync(join(got, "status.txt"), "utf8").trim() : "";
  const dump = existsSync(join(got, "dump-area.txt")) ? readFileSync(join(got, "dump-area.txt"), "utf8").trim() : "";
  if (got.startsWith(answers)) rmSync(got, { recursive: true, force: true });
  return { id, shots, collected, moved, foreign, status, dump, dir: got };
}

// A answers first and collects for 300 ms; B starts 50 ms later, restarts the server and answers inside A's collect.
const [a, b] = await Promise.all([
  look("A-1", 35, { collectDelayMs: 300 }),
  look("B-2", 35, { restart: true, startDelayMs: 50 }),
]);
for (const r of [a, b]) {
  check(r.collected === r.shots, `${r.id.slice(0, 1)}: ${r.collected} of ${r.shots} shots collected`);
  check(r.moved === r.shots, `${r.id.slice(0, 1)}: ${r.moved} of ${r.shots} shots moved into the worktree across drives`);
  check(r.foreign === 0, `${r.id.slice(0, 1)}: ${r.foreign} shot(s) of the other request in its folder`);
  check(r.status === `request ${r.id}`, `${r.id.slice(0, 1)}: status.txt is its own (got "${r.status}")`);
  check(r.dump === `dump ${r.id}`, `${r.id.slice(0, 1)}: the dump is its own (got "${r.dump}")`);
}
check(a.dir !== b.dir, "each request has its own answer folder");
// The server's own files stay where the plugin reads them; the request inbox is left for the next request to clear.
check(existsSync(join(raw, "serve.txt")) && existsSync(join(raw, "plan.txt")), "serve.txt and plan.txt stay in the raw folder");
check(!readdirSync(raw).some((f) => /\.png$|^status\.txt$|^done\.txt$/.test(f)), "no answer file is left in the shared folder");

// Leftover answer folders (a caller that crashed, or --keep) are pruned by age, fresh ones kept.
mkdirSync(join(answers, "old"), { recursive: true });
mkdirSync(join(answers, "new"), { recursive: true });
const old = (Date.now() - 7 * 60 * 60 * 1000) / 1000;
utimesSync(join(answers, "old"), old, old);
check(serve.pruneAnswers(answers) === 1 && !existsSync(join(answers, "old")) && existsSync(join(answers, "new")), "pruneAnswers drops folders over 6 h old only");
check(serve.pruneAnswers(join(base, "nope")) === 0, "pruneAnswers on a missing folder is a no-op");

// The wiring: serveLook claims before it returns and hands look the claimed folder; look removes that folder whole.
const here = dirname(fileURLToPath(import.meta.url));
const srv = readFileSync(join(here, "..", "visual", "look-serve.mjs"), "utf8");
const lk = readFileSync(join(here, "..", "visual", "look.mjs"), "utf8");
const body = srv.slice(srv.indexOf("export async function serveLook"), srv.indexOf("export async function serveCommand"));
check(/const own = serve?\.?claimAnswer\(res\.raw, answersOf\(st\.profile\), id\)|const own = claimAnswer\(res\.raw, answersOf\(st\.profile\), id\)/.test(body) && /raw: own/.test(body), "FAIL serveLook returns the claimed answer folder");
check(/else rmSync\(raw, \{ recursive: true, force: true \}\)/.test(lk), "look removes a served answer folder whole");
check(statSync(raw).isDirectory(), "the raw folder survives");

if (fails.length) {
  for (const f of fails) console.log(f);
  process.exit(1);
}
console.log("look-serve-overlap-selftest: ok");
