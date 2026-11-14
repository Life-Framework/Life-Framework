import { mkdirSync } from "node:fs";
import { join, resolve } from "node:path";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, sessionDir } from "../session.mjs";

// One picture per declared asset state (docs/asset-pipeline/states.md), on a running
// `up --clients 2 --profile full` session; `tools\cli asset states <slug> --shots` brings one up,
// runs this and closes it. Args: a comma-separated list of {GUID}path prefabs, then optionally the
// states to shoot (default hand,ground,inventory) and a folder for the PNGs.
//   hand       client 1 gets the item (itemgive), holds it (hold), reports where it sits (heldxf)
//              and shoots its own view; client 2 stands in front of the hand and shoots it
//   ground     client 1 drops the item (itemdrop); client 2's free camera looks down on it (the
//              session's one free capture: a second one crashed a client before)
//   inventory  client 1 opens the preview sheet with every prefab (the engine's inventory render)
// Several prefabs compare variants (each gets its own hand pictures; the last one is dropped).
const STATES = ["hand", "ground", "inventory"];
const leaf = (p) => p.split("/").pop().replace(/\.et$/, "");

async function runAssetStatesProof(ctx) {
  const args = ctx?.args ?? [];
  const prefabs = String(args[0] ?? "").split(",").filter(Boolean);
  const states = args[1] ? String(args[1]).split(",") : STATES;
  const out = args[2] ? resolve(String(args[2])) : "";
  const bad = prefabs.filter((p) => !/^\{[0-9A-F]{16}\}[\w/.-]+\.et$/.test(p));
  if (!prefabs.length || bad.length || states.some((s) => !STATES.includes(s))) {
    console.log(`usage: asset-states-proof <{GUID}path>[,<{GUID}path>...] [${STATES.join(",")}] [out dir]${bad.length ? `; not a prefab: ${bad.join(" ")}` : ""}`);
    return 2;
  }
  if (out) mkdirSync(out, { recursive: true });

  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    // A capture also lands as a PNG in the out folder, named after the shot.
    if (out && spec.verb === "capture" && r.status === "ok") {
      const shot = h.resolveCapture(r.detail);
      if (shot) console.log(`  saved ${bmpFileToPng(shot, join(out, `${spec.name}.png`))}`);
      else console.log(`  no file for ${spec.name}`);
    }
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what}: ${r.status} ${r.detail}`);
    return r;
  };
  const vec = (text) => (String(text).match(/<([-\d.e]+),\s*([-\d.e]+),\s*([-\d.e]+)>/) || []).slice(1).map(Number);

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  // Open ground south of the police desk, facing it (north, +z), as cuffs-held-proof stands.
  on(1, { verb: "stand", entityName: "Roadmap_Police_Desk", side: "s" });
  wait(2500);

  for (const prefab of prefabs) {
    must(on(1, { verb: "itemgive", prefab }, 10000), `give ${leaf(prefab)}`);
    wait(1500);
    if (!states.includes("hand")) continue;
    must(on(1, { verb: "hold", item: leaf(prefab) }, 10000), `hold ${leaf(prefab)}`);
    wait(3500);
    // Where the item sits in the character's frame (the numbers states.md's hand calibration uses).
    on(1, { verb: "heldxf" }, 10000);
    on(1, { verb: "capture", mode: "ui", name: `state_hand_own_${leaf(prefab)}` }, 25000);
    const pos = String(on(1, { verb: "state" }, 10000).detail.match(/pos=(\S+)/)?.[1] ?? "").split(",").map(Number).filter(Number.isFinite);
    if (pos.length === 3) {
      // A held gadget is up in front of the officer, to the left; client 2 stands in front of the
      // hand and to the side, about a metre from it.
      const hand = [pos[0] - 0.2, pos[1] + 1.3, pos[2] + 0.45];
      on(2, { verb: "capture", mode: "world", name: `state_hand_third_${leaf(prefab)}`, x: hand[0], y: hand[1], z: hand[2], ox: 0.55, oy: -1.3, oz: 0.8, hour: 12 }, 25000);
    } else fails.push("no position for client 1; no third-person picture");
  }

  if (states.includes("ground")) {
    // Every item is dropped at the officer's feet; one picture looks down on all of them.
    const spots = [];
    for (const prefab of prefabs) {
      const at = vec(must(on(1, { verb: "itemdrop", prefab }, 10000), `drop ${leaf(prefab)}`).detail);
      if (at.length === 3) spots.push(at);
      wait(1200);
    }
    // The officer steps round to the desk's far side so the boots are not in the picture.
    on(1, { verb: "stand", entityName: "Roadmap_Police_Desk", side: "n" });
    wait(2500);
    if (spots.length) {
      const mid = [0, 1, 2].map((k) => spots.reduce((s, p) => s + p[k], 0) / spots.length);
      const far = spots.length > 1 ? 1.8 : 1;
      on(2, { verb: "capture", mode: "world", name: "state_ground", x: mid[0], y: mid[1], z: mid[2], ox: 0.3 * far, oy: -1.15, oz: 0.25 * far, hour: 12, free: true }, 25000);
    } else fails.push("no drop reported a position; no ground picture");
  }

  if (states.includes("inventory")) {
    must(on(1, { verb: "previews", mode: "show", prefabs }, 10000), "preview sheet");
    wait(2500);
    on(1, { verb: "capture", mode: "ui", name: "state_inventory" }, 25000);
    on(1, { verb: "previews", mode: "hide" }, 10000);
  }

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "asset-states proof: FAIL" : `asset-states proof: done (read every state_* picture${out ? ` in ${out}` : ""})`);
  return fails.length ? 1 : 0;
}

export default { name: "asset-states-proof", usage: "asset-states-proof <{GUID}path>[,...] [hand,ground,inventory] [out dir]", run: (ctx) => runAssetStatesProof(ctx) };
