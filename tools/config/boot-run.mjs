// The boot stage runner (#299): one fresh dedicated server per `"boot": true` fixture.
//   tools\cli config-harness --stage boot [--only <name part>] [--dir <fixtures>]
// Per fixture: the fixture text goes to tmp/config-boot/<name>/config.json, `session up --clients 0 --wait
// --seed <that>` boots a server-only session on whichever pool machine has room (the pool queues it and
// picks the machine; the same boot code every proof uses), `session config-boot-proof --fixture <name>`
// reads that server's log where it runs and judges the sidecar's `bootLog`, `session down` frees the lane.
// The reloadable fixtures stay on the shared server (agent-box/proofs/config-harness-proof.mjs): faster.
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { loadFixtures, isBoot } from "./harness.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const session = (args, opts = {}) => spawnSync(process.execPath, [join(ROOT, "agent-box", "session.mjs"), ...args], { stdio: "inherit", cwd: ROOT, windowsHide: true, ...opts });

export function runBootStage(dir, only = "") {
  const fixtures = loadFixtures(dir).filter((f) => isBoot(f) && f.name.includes(only));
  if (!fixtures.length) { console.log("config-harness: no fixture says \"boot\": true"); return 2; }
  const t0 = Date.now();
  const bad = [];
  for (const fx of fixtures) {
    const slug = fx.name.replace(/[^a-z0-9]+/gi, "-");
    const seed = join(ROOT, "tmp", "config-boot", slug, "config.json");
    mkdirSync(dirname(seed), { recursive: true });
    writeFileSync(seed, fx.text);
    console.log(`config-harness boot: ${fx.name}`);
    const t = Date.now();
    try {
      if (session(["up", "--clients", "0", "--wait", "--seed", seed]).status !== 0) { bad.push(`${fx.name}: session up failed`); continue; }
      const r = session(["config-boot-proof", "--fixture", fx.name.split("/").pop(), ...(dir ? ["--dir", dir] : [])]);
      if (r.status !== 0) bad.push(`${fx.name}: the server log did not match`);
      else console.log(`PASS  ${fx.name}  (${Math.round((Date.now() - t) / 1000)} s)`);
    } finally {
      session(["down"]);
    }
  }
  for (const b of bad) console.log(`FAIL  ${b}`);
  console.log(`config-harness boot: ${bad.length ? `${bad.length} of ${fixtures.length} failing` : `OK (${fixtures.length} fixture(s), ${Math.round((Date.now() - t0) / 1000)} s)`}`);
  return bad.length ? 1 : 0;
}
