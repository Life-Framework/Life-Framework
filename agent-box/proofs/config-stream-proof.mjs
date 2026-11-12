import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, sessionDir, sessionPaths } from "../session.mjs";

// #285: the owner's config.json reaches the clients' price lists on a dedicated server.
//   config-stream-proof --prepare   before `up`: writes the server's config.json (water 17)
//   config-stream-proof             on `up --clients 1`: client 1 reads 17 on the general store
//     price list and the order slip; the file changes to 23 and /configreload streams it; client 1
//     reads 23; client 2 joins late and reads 23; the file is removed, reloaded, and client 1
//     reads the shipped 6 again.
const SHIPPED = 6;
const FIRST = 17;
const CHANGED = 23;
const SHOP = "Shop_GeneralStore_Debug";

function ownerFile() {
  return join(sessionPaths(sessionDir()).server, "profile", "LifeFramework", "config.json");
}

function writeOwner(price) {
  const file = ownerFile();
  mkdirSync(join(file, ".."), { recursive: true });
  writeFileSync(file, JSON.stringify({ schemaVersion: 1, shops: { priceRules: { "Prefabs/Items/Drinks/": { buyPrice: price } } } }) + "\n");
  console.log(`owner config: water ${price} -> ${file}`);
}

function clientLog(dir, n) {
  const logs = join(dir, `client${n}`, "logs");
  if (!existsSync(logs)) return "";
  return readdirSync(logs).sort().map((d) => join(logs, d, "console.log")).filter(existsSync).map((p) => readFileSync(p, "utf8")).join("\n");
}

async function runConfigStreamProof(args) {
  if (args.includes("--prepare")) {
    writeOwner(FIRST);
    return 0;
  }

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

  // Opens the price list, narrows it to water, puts one on the slip and returns the screen text.
  const readWater = (n, label) => {
    on(n, { verb: "stand", entityName: SHOP });
    wait(2000);
    on(n, { verb: "use", className: "LF_ShopAction", entityName: SHOP }, 10000);
    wait(1500);
    on(n, { verb: "type", widget: "Search", text: "Water" });
    wait(800);
    const list = on(n, { verb: "ui", mode: "text" }, 10000).detail;
    on(n, { verb: "click", widget: "Item0Row_Action" });
    wait(600);
    const slip = on(n, { verb: "ui", mode: "text" }, 10000).detail;
    on(n, { verb: "click", widget: "CloseButton" });
    wait(800);
    console.log(`${label} list: ${list}`);
    console.log(`${label} slip: ${slip}`);
    return { list, slip };
  };
  const expectPrice = (r, price, label) => {
    const money = `$${price}`;
    if (!r.list.includes(money)) fails.push(`${label}: the price list does not show ${money}`);
    if (!r.slip.includes(money)) fails.push(`${label}: the order slip does not show ${money}`);
  };

  arrive(on, 1, wait);
  wait(8000);
  expectPrice(readWater(1, "client 1 at join"), FIRST, "client 1 at join");

  writeOwner(CHANGED);
  const reload = on(1, { verb: "configreload" }, 10000);
  if (reload.status !== "ok") fails.push(`configreload returned ${reload.status} ${reload.detail}`);
  wait(3000);
  expectPrice(readWater(1, "client 1 after the change"), CHANGED, "client 1 after the change");

  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "2"], { windowsHide: true, stdio: "inherit" });
  if (joined.status !== 0) fails.push("client 2 did not join late");
  else {
    arrive(on, 2, wait);
    wait(8000);
    expectPrice(readWater(2, "client 2 late join"), CHANGED, "client 2 late join");
  }

  rmSync(ownerFile(), { force: true });
  console.log("owner config: removed");
  on(1, { verb: "configreload" }, 10000);
  wait(3000);
  expectPrice(readWater(1, "client 1 with no config.json"), SHIPPED, "client 1 with no config.json");

  for (const n of [1, 2]) {
    const lines = clientLog(dir, n).split("\n").filter((l) => l.includes("[LFDebug:Config] stream") || l.includes("catalog repriced") || l.includes("using the server's values"));
    console.log(`client ${n} stream lines:`);
    for (const l of lines) console.log(`  ${l.trim()}`);
  }

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "config stream proof: FAIL" : "config stream proof: PASS (owner price on join, on change, for a late joiner, and shipped again without config.json)");
  return fails.length ? 1 : 0;
}

export default {
  name: "config-stream-proof",
  usage: "config-stream-proof [--prepare]",
  run: (ctx) => runConfigStreamProof(ctx.args ?? []),
};
