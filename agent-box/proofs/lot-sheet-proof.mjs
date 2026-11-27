import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// The car lot and truck yard sheets through the `lotsheet` harness verb (LF_VehicleLotHarness), one client on the island:
//   node agent-box/session.mjs up --mission island --profile full --wait
//   node agent-box/session.mjs lot-sheet-proof
//   node agent-box/session.mjs down
// Stands at each lot's counter, `lotsheet open` (the server's own Show path, as the action takes), reads the rows back,
// takes a `ui` picture, and `lotsheet close`. A character with no truck licence sees "(needs truck license)" on every
// truck row and on no car row: the car lot sells no trucks, the truck yard only trucks. The proof never buys.
export const LOTS = [
  { id: "car lot", entity: "Fit_saint_philippe_carlot_vehicle_lot", shot: "lot_sheet_car_lot", trucks: "none" },
  { id: "truck yard", entity: "Fit_ne_compound_truckyard_lot", shot: "lot_sheet_truck_yard", trucks: "all" },
];
export const HINT = "(needs truck license)";

// Pure: the rows of a `lotsheet` answer ("lotsheet title=.. rows=N ## Car0=label | status | button ## ..."), as
// { name, label, status, button }. The change-locks row is not a car, so it is dropped.
export function parseRows(detail) {
  const parts = String(detail).split(" ## ").slice(1);
  const rows = [];
  for (const p of parts) {
    const eq = p.indexOf("=");
    if (eq < 0) continue;
    const name = p.slice(0, eq);
    const cells = p.slice(eq + 1).split(" | ");
    if (!/^Car\d+$/.test(name)) continue;
    rows.push({ name, label: cells[0] ?? "", status: cells[1] ?? "", button: cells[2] ?? "" });
  }
  return rows;
}

async function run() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 400)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const expect = (cond, what) => { console.log(`${cond ? "PASS" : "FAIL"}  ${what}`); if (!cond) fails.push(what); };
  const stateOf = () => {
    let r = on(1, { verb: "state" }, 10000);
    for (let i = 0; i < 8 && r.status !== "ok"; i++) { wait(2500); r = on(1, { verb: "state" }, 10000); }
    return parseState(r.detail);
  };

  arrive(on, 1, wait);
  stateOf();
  if (/WelcomeGuideDismissButton/.test(String(on(1, { verb: "ui" }).detail))) on(1, { verb: "click", widget: "WelcomeGuideDismissButton" });
  wait(1000);

  for (const lot of LOTS) {
    on(1, { verb: "stand", entityName: lot.entity });
    wait(2500);
    const open = on(1, { verb: "lotsheet", words: ["open"] }, 15000);
    expect(open.status === "ok", `${lot.id}: lotsheet open answers ok (${open.status} ${String(open.detail).slice(0, 80)})`);
    if (open.status !== "ok") continue;
    wait(800);
    const rows = parseRows(open.detail);
    expect(rows.length > 0, `${lot.id}: the sheet lists ${rows.length} car row(s)`);
    const trucks = rows.filter((r) => r.label.includes(HINT));
    console.log(`${lot.id}: ${rows.map((r) => `${r.name}=${r.label} [${r.status}]`).join("; ")}`);
    // The car lot sells no trucks (its rows keep plain names); the truck yard sells only trucks.
    if (lot.trucks === "all") expect(rows.length > 0 && trucks.length === rows.length, `${lot.id}: every row says ${HINT} (${trucks.length}/${rows.length})`);
    else expect(trucks.length === 0, `${lot.id}: no car row says ${HINT} (${trucks.length}/${rows.length})`);
    on(1, { verb: "capture", mode: "ui", name: lot.shot }, 20000);
    const again = on(1, { verb: "lotsheet", words: ["rows"] });
    expect(again.status === "ok" && String(again.detail).includes("rows="), `${lot.id}: rows reads the open sheet back`);
    const closed = on(1, { verb: "lotsheet", words: ["close"] });
    expect(closed.status === "ok", `${lot.id}: close closes it`);
    const after = on(1, { verb: "lotsheet", words: ["rows"] });
    expect(after.status === "error", `${lot.id}: no sheet reads back after close`);
  }

  console.log(fails.length ? `lot-sheet-proof: FAIL (${fails.length})` : "lot-sheet-proof: PASS");
  return fails.length ? 1 : 0;
}

export default {
  name: "lot-sheet-proof",
  usage: "lot-sheet-proof",
  run: () => run(),
};
