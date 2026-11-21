import { spawnSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, parseState, readSession, sessionDir } from "../session.mjs";

// #3197, shop shelves after a real dedicated restart (#3117 kept shelf copies out of the garbage system,
// #3127 out of the world save). On a running `session up --mission playtest|island` (one client):
//  1. the server fills the store and logs one `shelf audit` line per stock line (LF_StoreComponent.AuditShelf):
//     stock, the copies it should show (`want`), the copies it tracks and the loose copies of that prefab
//     really standing inside the shop boundary; the proof reads those lines, so a doubled shelf shows as
//     `orphan` and an empty one as `tracked` below `want`;
//  2. client 1 buys one item: it takes a shelf copy and pays at the till (`paid N for 1 item`);
//  3. `session restart`: the server's SHUTDOWN save, a fresh boot, the client rejoins;
//  4. after the store is restored the audit lines must show tracked = want = loose for every line, no
//     orphans, and the bought line's stock one lower (or restocked up to its cap);
//  5. one more buy must pay.
// Defaults are the DebugWorld general store (playtest) and the Saint-Philippe supermarket (island); `--stand`
// and `--till` name other entities, `--store` another store key.

const STORES = {
  playtest: { key: "general_store", stand: "Shop_GeneralStore_Debug", till: "Shop_GeneralStore_Till" },
  island: { key: "everon_sp_supermarket", stand: "Fit_saint_philippe_supermarket_supermarket_shelves", till: "Fit_saint_philippe_supermarket_supermarket_till" },
};
const AUDIT = /store (\S+) shelf audit: (\S+) stock=(-?\d+) want=(\d+) tracked=(\d+) loose=(\d+) orphan=(\d+)/;

const val = (args, name, dflt) => {
  const i = args.indexOf(name);
  return i === -1 || i + 1 >= args.length ? dflt : args[i + 1];
};
const read = (f) => (f && existsSync(f) ? readFileSync(f, "utf8") : "");

// The newest audit line per prefab of one store in a log text, from `from` on.
export function lastAudit(text, key, from = 0) {
  const out = new Map();
  for (const line of text.slice(from).split(/\r?\n/)) {
    const m = AUDIT.exec(line);
    if (!m || m[1] !== key) continue;
    out.set(m[2], { prefab: m[2], stock: +m[3], want: +m[4], tracked: +m[5], loose: +m[6], orphan: +m[7] });
  }
  return out;
}

// The verdict on the shelves after the restart. `before` is the audit before the buy, `bought` the prefab taken.
export function judgeShelves(after, before, bought) {
  const fails = [];
  if (!after.size) fails.push("no shelf audit line after the restart");
  for (const [prefab, a] of after) {
    const name = prefab.replace(/^.*\//, "");
    if (a.tracked !== a.want) fails.push(`${name}: the shelf shows ${a.tracked} copies, its stock ${a.stock} wants ${a.want}`);
    if (a.orphan > 0 || a.loose !== a.tracked) fails.push(`${name}: ${a.loose} loose copies in the shop for ${a.tracked} tracked (${a.orphan} orphan)`);
    const b = before.get(prefab);
    if (b && prefab === bought && a.stock < b.stock - 1) fails.push(`${name}: stock ${b.stock} before the buy, ${a.stock} after the restart (one unit was sold)`);
    if (b && prefab !== bought && a.stock < b.stock) fails.push(`${name}: stock fell from ${b.stock} to ${a.stock} for an item nobody bought`);
  }
  for (const prefab of before.keys()) if (!after.has(prefab)) fails.push(`${prefab.replace(/^.*\//, "")}: audited before the restart, missing after it`);
  return fails;
}

async function runShelfRestartProof(args) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const s0 = readSession(dir);
  if (!s0 || s0.stoppedAt) {
    console.log("FAIL  no running session: 'tools\\cli session up --mission playtest' (or island) first");
    return 1;
  }
  const preset = STORES[s0.mission];
  if (!preset) {
    console.log(`FAIL  the proof needs 'session up --mission playtest' or island (restart and the shutdown save); this session is ${s0.mission}`);
    return 1;
  }
  const key = val(args, "--store", preset.key);
  const stand = val(args, "--stand", preset.stand);
  const till = val(args, "--till", preset.till);
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverLog = () => newestLog(join(readSession(dir).serverProfile, "logs"));
  const fails = [];
  const step = (name, ok, detail) => {
    console.log(`${ok ? "ok  " : "FAIL"}  ${name}: ${detail}`);
    if (!ok) fails.push(`${name}: ${detail}`);
    return ok;
  };
  const verdict = () => {
    console.log(fails.length ? `shelf-restart-proof: FAIL at ${fails[0]}` : "shelf-restart-proof: PASS (after a real restart each line shows its stock once, no orphan copies, and a buy still pays)");
    return fails.length ? 1 : 0;
  };
  // Polls the newest server log until the store has audit lines.
  const awaitAudit = (seconds) => {
    const end = Date.now() + seconds * 1000;
    while (Date.now() < end) {
      const text = read(serverLog());
      if (lastAudit(text, key).size > 0) {
        wait(6000); // the audit runs 4 s after a fill or a load; leave room for the last line
        return read(serverLog());
      }
      wait(2000);
    }
    return read(serverLog());
  };
  const buy = (label) => {
    on(1, { verb: "stand", entityName: stand });
    wait(2500);
    const took = on(1, { verb: "shelf" }, 10000);
    const item = /item=(\S+)/.exec(took.detail)?.[1];
    wait(1500);
    on(1, { verb: "stand", entityName: till, side: "s" });
    wait(1500);
    const before = (read(serverLog()).match(/ paid \d+ for 1 item/g) ?? []).length;
    const pay = on(1, { verb: "use", className: "LF_StoreTillAction", entityName: till }, 10000);
    wait(1500);
    const paid = (read(serverLog()).match(/ paid \d+ for 1 item/g) ?? []).length - before;
    step(`${label}: took a shelf copy`, took.status === "ok" && took.detail.includes("shelf took"), took.detail);
    step(`${label}: paid at the till`, pay.status === "ok" && paid >= 1, `till ${pay.status} ${pay.detail}; ${paid} paid line(s) in the server log`);
    return item;
  };

  arrive(on, 1, wait);
  wait(8000);
  const opening = parseState(on(1, { verb: "state" }, 10000).detail);
  console.log(`opening: bank=${opening.bank} cash=${opening.cash}`);

  // 1. the shelf as it fills.
  const log0 = awaitAudit(120);
  const start = lastAudit(log0, key);
  for (const a of start.values()) console.log(`before: ${a.prefab.replace(/^.*\//, "")} stock=${a.stock} want=${a.want} tracked=${a.tracked} loose=${a.loose} orphan=${a.orphan}`);
  if (!step("the store fills", start.size > 0, start.size ? `${start.size} stock line(s) audited for ${key}` : `no "store ${key} shelf audit" line: is the store key right (--store)?`)) return verdict();
  step("the shelves start whole", [...start.values()].every((a) => a.tracked === a.want && a.orphan === 0), "tracked = want and no orphan on every line before the buy");

  // 2. one buy.
  const bought = buy("first buy");
  const boughtPrefab = bought && [...start.keys()].find((p) => p.replace(/^\{[0-9A-Fa-f]+\}/, "") === bought.replace(/^\{[0-9A-Fa-f]+\}/, ""));
  if (!bought) return verdict();

  // 3. the real restart.
  const boot = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "restart"], { encoding: "utf8", windowsHide: true, env: { ...process.env, LF_SESSION_ROUTED: "1" }, timeout: 420000 });
  process.stdout.write(boot.stdout ?? "");
  if (!step("session restart", boot.status === 0, `exited ${boot.status}`)) return verdict();
  wait(8000);

  // 4. the shelf after the load.
  const log1 = awaitAudit(150);
  const after = lastAudit(log1, key);
  const a0 = start.get(boughtPrefab);
  const a1 = after.get(boughtPrefab);
  if (a0 && a1 && a1.stock >= a0.stock) console.log(`WARN  the bought line is back at stock ${a1.stock} (${a0.stock} before the buy): a restock ran, or the sale was not saved`);
  for (const a of after.values()) console.log(`after:  ${a.prefab.replace(/^.*\//, "")} stock=${a.stock} want=${a.want} tracked=${a.tracked} loose=${a.loose} orphan=${a.orphan}`);
  const deleted = (log1.match(/a restored copy of [^\r\n]*deleted/g) ?? []).length;
  console.log(`restored copies the store had to delete: ${deleted}`);
  const bad = judgeShelves(after, start, boughtPrefab);
  step("one set of stock on the shelves", bad.length === 0, bad.length ? bad.join("; ") : `${after.size} line(s): tracked = want = loose, no orphan`);

  // 5. a buy after the restart.
  wait(4000);
  buy("buy after the restart");
  return verdict();
}

export default {
  name: "shelf-restart-proof",
  usage: "shelf-restart-proof [--store key] [--stand EntityName] [--till EntityName]   (session up --mission playtest|island)",
  run: ({ args = [] } = {}) => runShelfRestartProof(args),
};
