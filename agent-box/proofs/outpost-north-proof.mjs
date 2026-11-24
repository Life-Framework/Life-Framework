import { existsSync, mkdirSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// #4675: the north rebel outpost on the real island, played the way a player meets it. Run in a
// `session up --mission island --profile full --sound` session (`session down` straight after).
//
//   outpost-north-proof [out]
//     1. A walk up to the gate from the track side (pictures), the gate guard talks (scene opens, camera and caption).
//     2. The chalk board beside the quartermaster is read at standing 0: the server logs the rows and the hint line.
//     3. The quartermaster talks; at standing 0 the pistol is refused in words, the radio sells ($60 for anyone);
//        with the UNDERWORLD standing at tier 1 (`repgrant`) the pistol sells too, the rifle (tier 2) does not.
//     4. The ambience beds: at 12:00 the generator and the tent radio start, at 23:00 the fire and the wind do and
//        the generator stops (the client's own [LFDebug:Atmosphere] lines).
//   No coordinate is typed here: every spot is a layer entity of Place_outpost-north_camp.layer.
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const LAYER = join(ROOT, "addons", "LF_Everon1989", "Worlds", "MainWorld", "MainWorld_Layers", "Place_outpost-north_camp.layer");
const P = "Fit_outpost_north_camp_outpost_";
const SPOT = {
  gateSandbagS: `${P}gate_sandbag_s2`, // the east end of the gate line; the south guard stands 2 m north-west of it
  table: `${P}table`, // the quartermaster stands 2 m north of the spot 2 m east of the table
  board: `${P}board`,
  middle: `${P}stool_n`, // the camp's middle, inside every bed's radius
};
const NPC = { guard: `${P}npc_guard_s`, quartermaster: `${P}npc_quartermaster` };
const BED = { generator: `${P}bed_generator`, fire: `${P}bed_fire`, radio: `${P}bed_radio`, wind: `${P}bed_wind` };
const TALK = "LF_DialogueTalkAction";
const BUY = "LF_OutpostStockBuyAction";
const READ = "LF_OutpostBoardReadAction";

// Pure: whether the Atmosphere lines (oldest first) show the named emitter live after index `from`: `start <name> ...` with a sound
// manager, else `<name> is in range ... got no voice` (a session without `--sound`).
export function liveAfter(lines, name, from = 0) {
  return lines.slice(from).some((l) => l.includes(name) && (/^start\b/.test(l) || /is in range at/.test(l)));
}

// Pure: whether a `stop <name>` line follows index `from`.
export function stoppedAfter(lines, name, from = 0) {
  return lines.slice(from).some((l) => /^stop\b/.test(l) && l.includes(name));
}

// Pure: the layer entity's top-level coords, or null (a camera target is read from the layer, never typed).
export function coordsOf(text, name) {
  const m = new RegExp(`^\\w+ ${name}\\b[^\\n]*\\{[\\s\\S]*?\\n coords ([-\\d.]+) ([-\\d.]+) ([-\\d.]+)`, "m").exec(text);
  return m ? { x: Number(m[1]), y: Number(m[2]), z: Number(m[3]) } : null;
}

// Pure: the server lines (any order) that say `bought <id> at the outpost`, as the list of ids.
export function boughtIds(log) {
  return [...String(log).matchAll(/bought (\S+) at the outpost for (\d+)/g)].map((m) => m[1]);
}

async function main(args) {
  if (args.includes("--selftest")) return selftest();
  const out = args[0] ? resolve(String(args[0])) : "";
  if (out) mkdirSync(out, { recursive: true });
  const layer = readFileSync(LAYER, "utf8");
  const at = (name) => coordsOf(layer, name);
  const gate = at(SPOT.gateSandbagS);
  const board = at(SPOT.board);
  const middle = at(SPOT.middle);
  if (!gate || !board || !middle) { console.log("outpost-north: the layer lacks the gate sandbag, the board or the camp middle"); return 2; }

  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 240)}`);
    if (spec.verb === "capture" && r.status === "ok" && out) {
      const shot = h.resolveCapture(r.detail);
      if (shot) console.log(`  saved ${bmpFileToPng(shot, join(out, `${spec.name}.png`))}`);
    }
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const logLines = (kind, side) => {
    const dirOf = side === "server" ? join(sessionPaths(dir).server, "logs") : sessionPaths(dir).clients[0].logs;
    const f = newestLog(dirOf);
    if (!f || !existsSync(f)) return [];
    return readFileSync(f, "utf8").split("\n").filter((l) => l.includes(`[LFDebug:${kind}]`)).map((l) => l.replace(new RegExp(`^.*\\[LFDebug:${kind}\\]\\s*`), "").trim());
  };
  const atmo = () => logLines("Atmosphere", "client");
  const outpostServer = () => logLines("Outposts", "server");
  // The camera stands at the target (p.y + 1) plus the offset, so oy 0.7 is a standing player's eye over the ground.
  const free = (name, p, ox, oy, oz, hour) => on({ verb: "capture", mode: "world", name, x: p.x, y: p.y + 1, z: p.z, ox, oy, oz, hour, free: true }, 25000);
  // The free camera of a world shot stays until `clean`; a ui shot after it would show the free view, so end it first.
  const ui = (name) => { on({ verb: "clean" }, 10000); wait(2000); return on({ verb: "capture", mode: "ui", name }, 25000); };
  const fails = [];
  const expect = (cond, label) => { console.log(`  ${cond ? "ok  " : "FAIL"} ${label}`); if (!cond) fails.push(label); };

  arrive((n, spec, t) => on(spec, t), 1, wait);
  for (let i = 0; i < 8; i++) {
    wait(3000);
    if (on({ verb: "click", widget: "WelcomeGuideCloseButton" }, 15000).status === "ok") break;
  }
  on({ verb: "input", action: "MenuBack" });
  wait(1500);
  on({ verb: "resetplayer", cash: 3000 }, 10000);
  wait(1500);
  on({ verb: "time", hour: 12 }, 15000);

  // 1. The walk up: 25 m out on the approach side of the gate, then at the gate line, then the guard.
  on({ verb: "stand", entityName: SPOT.gateSandbagS, side: "s25" }, 25000);
  wait(4000);
  free("1_approach", gate, 0, 0.7, -22, 12);
  on({ verb: "stand", entityName: SPOT.gateSandbagS, side: "s4" }, 25000);
  wait(3000);
  free("2_gate", gate, 0, 0.7, -9, 12);
  // The guard streams in with the camp: stand beside him and look until the F key offers his Talk (up to four tries).
  let look = { detail: "" };
  for (let i = 0; i < 4 && !String(look.detail).includes(TALK); i++) {
    on({ verb: "stand", entityName: SPOT.gateSandbagS, side: "n2" }, 20000);
    wait(4000);
    look = on({ verb: "look" });
  }
  expect(String(look.detail).includes("Talk") || String(look.detail).includes(TALK), `the F key offers the guard's Talk: ${String(look.detail).slice(0, 160)}`);
  const talk = on({ verb: "use", className: TALK, entityName: NPC.guard }, 10000);
  expect(talk.status === "ok", "talking to the gate guard works");
  wait(1500);
  const scene = (arg, value) => on({ verb: "dialogue", arg, value }, 10000).detail;
  // Leaves a talk the way a player does: confirm skips a playing line, Back (the hint line says "0 or Back: goodbye") ends the scene once the replies show.
  const leaveScene = (who) => {
    for (let i = 0; i < 6; i++) {
      const s = String(scene());
      if (s.startsWith("closed")) break;
      if (/phase=choosing/.test(s)) { on({ verb: "input", action: "MenuBack" }); wait(3000); }
      else { on({ verb: "input", action: "MenuSelect" }); wait(2500); }
    }
    expect(String(scene()).startsWith("closed"), `${who}: Goodbye ends the scene and gives the camera back`);
  };
  const guardScene = scene();
  expect(/open node=greet/.test(String(guardScene)), `the guard's greeting opens (${guardScene})`);
  ui("3_guard_talk");
  leaveScene("the gate guard");

  // 2. The chalk board at standing 0.
  on({ verb: "stand", entityName: SPOT.board, side: "s2" }, 20000); // the board faces the approach (south)
  wait(2500);
  const mark = outpostServer().length;
  expect(on({ verb: "use", className: READ, entityName: SPOT.board }, 10000).status === "ok", "the board's Read row is used");
  wait(3500);
  const boardLines = outpostServer().slice(mark);
  expect(boardLines.some((l) => /outpost board read by player \d+: rows=3 hint=1/.test(l)), `the server answers the board with three rows and the hint (${boardLines.slice(-2).join(" | ")})`);
  free("4_board_from_south", board, 0, 0.6, -2.6, 12);
  free("5_board_from_north", board, 0, 0.6, 2.6, 12);

  // 3. The quartermaster: talk, the refusal in words, then standing and the buys.
  on({ verb: "stand", entityName: SPOT.table, side: "e2" }, 20000);
  wait(2500);
  const qmLook = on({ verb: "look" });
  expect(/Buy/i.test(String(qmLook.detail)), `the F key offers the quartermaster's Buy rows (${String(qmLook.detail).slice(0, 200)})`);
  ui("6_quartermaster_buy_rows");
  expect(on({ verb: "use", className: TALK, entityName: NPC.quartermaster }, 10000).status === "ok", "talking to the quartermaster works");
  wait(1500);
  expect(/open node=greet/.test(String(scene())), "the quartermaster's greeting opens");
  ui("7_quartermaster_talk");
  leaveScene("the quartermaster");

  const buy = (index) => on({ verb: "use", className: index ? `${BUY}__${index}` : BUY, entityName: NPC.quartermaster }, 10000);
  buy(0);
  wait(2500);
  expect(boughtIds(outpostServer().join("\n")).includes("gang_radio"), "standing 0: the gang radio sells ($60 for anyone)");
  buy(1);
  wait(2500);
  ui("8_pistol_refused_in_words");
  expect(!boughtIds(outpostServer().join("\n")).includes("pistol"), "standing 0: the pistol is refused");
  on({ verb: "repgrant", words: ["UNDERWORLD", "100"] }, 10000);
  wait(2500);
  buy(1);
  wait(2500);
  ui("9_pistol_bought");
  const bought = boughtIds(outpostServer().join("\n"));
  expect(bought.includes("pistol"), "standing tier 1: the pistol sells");
  buy(2);
  wait(2500);
  expect(!boughtIds(outpostServer().join("\n")).includes("rifle"), "standing tier 1: the rifle (tier 2) does not");

  // 4. The beds, from the middle of the camp.
  on({ verb: "stand", entityName: SPOT.middle, side: "n1" }, 20000);
  wait(2500);
  // The clock has stood at noon since the walk up, so the noon beds started while the player walked in: read the whole client log.
  wait(6000);
  let now = atmo();
  let m = now.length;
  expect(liveAfter(now, BED.generator, 0), "12:00: the generator bed starts");
  expect(liveAfter(now, BED.radio, 0), "12:00: the tent radio bed starts");
  expect(!liveAfter(now, BED.fire, 0) && !liveAfter(now, BED.wind, 0), "12:00: the fire and the wind are silent (outside their hours)");
  on({ verb: "clean" }, 10000);
  free("10_camp_noon", middle, 9, 5, -9, 12);
  on({ verb: "time", hour: 23 }, 15000);
  wait(9000);
  now = atmo();
  expect(liveAfter(now, BED.fire, m), "23:00: the fire bed starts");
  expect(liveAfter(now, BED.wind, m), "23:00: the wind bed starts");
  expect(stoppedAfter(now, BED.generator, m), "23:00: the generator bed stops");
  free("11_camp_night", middle, 9, 5, -9, 23);

  for (const l of atmo().slice(-12)) console.log(`    ${l.slice(0, 200)}`);
  console.log(fails.length ? `outpost-north: FAIL (${fails.join("; ")})` : "outpost-north: PASS");
  return fails.length ? 1 : 0;
}

function selftest() {
  const lines = ["start Fit_a at 4 m (radius 30) x.acp", "stop Fit_a (hours)", "Fit_b is in range at 5 m (radius 54) and got no voice for 5 s"];
  const layer = "GenericEntity Fit_a {\n components {\n }\n coords 1 2 3\n}\n";
  const log = "x bought gang_radio at the outpost for 60\n y bought pistol at the outpost for 450";
  const ok = liveAfter(lines, "Fit_a") && !liveAfter(lines, "Fit_a", 1) && stoppedAfter(lines, "Fit_a", 0) && !stoppedAfter(lines, "Fit_a", 2)
    && liveAfter(lines, "Fit_b", 2) && coordsOf(layer, "Fit_a")?.z === 3 && coordsOf(layer, "Nope") === null
    && boughtIds(log).join(",") === "gang_radio,pistol" && boughtIds("").length === 0;
  console.log(ok ? "outpost-north-proof selftest: OK" : "outpost-north-proof selftest: FAIL");
  return ok ? 0 : 1;
}

export default {
  name: "outpost-north-proof",
  usage: "outpost-north-proof [out]   (run in a --mission island --profile full --sound session; --selftest runs the pure parts)",
  run: (ctx) => main(ctx?.args ?? process.argv.slice(2)),
};
