import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// #4457 follow-up: the registrar stands behind the registry desk of the Saint-Philippe cabin and the flag wall stands
// outside it, one client on `up --mission island --profile full --sound`.
//   1. The client stands at the registrar and talks: the scene opens (`LF_DialogueTalkAction`), the founding line plays.
//   2. The client founds an organization at the registry desk, walks to the flag wall and picks flag 3: the registrar
//      within 25 m says "That is your flag" (log `[LFDebug:Dialogue]` announce line) instead of the no-registrar line.
//   node agent-box/session.mjs registrar-proof            (--selftest runs the pure parts)
// The founding fee is carried cash (LF_OrgConfig.DEFAULT_FOUNDING_FEE, 250). A character with less (after another proof,
// or a fresh $0 to $199 newcomer) is topped up to the fee plus a margin with the harness `resetplayer <cash> keep` before the
// desk; when the top-up is refused the proof SKIPs with that reason instead of failing at the founding.
// The proof fails when the talk or the pick is refused; read the server log for the voice line afterwards:
//   tools\cli logs "registrar|OrgFlagWall"
const REGISTRAR = "Fit_saint_philippe_org_registrar";
const DESK = "Fit_saint_philippe_org_registry_desk";
const WALL = "Fit_saint_philippe_org_flag_wall";
const FOUNDING_FEE = 250; // LF_OrgConfig.DEFAULT_FOUNDING_FEE
const TOPUP_MARGIN = 50;
const ORG_NAME = "AlleyKings"; // the name the org proofs use (the field caps the length)

// Pure: carried cash the character should hold before the founding, or null when it already holds the fee.
export function topUpTarget(cash, fee = FOUNDING_FEE, margin = TOPUP_MARGIN) {
  const have = Number(cash);
  if (Number.isFinite(have) && have >= fee) return null;
  return fee + margin;
}

async function run(args = []) {
  if (args.includes("--selftest")) return selftest();
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

  // The founding fee is carried cash: top the character up to it first (a newcomer holds $200, a proof before this one may have spent it).
  const have = parseState(on({ verb: "state" }, 10000).detail).cash;
  const target = topUpTarget(have);
  if (target !== null) {
    console.log(`carried cash ${have ?? "unknown"} is under the $${FOUNDING_FEE} founding fee: topping up to $${target}`);
    const topped = on({ verb: "resetplayer", cash: target, keepInventory: true }, 15000);
    wait(2500);
    const now = Number(parseState(on({ verb: "state" }, 10000).detail).cash);
    if (topped.status !== "ok" || !(now >= FOUNDING_FEE)) {
      console.log(`SKIP  registrar-proof: the character holds $${Number.isFinite(now) ? now : "?"} and the harness top-up (resetplayer ${target} keep) answered ${topped.status} ${String(topped.detail).slice(0, 120)}; the $${FOUNDING_FEE} founding fee cannot be paid, so the founding is not tried`);
      return 0;
    }
  }

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

function selftest() {
  const ok = topUpTarget(0) === 300 && topUpTarget(100) === 300 && topUpTarget(249) === 300
    && topUpTarget(250) === null && topUpTarget("450") === null && topUpTarget(undefined) === 300 && topUpTarget("x") === 300;
  console.log(ok ? "registrar-proof selftest: OK" : "registrar-proof selftest: FAIL");
  return ok ? 0 : 1;
}

export default {
  name: "registrar-proof",
  usage: "registrar-proof   (run in a --mission island --profile full --sound session; --selftest runs the pure parts)",
  run: (ctx) => run(ctx?.args ?? process.argv.slice(2)),
};
