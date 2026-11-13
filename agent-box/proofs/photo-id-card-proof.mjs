import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// Photo identity card (#902) on a running session, driven the way players do it.
//   node agent-box/session.mjs up --profile full --clients 2
//   node agent-box/session.mjs photo-id-card-proof [--name <shot prefix>]
//   node agent-box/session.mjs down
// Both players sign form R-89. Client 1 talks to the town clerk ("I am new in town.") and is handed
// the card. Client 1 reads it (Inspect: the sheet with the portrait, the printed lines and the
// stamp; shot). Client 1 holds it up to client 2 (Show ID): client 2's screen shows the same card
// (shot) and it closes by itself a few seconds later. The shots are the PR's pictures.
const CLERK = "Quest_Clerk";

async function runPhotoIdCardProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const at = process.argv.indexOf("--name");
  const prefix = at >= 0 && process.argv[at + 1] ? process.argv[at + 1] : "idcard";
  const shot = (n, name) => on(n, { verb: "capture", mode: "ui", name: `${prefix}_${name}` }, 25000);
  const ui = (n) => on(n, { verb: "ui" }, 10000).detail;
  const fails = [];
  const expect = (cond, what) => { if (!cond) fails.push(what); };

  for (const n of [1, 2]) arrive(on, n, wait);
  // The spawn and the arrival quest settle first; a talk before that hands nothing over.
  wait(12000);

  // The clerk registers client 1 and hands the card over.
  on(1, { verb: "stand", entityName: CLERK, side: "w" });
  wait(2500);
  const talk = on(1, { verb: "use", className: "LF_QuestAction", entityName: CLERK }, 10000);
  expect(talk.status === "ok", `talking to the clerk returned ${talk.status} ${talk.detail}`);
  wait(6500);
  on(1, { verb: "input", action: "MenuSelect" });
  wait(3000);
  on(1, { verb: "input", action: "MenuSelect" });
  wait(1500);
  on(1, { verb: "input", action: "MenuBack" });
  wait(2500);
  const state1 = on(1, { verb: "state" }, 10000).detail;
  expect(/idcard=(?!none)\S+/.test(state1), `client 1 carries a card after the clerk (state: ${state1})`);

  // Inspect: client 1 reads their own card.
  const inspect = on(1, { verb: "open", menu: "IdCard" }, 10000);
  expect(inspect.status === "ok", `inspect returned ${inspect.status} ${inspect.detail}`);
  wait(3000);
  const sheet = ui(1);
  // `ui` names the focused button: the card's "Put it away" (IdClose) means the card sheet is up.
  expect(sheet.includes("IdClose"), `client 1 sees the card sheet (ui: ${sheet.slice(0, 200)})`);
  shot(1, "1_own_card");
  on(1, { verb: "input", action: "MenuBack" });
  wait(1500);
  expect(!ui(1).includes("IdClose"), "Back puts the card away");

  // Show ID: client 1 holds the card up to client 2.
  const other = `Player${parseState(on(2, { verb: "state" }, 10000).detail).player}`;
  const viewer = `Player${parseState(state1).player}`;
  on(2, { verb: "stand", entityName: CLERK, side: "n" });
  wait(2000);
  on(1, { verb: "stand", entityName: other });
  on(2, { verb: "stand", entityName: viewer });
  wait(2500);
  const show = on(1, { verb: "use", className: "LF_ShowIDAction", entityName: other }, 10000);
  expect(show.status === "ok", `Show ID returned ${show.status} ${show.detail}`);
  wait(3000);
  const shown = ui(2);
  expect(shown.includes("IdClose"), `client 2 sees the card held up to them (ui: ${shown.slice(0, 200)})`);
  shot(2, "2_shown_card");
  wait(8000);
  expect(!ui(2).includes("IdClose"), "the shown card closes by itself after a few seconds");
  shot(2, "3_after_show");

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "photo-id-card-proof: FAIL" : "photo-id-card-proof: OK (read the shots: the portrait, the lines, the stamp)");
  return fails.length ? 1 : 0;
}

export default { name: "photo-id-card-proof", usage: "photo-id-card-proof [--name <shot prefix>]   (up --profile full --clients 2)", run: () => runPhotoIdCardProof() };
