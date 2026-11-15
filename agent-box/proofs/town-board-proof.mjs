import { arrive, clientBox, clientSees, sessionDir } from "../session.mjs";

// The town board on the spawn square (Sign_TownMap), driven the way a player does it, on a running
// `up --profile full` session (one client). The client walks up: the face draws the eight cards.
// It reads the first card (work): the sheet shows the title, WHERE and the detail with a Set waypoint
// button. Set waypoint closes the sheet and puts the marker over the post office; `seen` reads the
// board (cards/face) and the waypoint (card@metres/hud). Shots: the board from the square, the cards
// up close, the sheet, the marker on the screen.
//   node agent-box/session.mjs up --profile full --wait
//   node agent-box/session.mjs town-board-proof
//   node agent-box/session.mjs down
const SITE = "Sign_TownMap";
const ACTION = "LF_TownBoardCardAction";
// Signs.layer: Sign_TownMap at 130.5 0 246.5, the cork's centre 1.555 m up, reading toward +x (east, the street).
const BOARD_AT = { x: 130.5, y: 1.6, z: 246.5 };
const FACING = { x: 1, z: 0 };
// Where the character waits out of the close-up: the clerk, a few steps from the board.
const AWAY = "Quest_Clerk";
// Sign_PostOffice in Signs.layer: the work card's place, for the marker shot.
const POST_OFFICE = { x: 152.5, y: 2.5, z: 258.49 };

export function boardOf(line) {
  const board = /\btownboard=(\d+)\/(\w+)/.exec(String(line ?? ""));
  const mark = /\bwaypoint=(?:-|(\w+)@(\d+)\/(\w+))/.exec(String(line ?? ""));
  if (!board || !mark) return null;
  return { cards: Number(board[1]), face: board[2], mark: mark[1] ?? "", meters: Number(mark[2] ?? -1), hud: mark[3] ?? "" };
}

async function runTownBoardProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 220)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  const expect = (label, judge, budgetMs = 15000) => {
    const r = clientSees(on, wait, 1, (b) => judge(b), { label, budgetMs, parse: boardOf });
    fails.push(...r.fails);
    if (!r.fails.length) console.log(`ok    ${label} (${r.seconds.toFixed(1)} s)`);
    return r;
  };
  const camera = (name, from, to, fov = 55, mode = "ui") => {
    on(1, { verb: "camera", args: "hud off" });
    on(1, { verb: "camera", args: "stop" });
    on(1, { verb: "camera", args: `static ${from.x} ${from.y} ${from.z} ${to.x} ${to.y} ${to.z} ${fov} 50` });
    wait(2500);
    const r = on(1, { verb: "capture", mode, name }, 25000);
    on(1, { verb: "camera", args: "stop" });
    on(1, { verb: "camera", args: "hud on" });
    return r;
  };

  arrive(on, 1, wait);
  must(on(1, { verb: "stand", entityName: SITE }), "stand at the board");
  wait(3000);
  expect("the board draws its eight cards", (b) => (b.cards === 8 && b.face !== "released" ? [] : [`townboard=${b.cards}/${b.face}`]));
  // After the settle refresh, so the cork and the paper are at full detail on the frozen frame.
  wait(12000);
  const out = (d) => ({ x: BOARD_AT.x + FACING.x * d, y: BOARD_AT.y, z: BOARD_AT.z + FACING.z * d });
  camera("town_board_square", { ...out(4.2), y: 1.8 }, BOARD_AT, 60);
  // The character steps out of the close-up first (it stands in front of the board).
  on(1, { verb: "stand", entityName: AWAY });
  wait(1500);
  camera("town_board_close", out(1.55), BOARD_AT, 60);
  camera("town_board_back", { ...out(-3.0), y: 1.8 }, BOARD_AT, 60);
  must(on(1, { verb: "stand", entityName: SITE }), "back at the board");
  wait(1500);

  // Read the first card (work): the sheet.
  on(1, { verb: "look" });
  must(on(1, { verb: "use", className: `${ACTION}__0`, entityName: SITE }, 10000), "read the work card");
  wait(1500);
  const sheet = must(on(1, { verb: "ui", mode: "text" }), "read the sheet");
  for (const want of ["MarkButton", "Close"]) if (!String(sheet.detail).includes(want)) fails.push(`the sheet has no ${want}: ${String(sheet.detail).slice(0, 300)}`);
  must(on(1, { verb: "capture", mode: "ui", name: "town_board_sheet" }, 25000), "shot of the sheet");

  // Set waypoint: the sheet closes and the marker stands over the post office.
  must(on(1, { verb: "click", widget: "MarkButton" }), "set waypoint");
  wait(1500);
  expect("the waypoint is set on the work card's place", (b) => (b.mark === "work" && b.meters > 8 && b.hud !== "none" ? [] : [`waypoint=${b.mark}@${b.meters}/${b.hud}`]));
  const after = on(1, { verb: "ui", mode: "text" });
  if (String(after.detail).includes("MarkButton")) fails.push("the sheet stayed open after Set waypoint");
  // From in front of the board toward the post office, HUD on: the marker over the place.
  on(1, { verb: "camera", args: "stop" });
  on(1, { verb: "camera", args: `static ${out(2).x} 1.8 ${out(2).z} ${POST_OFFICE.x} ${POST_OFFICE.y} ${POST_OFFICE.z} 60 50` });
  wait(2000);
  must(on(1, { verb: "capture", mode: "ui", name: "town_board_waypoint" }, 25000), "shot of the marker");
  on(1, { verb: "camera", args: "stop" });

  if (fails.length) {
    console.log(`town-board-proof: FAIL`);
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("town-board-proof: OK");
  return 0;
}

export default { name: "town-board-proof", usage: "town-board-proof", run: () => runTownBoardProof() };
