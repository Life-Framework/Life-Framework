import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { bmpFileToPng, decodeBmp } from "../bmp.mjs";
import { ROB_TARGET, ROOT, arrive, clientBox, sessionDir } from "../session.mjs";

// #1005 integration: one pack, four overlays, booted together and read on a client.
// The slices (UI theme #1234, prices and items #1238, prefab slot swap #1239, terms #1241) were each proven
// through the owner config.json or a test stub; this proves them inside a booted pack. The pack is
// `tools\cli pack new Retheme` plus tools/test/pack-fixtures/retheme (tools/pack/retheme-fixture.mjs).
//
//   node agent-box/proofs/retheme-proof.mjs --prepare   (or `session.mjs retheme-proof --prepare`) before `up`:
//       scaffolds addons/LF_Retheme when missing, applies the fixture, runs `pack check`
//   tools\cli session up --pack LF_Retheme --clients 1 --profile full
//   node agent-box/session.mjs retheme-proof [out dir]        (runs where the session runs)
//   node agent-box/session.mjs down
//
// One PASS or FAIL line per overlay: theme, prices, items, slot, terms. Each reads what client 1 sees,
// then the server log says the overlay loaded. The picture of the themed price list goes to <out dir>
// (default agent-box/reports/retheme, which `session pull` brings home).
const SHOP = "Shop_GeneralStore_Debug";
const GUNS = "Annex_GunCounter";
const DESK = "Roadmap_Police_Desk";
const RECEIPT = "{3E8A5C1D7B2F4096}Prefabs/Items/Roleplay/LF_Receipt.et";
const PAPER = [0xe8, 0xf1, 0xf7]; // the fixture's paper colour, #E8F1F7 (the flagship paper is warm)

async function runPrepare() {
  const addons = join(ROOT, "addons");
  const { applyRetheme } = await import("../../tools/pack/retheme-fixture.mjs");
  const cli = join(ROOT, "tools", "cli.mjs");
  if (!existsSync(join(addons, "LF_Retheme", "Missions", "LF_Retheme.conf"))) {
    const r = spawnSync(process.execPath, [cli, "pack", "new", "Retheme"], { cwd: ROOT, encoding: "utf8", windowsHide: true });
    if (r.status !== 0) {
      console.log(`FAIL  pack new Retheme: ${(r.stdout + r.stderr).trim().slice(-400)}`);
      return 1;
    }
  }
  applyRetheme(addons);
  // The pack's strings reach the engine through its runtime tables (Language/*.conf), compiled from the .st.
  const regen = spawnSync(process.execPath, [cli, "regen-localization"], { cwd: ROOT, encoding: "utf8", windowsHide: true });
  if (regen.status !== 0) {
    console.log(`FAIL  regen-localization: ${(regen.stdout + regen.stderr).trim().slice(-400)}`);
    return 1;
  }
  const check = spawnSync(process.execPath, [cli, "pack", "check", "LF_Retheme"], { cwd: ROOT, encoding: "utf8", windowsHide: true });
  const out = (check.stdout + check.stderr).trim();
  console.log(out);
  if (check.status !== 0) {
    console.log("FAIL  pack check LF_Retheme");
    return 1;
  }
  console.log("retheme-proof --prepare: addons/LF_Retheme is ready; `tools\\cli session up --pack LF_Retheme --clients 1 --profile full`");
  return 0;
}

// This boot's log only: the session folder keeps the logs of earlier boots. `who` is "server" or "client1".
function bootLog(dir, who) {
  const logs = join(dir, who, "logs");
  if (!existsSync(logs)) return "";
  const runs = readdirSync(logs).filter((d) => d.startsWith("logs_")).sort();
  const newest = runs.length ? join(logs, runs[runs.length - 1], "console.log") : "";
  return newest && existsSync(newest) ? readFileSync(newest, "utf8") : "";
}

// Fraction of pixels within `tol` (per channel) of a colour.
export function colourShare(rgb, pixels, tol) {
  let hit = 0;
  const n = pixels.length / 3;
  for (let i = 0; i < pixels.length; i += 3) {
    if (Math.abs(pixels[i] - rgb[0]) <= tol && Math.abs(pixels[i + 1] - rgb[1]) <= tol && Math.abs(pixels[i + 2] - rgb[2]) <= tol) hit++;
  }
  return hit / n;
}

async function runRetheme(args) {
  if (args.includes("--prepare")) return runPrepare();

  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const out = resolve(String(args.find((a) => !a.startsWith("--")) ?? join(ROOT, "agent-box", "reports", "retheme")));
  mkdirSync(out, { recursive: true });
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const verdict = {}; // overlay -> [failures]
  const judge = (overlay, fails) => {
    verdict[overlay] = [...(verdict[overlay] ?? []), ...fails];
  };
  const noText = (r) => (r.status === "ok" ? "" : `(${r.status} ${r.detail})`);

  arrive(on, 1, wait);
  wait(8000);

  // Prices and the themed sheet: the general store's price list, narrowed to water.
  on(1, { verb: "stand", entityName: SHOP });
  wait(2000);
  const open = on(1, { verb: "use", className: "LF_ShopAction", entityName: SHOP }, 10000);
  wait(1500);
  on(1, { verb: "type", widget: "Search", text: "Water" });
  wait(800);
  const priceList = on(1, { verb: "ui", mode: "text" }, 10000);
  console.log(`price list: ${priceList.detail}`);
  if (open.status !== "ok") judge("prices", [`the shop did not open ${noText(open)}`]);
  if (!String(priceList.detail).includes("$31")) judge("prices", ["the price list does not show $31 for water (the core price is $6)"]);

  // The picture, with the list open: the sheet's face is the pack's paper colour.
  on(1, { verb: "camera", args: "hud off" });
  const shot = on(1, { verb: "capture", mode: "ui", name: "retheme_pricelist" }, 25000);
  on(1, { verb: "camera", args: "hud on" });
  const bmp = shot.status === "ok" ? h.resolveCapture(shot.detail) : null;
  if (!bmp) judge("theme", [`no picture of the price list ${noText(shot)}`]);
  else {
    const png = bmpFileToPng(bmp, join(out, "retheme_pricelist.png"));
    console.log(`saved ${png}`);
    const { pixels } = decodeBmp(readFileSync(bmp));
    const share = colourShare(PAPER, pixels, 10);
    console.log(`theme: ${(share * 100).toFixed(1)}% of the picture is the pack's paper colour #E8F1F7 (+-10)`);
    if (share < 0.02) judge("theme", [`only ${(share * 100).toFixed(1)}% of the picture is the pack's paper colour #E8F1F7`]);
  }
  on(1, { verb: "click", widget: "CloseButton" });
  wait(800);

  // Items: the gun counter no longer sells the M9 (shipped $500).
  on(1, { verb: "stand", entityName: GUNS });
  wait(2000);
  const guns = on(1, { verb: "use", className: "LF_ShopAction", entityName: GUNS }, 10000);
  wait(1500);
  const gunList = on(1, { verb: "ui", mode: "text" }, 10000);
  console.log(`gun counter: ${gunList.detail}`);
  if (guns.status !== "ok") judge("items", [`the gun counter did not open ${noText(guns)}`]);
  const gunText = String(gunList.detail);
  if (gunText.includes("$500") || /M9/i.test(gunText)) judge("items", ["the gun counter still lists the M9 (shipped $500)"]);
  on(1, { verb: "click", widget: "CloseButton" });
  wait(800);

  // Terms: the police application's title reads the pack's word.
  on(1, { verb: "stand", entityName: DESK });
  wait(2500);
  const apply = on(1, { verb: "use", className: "LF_PoliceApplyAction", entityName: DESK }, 10000);
  wait(1500);
  const form = on(1, { verb: "ui", mode: "text" }, 10000);
  console.log(`police application: ${form.detail}`);
  if (apply.status !== "ok") judge("terms", [`the police application did not open ${noText(apply)}`]);
  if (!String(form.detail).includes("Application to the Sheriff")) judge("terms", ["the police application does not read 'Application to the Sheriff'"]);
  if (/Application to the police/i.test(String(form.detail))) judge("terms", ["the police application still reads 'police'"]);
  on(1, { verb: "click", widget: "PolAppClose" });
  wait(800);

  // Slot swap: rob the till (opens an evidence scene), drop an item beside it; the marker the server spawns
  // for it is the pack's prefab, the shell casing, and this client's copy of it says so.
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  const rob = on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000);
  wait(4000);
  if (rob.status !== "ok") judge("slot", [`the till robbery did not start ${noText(rob)}`]);
  const give = on(1, { verb: "itemgive", prefab: RECEIPT }, 10000);
  wait(900);
  const drop = on(1, { verb: "itemdrop", prefab: RECEIPT }, 10000);
  if (give.status !== "ok" || drop.status !== "ok") judge("slot", [`could not drop an item ${noText(give)} ${noText(drop)}`]);
  let evidence = "";
  const t0 = Date.now();
  do {
    wait(1500);
    evidence = /evidence=(\S+)/.exec(String(on(1, { verb: "seen", radius: 20 }, 10000).detail))?.[1] ?? "";
  } while (!evidence.includes("LF_EvidenceCasing") && Date.now() - t0 < 20000);
  console.log(`evidence this client sees: ${evidence}`);
  if (!evidence.includes("LF_EvidenceCasing")) judge("slot", [`no LF_EvidenceCasing near the dropped item (evidence=${evidence})`]);
  if (evidence.includes("LF_EvidenceMarker")) judge("slot", ["the core's LF_EvidenceMarker still spawns"]);

  // The logs: each overlay says it loaded. The server loads the economy, slot and terms overlays; the
  // UI theme is read on the client only (a dedicated server has no UI), so its line is in the client log.
  const serverText = bootLog(dir, "server");
  const clientText = bootLog(dir, "client1");
  const logLines = [];
  const logged = (overlay, text, who, re, what) => {
    const m = text.split("\n").find((l) => re.test(l));
    if (m) logLines.push(`${who}: ${m.trim()}`);
    else judge(overlay, [`the ${who} log has no line for ${what}`]);
  };
  logged("theme", clientText, "client", /\[LFDebug:UITheme\] pack overlay .*UITheme\.conf changed [1-9]\d* themes/, "the UI theme overlay");
  logged("prices", serverText, "server", /\[LFDebug:Config\] shops: pack overlay .*ShopPrices\.conf set [1-9]\d* fields/, "the shop price overlay");
  logged("items", serverText, "server", /\[LFDebug:Config\] catalogue: pack overlay .*Catalogue\.conf changed [1-9]\d* entries/, "the catalogue overlay");
  logged("slot", serverText, "server", /\[LFDebug:PrefabSlots\] pack overlay .*PrefabSlots\.conf changed [1-9]\d* slots/, "the prefab slot overlay");
  logged("terms", serverText, "server", /\[LFDebug:Config\] terms: pack overlay .*Terms\.conf changed [1-9]\d* terms/, "the terms overlay");
  // A pack string the engine cannot find shows as a raw key; the terms overlay would then have no words.
  for (const l of clientText.split("\n").filter((x) => /Missing string ID = LF_Retheme|\[LFDebug:Terms\] term police has no words/.test(x)).slice(0, 3)) judge("terms", [`client log: ${l.trim()}`]);
  for (const l of [...serverText.split("\n"), ...clientText.split("\n")].filter((x) => /LF_Retheme/.test(x) && /refused|does not resolve|rejected/.test(x))) console.log(`WARN  ${l.trim()}`);
  writeFileSync(join(out, "overlay-log-lines.txt"), `${logLines.join("\n")}\n`);
  for (const l of logLines) console.log(`log: ${l}`);

  let bad = 0;
  for (const overlay of ["theme", "prices", "items", "slot", "terms"]) {
    const fails = verdict[overlay] ?? [];
    if (fails.length) bad++;
    console.log(`${fails.length ? "FAIL" : "PASS"}  ${overlay}${fails.length ? `: ${fails.join("; ")}` : ""}`);
  }
  console.log(bad ? `retheme proof: FAIL (${bad} of 5 overlays)` : "retheme proof: PASS (5 of 5 overlays read on the client and loaded on the server)");
  return bad ? 1 : 0;
}

export default {
  name: "retheme-proof",
  usage: "retheme-proof [out dir]   (up --pack LF_Retheme --clients 1 --profile full); retheme-proof --prepare before up",
  run: (ctx) => runRetheme(ctx.args ?? []),
};
