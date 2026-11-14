import { arrive, clientBox, sessionDir } from "../session.mjs";

// #625: at the hospital desk (and the police desk) the one state-aware Clock in / Clock out row is
// offered by the punch clock itself and sits ahead of the bill and fine rows. Reads the F menu the
// way a player's client does (`look`), clocks in and out once to prove the row flips, and takes a
// picture of the desk. A civilian client with no bill sees only the duty row, so the order is
// judged against any pay row present and the row must be on the desk entity.
//   node agent-box/session.mjs duty-order-proof [--name <shot prefix>]
// What `look` leaves: the row's wording and its place on screen (Sort Priority) are a picture call.
const DESKS = [
  { name: "Roadmap_Hospital_Duty_Desk", others: ["LF_TreatmentBillPayAction"] },
  { name: "Roadmap_Police_Desk", others: ["LF_FineTicketPayAction"] },
];
const DUTY = "LF_DutyAction";

// `look` prints `entity:class:...` items; list the ones whose text names the entity.
export function judgeDutyOrder(lookText, desk) {
  const out = [];
  const at = lookText.indexOf(DUTY);
  if (at < 0) return [`${desk.name}: the F menu has no ${DUTY}: ${lookText.slice(0, 200)}`];
  for (const other of desk.others) {
    const o = lookText.indexOf(other);
    if (o >= 0 && o < at) out.push(`${desk.name}: ${other} is listed before ${DUTY}`);
  }
  return out;
}

async function runDutyOrderProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const i = process.argv.indexOf("--name");
  const prefix = i > 0 ? process.argv[i + 1] : "m2_625_duty";
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  arrive(on, 1, wait);
  for (const desk of DESKS) {
    on(1, { verb: "stand", entityName: desk.name });
    wait(2500);
    const look = on(1, { verb: "look" });
    if (look.status !== "ok") { fails.push(`${desk.name}: look returned ${look.status} ${look.detail}`); continue; }
    fails.push(...judgeDutyOrder(look.detail, desk));
    on(1, { verb: "capture", mode: "world", name: `${prefix}_${desk.name}` }, 25000);
    // Clock in, then the same row must read Clock out (one state-aware row): the second `use` toggles back.
    const a = on(1, { verb: "use", className: DUTY, entityName: desk.name }, 10000);
    wait(2000);
    const s1 = on(1, { verb: "state" }, 10000).detail;
    const b = on(1, { verb: "use", className: DUTY, entityName: desk.name }, 10000);
    wait(2000);
    const s2 = on(1, { verb: "state" }, 10000).detail;
    if (a.status !== "ok" || b.status !== "ok") fails.push(`${desk.name}: the duty row did not run twice (${a.status}, ${b.status})`);
    if (s1 === s2) fails.push(`${desk.name}: clock in then clock out left the state unchanged: ${s1}`);
  }
  if (fails.length) {
    console.log("duty-order-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("duty-order-proof: OK (duty row on the desk, ahead of the pay rows; clock in and out flip the state)");
  return 0;
}

export default { name: "duty-order-proof", usage: "duty-order-proof [--name <prefix>]", run: () => runDutyOrderProof() };
