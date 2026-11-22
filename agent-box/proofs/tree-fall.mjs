import { mkdirSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// A felled tree falls over and leaves its logs, seen by another player on a dedicated server.
// Client 1 stands 2.2 m west of the nearest standing tree and fells it (`treefall fell`, the server runs
// LF_FellAction.TryFell); client 2 stands to the south and films it from a static camera. Pass: the server
// drops one log per yield log along the line, client 2 draws the fall and lays branch props, and the clip
// and stills are saved.
//
//   session up --clients 2 --profile full ; session tree-fall [out dir] [--radius 80]
//   tree-fall --selftest      the parsers on made-up text, no game

// `key=<key> pos=<x> <y> <z>` from `fellsync find`.
export function parseFind(detail) {
  const m = /key=(\S+) pos=(-?[\d.e+-]+) (-?[\d.e+-]+) (-?[\d.e+-]+)/.exec(String(detail));
  if (!m) return null;
  return { key: m[1], pos: [Number(m[2]), Number(m[3]), Number(m[4])] };
}

// `tree fall: N of M logs dropped` from the server log.
export function parseDrop(text) {
  const m = /tree fall: (\d+) of (\d+) logs dropped/.exec(String(text));
  return m ? { spawned: Number(m[1]), planned: Number(m[2]) } : null;
}

function selftest() {
  const fails = [];
  const f = parseFind("key=1:2:prefabs/vegetation/tree/x/t_x_2f.et pos=10.5 3 -4");
  if (!f || f.pos.join() !== "10.5,3,-4") fails.push("find");
  const d = parseDrop("[LFDebug:Lumber] tree fall: 2 of 2 logs dropped along yaw 90 (2 loose on the server)");
  if (!d || d.spawned !== 2 || d.planned !== 2) fails.push("drop");
  if (parseDrop("nothing")) fails.push("drop refuses junk");
  for (const x of fails) console.log(`FAIL  ${x}`);
  console.log(fails.length ? "tree-fall selftest: FAIL" : "tree-fall selftest: PASS");
  return fails.length ? 1 : 0;
}

const readLog = (dir) => {
  const f = newestLog(dir);
  if (!f) return "";
  try {
    return readFileSync(f, "utf8");
  } catch {
    return "";
  }
};

async function run(args) {
  if (args.includes("--selftest")) return selftest();
  const val = (flag, d) => (args.includes(flag) ? args[args.indexOf(flag) + 1] : d);
  const radius = Number(val("--radius", 80));
  const out = resolve(String(args.find((a, i) => !a.startsWith("--") && !String(args[i - 1] ?? "").startsWith("--")) ?? "tmp/tree-fall"));
  mkdirSync(out, { recursive: true });

  const h = await import("../harness.mjs");
  const eng = await import("../../tools/film/engine.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 200)}`);
    if (spec.verb === "capture" && r.status === "ok") {
      const shot = h.resolveCapture(r.detail);
      if (shot) console.log(`  saved ${bmpFileToPng(shot, join(out, `${spec.name}.png`))}`);
    }
    return r;
  };
  const send = (n) => (spec, t) => { h.setBoxes([clientBox(dir, n)]); return h.sendCommand(spec, t); };
  const resolveAlias = (alias) => { h.setBoxes([clientBox(dir, 2)]); return h.resolveCapture(alias); };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];

  arrive(on, 1, wait);
  arrive(on, 2, wait);
  wait(6000);

  const found = on(1, { verb: "treefall", words: ["find", String(radius)] }, 20000);
  const tree = found.status === "ok" ? parseFind(found.detail) : null;
  if (!tree) {
    console.log(`FAIL  no standing tree within ${radius} m (${found.status} ${found.detail})`);
    return 1;
  }
  const [x, y, z] = tree.pos;
  console.log(`tree ${tree.key} at ${x} ${y} ${z}`);

  // Client 1 west of the trunk (it falls east), client 2 to the south; both put there by a world capture.
  on(1, { verb: "capture", mode: "world", name: "tree_fall_feller_view", x, y, z, ox: -2.2, oy: 1.6, oz: 0, hour: 12 }, 25000);
  on(2, { verb: "capture", mode: "world", name: "tree_fall_before", x: x + 5, y: y + 4, z, ox: 0, oy: 1.6, oz: -24, hour: 12 }, 25000);
  wait(2000);
  // Behind the feller, looking along the line the tree falls (east).
  const cam = `static ${x - 9} ${y + 3.5} ${z - 4} ${x + 6} ${y + 3} ${z} 70 50`;
  on(2, { verb: "camera", args: "hud off" });
  on(2, { verb: "camera", args: "stop" });
  on(2, { verb: "camera", args: cam });
  wait(2500);
  on(2, { verb: "capture", mode: "ui", name: "tree_fall_0_standing" }, 25000);

  const start = eng.engineStart(send(2), "tree_fall", { fps: 30, seconds: 30 });
  if (!start.ok) fails.push(`film start failed: ${start.error}`);
  wait(800);
  const felled = on(1, { verb: "treefall", words: ["fell", tree.key] }, 15000);
  if (felled.status !== "ok" || !/refusal=OK/.test(felled.detail)) fails.push(`the fell was refused: ${felled.status} ${felled.detail}`);
  wait(1400);
  const mid = on(2, { verb: "treefall", words: ["state"] }, 5000);
  if (!/anims=[1-9]/.test(mid.detail)) fails.push(`client 2 draws no falling tree mid-fall (${mid.detail})`);
  wait(5500);
  const stop = eng.engineStop(send(2), resolveAlias, { label: "tree-fall", fps: 30, out: join(out, "tree_fall.mp4") });
  if (stop.ok) console.log(`clip: ${eng.describe(stop)}`);
  else fails.push(`film stop failed: ${stop.error}`);

  on(2, { verb: "capture", mode: "ui", name: "tree_fall_1_landed" }, 25000);
  on(2, { verb: "camera", args: "stop" });
  on(2, { verb: "camera", args: `static ${x - 2} ${y + 4.5} ${z - 3} ${x + 5} ${y} ${z} 65 50` });
  wait(2000);
  on(2, { verb: "capture", mode: "ui", name: "tree_fall_2_logs_close" }, 25000);
  const after = on(2, { verb: "treefall", words: ["state"] }, 5000);
  if (!/debris=[1-9]/.test(after.detail)) fails.push(`client 2 lays no branch props (${after.detail})`);
  on(2, { verb: "camera", args: "stop" });
  on(2, { verb: "camera", args: "hud on" });

  const paths = sessionPaths(dir, 2);
  const drop = parseDrop(readLog(join(paths.server, "logs")) || readLog(paths.server));
  if (!drop) fails.push("the server never logged 'tree fall: N of M logs dropped'");
  else if (drop.spawned !== drop.planned || drop.spawned < 1) fails.push(`the server dropped ${drop.spawned} of ${drop.planned} logs`);
  else console.log(`ok    the server dropped ${drop.spawned} of ${drop.planned} logs`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "tree-fall: FAIL" : `tree-fall: PASS (client 2 saw the tree fall and its logs land; pictures and clip in ${out})`);
  return fails.length ? 1 : 0;
}

export default {
  name: "tree-fall",
  noReset: true,
  usage: "tree-fall [out dir] [--radius 80] | --selftest   (session up --clients 2 --profile full)",
  run: (ctx) => run(ctx.args ?? []),
};
