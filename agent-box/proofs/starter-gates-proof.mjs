import { arrive, clientBox, sessionDir } from "../session.mjs";

// F2 "What to do next": the four island starters are gated (#4771).
//   node agent-box/session.mjs up --mission island --profile full
//   node agent-box/session.mjs starter-gates-proof
//   node agent-box/session.mjs down
// A fresh civilian opens the Next tab: the crew row is listed, the truck check, perk pick and rumor rows are not
// (shot one). The client then earns Hauling XP with `levelupcard grant` (the real AddXp path, DELIVERY 5000 is level 10)
// and the tab is read and shot again: Hauling 5 opens the truck check and a trade at level 10 the perk pick; the rumor
// stays hidden with no criminal record. Run it on a fresh session: the XP stays with the account.
const ANSWER_BUDGET_MS = 15000;

async function runStarterGatesProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  const rowsNow = () => {
    let rows = "";
    for (let waited = 0; waited < ANSWER_BUDGET_MS && !rows.includes("mining:"); waited += 1000) {
      rows = on(1, { verb: "activities", words: ["state"] }).detail;
      if (!rows.includes("mining:")) wait(1000);
    }
    return rows;
  };
  const has = (rows, id) => rows.includes(`${id}:`);

  arrive(on, 1, wait);
  wait(12000);
  on(1, { verb: "guide", words: ["close"] });
  wait(500);

  // The clerk's papers come first: no run (and no starter row) is listed before them. Talk at the town hall
  // desk the way arrival-island does, answering each reply as it appears, until the card is carried.
  const CLERK = "Fit_saint_philippe_arrival_town_hall_clerk";
  must(on(1, { verb: "stand", entityName: CLERK, side: "e2" }, 20000), "stand at the clerk");
  wait(1500);
  must(on(1, { verb: "use", className: "LF_QuestAction", entityName: CLERK }, 10000), "talk to the clerk");
  for (let turn = 0; turn < 12 && !/idcard=(?!none)\S+/.test(on(1, { verb: "state" }, 10000).detail); turn++) {
    for (let waited = 0; waited < 20000 && !/phase=choosing/.test(String(on(1, { verb: "dialogue" }, 10000).detail)); waited += 1000) wait(1000);
    on(1, { verb: "input", action: "MenuSelect" });
    wait(2500);
  }
  wait(9000);
  on(1, { verb: "input", action: "MenuBack" });
  wait(2000);
  if (!/idcard=(?!none)\S+/.test(on(1, { verb: "state" }, 10000).detail)) fails.push("the clerk did not hand over the ID card");

  must(on(1, { verb: "playermenu", words: ["open", "next"] }), "open the file on NEXT");
  let rows = rowsNow();
  if (!has(rows, "crew")) fails.push(`a new civilian is not offered the crew row: ${rows}`);
  for (const id of ["truckcheck", "perkpick", "rumor"]) {
    if (has(rows, id)) fails.push(`a new civilian is offered ${id}: ${rows}`);
  }
  const indexOf = (r, id) => r.replace(/.*ids=/, "").split(",").findIndex((x) => x.startsWith(id + ":"));
  must(on(1, { verb: "activities", words: ["pick", String(Math.max(indexOf(rows, "crew"), 0))] }), "pick the crew row");
  wait(1200);
  on(1, { verb: "capture", mode: "ui", name: "starters_new_civilian" }, 25000);
  must(on(1, { verb: "playermenu", words: ["close"] }), "close the file");

  // Hauling XP through the real award path, then read the tab again.
  must(on(1, { verb: "levelupcard", words: ["grant", "DELIVERY", "5000"] }, 10000), "grant Hauling XP");
  wait(3000);
  must(on(1, { verb: "playermenu", words: ["open", "next"] }), "reopen the file on NEXT");
  rows = rowsNow();
  console.log(`after the Hauling grant: ${rows}`);
  if (!has(rows, "truckcheck")) fails.push(`Hauling 5 does not open the truck check: ${rows}`);
  if (!has(rows, "perkpick")) fails.push(`a trade at level 10 does not open the perk pick: ${rows}`);
  if (has(rows, "rumor")) fails.push(`the rumor is offered with no criminal record: ${rows}`);
  must(on(1, { verb: "activities", words: ["pick", String(Math.max(indexOf(rows, "truckcheck"), 0))] }), "pick the truck check row");
  wait(1200);
  on(1, { verb: "capture", mode: "ui", name: "starters_after_hauling_grant" }, 25000);
  must(on(1, { verb: "activities", words: ["pick", String(Math.max(indexOf(rows, "perkpick"), 0))] }), "pick the perk pick row");
  wait(1200);
  on(1, { verb: "capture", mode: "ui", name: "starters_perk_pick_row" }, 25000);
  must(on(1, { verb: "playermenu", words: ["close"] }), "close the file again");

  if (fails.length) {
    console.log("starter-gates-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("starter-gates-proof: OK (read the shots)");
  return 0;
}

export default { name: "starter-gates-proof", usage: "starter-gates-proof", run: () => runStarterGatesProof() };
