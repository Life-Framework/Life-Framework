import { arrive, clientBox, sessionDir } from "../session.mjs";

// #1028 aimed placement on a running `up --clients 2 --profile full` session with client 1 on the
// roster (`session.mjs roster 1` before up). Client 1 clocks in, draws the patrol kit, drops the
// folded strip on the main street's south verge and enters the mode through the item's own action
// ("Place where I aim", `spikeaim begin`). The harness verbs call what the keys call:
//   face N   the camera aims another way: the ghost must move to where it now points
//   turn N   the turn key: the ghost's heading changes by 15 degrees a step
//   place    the place key: the ghost's spot goes to the server (RpcAsk_LF_PlaceAimed -> Confirm)
//   cancel   the cancel key: the ghost is hidden and nothing is placed
// Pass: the ghost moved with the aim and turned with the key; the strip then lies where the ghost
// was, with its heading, on both clients; a second strip cancelled leaves no strip and no ghost.
const MAX_OFF = 0.1; // metres between the ghost and the strip on a client
const MIN_MOVE = 0.5; // metres the ghost must move when the aim turns 70 degrees
const MAX_TURN_ERR = 3; // degrees between the turned ghost and the strip's heading

function parseVec(text, label) {
  const m = new RegExp(`${label} <\\s*([-\\d.e]+),\\s*([-\\d.e]+),\\s*([-\\d.e]+)\\s*>`).exec(text || "");
  return m ? m.slice(1).map(Number) : null;
}

const yawOf = (v) => (Math.atan2(v[0], v[2]) * 180) / Math.PI;
const wrap = (d) => ((d + 540) % 360) - 180;

async function runPlacementAimProof() {
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
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what}: ${r.status} ${r.detail}`);
    return r;
  };
  const aim = (verb, value, what) => must(on(1, { verb: "spikeaim", arg: verb, value }, 10000), what || `aim ${verb}`);
  // Two camera spots around the ghost (a tree stands west of the verge): east along the street, and north and high.
  const views = [["a", 5, 2.5, -3], ["b", 0, 3.5, 5]];
  const shot = (n, name, at) => {
    for (const [tag, ox, oy, oz] of views) on(n, { verb: "capture", mode: "world", name: `${name}_${tag}`, x: at[0], y: at[1], z: at[2], ox, oy, oz, hour: 12 }, 25000);
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  on(1, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  must(on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000), "clock in");
  wait(3000);
  on(1, { verb: "stand", entityName: "Roadmap_Police_Armory" });
  wait(2500);
  must(on(1, { verb: "use", className: "LF_ArmoryDrawAction", entityName: "Roadmap_Police_Armory" }, 10000), "armory draw");
  wait(2500);

  // --- the mode: the ghost follows the aim and turns with the key
  must(on(1, { verb: "spikeverge" }, 10000), "to the verge");
  wait(2500);
  must(on(1, { verb: "spikedrop" }, 10000), "drop the folded strip");
  wait(2000);
  const begin = aim("begin", "", "enter the mode from the item's action");
  wait(1000);
  const north = aim("state", "", "ghost aimed north");
  const nOrigin = parseVec(north.detail, "at");
  aim("face", "70", "aim east of north");
  wait(1500);
  const east = aim("state", "", "ghost aimed east");
  const eOrigin = parseVec(east.detail, "at");
  if (!nOrigin || !eOrigin) fails.push(`no ghost spots (${north.detail} / ${east.detail})`);
  else {
    const moved = Math.hypot(eOrigin[0] - nOrigin[0], eOrigin[2] - nOrigin[2]);
    console.log(`the ghost moved ${moved.toFixed(2)} m when the aim turned 70 degrees`);
    if (moved < MIN_MOVE) fails.push(`the ghost did not follow the aim: moved ${moved.toFixed(2)} m`);
  }

  const before = aim("state", "", "heading before the turn");
  const turned = aim("turn", "2", "turn the ghost two steps");
  const hb = parseVec(before.detail, "heading");
  const ha = parseVec(turned.detail, "heading");
  if (!hb || !ha) fails.push(`no headings (${before.detail} / ${turned.detail})`);
  else {
    const dyaw = Math.abs(wrap(yawOf(ha) - yawOf(hb)));
    console.log(`two turn steps changed the heading by ${dyaw.toFixed(1)} degrees`);
    if (Math.abs(dyaw - 30) > MAX_TURN_ERR) fails.push(`two 15 degree steps turned the ghost ${dyaw.toFixed(1)} degrees`);
  }
  if (!/^ghost valid/.test(turned.detail)) fails.push(`the ghost is not valid to place: ${turned.detail}`);

  // The ghost in the world: held so the shot's camera does not move it.
  aim("hold", "", "hold the ghost for the shot");
  const gOrigin = parseVec(turned.detail, "at") || [300, 0.1, 248.25];
  shot(1, "aim_1_ghost", gOrigin);
  aim("release", "", "release the ghost");
  must(on(1, { verb: "spikeverge" }, 10000), "back to the verge");
  wait(2000);

  // --- place: the strip lies where the ghost was, with its heading, on both clients
  aim("face", "70", "aim east again");
  wait(1500);
  aim("state", "", "ghost after the shot");
  const final = aim("turn", "2", "turn two steps again");
  const fOrigin = parseVec(final.detail, "at");
  const fHeading = parseVec(final.detail, "heading");
  const sent = aim("place", "", "place");
  wait(3000);
  const where1 = must(on(1, { verb: "spikewhere" }, 10000), "client 1 sees the strip");
  const where2 = must(on(2, { verb: "spikewhere" }, 10000), "client 2 sees the strip");
  for (const [who, r] of [["client 1", where1], ["client 2", where2]]) {
    const at = parseVec(r.detail, "strip at");
    const hd = parseVec(r.detail, "heading");
    if (!fOrigin || !at) {
      fails.push(`${who}: no position to compare (ghost ${final.detail}; strip ${r.detail})`);
      continue;
    }
    // spikewhere looks within 6 m of the verge; the strip is the nearest placed one
    const off = Math.hypot(at[0] - fOrigin[0], at[2] - fOrigin[2]);
    console.log(`${who}: strip ${off.toFixed(3)} m from the ghost`);
    if (off > MAX_OFF) fails.push(`${who}: the strip lies ${off.toFixed(2)} m from where the ghost was`);
    if (hd && fHeading) {
      const err = Math.abs(wrap(yawOf(hd) - yawOf(fHeading)));
      if (err > MAX_TURN_ERR) fails.push(`${who}: the strip's heading is ${err.toFixed(1)} degrees off the ghost's`);
    }
  }
  const gone = on(1, { verb: "spikeaim", arg: "state" }, 10000);
  if (!/no ghost running=(false|0)/.test(gone.detail)) fails.push(`the mode did not end after placing: ${gone.detail}`);
  shot(2, "aim_2_placed", fOrigin || gOrigin);
  shot(1, "aim_3_placed_officer", fOrigin || gOrigin);

  // --- cancel: pick the strip up, drop it, aim, cancel: nothing placed, no ghost left
  must(on(1, { verb: "spikeverge" }, 10000), "back to the verge to pick up");
  wait(1500);
  const where = on(1, { verb: "spikewhere" }, 10000);
  must(on(1, { verb: "use", className: "LF_SpikeStripPickUpAction" }, 10000), "pick up");
  wait(2000);
  must(on(1, { verb: "spikedrop" }, 10000), "drop the folded strip again");
  wait(2000);
  aim("begin", "", "enter the mode again");
  wait(800);
  const cancelled = aim("cancel", "", "cancel");
  if (!/running=(false|0) ghost=(false|0)/.test(cancelled.detail)) fails.push(`cancel left the mode or the ghost up: ${cancelled.detail}`);
  wait(2000);
  const after = on(1, { verb: "spikewhere" }, 10000);
  if (after.status === "ok") fails.push(`a strip lies by the verge after cancel: ${after.detail}`);

  console.log("");
  console.log(`begin:   ${begin.detail}`);
  console.log(`north:   ${north.detail}`);
  console.log(`east:    ${east.detail}`);
  console.log(`placed:  ${sent.detail}`);
  console.log(`client 1: ${where1.detail}`);
  console.log(`client 2: ${where2.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "placement aim proof: FAIL" : "placement aim proof: PASS (the ghost followed the aim, turned in steps, the strip lies where it was on both clients, a cancelled one placed nothing)");
  return fails.length ? 1 : 0;
}

export default { name: "placement-aim-proof", usage: "placement-aim-proof", run: () => runPlacementAimProof() };
