import { spawnSync } from "node:child_process";
import { join } from "node:path";
import { ROOT, arrive, clientBox, sessionDir } from "../session.mjs";

// Emote radial and sitting (#982), driven the way a player does it:
//   node agent-box/session.mjs up --profile full --clients 1
//   node agent-box/session.mjs sit-proof        (client 2 joins late, during the proof)
//   node agent-box/session.mjs down
// (on a pool machine: `session.mjs run agent-box/session.mjs sit-proof`)
// Client 1 sits on the market bench through the engine perform path (the F key's). Client 2 then
// joins late and must see the sit from the join data alone (JIP) and pictures it. Client 1 stands up
// and client 2 sees it end. Client 1 opens the emote radial (picture), salutes and smokes; client 2 sees
// the gesture and the held loiter on ITS copy of the world (`emote seen`) and pictures the smoke.
// With a session started with --clients 2 the late join is skipped and client 2 watches live.
const BENCH = "Market_Bench_W";
const BENCH_AT = [193.243, 0.0, 238.501];
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
  const unit = (v) => {
    const len = Math.hypot(v[0], v[1]) || 1;
    return [v[0] / len, v[1] / len];
  };
  const frontOf = (at, d, back, lift) => [at[0] + d[0] * back, at[1] + lift, at[2] + d[1] * back];

  arrive(on, 1, wait);
  const twoAtStart = on(2, { verb: "state" }, 5000).status === "ok";
  if (twoAtStart) arrive(on, 2, wait);
  wait(5000);

  // 1. Sit on the market bench.
  on(1, { verb: "stand", entityName: BENCH });
  wait(3000);
  const look = on(1, { verb: "look" });
  if (!look.detail.includes(SIT_ACTION)) fails.push(`the F key does not offer ${SIT_ACTION} at ${BENCH}: ${look.detail.slice(0, 200)}`);
  must(on(1, { verb: "emote", arg: "sit" }, 10000), "sit down");
  wait(4000);

  // 2. Client 2 sees it: live, or from the join data when it joins late.
  if (!twoAtStart) {
    const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "2"], { windowsHide: true, stdio: "inherit" });
    if (joined.status !== 0) {
      fails.push("client 2 did not join late");
      for (const f of fails) console.log(`FAIL  ${f}`);
      console.log("sit-proof: FAIL");
      return 1;
    }
    arrive(on, 2, wait);
    wait(4000);
  }
  const label = twoAtStart ? "client 2 sees player 1 sitting (CUSTOM loiter)" : "client 2, joined late, sees player 1 sitting (CUSTOM loiter)";
  const seated = sees(2, label, (p) => p.loiter === CUSTOM_LOITER, 20000);
  if (seated) {
    const gap = Math.hypot(seated.at[0] - BENCH_AT[0], seated.at[2] - BENCH_AT[2]);
    if (!(gap < 1.2)) fails.push(`the sitter is ${gap.toFixed(2)} m from the bench, expected on it`);
    on(2, { verb: "stand", entityName: "Market_Bench_E", side: "w2" });
    wait(3000);
    const d = unit([seated.at[0] - BENCH_AT[0], seated.at[2] - BENCH_AT[2]]);
    shot(2, "sit_bench_seen_client2", frontOf(seated.at, d, 2.8, 1.2), [seated.at[0], seated.at[1] + 0.7, seated.at[2]]);
    const side = [-d[1], d[0]];
    shot(2, "sit_bench_side_client2", [seated.at[0] + side[0] * 2.4 + d[0] * 1.2, seated.at[1] + 1.1, seated.at[2] + side[1] * 2.4 + d[1] * 1.2], [seated.at[0], seated.at[1] + 0.6, seated.at[2]]);
  }

  // 3. Stand up: the same action again.
  must(on(1, { verb: "emote", arg: "sit" }, 10000), "stand up");
  sees(2, "client 2 sees player 1 stand up", (p) => p.loiter < 0, 12000);

  // 4. The radial, opened the way holding F1 does.
  on(1, { verb: "stand", entityName: "Market_Bench_E", side: "e2" });
  on(2, { verb: "stand", entityName: "Market_Bench_E", side: "w2" });
  wait(3000);
  must(on(1, { verb: "emote", arg: "open" }), "open the emote radial");
  wait(1500);
  must(on(1, { verb: "capture", mode: "ui", name: "emote_radial_open_client1" }, 25000), "picture the radial");
  const closed = on(1, { verb: "emote", arg: "close" });
  if (!/wasOpen=(1|true)/.test(closed.detail)) fails.push(`the radial was not open: ${closed.detail}`);
  wait(1000);

  // 5. A gesture and a held emote, seen from client 2.
  must(on(1, { verb: "emote", arg: "CMD_Salute" }), "salute");
  sees(2, "client 2 sees player 1 salute", (p) => p.gesture);
  wait(3000);
  must(on(1, { verb: "emote", arg: "Emote_Smoke" }), "smoke");
  const smoking = sees(2, "client 2 sees player 1 smoke (held loiter)", (p) => p.loiter > 0, 10000);
  if (smoking) {
    wait(3500);
    // From the open east side of the square (the other bench stands to the west).
    const d = [1, 0];
    shot(2, "emote_smoke_seen_client2", frontOf(smoking.at, d, 2.6, 1.1), [smoking.at[0], smoking.at[1] + 1.2, smoking.at[2]]);
  }
  on(1, { verb: "emote", arg: "Emote_Smoke" });
  sees(2, "client 2 sees the smoke end", (p) => p.loiter < 0, 12000);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "sit-proof: FAIL" : `sit-proof: OK (sit seen ${twoAtStart ? "live" : "by a late joiner"}, stand up, radial, salute and smoke seen from client 2)`);
  return fails.length ? 1 : 0;
}

export default { name: "sit-proof", usage: "sit-proof   (up --profile full --clients 1; client 2 joins late)", run: () => runSitProof() };
