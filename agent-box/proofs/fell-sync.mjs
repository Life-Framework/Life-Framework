import { spawnSync } from "node:child_process";
import { mkdirSync } from "node:fs";
import { join, resolve } from "node:path";
import { bmpFileToPng } from "../bmp.mjs";
import { ROOT, arrive, clientBox, sessionDir } from "../session.mjs";

// #3261: a felled map tree disappears on every client, and for a client that joins late. The server
// publishes the tree's key (what the axe action does after its own checks); each client finds the map
// tree near the key and hides it. Client 1 finds the nearest standing tree and asks the server to publish
// it; client 2 (there from the start) and client 3 (joins after the fell, so it only has the join
// snapshot) must each report that tree `down`. Pictures of the same spot before and after on client 2 and
// after on client 3 are saved under the out dir. Last, the tree is made to stand again and client 2 must
// show it.
//
//   session up --clients 2 --profile full ; session fell-sync [out dir] [--radius 60] [--stand <EntityName>]
//   fell-sync --selftest      the parsers on made-up text, no game
//
// Red: without the client side (RPC handler, RplLoad, the pending retry) client 2 or client 3 reports
// `standing` for the felled tree and the proof fails at "client 2 hides" or "client 3 hides".
const DEFAULT_RADIUS = 60;
const DOWN_SECONDS = 40;

// `key=<key> pos=<x> <y> <z>` from `fellsync find`.
export function parseFind(detail) {
  const m = /key=(\S+) pos=(-?[\d.e+-]+) (-?[\d.e+-]+) (-?[\d.e+-]+)/.exec(String(detail));
  if (!m) return null;
  return { key: m[1], pos: [Number(m[2]), Number(m[3]), Number(m[4])] };
}

// `state=<down|standing|missing> ...` from `fellsync state`.
export function parseState(detail) {
  const m = /state=(down|standing|missing)/.exec(String(detail));
  return m ? m[1] : null;
}

function selftest() {
  const fails = [];
  const find = parseFind("key=12345:-8731:{abc}prefabs/vegetation/tree/pinus/t_pinus_2f.et pos=1234.5 10.25 -873.1");
  if (!find || find.key !== "12345:-8731:{abc}prefabs/vegetation/tree/pinus/t_pinus_2f.et") fails.push("find key");
  if (!find || find.pos.join() !== "1234.5,10.25,-873.1") fails.push("find position");
  if (parseFind("error no tree") !== null) fails.push("find refuses an error");
  if (parseState("state=down mirror=1 hidden=1 pending=0") !== "down") fails.push("state down");
  if (parseState("state=standing mirror=0 hidden=0 pending=0") !== "standing") fails.push("state standing");
  if (parseState("state=missing mirror=1 hidden=0 pending=1") !== "missing") fails.push("state missing");
  if (parseState("nothing") !== null) fails.push("state refuses junk");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "fell-sync selftest: FAIL" : "fell-sync selftest: PASS");
  return fails.length ? 1 : 0;
}

async function runFellSync(args) {
  if (args.includes("--selftest")) return selftest();
  const val = (f, d) => (args.includes(f) ? args[args.indexOf(f) + 1] : d);
  const radius = Number(val("--radius", DEFAULT_RADIUS));
  const stand = val("--stand", "");
  const out = resolve(String(args.find((a, i) => !a.startsWith("--") && !String(args[i - 1] ?? "").startsWith("--")) ?? "tmp/fell-sync"));
  mkdirSync(out, { recursive: true });

  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    if (spec.verb === "capture" && r.status === "ok") {
      const shot = h.resolveCapture(r.detail);
      if (shot) console.log(`  saved ${bmpFileToPng(shot, join(out, `${spec.name}.png`))}`);
    }
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const done = () => {
    for (const f of fails) console.log(`FAIL  ${f}`);
    console.log(fails.length ? "fell-sync: FAIL" : `fell-sync: PASS (a felled tree is gone on both clients and for the late joiner; pictures in ${out})`);
    return fails.length ? 1 : 0;
  };
  const stateOf = (n, key) => parseState(on(n, { verb: "fellsync", words: ["state", key] }, 10000).detail);
  // Bounded: a client that has not streamed the tree in yet reports `missing` until it has.
  const waitFor = (n, key, want) => {
    let seen = null;
    for (let t = 0; t <= DOWN_SECONDS; t += 2) {
      seen = stateOf(n, key);
      if (seen === want) return true;
      wait(2000);
    }
    console.log(`client ${n} still reports ${seen} (wanted ${want}) after ${DOWN_SECONDS} s`);
    return false;
  };

  arrive(on, 1, wait);
  arrive(on, 2, wait);
  wait(8000);
  if (stand) {
    on(1, { verb: "stand", entityName: stand }, 10000);
    on(2, { verb: "stand", entityName: stand }, 10000);
    wait(3000);
  }

  const found = on(1, { verb: "fellsync", words: ["find", String(radius)] }, 10000);
  const tree = found.status === "ok" ? parseFind(found.detail) : null;
  if (!tree) { fails.push(`find: no standing map tree within ${radius} m of client 1 (${found.status} ${found.detail}); try --stand <EntityName>`); return done(); }
  console.log(`tree ${tree.key} at ${tree.pos.join(" ")}`);

  // The same spot on every shot: 8 m west of the tree, looking at it.
  const shot = (n, name) => on(n, { verb: "capture", mode: "world", name, x: tree.pos[0], y: tree.pos[1], z: tree.pos[2], ox: -8, oy: 1.5, oz: 0, hour: 12 }, 25000);
  if (!waitFor(2, tree.key, "standing")) fails.push("client 2 sees the tree standing before the fell");
  shot(2, "fell_sync_before_client2");

  const felled = on(1, { verb: "fellsync", words: ["fell", tree.key] }, 10000);
  if (felled.status !== "ok" || !felled.detail.includes("changed=true") && !felled.detail.includes("changed=1")) fails.push(`the server did not publish the tree: ${felled.status} ${felled.detail}`);

  if (!waitFor(1, tree.key, "down")) fails.push("client 1 hides: the client that asked still shows the tree");
  if (!waitFor(2, tree.key, "down")) fails.push("client 2 hides: a client that was already in shows the tree");
  shot(2, "fell_sync_after_client2");

  // Client 3 joins after the fell: it only has the join snapshot.
  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "3"], { windowsHide: true, stdio: "inherit" });
  if (joined.status !== 0) { fails.push("join: client 3 did not join late"); return done(); }
  arrive(on, 3, wait);
  wait(6000);
  if (!waitFor(3, tree.key, "down")) fails.push("client 3 hides: the late joiner shows the tree");
  shot(3, "fell_sync_after_client3_late");

  // Regrow: the tree stands again for everyone.
  on(1, { verb: "fellsync", words: ["up", tree.key] }, 10000);
  if (!waitFor(2, tree.key, "standing")) fails.push("client 2 shows the tree again after it regrows");
  if (!waitFor(3, tree.key, "standing")) fails.push("client 3 shows the tree again after it regrows");
  return done();
}

export default {
  name: "fell-sync",
  noReset: true,
  usage: "fell-sync [out dir] [--radius 60] [--stand <EntityName>] | --selftest   (session up --clients 2 --profile full)",
  run: (ctx) => runFellSync(ctx.args ?? []),
};
