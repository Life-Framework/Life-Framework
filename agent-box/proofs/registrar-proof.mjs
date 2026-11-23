import { arrive, clientBox, sessionDir } from "../session.mjs";

// #4457 follow-up: the registrar stands behind the registry desk of the Saint-Philippe cabin and the flag wall stands
// outside it, one client on `up --mission island --profile full --sound`.
//   1. The client stands at the registrar and talks: the scene opens (`LF_DialogueTalkAction`), the founding line plays.
//   2. The client founds an organization at the registry desk, walks to the flag wall and picks flag 3: the registrar
//      within 25 m says "That is your flag" (log `[LFDebug:Dialogue]` announce line) instead of the no-registrar line.
//   node agent-box/session.mjs registrar-proof
// The proof fails when the talk or the pick is refused; read the server log for the voice line afterwards:
//   tools\cli logs "registrar|OrgFlagWall"
const REGISTRAR = "Fit_saint_philippe_org_registrar";
const DESK = "Fit_saint_philippe_org_registry_desk";
const WALL = "Fit_saint_philippe_org_flag_wall";
const ATM = "Fit_saint_philippe_atms_drop_b79";
const ORG_NAME = "AlleyKings"; // the name the org proofs use (the field caps the length)

async function run() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const scene = () => on({ verb: "dialogue" }, 10000).detail;
  const shot = (name) => on({ verb: "capture", mode: "ui", name: `registrar_${name}` }, 25000);
  const fails = [];

  arrive((n, spec, t) => on(spec, t), 1, wait);

  // 1. Talk to the registrar behind the desk.
  const stood = on({ verb: "stand", entityName: REGISTRAR, side: "w" }, 20000);
  if (stood.status !== "ok") {
    console.log(`FAIL  stand at the registrar: ${stood.status} ${stood.detail}`);
    return 1;
  }
  const talk = on({ verb: "use", className: "LF_DialogueTalkAction", entityName: REGISTRAR }, 10000);
  if (talk.status !== "ok") fails.push(`the talk returned ${talk.status} ${talk.detail}`);
  let replies = "";
  for (let waited = 0; waited < 30000 && !/phase=choosing|^closed/.test(replies); waited += 2000) {
    wait(2000);
    replies = String(scene());
  }
  console.log(`scene after the greeting: ${replies}`);
  if (/^closed/.test(replies)) fails.push(`the talk did not open a scene (got: ${replies})`);
  shot("1_talk");
  on({ verb: "input", action: "MenuSelect" });
  wait(2500);

  // The founding fee is carried cash and a newcomer holds $200: withdraw $100 at the ATM (row 2 of its list).
  for (const side of ["e", "w", "n", "s"]) {
    on({ verb: "stand", entityName: ATM, side });
    wait(2500);
    if (String(on({ verb: "look" }).detail).includes("LF_ATMQuickAction")) break;
  }
  const withdraw = on({ verb: "use", className: "LF_ATMQuickAction__2" }, 10000);
  if (withdraw.status !== "ok") fails.push(`the ATM withdraw returned ${withdraw.status} ${withdraw.detail}`);
  wait(2000);

  // 2. Found an organization at the registry desk, then pick a flag at the wall.
  on({ verb: "stand", entityName: DESK }, 20000);
  wait(2500);
  const open = on({ verb: "use", className: "LF_OrganizationDeskAction", entityName: DESK }, 10000);
  if (open.status !== "ok") fails.push(`the registry desk returned ${open.status} ${open.detail}`);
  wait(1500);
  on({ verb: "type", widget: "OrgName", text: ORG_NAME });
  on({ verb: "click", widget: "FoundButton" });
  wait(3000);
  on({ verb: "click", widget: "CloseButton" });
  wait(1000);
  const founded = on({ verb: "state" }, 10000).detail;
  if (!founded.includes("org=AlleyKings")) fails.push(`the character did not found the organization (state: ${founded.slice(0, 200)})`);

  const walled = on({ verb: "stand", entityName: WALL }, 20000);
  if (walled.status !== "ok") fails.push(`stand at the flag wall returned ${walled.status} ${walled.detail}`);
  wait(2500);
  shot("2_wall");
  const pick = on({ verb: "use", className: "LF_OrgFlagWallAction__3", entityName: WALL }, 10000);
  if (pick.status !== "ok") fails.push(`the flag pick returned ${pick.status} ${pick.detail}`);
  wait(6000);
  shot("3_picked");

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "registrar-proof: FAIL" : "registrar-proof: PASS (grep the server log for the talk and the 'registrar' announce line)");
  return fails.length ? 1 : 0;
}

export default {
  name: "registrar-proof",
  usage: "registrar-proof   (run in a --mission island --profile full --sound session)",
  run,
};
