import { spawnSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, clientSees, parseState, readSession, sessionDir } from "../session.mjs";

// The wanted board's face (docs/architecture/visual-boards-spike.md) on a running `up --clients 1`
// session (`--profile full` for the pictures): client 1 robs the fuel till and its own board shows
// the poster; client 2 then joins late, after the change, and must draw it from RplLoad; client 2
// walks out of the draw radius (the face is released) and back (rebuilt); client 1's wanted level
// is cleared and the poster leaves both boards by the broadcast. The face state comes from the
// harness `seen` (`posters=<count>/<live|frozen|released>`); the client logs give the source.
// The fixtures stand in this proof's own grid cell (Grid_wanted_posters.layer, `tools\cli grid place wanted-posters`), so it runs on
// `session up --clients 1 --mission grid` (three-worlds.md, #1897). Where the board is comes from the layer, never from a constant.
export const BOARD = "Wanted_Board";
export const LOCKER = "Wanted_Locker"; // the character steps here, out of the film camera's picture
export const TILL = "Wanted_Till"; // robbed to put a poster on the board
// The "far" point is a cell fixture plus a stand side up to 30 m out: the face is released only beyond the draw radius (30 m) and its
// margin (5 m), which no point inside a 12 m cell reaches. The proof measures the distance it got to rather than assuming it.
const RELEASE_BEYOND = 35.2;
const FAR_STANDS = [[TILL, "e30"], [LOCKER, "e30"], [TILL, "w30"], [LOCKER, "w30"]];
const CELL_LAYER = join(ROOT, "addons", "LifeFramework", "Worlds", "GridWorld", "GridWorld_Layers", "Grid_wanted_posters.layer");

// One entity of the layer text: { x, y, z, angleY } from its own `coords` and `angleY` lines, or null.
export function entityAt(layerText, name) {
  const m = new RegExp("^\\S+ " + name + "\\b[^\\n]*\\{\\n([\\s\\S]*?)\\n\\}", "m").exec(layerText);
  if (!m) return null;
  const c = /^ coords (-?[\d.]+) (-?[\d.]+) (-?[\d.]+)/m.exec(m[1]);
  if (!c) return null;
  return { x: Number(c[1]), y: Number(c[2]), z: Number(c[3]), angleY: Number(/^ angleY (-?[\d.]+)/m.exec(m[1])?.[1] ?? 0) };
}

// Offline: the board, the locker and the till are in the cell. No session needed.
export function checkFixtures(layerText) {
  return [BOARD, LOCKER, TILL].filter((n) => !entityAt(layerText, n)).map((n) => `${n} is not in Grid_wanted_posters.layer`);
}

function runCheck() {
  const fails = existsSync(CELL_LAYER) ? checkFixtures(readFileSync(CELL_LAYER, "utf8")) : ["Grid_wanted_posters.layer does not exist (tools\\cli grid place wanted-posters)"];
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "wanted-posters-proof --check: FAIL" : "wanted-posters-proof --check: PASS (board, locker and till are in the cell)");
  return fails.length ? 1 : 0;
}

export function postersOf(seen) {
  const m = /^(\d+)\/(\w+)$/.exec(seen?.posters ?? "");
  return m ? { count: Number(m[1]), face: m[2] } : null;
}

function clientLog(dir, n) {
  const logs = join(dir, `client${n}`, "logs");
  if (!existsSync(logs)) return "";
  return readdirSync(logs).sort().map((d) => join(logs, d, "console.log")).filter(existsSync).map((p) => readFileSync(p, "utf8")).join("\n");
}

async function runWantedPostersProof(args = []) {
  if (args.includes("--check")) return runCheck();
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const wrong = h.missionProblem(readSession(dir), "grid");
  if (wrong) {
    console.log(`FAIL  ${wrong}`);
    return 1;
  }
  const board = existsSync(CELL_LAYER) ? entityAt(readFileSync(CELL_LAYER, "utf8"), BOARD) : null;
  if (!board) {
    console.log(`FAIL  ${BOARD} is not in Grid_wanted_posters.layer (tools\\cli grid place wanted-posters)`);
    return 1;
  }
  // The face centre sits 1.2 m above the board's origin; the board's front is its local +Z, turned by angleY.
  const rad = (board.angleY * Math.PI) / 180;
  const front = { x: Math.sin(rad), z: Math.cos(rad) };
  const BOARD_AT = { x: board.x, y: board.y + 1.2, z: board.z };
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 220)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const expect = (n, label, judge, budgetMs = 15000) => {
    const r = clientSees(on, wait, n, (s) => {
      const p = postersOf(s);
      return p ? judge(p) : [`no posters field: ${s.posters}`];
    }, { label, budgetMs });
    fails.push(...r.fails);
    if (!r.fails.length) console.log(`ok    ${label} (${r.seconds.toFixed(1)} s)`);
    return r;
  };
  // A fixed film camera in front of the board (the character camera cannot pitch down to it), HUD off.
  const shot = (n, name, back = 1.7, lift = 0.45) => {
    // The character steps behind the board, out of the picture.
    on(n, { verb: "stand", entityName: LOCKER });
    wait(1500);
    on(n, { verb: "camera", args: "hud off" });
    on(n, { verb: "camera", args: "stop" });
    on(n, { verb: "camera", args: `static ${BOARD_AT.x + front.x * back} ${BOARD_AT.y + lift} ${BOARD_AT.z + front.z * back} ${BOARD_AT.x} ${BOARD_AT.y} ${BOARD_AT.z} 60 50` });
    wait(2500);
    const r = on(n, { verb: "capture", mode: "ui", name }, 25000);
    on(n, { verb: "camera", args: "stop" });
    on(n, { verb: "camera", args: "hud on" });
    return r;
  };

  arrive(on, 1, wait);
  wait(6000);
  on(1, { verb: "stand", entityName: BOARD });
  wait(3000);
  expect(1, "an empty board draws its face near the viewer", (p) => (p.count === 0 && p.face !== "released" ? [] : [`posters=${p.count}/${p.face}`]));

  on(1, { verb: "stand", entityName: TILL });
  wait(2500);
  on(1, { verb: "look" });
  const rob = on(1, { verb: "use", className: "LF_RobAction", entityName: TILL }, 10000);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  wait(6000);
  on(1, { verb: "stand", entityName: BOARD });
  expect(1, "the robber's poster reaches the robber's board", (p) => (p.count === 1 ? [] : [`posters=${p.count}/${p.face}`]));
  // The live window's cost (one portrait) against the frozen face, on `--levers uncapped`.
  on(1, { verb: "perf", seconds: 2, label: "face_live" }, 20000);
  expect(1, "the face freezes after its live window", (p) => (p.face === "frozen" ? [] : [`face=${p.face}`]), 10000);
  shot(1, "wanted_posters_robbery_client1");
  shot(1, "wanted_posters_portrait_client1", 0.75, 0.15);
  wait(25000);
  on(1, { verb: "perf", seconds: 5, label: "face_frozen" }, 25000);
  shot(1, "wanted_posters_street_client1", 6, 1.1);

  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "2"], { windowsHide: true, stdio: "inherit" });
  if (joined.status !== 0) fails.push("client 2 did not join late");
  else {
    arrive(on, 2, wait);
    wait(6000);
    on(2, { verb: "stand", entityName: BOARD });
    expect(2, "the late joiner draws the poster it got on join", (p) => (p.count === 1 && p.face !== "released" ? [] : [`posters=${p.count}/${p.face}`]), 20000);
    // After the face's late refresh (30 s after it was built), so streamed-in textures are on it.
    wait(33000);
    shot(2, "wanted_posters_late_client2");

    // Out of the draw radius and back: the face is released and rebuilt.
    // Out past the release distance: 30 m out from a cell fixture, on whichever side lands beyond it.
    let away = 0;
    for (const [entityName, side] of FAR_STANDS) {
      on(2, { verb: "stand", entityName, side });
      wait(3000);
      const [fx, , fz] = String(parseState(on(2, { verb: "state" }, 10000).detail).pos ?? "0,0,0").split(",").map(Number);
      away = Math.hypot(fx - board.x, fz - board.z);
      if (away > RELEASE_BEYOND) break;
    }
    console.log(`client 2 stands ${away.toFixed(1)} m from the board`);
    if (away <= RELEASE_BEYOND) fails.push(`no far point more than ${RELEASE_BEYOND} m from the board (best ${away.toFixed(1)} m)`);
    on(1, { verb: "resetcrime" }, 10000);
    expect(1, "a cleared wanted level takes the poster down", (p) => (p.count === 0 ? [] : [`posters=${p.count}/${p.face}`]));
    expect(2, "the far client hears the change too", (p) => (p.count === 0 ? [] : [`posters=${p.count}/${p.face}`]));
    on(2, { verb: "stand", entityName: BOARD });
    expect(2, "back in range the face is rebuilt with the current list", (p) => (p.count === 0 && p.face !== "released" ? [] : [`posters=${p.count}/${p.face}`]));
  }

  on(1, { verb: "resetcrime" }, 10000);
  const log1 = clientLog(dir, 1);
  const log2 = clientLog(dir, 2);
  if (!/WantedPosters\] client: 1 poster\(s\) from rpc/.test(log1)) fails.push("client 1 never logged the poster arriving by broadcast");
  if (!/WantedPosters\] portrait: dressed dummy with [1-9]\d* item\(s\)/.test(log1)) fails.push("client 1 never drew the robber's portrait as a dressed dummy");
  if (!/WantedPosters\] client: 1 poster\(s\) from jip/.test(log2)) fails.push("client 2 never logged the poster arriving on join (jip)");
  if (!new RegExp(`WantedPosters\\] client: face released at <${Math.floor(board.x)}`).test(log2)) fails.push("client 2 never released the board's face out of range");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "wanted posters proof: FAIL" : "wanted posters proof: PASS (broadcast, late join, distance gate, poster leaves)");
  return fails.length ? 1 : 0;
}

export default {
  name: "wanted-posters-proof",
  usage: "wanted-posters-proof [--check]   (up --clients 1 --mission grid; client 2 joins late); --check is the offline fixture check",
  run: (ctx) => runWantedPostersProof(ctx?.args ?? []),
};
