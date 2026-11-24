import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng } from "../bmp.mjs";
import { parseState } from "../flow-asserts.mjs";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// The whole iron chain on the island as a MINER level 4 takes it (#3245, parent #3206), with each step's wall-clock time:
//   node agent-box/session.mjs up --mission island --profile full --wait
//   node agent-box/session.mjs mining-iron-chain [--cmd-timeout ms] [--name <picture prefix>]
//   node agent-box/session.mjs down
// `levelset MINER 4` (a LF_HARNESS build), a pickaxe, then: ten Iron Nuggets from two Highstone iron rocks (5 pieces each),
// five Coal Lumps from an East Rise coal rock, the compound smelter (5 batches in one run), the five bars picked up, and
// the bars sold at the dock wholesale iron buyer (`Fit_saint_philippe_dock_wholesale_iron_buyer`). The harness cannot
// drive: `stand` puts the character beside each stop where a player would drive or walk, so the "travel" rows time the
// teleport, not a road. Every verdict reads a server `[LFDebug:...]` line or the client's own state. The per-step
// seconds print as a table and go to tmp/mining-iron-chain-timings.json (the measured rates for #3246 / E2).
// `iron-chain-proof` (#3244) stays the SMELTING 1, one-batch proof of the same stops.
const PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
const IRON_A = { entity: "Fit_highstone_mine_mine_ore_2", x: 4956.3, y: 115.795, z: 8474.6 };
const IRON_B = { entity: "Fit_highstone_mine_mine_ore_3" };
const COAL = { entity: "Fit_eastrise_mine_mine_ore_2", x: 6681.3, y: 148.933, z: 7929.6 };
const SMELTER = { entity: "Fit_eastcompound_smelter_smelter_furnace" };
const BUYER = { entity: "Fit_saint_philippe_dock_wholesale_iron_buyer" };
const PROCESS = "LF_ProcessAction";
const BATCHES = 5;
const SWINGS_PER_PIECE = 3;
const RUN_SECONDS = 60;
const RETRY_VERBS = ["carries", "capture", "look", "seen", "stand", "hold", "state"];

async function runMiningIronChain() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const prefix = flag("--name", "mining_iron_chain");
  const cmdFloor = Number(flag("--cmd-timeout", "8000"));
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    let r;
    for (let attempt = 0; attempt < 5; attempt++) {
      r = h.sendCommand(spec, Math.max(timeout ?? 8000, cmdFloor));
      console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 260)}`);
      // Only a read or a move is asked twice: a `no_session` reply can come after the command ran (a smelt started, a swing swung).
      if (!(r.status === "error" && /^(no_session|capture_busy)/.test(r.detail) && RETRY_VERBS.includes(spec.verb))) break;
      wait(2500);
    }
    return r;
  };
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const results = [];
  const timings = [];
  const proofStart = Date.now();
  let lap = Date.now();
  // A step's seconds are the time since the previous step ended, so the rows add up to the whole run.
  const step = (name, ok, detail) => {
    const seconds = Math.round((Date.now() - lap) / 100) / 10;
    lap = Date.now();
    results.push({ name, ok });
    timings.push({ step: name, ok, seconds, detail: detail ?? "" });
    console.log(`${ok ? "PASS" : "FAIL"}  ${name} [${seconds} s]${detail ? `: ${detail}` : ""}`);
    return ok;
  };
  const carried = (text) => {
    const r = on({ verb: "carries", words: [text] }, 10000);
    const m = /count=(\d+)/.exec(r.detail);
    return r.status === "ok" && m ? Number(m[1]) : 0;
  };
  const cash = () => Number(parseState(on({ verb: "state" }, 10000).detail).cash);
  // The pieces lie in a fan on the side the miner stood (LF_OreDrop): walk round the rock, F-key pick up what lies in reach.
  const SIDES = ["e", "e3", "n2", "s2", "w3", "n4", "s4"];
  const collect = (site, item, want) => {
    let have = carried(item);
    for (let i = 0; i < SIDES.length * 2 && have < want; i++) {
      on({ verb: "stand", entityName: site.entity, side: SIDES[i % SIDES.length] });
      wait(1500);
      on({ verb: "pickup", item }, 15000);
      wait(900);
      have = carried(item);
    }
    return have;
  };
  // The capture shows the miner's own view: level the aim and holster the pickaxe for it, then take both back.
  let pitch = 0;
  let drawn = false;
  const shoot = (spec) => {
    if (pitch !== 0) on({ verb: "input", action: "CharacterTurnUp", value: String(-pitch) });
    if (drawn) { on({ verb: "hold", item: "none" }); wait(1800); }
    on(spec, 30000);
    if (drawn) { on({ verb: "hold", item: "weapon" }); wait(1800); }
    if (pitch !== 0) on({ verb: "input", action: "CharacterTurnUp", value: String(pitch) });
  };
  const siteShot = (site, name) => shoot({ verb: "capture", mode: "world", name: `${prefix}_${name}`, x: site.x, y: site.y, z: site.z, ox: 3, oy: 0.8, oz: -3, hour: 11 });

  // Mines a rock until the log shows `pieces` pieces broken off it. Aim is the one thing a player does by hand: a swing
  // with no `rock hit` line tips the aim one step and tries again. Each piece is picked up as it drops.
  const mine = (site, node, pieces, label, item) => {
    // The node id is in the log line: `iron rock hit` for iron, `coal-seam rock hit` for the East Rise coal.
    const hitRe = new RegExp(`\\[LFDebug:Mining\\][^\\n]*${node} rock hit[^\\n]*piece (\\d+)/(\\d+)[^\\n]*dropped (\\d+)`, "g");
    const startLen = serverLog().length;
    let dropped = 0;
    let lastPiece = 0;
    let misses = 0;
    for (let swing = 1; swing <= pieces * SWINGS_PER_PIECE + 12 && dropped < pieces; swing++) {
      if (swing === 1 || misses >= 2) {
        on({ verb: "stand", entityName: site.entity });
        wait(1800);
        if (swing === 1 && pitch === 0) { on({ verb: "input", action: "CharacterTurnUp", value: "-30" }); pitch -= 30; wait(800); }
        if (misses >= 2) { on({ verb: "input", action: "CharacterTurnUp", value: "-8" }); pitch -= 8; misses = 0; wait(800); }
      }
      const before = [...serverLog().slice(startLen).matchAll(hitRe)].length;
      on({ verb: "input", action: "CharacterMelee" });
      wait(3200);
      const hits = [...serverLog().slice(startLen).matchAll(hitRe)];
      if (hits.length === before) { misses++; continue; }
      misses = 0;
      const last = hits[hits.length - 1];
      const droppedBefore = dropped;
      dropped = hits.reduce((n, m) => n + Number(m[3]), 0);
      lastPiece = Math.max(...hits.map((m) => Number(m[1])));
      console.log(`  ${label}: hit ${hits.length}: piece ${last[1]}/${last[2]}, dropped so far ${dropped}`);
      if (dropped > droppedBefore) {
        const r = on({ verb: "pickup", item }, 15000);
        if (r.status !== "ok") { wait(1200); on({ verb: "pickup", item }, 15000); }
        wait(900);
      }
    }
    return { dropped, pieces: lastPiece };
  };

  arrive((n, spec, timeout) => on(spec, timeout), 1, wait);
  wait(8000);
  on({ verb: "input", action: "MenuBack" });
  wait(4000);
  on({ verb: "time", hour: 11.0 }, 15000);
  lap = Date.now();

  // 0. A MINER level 4, a fresh SMELTING 1. `levelset` logs `harness set <JOB> xp A -> B for <uid> (level X -> Y, ...)`.
  const levelSet = (job, level) => {
    const mark = serverLog().length;
    on({ verb: "levelset", words: [job, String(level)] }, 10000);
    const re = new RegExp(`harness set ${job} xp (\\d+) -> (\\d+) for (\\S+) \\(level (\\d+) -> (\\d+)`);
    for (let i = 0; i < 10; i++) {
      wait(1000);
      const m = re.exec(serverLog().slice(mark));
      if (m) return { xp: Number(m[2]), level: Number(m[5]) };
    }
    return null;
  };
  const miner = levelSet("MINER", 4);
  const smelting = levelSet("SMELTING", 1);
  step("0 the character is MINER 4 and SMELTING 1", !!miner && miner.level === 4 && !!smelting && smelting.level === 1 && smelting.xp === 0,
    `MINER ${miner ? `level ${miner.level} xp ${miner.xp}` : "no `harness set` line"}, SMELTING ${smelting ? `level ${smelting.level} xp ${smelting.xp}` : "no `harness set` line"}`);
  const base = { nuggets: carried("IronNugget"), coal: carried("CoalLump"), bars: carried("IronBar") };

  // 1. A pickaxe in hand (the dev verb stands in for the $150 purchase at the miners' till; `mine-till-proof` covers that).
  const give = on({ verb: "itemgive", prefab: PICKAXE }, 10000);
  const drew = on({ verb: "hold", item: "weapon" }, 10000);
  drawn = true;
  wait(2500);
  step("1 a pickaxe is in the pack and drawn", give.status === "ok" && carried("Pickaxe") > 0 && drew.status === "ok" && /Pickaxe/.test(drew.detail), `itemgive ${give.status}, hold ${drew.detail.slice(0, 80)}`);

  // 2. Highstone: two iron rocks, five pieces each, ten nuggets.
  siteShot(IRON_A, "1_highstone");
  lap = Date.now();
  const ironA = mine(IRON_A, "iron", 5, "highstone rock 1", "IronNugget");
  step("2 the first Highstone iron rock gives five pieces", ironA.pieces >= 5 && ironA.dropped >= 5, `pieces=${ironA.pieces} dropped=${ironA.dropped}`);
  siteShot(IRON_A, "2_highstone_mined");
  lap = Date.now();
  const ironB = mine(IRON_B, "iron", 5, "highstone rock 2", "IronNugget");
  step("3 the second Highstone iron rock gives five pieces", ironB.pieces >= 5 && ironB.dropped >= 5, `pieces=${ironB.pieces} dropped=${ironB.dropped}`);
  const nuggets = collect(IRON_B, "IronNugget", base.nuggets + 2 * BATCHES);
  step("4 ten Iron Nuggets are in the pack", nuggets >= base.nuggets + 2 * BATCHES, `carried IronNugget=${nuggets}`);

  // 3. East Rise: five pieces of the coal rock, five lumps.
  siteShot(COAL, "3_eastrise");
  lap = Date.now();
  const coal = mine(COAL, "coal-seam", BATCHES, "east rise", "CoalLump");
  step("5 the East Rise coal rock gives five pieces", coal.pieces >= BATCHES && coal.dropped >= BATCHES, `pieces=${coal.pieces} dropped=${coal.dropped}`);
  siteShot(COAL, "4_eastrise_mined");
  lap = Date.now();
  const lumps = collect(COAL, "CoalLump", base.coal + BATCHES);
  step("6 five Coal Lumps are in the pack", lumps >= base.coal + BATCHES, `carried CoalLump=${lumps}`);

  // 4. To the compound (the harness cannot drive: this times the teleport), then five batches in one run.
  on({ verb: "hold", item: "none" });
  drawn = false;
  wait(1800);
  if (pitch !== 0) { on({ verb: "input", action: "CharacterTurnUp", value: String(-pitch) }); pitch = 0; wait(600); }
  lap = Date.now();
  on({ verb: "stand", entityName: SMELTER.entity });
  wait(2500);
  const look = on({ verb: "look" }, 10000);
  step("7 at the compound the smelter's process action is in reach", look.detail.includes(`${PROCESS}@${SMELTER.entity}`), look.detail.slice(0, 160));
  on({ verb: "capture", mode: "ui", name: `${prefix}_5_smelter` }, 25000);
  lap = Date.now();
  const runMark = serverLog().length;
  const smelt = on({ verb: "use", className: PROCESS, entityName: SMELTER.entity }, 10000);
  wait(1500);
  const left = { nuggets: carried("IronNugget"), coal: carried("CoalLump") };
  step("8 the smelt starts and takes ten nuggets and five lumps", smelt.status === "ok" && left.nuggets === base.nuggets && left.coal === base.coal, `nuggets left=${left.nuggets} coal left=${left.coal}`);
  on({ verb: "capture", mode: "ui", name: `${prefix}_6_smelting` }, 25000);
  lap = Date.now();
  const t0 = Date.now();
  let done = "";
  while (Date.now() - t0 < (RUN_SECONDS + 90) * 1000) {
    done = serverLog().slice(runMark);
    if (/SMELTING xp for/.test(done)) break;
    wait(4000);
  }
  const runSeconds = Math.round((Date.now() - t0) / 1000);
  const xpLines = [...done.matchAll(/\+(\d+) SMELTING xp for \S+ from ([^\n(]+)/g)];
  const xpTotal = xpLines.reduce((n, m) => n + Number(m[1]), 0);
  step("9 the run ends and SMELTING XP is awarded", xpLines.length > 0, xpLines.length ? `${xpLines.length} award(s), +${xpTotal} xp, run ${runSeconds} s` : `no \`SMELTING xp\` line in ${runSeconds} s`);

  // 5. The bars on the ground at the machine, then in the pack.
  lap = Date.now();
  on({ verb: "stand", entityName: SMELTER.entity });
  wait(1500);
  on({ verb: "capture", mode: "ui", name: `${prefix}_7_bars_on_the_ground` }, 25000);
  lap = Date.now();
  const bars = collect(SMELTER, "IronBar", base.bars + BATCHES);
  step("10 five Iron Bars are in the pack", bars >= base.bars + BATCHES, `carried IronBar=${bars}`);

  // 6. Sell them at the dock wholesale iron buyer (the dev `stand` stands in for the drive to the harbour).
  lap = Date.now();
  on({ verb: "stand", entityName: BUYER.entity });
  wait(2500);
  const buyerLook = on({ verb: "look" }, 10000);
  const action = new RegExp(`(LF_\\w+Action)@${BUYER.entity}`).exec(buyerLook.detail);
  step("11 at the dock the iron buyer's trade action is in reach", !!action, action ? action[1] : buyerLook.detail.slice(0, 200));
  let sold = 0;
  let cashBefore = 0;
  let cashAfter = 0;
  if (action) {
    cashBefore = cash();
    lap = Date.now();
    // The buyer's F action sells one carried bar a press (`sold ... x1 of 1`): press it once per bar, as a player does.
    on({ verb: "capture", mode: "ui", name: `${prefix}_8_buyer` }, 25000);
    lap = Date.now();
    const pays = [];
    for (let press = 1; press <= BATCHES + 2 && carried("IronBar") > base.bars; press++) {
      const mark = serverLog().length;
      on({ verb: "use", className: action[1], entityName: BUYER.entity }, 10000);
      for (let i = 0; i < 8; i++) {
        wait(700);
        const m = /\[LFDebug:Trader\] sold \S+IronBar\.et x(\d+) of \d+ to (\S+) for (\d+)/.exec(serverLog().slice(mark));
        if (m) { pays.push({ buyer: m[2], n: Number(m[1]), paid: Number(m[3]) }); break; }
      }
    }
    on({ verb: "capture", mode: "ui", name: `${prefix}_9_sold` }, 25000);
    wait(800);
    cashAfter = cash();
    sold = pays.reduce((n, p) => n + p.n, 0);
    const paid = pays.reduce((n, p) => n + p.paid, 0);
    step("12 the buyer pays for all five bars", sold === BATCHES && carried("IronBar") === base.bars && cashAfter - cashBefore === paid && paid > 0,
      `${pays.length} press(es) sold ${sold} bar(s) to ${pays[0] ? pays[0].buyer : "nobody"} for $${paid}, cash ${cashBefore} -> ${cashAfter}`);
  } else {
    step("12 the buyer pays for the bars", false, "no trade action to open");
  }

  // Keep the pictures before `session down` (the session folder is swept): every capture of this run as a PNG in tmp/mining-iron-chain-pics.
  try {
    const out = join(dirname(fileURLToPath(import.meta.url)), "../../tmp/mining-iron-chain-pics");
    mkdirSync(out, { recursive: true });
    const logs = sessionPaths(dir).clients[0].logs;
    let saved = 0;
    for (const run of readdirSync(logs)) {
      const shots = join(logs, run, "el-captures");
      if (!existsSync(shots)) continue;
      for (const f of readdirSync(shots)) {
        if (!f.startsWith(prefix + "_") || !f.endsWith(".bmp")) continue;
        bmpFileToPng(join(shots, f), join(out, f.replace(/_\d+\.bmp$/, ".png")));
        saved++;
      }
    }
    console.log(`pictures: ${saved} saved in ${out}`);
  } catch (e) {
    console.log(`pictures: not saved (${e.message})`);
  }

  // The measured rates: seconds per step, and the pieces per minute while swinging.
  const total = Math.round((Date.now() - proofStart) / 1000);
  console.log("\nstep timings (seconds since the previous step):");
  for (const t of timings) console.log(`  ${t.ok ? "ok  " : "FAIL"} ${String(t.seconds).padStart(7)} s  ${t.step}`);
  console.log(`  total ${total} s, smelter run ${runSeconds} s for ${BATCHES} batches`);
  try {
    const out = join(dirname(fileURLToPath(import.meta.url)), "../../tmp/mining-iron-chain-timings.json");
    mkdirSync(dirname(out), { recursive: true });
    writeFileSync(out, JSON.stringify({ totalSeconds: total, smelterRunSeconds: runSeconds, batches: BATCHES, cash: { before: cashBefore, after: cashAfter }, steps: timings }, null, 2));
    console.log(`timings: ${out}`);
  } catch (e) {
    console.log(`timings: not saved (${e.message})`);
  }

  const failed = results.filter((r) => !r.ok);
  console.log(failed.length ? `mining-iron-chain proof: FAIL (${failed.length} of ${results.length}: ${failed.map((r) => r.name).join("; ")})` : `mining-iron-chain proof: PASS (${results.length} of ${results.length})`);
  return failed.length ? 1 : 0;
}

export default {
  name: "mining-iron-chain",
  usage: "mining-iron-chain [--cmd-timeout ms] [--name <picture prefix>]",
  run: () => runMiningIronChain(),
};
