import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, sessionDir, sessionPaths } from "../session.mjs";
import { firstMoney } from "./_money.mjs";

// #1487 (the #310 acceptance, driven by an agent): a price fix goes live in one restart, with no Workshop
// build and no script change. On `up --clients 1 --mission playtest` (restart needs the playtest mission):
//   1. client 1 opens the general store's price list and reads the price of water (OLD);
//   2. the proof changes that price in the server's PROFILE copy of config.json only (never
//      server/flagship/config.json: the repo file is not touched);
//   3. the server restarts (`session restart`: shutdown save, boot with the save, same clients rejoin);
//   4. client 1 reads the price list and the order slip again and both show the new price (NEW).
// `--skip-restart` leaves step 3 out: the server never reads the file, so the proof must fail at "new price"
// (the red run). `--prepare` (before `up`) writes the starting config.json the session boots with.
const SHOP = "Shop_GeneralStore_Debug";
const RULE = "Prefabs/Items/Drinks/";
const PREPARED = 17;
const RAISE = 7;

function ownerFile() {
  return join(sessionPaths(sessionDir()).server, "profile", "LifeFramework", "config.json");
}

// The owner config with the drinks rule set to `price`; every other key the file already holds stays.
export function withDrinkPrice(text, price) {
  let cfg = { schemaVersion: 1 };
  if (text) {
    try { cfg = JSON.parse(text); } catch { cfg = { schemaVersion: 1 }; }
  }
  cfg.shops = cfg.shops && typeof cfg.shops === "object" ? cfg.shops : {};
  cfg.shops.priceRules = cfg.shops.priceRules && typeof cfg.shops.priceRules === "object" ? cfg.shops.priceRules : {};
  cfg.shops.priceRules[RULE] = { ...(cfg.shops.priceRules[RULE] || {}), buyPrice: price };
  return JSON.stringify(cfg, null, 2) + "\n";
}

// The first dollar amount on a screen text, or -1.
export function firstPrice(text) {
  return firstMoney(text) ?? -1;
}

async function runConfigHotfixProof(args) {
  const file = ownerFile();
  if (args.includes("--prepare")) {
    mkdirSync(join(file, ".."), { recursive: true });
    writeFileSync(file, withDrinkPrice("", PREPARED));
    console.log(`profile config: water ${PREPARED} -> ${file}`);
    return 0;
  }
  const skipRestart = args.includes("--skip-restart");

  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^[0-9]+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];

  // Opens the price list, narrows it to water, puts one on the order slip, returns both screens' text.
  const readWater = (label) => {
    on(1, { verb: "stand", entityName: SHOP });
    wait(2000);
    on(1, { verb: "use", className: "LF_ShopAction", entityName: SHOP }, 10000);
    wait(1500);
    on(1, { verb: "type", widget: "Search", text: "Water" });
    wait(800);
    const list = on(1, { verb: "ui", mode: "text" }, 10000).detail;
    on(1, { verb: "click", widget: "Item0Row_Action" });
    wait(600);
    const slip = on(1, { verb: "ui", mode: "text" }, 10000).detail;
    on(1, { verb: "click", widget: "CloseButton" });
    wait(800);
    console.log(`${label} list: ${list}`);
    console.log(`${label} slip: ${slip}`);
    return { list, slip };
  };

  const original = existsSync(file) ? readFileSync(file, "utf8") : null;
  try {
    arrive(on, 1, wait);
    wait(8000);
    const before = readWater("before");
    const oldPrice = firstPrice(before.list);
    if (oldPrice < 0) {
      fails.push("old price: the price list shows no dollar amount for water");
    } else {
      let prepared = -1;
      try { prepared = Number(JSON.parse(original || "{}")?.shops?.priceRules?.[RULE]?.buyPrice ?? -1); } catch { prepared = -1; }
      if (prepared >= 0 && oldPrice !== prepared) fails.push(`old price: the booted server lists $${oldPrice}, the profile config.json says $${prepared}`);
    }
    const newPrice = (oldPrice < 0 ? PREPARED : oldPrice) + RAISE;

    // The fix: the profile copy only. server/flagship/config.json is never opened.
    mkdirSync(join(file, ".."), { recursive: true });
    writeFileSync(file, withDrinkPrice(original, newPrice));
    console.log(`profile config: water ${oldPrice} -> ${newPrice} (${file})`);

    if (skipRestart) {
      console.log("restart skipped (--skip-restart): the server keeps the config it booted with");
    } else {
      const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "restart"], {
        windowsHide: true, stdio: "inherit", cwd: process.cwd(), env: { ...process.env, LF_SESSION_ROUTED: "1" }, timeout: 420000,
      });
      if (r.status !== 0) fails.push(`restart: session restart exited ${r.status}`);
      else {
        arrive(on, 1, wait);
        wait(8000);
      }
    }

    if (!fails.length || skipRestart) {
      const after = readWater("after");
      const money = `$${newPrice}`;
      if (!after.list.includes(money)) fails.push(`new price: the price list does not show ${money} (old ${oldPrice})`);
      if (!after.slip.includes(money)) fails.push(`new price: the order slip does not show ${money} (old ${oldPrice})`);
    }
  } finally {
    // The profile copy goes back to what the session booted with.
    if (original === null) rmSync(file, { force: true });
    else writeFileSync(file, original);
  }

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "config hotfix proof: FAIL" : "config hotfix proof: PASS (a price changed in the profile config.json is on the store's list and slip after one restart; no build, no script change)");
  return fails.length ? 1 : 0;
}

export default {
  name: "config-hotfix-proof",
  usage: "config-hotfix-proof [--skip-restart] [--prepare]   (up --clients 1 --mission playtest)",
  run: (ctx) => runConfigHotfixProof(ctx.args ?? []),
};
