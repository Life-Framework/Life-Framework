import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// #4790: a tier pickaxe bought at the miners' till mines. On a running island session
// (`tools\cli session up --mission island --profile full --wait`, one client): reset to $205 cash, open the Coalman's
// Brow miners' supply till, order the WORN pickaxe ($150), pay, then walk to a Highstone hill iron rock, take the
// pickaxe in hand and swing. The server log must carry `[LFDebug:Mining] iron rock hit` and never `takes no damage from
// LF_Tool_Pickaxe_Worn`; the proof checks the log line count through the harness `logs` of the session.
const DESK = "Fit_coalmans_brow_mine_mine_desk";
const TILL = "Fit_coalmans_brow_mine_mine_supply_till";
const ROCK = "Fit_highstone_mine_mine_ore_2";
const SWINGS = 2;

// The Mining and refusal lines of the newest server console log of this session.
function miningLines() {
  const root = join(sessionDir(), "server", "logs");
  if (!existsSync(root)) return [];
  const runs = readdirSync(root).filter((d) => existsSync(join(root, d, "console.log")));
  runs.sort((a, b) => statSync(join(root, a, "console.log")).mtimeMs - statSync(join(root, b, "console.log")).mtimeMs);
  if (!runs.length) return [];
  return readFileSync(join(root, runs[runs.length - 1], "console.log"), "utf8").split("\n").filter((l) => /LFDebug:Mining|not one of its tools/.test(l));
}

async function runTierToolMineProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail ?? "").slice(0, 400)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  arrive(on, 1, wait);
  wait(4000);
  on(1, { verb: "resetplayer", cash: 205 }, 15000);
  on(1, { verb: "guide", words: ["close"] });
  // The desk toggles the job: take it only when the character does not hold it yet.
  if (parseState(on(1, { verb: "state" }, 10000).detail).job !== "MINER") {
    on(1, { verb: "stand", entityName: DESK }, 10000);
    on(1, { verb: "use", className: "LF_WorkplaceJobAction", entityName: DESK }, 10000);
    wait(1500);
  }
  on(1, { verb: "stand", entityName: TILL }, 10000);
  wait(1500);
  const open = on(1, { verb: "use", className: "LF_ShopAction", entityName: TILL }, 10000);
  if (open.status !== "ok") fails.push(`the till did not open: ${open.status} ${open.detail}`);
  wait(1500);
  const sheet = on(1, { verb: "ui", mode: "text" }, 10000);
  console.log(String(sheet.detail ?? ""));
  // The worn pickaxe's row: the first row whose name reads "Worn" (the sheet lists them in catalogue order).
  const rows = Number(process.env.TIER_ROW ?? "0");
  on(1, { verb: "click", widget: `Item${rows}Row_Action` }, 10000);
  wait(800);
  on(1, { verb: "capture", mode: "ui", name: "tier_till_ordered" }, 25000);
  on(1, { verb: "click", widget: "PayButton" }, 10000);
  wait(2500);
  const paid = on(1, { verb: "ui", mode: "text" }, 10000);
  if (!/Cash in hand: \$55\b/.test(String(paid.detail ?? ""))) fails.push(`the Cash line does not read $55: ${String(paid.detail ?? "").slice(0, 300)}`);
  on(1, { verb: "guide", words: ["close"] });
  const cash = Number(parseState(on(1, { verb: "state" }, 10000).detail).cash || 0);
  if (cash !== 55) fails.push(`the wallet holds $${cash}, not $55`);

  // The pickaxe is bought and carried; draw it, stand 2 m east of the rock facing it, look down a little and swing.
  const held = on(1, { verb: "hold", item: "weapon" }, 10000);
  if (!/drew .*LF_Tool_Pickaxe_Worn.* 1$/.test(String(held.detail ?? ""))) fails.push(`the worn pickaxe was not drawn: ${held.detail}`);
  wait(2500);
  on(1, { verb: "stand", entityName: ROCK, side: "e2" }, 15000);
  wait(2000);
  // The swing traces from the aim, so sweep the pitch: the first iron hit proves the tool, the rest only repeat it.
  const before = miningLines();
  let pitch = 0;
  for (const turn of [-30, 15, 15, 15]) {
    on(1, { verb: "input", action: "CharacterTurnUp", value: String(turn) });
    pitch += turn;
    for (let i = 0; i < SWINGS; i++) {
      on(1, { verb: "input", action: "CharacterMelee" });
      wait(3200);
    }
    if (miningLines().length > before.length) break;
  }
  on(1, { verb: "input", action: "CharacterTurnUp", value: String(-pitch) });
  wait(1500);
  const lines = miningLines().slice(before.length);
  const hits = lines.filter((l) => /iron rock hit/.test(l));
  const refused = lines.filter((l) => /takes no damage from .*LF_Tool_Pickaxe_Worn/.test(l));
  console.log(`server log: ${hits.length} iron rock hit line(s), ${refused.length} refusal line(s)`);
  for (const l of hits.slice(0, 3)) console.log(`  ${l}`);
  if (refused.length) fails.push(`the worn pickaxe was refused: ${refused[0]}`);
  if (!hits.length) fails.push("no `[LFDebug:Mining] iron rock hit` line after the swings with the worn pickaxe");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "tier tool mine proof: FAIL" : "tier tool mine proof: PASS (a worn pickaxe bought at the till mined the Highstone iron rock)");
  return fails.length ? 1 : 0;
}

export default {
  name: "tier-tool-mine-proof",
  usage: "tier-tool-mine-proof",
  run: () => runTierToolMineProof(),
};
