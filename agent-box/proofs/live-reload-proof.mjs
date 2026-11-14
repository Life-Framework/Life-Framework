import { existsSync, mkdirSync, unlinkSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, sessionDir, sessionPaths } from "../session.mjs";

// #970: change content on a running session, `reload`, see the new value without a restart.
//   live-reload-proof    on `up --clients 1`: client 1 talks to the town clerk and reads the greeting
//     caption, the dev caption file changes, `reload dialogue` runs, the next talk shows the new line
//     (twice, then the file is deleted and the shipped line returns). Then the owner's config.json
//     changes the price of water and a plain `reload` (config and dialogue) shows the new price on the
//     general store's list. Nothing restarts; `[LFDebug:Dialogue] reload:` and `[LFDebug:Config] reload`
//     in the server log name what changed.
const KEY = "#LF-Dialogue_Clerk_Greet";
const ONE = "RELOAD-ONE Papers please";
const TWO = "RELOAD-TWO Next window";
const SHOP = "Shop_GeneralStore_Debug";
const WATER = 31;

function serverFile(...parts) {
  return join(sessionPaths(sessionDir()).server, "profile", "LifeFramework", ...parts);
}

function writeDev(text) {
  const file = serverFile("dev", "dialogue.json");
  mkdirSync(join(file, ".."), { recursive: true });
  writeFileSync(file, JSON.stringify({ captions: { [KEY]: text } }) + "\n");
  console.log(`dev captions: "${text}" -> ${file}`);
}

function dropFile(file) {
  if (existsSync(file)) unlinkSync(file);
}

async function runLiveReloadProof() {
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
  const expect = (cond, what) => { if (!cond) fails.push(what); };

  // Talks to the clerk, returns the caption the scene showed (`dialogue shown`), then leaves the scene.
  const talk = () => {
    const use = on(1, { verb: "use", className: "LF_QuestAction", entityName: "Quest_Clerk" }, 10000);
    expect(use.status === "ok", `talking to the clerk returned ${use.status} ${use.detail}`);
    wait(600);
    const text = on(1, { verb: "dialogue", arg: "shown" }, 10000).detail;
    wait(4500);
    on(1, { verb: "input", action: "MenuBack" });
    wait(1800);
    return text;
  };
  const caption = () => on(1, { verb: "dialogue", arg: "caption", value: KEY }, 10000).detail;

  arrive(on, 1, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: "Quest_Clerk", side: "w" });
  wait(2500);

  const shipped = talk();
  expect(!shipped.includes("RELOAD-"), "the shipped greeting shows before any override");

  writeDev(ONE);
  const r1 = on(1, { verb: "reload", what: "dialogue" }, 10000);
  expect(r1.status === "ok", `reload dialogue returned ${r1.status} ${r1.detail}`);
  wait(2500);
  const resolved = caption();
  expect(resolved.includes(ONE), `the client resolves the caption to the new text (got: ${resolved})`);
  const first = talk();
  expect(first.includes(ONE), `the greeting on screen is the new line (got: ${first})`);

  writeDev(TWO);
  on(1, { verb: "reload", what: "dialogue" }, 10000);
  wait(2500);
  const second = talk();
  expect(second.includes(TWO) && !second.includes(ONE), `an edit shows on the next reload (got: ${second})`);

  dropFile(serverFile("dev", "dialogue.json"));
  on(1, { verb: "reload", what: "dialogue" }, 10000);
  wait(2500);
  const cleared = caption();
  expect(/overrides=0/.test(cleared), `deleting the file clears the overrides (got: ${cleared})`);
  const back = talk();
  expect(!back.includes("RELOAD-"), `the shipped greeting returns (got: ${back})`);

  // The owner config half: the price of water, read on the shop's list, with the plain `reload`.
  const readWater = () => {
    on(1, { verb: "stand", entityName: SHOP });
    wait(2000);
    on(1, { verb: "use", className: "LF_ShopAction", entityName: SHOP }, 10000);
    wait(1500);
    on(1, { verb: "type", widget: "Search", text: "Water" });
    wait(800);
    const list = on(1, { verb: "ui", mode: "text" }, 10000).detail;
    on(1, { verb: "click", widget: "CloseButton" });
    wait(800);
    return list;
  };
  const owner = serverFile("config.json");
  mkdirSync(join(owner, ".."), { recursive: true });
  writeFileSync(owner, JSON.stringify({ schemaVersion: 1, shops: { priceRules: { "Prefabs/Items/Drinks/": { buyPrice: WATER } } } }) + "\n");
  writeDev(ONE);
  const both = on(1, { verb: "reload" }, 10000);
  expect(both.status === "ok" && /config,dialogue/.test(both.detail), `a plain reload asks for both (got: ${both.detail})`);
  wait(3500);
  const list = readWater();
  expect(list.includes(`$${WATER}`), `the shop list shows water at $${WATER} after the reload`);
  const again = caption();
  expect(again.includes(ONE), `the same reload brought the caption back (got: ${again})`);

  dropFile(owner);
  dropFile(serverFile("dev", "dialogue.json"));
  on(1, { verb: "reload" }, 10000);
  wait(3000);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "live reload proof: FAIL" : "live reload proof: PASS (a dialogue line and a shop price changed in a running session, no restart)");
  return fails.length ? 1 : 0;
}

export default {
  name: "live-reload-proof",
  usage: "live-reload-proof   (up --clients 1)",
  run: () => runLiveReloadProof(),
};
