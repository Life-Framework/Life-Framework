import { spawnSync } from "node:child_process";
import { join } from "node:path";
import { ROOT, arrive, clientBox, sessionDir } from "../session.mjs";

// #3178: a planted crop reaches a client that joins after the planting, on a dedicated server, and the
// world reset (#2820) clears it for every client. Client 1 asks the server to plant a cabbage two metres
// ahead of it (`plantsync plant`), the server's farming clock moves on one stage (`plantsync grow`),
// client 2 joins late (it only has the game mode's join snapshot, LF_PlantReplication's RplLoad), and
// both clients must report the same plant id at the same stage (`plantsync state` reads each client's
// own mirror). Then the server runs the world reset and both must report no plants.
//
//   session up --clients 1 --profile lowspec --mission island ; session plant-late-proof [--stand <EntityName>]
//   plant-late-proof --selftest      the parser and the judge on made-up text, no game
//
// Red: without the RplLoad override (or with Write/Read out of step) client 2 reports rows=0 or another
// stage and the proof fails at "late joiner"; without the clear broadcast a client keeps its row after
// the reset and it fails at "reset".
const SHOW_SECONDS = 30;
// The island (`session up --mission island`, the flagship header that carries the pack's crops): spawn is
// the Saint-Philippe safe zone, so the plant is made out at the farm compound. `--stand ""` plants at spawn.
const DEFAULT_STAND = "Fit_farmcompound_compound_site_flag";
const SIDES = ["s9", "n9", "e9", "w9", "s20", "n20"];

// `side=client rows=1 meshes=1 p123-1:1 p124-2:0` -> { side, rows: { id: stage }, count, meshes }.
export function parseState(detail) {
  const text = String(detail);
  const head = /side=(\w+) rows=(\d+) meshes=(\d+)/.exec(text);
  if (!head) return null;
  const rows = {};
  for (const m of text.slice(head.index + head[0].length).matchAll(/(\S+):(-?\d+)/g)) {
    if (m[1].includes("=")) break;
    rows[m[1]] = Number(m[2]);
  }
  return { side: head[1], count: Number(head[2]), meshes: Number(head[3]), rows };
}

// `planted=p123-1 | ...` -> "p123-1".
export function parsePlanted(detail) {
  const m = /planted=(\S+)/.exec(String(detail));
  return m ? m[1] : null;
}

// Pure: does a client's state show exactly the plant at the stage.
export function judgeShows(state, plantId, stage) {
  if (!state) return { ok: false, detail: "no readable state" };
  const got = state.rows[plantId];
  const ok = state.side === "client" && got === stage;
  return { ok, detail: `${state.side} shows ${state.count} plant(s), ${plantId} at stage ${got ?? "none"} (want ${stage})` };
}

function selftest() {
  const fails = [];
  const s = parseState("side=client rows=2 meshes=2 p1800000000-1:1 p1800000000-2:0");
  if (!s || s.count !== 2 || s.rows["p1800000000-1"] !== 1 || s.rows["p1800000000-2"] !== 0) fails.push("parse two rows");
  if (parseState("side=client rows=0 meshes=0")?.count !== 0) fails.push("parse empty");
  if (parseState("error usage") !== null) fails.push("parse refuses junk");
  if (parsePlanted("planted=p9-3 | side=server rows=1 meshes=0 p9-3:0") !== "p9-3") fails.push("planted id");
  if (parsePlanted("plantsync plant refused: PLACE_REFUSED SAFE_ZONE") !== null) fails.push("planted refuses a refusal");
  if (!judgeShows(s, "p1800000000-1", 1).ok) fails.push("judge same stage");
  if (judgeShows(s, "p1800000000-1", 2).ok) fails.push("judge other stage");
  if (judgeShows(parseState("side=client rows=0 meshes=0"), "p1", 1).ok) fails.push("judge missing row");
  if (judgeShows(parseState("side=server rows=1 meshes=0 p1:1"), "p1", 1).ok) fails.push("judge a server read");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "plant-late-proof selftest: FAIL" : "plant-late-proof selftest: PASS");
  return fails.length ? 1 : 0;
}

async function runPlantLate(args) {
  if (args.includes("--selftest")) return selftest();
  const val = (f, d) => (args.includes(f) ? args[args.indexOf(f) + 1] : d);
  const stand = val("--stand", DEFAULT_STAND);

  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const done = () => {
    for (const f of fails) console.log(`FAIL  ${f}`);
    console.log(fails.length
      ? "plant-late-proof: FAIL"
      : "plant-late-proof: PASS (a late joiner on a dedicated server shows the planted crop at the server's stage, and the world reset clears it on both clients)");
    return fails.length ? 1 : 0;
  };
  const stateOf = (n) => parseState(on(n, { verb: "plantsync", words: ["state"] }, 10000).detail);
  // Bounded: a broadcast or a join snapshot lands within a few seconds.
  const waitShows = (n, plantId, stage) => {
    let j = null;
    for (let t = 0; t <= SHOW_SECONDS; t += 2) {
      j = judgeShows(stateOf(n), plantId, stage);
      if (j.ok) return j;
      wait(2000);
    }
    return j;
  };
  const waitEmpty = (n) => {
    let s = null;
    for (let t = 0; t <= SHOW_SECONDS; t += 2) {
      s = stateOf(n);
      if (s && s.count === 0 && s.meshes === 0) return true;
      wait(2000);
    }
    console.log(`client ${n} still shows ${s ? `${s.count} plant(s), ${s.meshes} mesh(es)` : "no readable state"} after ${SHOW_SECONDS} s`);
    return false;
  };

  arrive(on, 1, wait);
  wait(6000);

  // 2. Client 1 plants a cabbage; the server answers with the plant id and its own table. The spawn is a
  // safe zone, so it walks out to `--stand` first and tries each side until one is open soil.
  let planted = null;
  let plantId = null;
  for (const side of stand ? SIDES : [""]) {
    if (stand) {
      on(1, { verb: "stand", entityName: stand, side }, 10000);
      wait(3000);
    }
    planted = on(1, { verb: "plantsync", words: ["plant", "cabbage"] }, 15000);
    plantId = planted.status === "ok" ? parsePlanted(planted.detail) : null;
    if (plantId) break;
  }
  if (!plantId) { fails.push(`plant: the server did not plant a cabbage (${planted.status} ${planted.detail}); try --stand <EntityName> on open soil outside a safe zone`); return done(); }
  const first = waitShows(1, plantId, 0);
  console.log(`client 1 after planting: ${first.detail}`);
  if (!first.ok) fails.push(`planting: ${first.detail}`);

  // 3. One growth stage on the server clock.
  const grown = on(1, { verb: "plantsync", words: ["grow", "cabbage"] }, 15000);
  const serverStage = parseState(String(grown.detail).split("|").pop())?.rows[plantId];
  if (grown.status !== "ok" || serverStage !== 1) { fails.push(`grow: the server's table shows ${plantId} at stage ${serverStage ?? "none"}, want 1 (${grown.status} ${grown.detail})`); return done(); }
  const one = waitShows(1, plantId, 1);
  console.log(`client 1 after one stage: ${one.detail}`);
  if (!one.ok) fails.push(`client 1 grows: ${one.detail}`);

  // 4. Client 2 joins late: it has only the join snapshot.
  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "2"], { windowsHide: true, stdio: "inherit" });
  if (joined.status !== 0) { fails.push("join: client 2 did not join late"); return done(); }
  arrive(on, 2, wait);
  wait(4000);
  const two = waitShows(2, plantId, 1);
  console.log(`client 2 (late): ${two.detail}`);
  if (!two.ok) fails.push(`late joiner: ${two.detail}`);
  const twoState = stateOf(2);
  if (twoState && twoState.meshes !== twoState.count) fails.push(`late joiner draws ${twoState.meshes} mesh(es) for ${twoState.count} plant(s)`);

  // 5. The world reset clears it for both.
  const reset = on(1, { verb: "plantsync", words: ["reset"] }, 20000);
  if (reset.status !== "ok") fails.push(`reset: ${reset.status} ${reset.detail}`);
  if (!waitEmpty(1)) fails.push("reset: client 1 still shows the crop");
  if (!waitEmpty(2)) fails.push("reset: client 2 still shows the crop");
  return done();
}

export default {
  name: "plant-late-proof",
  noReset: true,
  usage: "plant-late-proof [--stand <EntityName>] | --selftest   (session up --clients 1 --profile lowspec --mission island)",
  run: (ctx) => runPlantLate(ctx.args ?? []),
};
