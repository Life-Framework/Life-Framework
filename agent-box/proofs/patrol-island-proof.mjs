import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";
import { rosterHas } from "./scanner-proof.mjs";

// #3087: the first police call on the real island (#1014 beat sheet), in one two-player session.
//   node agent-box/session.mjs roster 2
//   node agent-box/session.mjs up --mission island --clients 2 --profile full --wait
//   node agent-box/session.mjs patrol-island-proof
//   node agent-box/session.mjs down
// It reuses the grid proofs' steps (robbery-alarm, dispatch, spike-strip, vault-offer) on the island's own
// fitted entities (Fit_saint_philippe_<slot>_<kit row>, found by name in the running world):
//   1. client 2 clocks in at the police desk (the police slot is still a draft: a missing desk is a FAIL)
//   2. client 1 reads the vault prompt as a dedicated client does, drills the Saint-Philippe vault
//   3. the server's alarm line names the bank kind, not the shop buzzer
//   4. client 2 (on duty) receives the call, and its place reads Saint-Philippe
//   5. client 2 draws the kit at the armory, lays a strip across the south road (the arrival's road) and
//      a car rolling over it loses tyre health, on the server and on both clients
// Every step is PASS or FAIL and a failed step does not stop the run. A failure that shows only on the
// island is a `weekend` issue; this file fixes nothing outside itself.
const FIT = "Fit_saint_philippe_";
const VAULT = `${FIT}bank_lf_bank_vault`;
const VAULT_ID = "bank_vault"; // LF_BankVaultComponent.m_sVaultId on the vault prefab (vaultoffer looks it up by place id)
const DESK = `${FIT}police_Roadmap_Police_Desk`;
const ARMORY = `${FIT}police_Roadmap_Police_Armory`;
const ROAD = `${FIT}arrival_Place`; // the spawn lies on the south road into the square
const DRILL_SECONDS = 30; // LF_VaultDrill.DEFAULT_SECONDS; the vault prefab does not override it
const PLACE_WORDS = /Saint[- ]Philippe/i;

async function runPatrolIslandProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const paths = sessionPaths(dir);
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 260)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const readLog = (logsDir) => {
    const f = newestLog(logsDir);
    return f ? readFileSync(f, "utf8") : "";
  };
  const results = [];
  const check = (name, pass, detail) => {
    results.push({ name, pass });
    console.log(`${pass ? "PASS" : "FAIL"}  ${name}: ${detail}`);
  };

  if (!rosterHas(dir, 2)) {
    console.log("patrol-island-proof: FAIL  client 2 is not on the police roster; `session.mjs down`, `session.mjs roster 2`, then `up --mission island --clients 2` again");
    return 1;
  }
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  // 1. Client 2 on duty.
  on(2, { verb: "stand", entityName: DESK });
  wait(2500);
  on(2, { verb: "look" });
  const duty = on(2, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
  wait(2500);
  const officer = parseState(on(2, { verb: "state" }, 10000).detail);
  check("client 2 clocks in at the island police desk", duty.status === "ok" && officer.duty === "1", `use=${duty.status} ${duty.detail.slice(0, 120)} duty=${officer.duty}`);

  // 2. Client 1 reads the vault prompt, then drills it.
  on(1, { verb: "resetcrime" }, 10000);
  on(1, { verb: "stand", entityName: VAULT });
  wait(3000);
  const offer = on(1, { verb: "vaultoffer", words: [VAULT_ID] }, 10000);
  check("the vault prompt reads on a remote client at the island bank", offer.status === "ok" && /server=0 /.test(offer.detail), offer.detail.slice(0, 160));
  on(1, { verb: "drill" }, 10000);
  wait(1000);
  wait(1500);
  // The rob row wants the drill in the hands: a quick-slot press, repeated until `held=1` (a press toggles, so one that
  // finds it held leaves it be).
  for (let slotTry = 0; slotTry < 3; slotTry++) {
    if (/held=1/.test(String(on(1, { verb: "quickslot", words: ["drill"] }, 10000).detail))) break;
    on(1, { verb: "quickslot", words: ["drill", "press"] }, 10000);
    wait(3000);
  }
  wait(1500);
  const drill = on(1, { verb: "use", className: "LF_BankRobAction", entityName: VAULT }, 10000);
  check("client 1 starts drilling the Saint-Philippe vault", drill.status === "ok", `${drill.status} ${drill.detail.slice(0, 160)}`);
  console.log(`waiting ${DRILL_SECONDS + 8} s for the drill to break through`);
  wait((DRILL_SECONDS + 8) * 1000);
  const robber = parseState(on(1, { verb: "state" }, 10000).detail);
  check("the robber is wanted after the vault", Number(robber.wanted) > 0, `wanted=${robber.wanted}`);

  // 3. The alarm kind (server log), 4. the call (server log and the officer's state).
  const server = readLog(join(paths.server, "logs"));
  const alarm = /alarm sounding at .*kind '([^']*)', prefab (\S+)/g;
  const kinds = [...server.matchAll(alarm)].map((m) => `${m[1]} ${m[2].split("/").pop()}`);
  check("the vault robbery rings the bank alarm kind, not the shop buzzer", kinds.some((k) => /^bank /.test(k)), `alarm lines: ${kinds.join(" | ") || "none"}`);
  const calls = [...server.matchAll(/dispatch #\d+ .* at '([^']*)': officers=(\d+)/g)];
  const last = calls.length ? calls[calls.length - 1] : null;
  check("the dispatch reaches an on-duty officer", !!last && Number(last[2]) > 0, last ? `place='${last[1]}' officers=${last[2]}` : "no dispatch line in the server log");
  const heard = on(2, { verb: "state" }, 10000).detail;
  check("the officer's call names Saint-Philippe", PLACE_WORDS.test(heard), heard.slice(0, 220));
  on(2, { verb: "capture", mode: "ui", name: "patrol_island_dispatch_client2" }, 25000);

  // 5. The strip on the south road.
  on(2, { verb: "stand", entityName: ARMORY });
  wait(2500);
  const draw = on(2, { verb: "use", className: "LF_ArmoryDrawAction", entityName: ARMORY }, 10000);
  check("client 2 draws the patrol kit at the island armory", draw.status === "ok", `${draw.status} ${draw.detail.slice(0, 120)}`);
  wait(2500);
  on(2, { verb: "stand", entityName: ROAD });
  wait(2500);
  const drop = on(2, { verb: "spikedrop" }, 10000);
  wait(2000);
  const deploy = on(2, { verb: "use", className: "LF_SpikeStripDeployAction" }, 10000);
  wait(3000);
  check("client 2 lays the strip across the south road", drop.status === "ok" && deploy.status === "ok", `drop=${drop.status} ${drop.detail.slice(0, 80)} deploy=${deploy.status} ${deploy.detail.slice(0, 80)}`);
  const car = on(2, { verb: "spikecar" }, 10000);
  wait(6000);
  const onServer = on(2, { verb: "spikecheck" }, 10000);
  const seen1 = on(1, { verb: "spikeseen" }, 10000);
  const seen2 = on(2, { verb: "spikeseen" }, 10000);
  const flat = (r) => /flat=([1-4]) of 4/.test(r.detail);
  check("a car crossing the strip loses tyre health (server, client 1, client 2)", car.status === "ok" && flat(onServer) && flat(seen1) && flat(seen2), `car=${car.status} server: ${onServer.detail.slice(0, 120)} | c1: ${seen1.detail.slice(0, 60)} | c2: ${seen2.detail.slice(0, 60)}`);

  const failed = results.filter((r) => !r.pass);
  console.log(failed.length ? `patrol-island-proof: FAIL (${failed.length} of ${results.length})` : `patrol-island-proof: PASS (${results.length} checks)`);
  return failed.length ? 1 : 0;
}

export default {
  name: "patrol-island-proof",
  usage: "patrol-island-proof   (roster 2, then up --mission island --clients 2 --profile full)",
  run: () => runPatrolIslandProof(),
};
