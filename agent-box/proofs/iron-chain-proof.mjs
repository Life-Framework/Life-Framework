import { existsSync, mkdirSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// The iron chain on the island (#3244), the way a miner takes it, with the dev verbs only where no player action exists:
//   node agent-box/session.mjs up --mission island --profile full --wait
//   node agent-box/session.mjs iron-chain-proof [--cmd-timeout ms] [--name <picture prefix>]
//   node agent-box/session.mjs down
// One client at SMELTING 1: swings a pickaxe at a Highstone iron rock (2 pieces = 2 Iron Nuggets) and an East Rise coal
// rock (1 piece = 1 Coal Lump), reading the `[LFDebug:Mining] ... rock hit ... dropped` lines and picking the drops up
// with the F-key `pickup`. At the iron smelter of the centre-east compound the smelt runs at SMELTING 1 (the machine has
// no level gate, #4856): 2 nuggets and the coal go in, the 60 s run ends, the bar drops, is picked up and `carries`
// counts it, and the server log shows the first SMELTING XP award. A last use with no nuggets left and `coalgive 2` in
// hand names the missing input. PASS or FAIL per
// step; every verdict reads the server's `[LFDebug:...]` lines or the client's own state, never a guess.
const PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
const IRON_ROCK = { entity: "Fit_highstone_mine_mine_ore_2", x: 4956.3, y: 115.795, z: 8474.6 };
const COAL_ROCK = { entity: "Fit_eastrise_mine_mine_ore_2", x: 6681.3, y: 148.933, z: 7929.6 };
const SMELTER = { entity: "Fit_eastcompound_smelter_smelter_furnace", x: 6442.481, y: 162.195, z: 6489.772 };
// The node ids the server prints in `<node> rock hit` (a regex: the pack renamed them to iron-rich and coal-seam).
const IRON_NODE = "iron(?:-rich)?";
const COAL_NODE = "coal(?:-seam)?";
const PROCESS = "LF_ProcessAction";
const SWINGS_PER_PIECE = 3;
const RUN_SECONDS = 60;
const RETRY_VERBS = ["carries", "capture", "look", "seen", "stand", "hold", "state"];

async function runIronChainProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const prefix = flag("--name", "iron_chain");
  const cmdFloor = Number(flag("--cmd-timeout", "8000"));
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  // A busy client (a capture settling, a loaded pool PC) answers `no_session` or `capture_busy` for a moment: ask again.
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
  const clientLog = () => {
    const f = newestLog(sessionPaths(dir).clients[0].logs);
    return f ? readFileSync(f, "utf8") : "";
  };
  const results = [];
  const step = (name, ok, detail) => {
    results.push({ name, ok });
    console.log(`${ok ? "PASS" : "FAIL"}  ${name}${detail ? `: ${detail}` : ""}`);
    return ok;
  };
  const carried = (text) => {
    const r = on({ verb: "carries", words: [text] }, 10000);
    const m = /count=(\d+)/.exec(r.detail);
    return r.status === "ok" && m ? Number(m[1]) : 0;
  };
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
  const stringText = (key) => {
    const id = key.replace(/^#/, "");
    const table = readFileSync(join(dirname(fileURLToPath(import.meta.url)), "../../addons/LifeFramework/Language/everonlife_localization.st"), "utf8");
    const m = new RegExp(`Id "${id}"\\s+Target_en_us "([^"]*)"`).exec(table);
    return m ? m[1] : "";
  };
  const toast = () => /toast=([^|]*?)(?:\s\w+=|$)/.exec(on({ verb: "seen", radius: 6 }, 10000).detail)?.[1]?.trim() ?? "";
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
  const siteShot = (site, name, off) => shoot({ verb: "capture", mode: "world", name: `${prefix}_${name}`, x: site.x, y: site.y, z: site.z, ox: off[0], oy: off[1], oz: off[2], hour: 11 });

  // Mines a rock until the log shows `pieces` pieces broken off it, returns the drops reported. Aim is the one thing a
  // player does by hand: a swing with no `rock hit` line tips the aim one step and tries again.
  const mine = (site, node, pieces, label, item) => {
    const hitRe = new RegExp(`\\[LFDebug:Mining\\][^\\n]*${node} rock hit[^\\n]*piece (\\d+)/(\\d+)[^\\n]*dropped (\\d+)`, "g");
    const startLen = serverLog().length;
    let dropped = 0;
    let lastPiece = 0;
    let misses = 0;
    for (let swing = 1; swing <= pieces * SWINGS_PER_PIECE + 12 && dropped < pieces; swing++) {
      if (swing === 1 || misses >= 2) {
        on({ verb: "stand", entityName: site.entity });
        wait(1800);
        if (swing === 1) { on({ verb: "input", action: "CharacterTurnUp", value: "-30" }); pitch -= 30; wait(800); }
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
      // A piece lies in a fan beside the rock: take it as soon as it drops, from the spot the miner swung from, the way a player does.
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

  // 0. A fresh account is SMELTING 1 (the set puts a re-run back there too).
  // `levelset` (#4773, a LF_HARNESS build) sets the trade to exactly that level, skipping the daily cap; the server logs
  // `harness set SMELTING xp A -> B for <uid> (level X -> Y, daily cap skipped)`.
  const levelSet = (level) => {
    const mark = serverLog().length;
    on({ verb: "levelset", words: ["SMELTING", String(level)] }, 10000);
    for (let i = 0; i < 10; i++) {
      wait(1000);
      const m = /harness set SMELTING xp (\d+) -> (\d+) for (\S+) \(level (\d+) -> (\d+)/.exec(serverLog().slice(mark));
      if (m) return { from: Number(m[1]), xp: Number(m[2]), uid: m[3], level: Number(m[5]) };
    }
    return null;
  };
  const level1 = levelSet(1);
  const base = { nuggets: carried("IronNugget"), coal: carried("CoalLump"), bars: carried("IronBar") };
  step("0 the character starts at SMELTING 1", !!level1 && level1.level === 1 && level1.xp === 0, level1 ? `xp=${level1.xp} level=${level1.level}` : "no `harness set` line in the server log");

  // 1. A pickaxe in hand (the island sells it at the mine desk; the dev verb stands in for the purchase, #1xx mine-till-proof covers that).
  const give = on({ verb: "itemgive", prefab: PICKAXE }, 10000);
  step("1 the pickaxe is in the pack", give.status === "ok" && carried("Pickaxe") > 0, `itemgive ${give.status}`);
  const drew = on({ verb: "hold", item: "weapon" }, 10000);
  step("1b the pickaxe is drawn", drew.status === "ok" && /Pickaxe/.test(drew.detail), drew.detail);
  drawn = true;
  wait(2500);

  // 2. Highstone: two pieces of the iron rock, two nuggets.
  siteShot(IRON_ROCK, "1_highstone", [3, 0.8, -3]);
  const iron = mine(IRON_ROCK, IRON_NODE, 2, "highstone", "IronNugget");
  siteShot(IRON_ROCK, "2_highstone_mined", [3, 0.8, -3]);
  step("2 the Highstone iron rock gives two pieces to the pickaxe", iron.pieces >= 2 && iron.dropped >= 2, `pieces=${iron.pieces} dropped=${iron.dropped} (server \`iron rock hit\` lines)`);
  const nuggets = collect(IRON_ROCK, "IronNugget", base.nuggets + 2);
  step("3 two Iron Nuggets are picked up", nuggets >= base.nuggets + 2, `carried IronNugget=${nuggets}`);

  // 3. East Rise: one piece of the coal rock, one lump.
  siteShot(COAL_ROCK, "3_eastrise", [3, 0.8, -3]);
  const coal = mine(COAL_ROCK, COAL_NODE, 1, "east rise", "CoalLump");
  siteShot(COAL_ROCK, "4_eastrise_mined", [3, 0.8, -3]);
  step("4 the East Rise coal rock gives a piece to the pickaxe", coal.pieces >= 1 && coal.dropped >= 1, `pieces=${coal.pieces} dropped=${coal.dropped} (server \`coal-seam rock hit\` lines)`);
  const lumps = collect(COAL_ROCK, "CoalLump", base.coal + 1);
  step("5 one Coal Lump is picked up", lumps >= base.coal + 1, `carried CoalLump=${lumps}`);

  // 4. The smelter turns SMELTING 1 away, in words.
  on({ verb: "hold", item: "none" });
  drawn = false;
  wait(1800);
  if (pitch !== 0) { on({ verb: "input", action: "CharacterTurnUp", value: String(-pitch) }); pitch = 0; wait(600); }
  on({ verb: "stand", entityName: SMELTER.entity });
  wait(2500);
  const look = on({ verb: "look" }, 10000);
  step("6 the smelter's process action is in reach", look.detail.includes(`${PROCESS}@${SMELTER.entity}`), look.detail.slice(0, 160));
  on({ verb: "capture", mode: "ui", name: `${prefix}_5_smelter` }, 25000);
  // 5. The real smelt at SMELTING 1: the iron smelter has no level gate (#4856), the trade is earned by smelting.
  const xpBefore = level1 ? level1.xp : 0;
  const runMark = serverLog().length;
  const smelt = on({ verb: "use", className: PROCESS, entityName: SMELTER.entity }, 10000);
  wait(1500);
  const started = serverLog().slice(runMark);
  step("7 the smelt starts and takes 2 nuggets and 1 coal", smelt.status === "ok" && carried("IronNugget") === base.nuggets && carried("CoalLump") === base.coal, `nuggets=${carried("IronNugget")} coal=${carried("CoalLump")} toast="${toast()}"`);
  on({ verb: "capture", mode: "ui", name: `${prefix}_7_smelting` }, 25000);
  const t0 = Date.now();
  let done = "";
  while (Date.now() - t0 < (RUN_SECONDS + 45) * 1000) {
    done = serverLog().slice(runMark);
    if (/SMELTING xp for/.test(done)) break;
    wait(4000);
  }
  const xpLine = /\+(\d+) SMELTING xp for (\S+) from ([^\n(]+)/.exec(done);
  step("8 the run ends and SMELTING XP is awarded", !!xpLine, xpLine ? `+${xpLine[1]} xp (${xpLine[3].trim()})` : `no \`SMELTING xp\` line in ${Math.round((Date.now() - t0) / 1000)} s`);
  // The award line carries the new total: `+28 SMELTING xp for <uid> from smelt_batch (total 3028, level 8)`.
  const total = /\+\d+ SMELTING xp for \S+ from [^(]+\(total (\d+), level (\d+)\)/.exec(done);
  step("9 the SMELTING total moved from 0 by the award (the first XP the character has)", !!total && !!xpLine && Number(total[1]) === xpBefore + Number(xpLine[1]) && total[2] === "1", total ? `${xpBefore} -> ${total[1]} (level ${total[2]})` : "no total in the award line");

  // 6. The bar: on the ground at the machine, then in the pack.
  on({ verb: "stand", entityName: SMELTER.entity });
  wait(1500);
  on({ verb: "capture", mode: "ui", name: `${prefix}_8_bar_on_the_ground` }, 25000);
  const bars = collect(SMELTER, "IronBar", base.bars + 1);
  step("10 an Iron Bar is in the inventory", bars >= base.bars + 1, `carried IronBar=${bars}`);
  on({ verb: "capture", mode: "ui", name: `${prefix}_9_bar_carried` }, 25000);

  // 7. The dev coal verb, and the machine names what is still missing.
  on({ verb: "coalgive", words: ["2"] }, 10000);
  // The server log is flushed in bursts: poll for the line instead of reading it once.
  let coalGrant = null;
  for (let i = 0; i < 10 && !coalGrant; i++) {
    wait(1000);
    coalGrant = /harness coal \S+ added=(\d+) held=(\d+)/.exec(serverLog().split("\n").filter((l) => /harness coal/.test(l)).pop() ?? "");
  }
  step("11 `coalgive 2` puts two lumps in the pack", !!coalGrant && coalGrant[1] === "2" && carried("CoalLump") === base.coal + 2, coalGrant ? `added=${coalGrant[1]} held=${coalGrant[2]}` : "no `harness coal` line");
  const lowMark = serverLog().length;
  const lowUse = on({ verb: "use", className: PROCESS, entityName: SMELTER.entity }, 10000);
  const lowReason = /cannot_perform reason=(.+)/.exec(serverLog().slice(lowMark));
  step("12 with coal but no nuggets the smelter will not run and names what is missing", lowUse.status === "error" && !!lowReason && /Iron Nugget/.test(lowReason[1]) && carried("CoalLump") === base.coal + 2, `use=${lowUse.status} reason="${lowReason ? lowReason[1].trim() : "none"}" coal=${carried("CoalLump")}`);

  // Keep the pictures before `session down` (the session folder is swept): every capture of this run as a PNG in tmp/iron-chain-pics.
  try {
    const out = join(dirname(fileURLToPath(import.meta.url)), "../../tmp/iron-chain-pics");
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

  const failed = results.filter((r) => !r.ok);
  console.log(failed.length ? `iron-chain proof: FAIL (${failed.length} of ${results.length}: ${failed.map((r) => r.name).join("; ")})` : `iron-chain proof: PASS (${results.length} of ${results.length})`);
  return failed.length ? 1 : 0;
}

export default {
  name: "iron-chain-proof",
  usage: "iron-chain-proof [--cmd-timeout ms] [--name <picture prefix>]",
  run: () => runIronChainProof(),
};
