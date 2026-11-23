import { arrive, clientBox, sessionDir } from "../session.mjs";

// #4306 on a running `up --clients 1` session (a dedicated server and one remote client): the client
// takes the robbery kit at the annex stand, the drill must land in one of its 1-0 quick slots on that
// client, pressing the slot (the vanilla quick bar, as a player's key does) must put it in the hands,
// and the vault must then offer and take the rob. When the drilling ends the drill lies by the vault;
// the robber must be able to pick it back up into a quick slot.
//   tools\cli session up
//   node agent-box/session.mjs drill-quickslot-proof
//   tools\cli session down
const KIT = "Annex_Bank_RobKit";
const VAULT = "Annex_Bank_Vault";
// `--island` (on `session up --mission island`): a fresh Everon character is handed the drill as a fence
// sale hands it (no kit stand there), then the same slot press and the Saint-Philippe vault.
const ISLAND_VAULT = "Fit_saint_philippe_bank_lf_bank_vault";
const ISLAND_VAULT_ID = "bank_vault";

export function parseFields(detail) {
  const out = {};
  for (const m of String(detail).matchAll(/(\w+)=(\S+)/g)) out[m[1]] = m[2];
  return out;
}

const truthy = (v) => v === "1" || v === "true";

// Problems with what the client reports: after the kit (`before`), the press, and a moment later (`after`).
export function quickSlotFails(before, press, after) {
  if (before.status !== "ok") return [`the client does not carry the drill after the kit: ${before.detail}`];
  const b = parseFields(before.detail);
  if (Number(b.slot) < 0) return [`the drill has no 1-0 quick slot on the client (mode=${b.mode})`];
  const out = [];
  if (!truthy(parseFields(press.detail).pressed)) out.push("the quick bar did not take the press");
  const a = parseFields(after.detail);
  if (!truthy(a.held)) out.push(`pressing slot ${Number(b.slot) + 1} did not put the drill in the hands (mode=${a.mode})`);
  return out;
}

async function runDrillQuickSlotProof(args = []) {
  const island = args.includes("--island");
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  arrive(on, 1, wait);
  wait(8000);
  on(1, { verb: "resetcrime" }, 10000);
  if (island) {
    on(1, { verb: "drill" }, 10000);
  } else {
    on(1, { verb: "stand", entityName: KIT });
    wait(2500);
    on(1, { verb: "look" });
    on(1, { verb: "use", className: "LF_BankRobKitAction", entityName: KIT }, 10000);
  }
  wait(3000);
  const before = on(1, { verb: "quickslot", words: ["drill"] }, 10000);
  let press = before;
  let after = before;
  if (before.status === "ok" && Number(parseFields(before.detail).slot) >= 0) {
    press = on(1, { verb: "quickslot", words: ["drill", "press"] }, 10000);
    wait(3000);
    after = on(1, { verb: "quickslot", words: ["drill"] }, 10000);
  }
  const fails = quickSlotFails(before, press, after);
  const vault = island ? ISLAND_VAULT : VAULT;
  on(1, { verb: "stand", entityName: vault });
  wait(2500);
  on(1, { verb: "look" });
  const offer = on(1, { verb: "vaultoffer", words: [island ? ISLAND_VAULT_ID : VAULT] }, 10000);
  if (!/perform=(1|true)/.test(offer.detail)) fails.push(`the vault does not offer the rob on the client: ${offer.detail}`);
  const rob = on(1, { verb: "use", className: "LF_BankRobAction", entityName: vault }, 10000);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  // The drill finishes and is left lying by the vault (LF_Bank.LeaveTool); the robber picks it back up.
  let left = false;
  for (let i = 0; i < 16 && !left; i++) {
    wait(5000);
    left = /^not_carried/.test(on(1, { verb: "quickslot", words: ["drill"] }, 10000).detail);
  }
  if (!left) fails.push("the drill was not left by the vault within 80 s");
  else {
    on(1, { verb: "quickslot", words: ["drill", "ground"] }, 10000);
    const pick = on(1, { verb: "quickslot", words: ["drill", "pickup"] }, 10000);
    wait(3000);
    const back = on(1, { verb: "quickslot", words: ["drill"] }, 10000);
    if (!truthy(parseFields(pick.detail).picked) || back.status !== "ok") fails.push(`the drill left by the vault does not pick back up: ${pick.detail}`);
    else if (Number(parseFields(back.detail).slot) < 0) fails.push(`the drill picked back up has no quick slot: ${back.detail}`);
  }
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "drill quickslot proof: FAIL" : "drill quickslot proof: PASS");
  return fails.length ? 1 : 0;
}

export default {
  name: "drill-quickslot-proof",
  usage: "drill-quickslot-proof [--island]",
  run: (ctx) => runDrillQuickSlotProof(ctx?.args ?? []),
};
