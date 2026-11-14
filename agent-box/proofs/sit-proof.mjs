import { spawnSync } from "node:child_process";
import { join } from "node:path";
import { ROOT, arrive, clientBox, sessionDir } from "../session.mjs";

// Emote radial and sitting (#982) on a running two-client session:
//   node agent-box/session.mjs up --profile full --clients 2
//   node agent-box/session.mjs sit-proof            (adds a late joiner, client 3, unless --no-late)
//   node agent-box/session.mjs down
// Client 1 opens the emote radial (picture), salutes and smokes; client 2 must see the gesture and the
// held loiter on ITS copy of the world (`emote seen`) and pictures the smoke. Client 1 then sits on the
// market bench with the F key; client 2 sees the sit loiter (CUSTOM, 7) and pictures it; a late joiner
// sees it from the join data; client 1 stands up and the others see it end.
const BENCH = "Market_Bench_W";
const SIT_ACTION = "LF_SitAction";
const CUSTOM_LOITER = 7;

function parseSeen(detail, id) {
  const m = new RegExp(`p${id}:gesture=(\\w+),loiter=(-?\\d+),at=<?(-?[\\d.]+),\\s*(-?[\\d.]+),\\s*(-?[\\d.]+)`).exec(String(detail ?? ""));
  if (!m) return null;
  return { gesture: m[1] === "1" || m[1] === "true", loiter: Number(m[2]), at: [Number(m[3]), Number(m[4]), Number(m[5])] };
}

async function runSitProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 220)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  // What client n shows of player 1, polled until judge passes or the budget runs out.
  const sees = (n, label, judge, budgetMs = 8000) => {
    const start = Date.now();
    let last = "";
    while (Date.now() - start < budgetMs) {
      const r = on(n, { verb: "emote", arg: "seen" }, 10000);
      const p = parseSeen(r.detail, 1);
      last = r.detail;
      if (p && judge(p)) {
        console.log(`ok    ${label} (${((Date.now() - start) / 1000).toFixed(1)} s)`);
        return p;
      }
      wait(400);
    }
    fails.push(`${label}: client ${n} saw ${last}`);
    return parseSeen(last, 1);
  };
  // A fixed camera on client n looking at a point, HUD off, one picture.
  const shot = (n, name, from, to) => {
    on(n, { verb: "camera", args: "hud off" });
    on(n, { verb: "camera", args: "stop" });
    on(n, { verb: "camera", args: `static ${from.map((v) => v.toFixed(2)).join(" ")} ${to.map((v) => v.toFixed(2)).join(" ")} 55 50` });
    wait(2500);
    const r = on(n, { verb: "capture", mode: "ui", name }, 25000);
    on(n, { verb: "camera", args: "stop" });
    on(n, { verb: "camera", args: "hud on" });
    return r;
  };
  // A camera 3 m in front of and above a body, from the side the body faces (dir: unit xz).
  const frontOf = (at, dir, back = 3, lift = 1.5) => [at[0] + dir[0] * back, at[1] + lift, at[2] + dir[1] * back];

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(6000);

  // 1. The radial, opened the way holding F1 does.
  on(1, { verb: "stand", entityName: "Market_Bench_E", side: "e2" });
  on(2, { verb: "stand", entityName: "Market_Bench_E", side: "w2" });
  wait(3000);
  const open = must(on(1, { verb: "emote", arg: "open" }), "open the emote radial");
  if (!/open=1|open=true/.test(open.detail)) fails.push(`the radial did not open: ${open.detail}`);
  wait(1500);
  must(on(1, { verb: "capture", mode: "ui", name: "emote_radial_open_client1" }, 25000), "picture the radial");
  on(1, { verb: "emote", arg: "close" });
  wait(1000);

  // 2. A gesture and a held emote, seen from client 2.
  must(on(1, { verb: "emote", arg: "CMD_Salute" }), "salute");
  sees(2, "client 2 sees player 1 salute", (p) => p.gesture);
  wait(3000);
  must(on(1, { verb: "emote", arg: "Emote_Smoke" }), "smoke");
  const smoking = sees(2, "client 2 sees player 1 smoke (held loiter)", (p) => p.loiter > 0, 10000);
  if (smoking) {
    wait(3500);
    const me = parseSeen(on(1, { verb: "emote", arg: "seen" }).detail, 2);
    const toward = me ? [me.at[0] - smoking.at[0], me.at[2] - smoking.at[2]] : [1, 0];
    const len = Math.hypot(toward[0], toward[1]) || 1;
    shot(2, "emote_smoke_seen_client2", frontOf(smoking.at, [toward[0] / len, toward[1] / len], 2.6, 1.1), [smoking.at[0], smoking.at[1] + 1.2, smoking.at[2]]);
  }
  on(1, { verb: "emote", arg: "Emote_Smoke" });
  sees(2, "client 2 sees the smoke end", (p) => p.loiter < 0, 12000);

  // 3. Sit on the market bench with the F key, seen from client 2 and a late joiner.
  on(1, { verb: "stand", entityName: BENCH });
  wait(3000);
  const look = on(1, { verb: "look" });
  if (!look.detail.includes(SIT_ACTION)) fails.push(`the F key does not offer ${SIT_ACTION} at ${BENCH}: ${look.detail.slice(0, 200)}`);
  must(on(1, { verb: "use", className: SIT_ACTION, entityName: BENCH }, 10000), "sit down");
  const seated = sees(2, "client 2 sees player 1 sitting (CUSTOM loiter)", (p) => p.loiter === CUSTOM_LOITER, 12000);
  const benchAt = [193.243, 0.0, 238.501];
  if (seated) {
    const gap = Math.hypot(seated.at[0] - benchAt[0], seated.at[2] - benchAt[2]);
    if (!(gap < 1.2)) fails.push(`the sitter is ${gap.toFixed(2)} m from the bench, expected on it`);
    wait(3000);
    const out = [seated.at[0] - benchAt[0], seated.at[2] - benchAt[2]];
    const len = Math.hypot(out[0], out[1]) || 1;
    shot(2, "sit_bench_seen_client2", frontOf(seated.at, [out[0] / len, out[1] / len], 3, 1.3), [seated.at[0], seated.at[1] + 0.7, seated.at[2]]);
  }

  if (!process.argv.includes("--no-late")) {
    const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "3"], { windowsHide: true, stdio: "inherit" });
    if (joined.status !== 0) fails.push("client 3 did not join late");
    else {
      arrive(on, 3, wait);
      wait(4000);
      sees(3, "the late joiner sees player 1 sitting", (p) => p.loiter === CUSTOM_LOITER, 20000);
    }
  }

  // 4. Stand up: the same key again.
  must(on(1, { verb: "use", className: SIT_ACTION, entityName: BENCH }, 10000), "stand up");
  sees(2, "client 2 sees player 1 stand up", (p) => p.loiter < 0, 12000);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "sit-proof: FAIL" : "sit-proof: OK (radial, salute and smoke seen, sit seen live and by a late joiner, stand up)");
  return fails.length ? 1 : 0;
}

export default { name: "sit-proof", usage: "sit-proof [--no-late]   (up --profile full --clients 2)", run: () => runSitProof() };
