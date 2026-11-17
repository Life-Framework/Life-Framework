// tools\cli building proof <slug>: one command proves a placed building works for a player.
//   generic: the prefab resolves to a model and its bounds; its size against the Everon buildings standing
//   near it; `audit placement --strict` on its layer (which boots the dedicated server, so the prefab loads
//   there); then one full-profile session: every door offered, opened, walkable and closed, and the picture
//   set (agent-box/proofs/building-proof.mjs);
//   building: tools/buildings/<slug>/proof.json, the building's own scenario (scenario.mjs), in that session.
// Pictures land in tmp/buildings/<slug>/proof/. --skip-audit, --no-session (offline checks only), --entity E.
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { loadScenario, scenarioProblems } from "./scenario.mjs";
import { buildingFiles } from "./store-slots.mjs";

// Prefab paths of Everon's own buildings, the ones a new building is compared with.
export const EVERON_BUILDING = /^Prefabs\/Structures\/(Houses|Commercial|Industrial|Civic|Cultural|Military|Agriculture)\//;
// A building more than this far off the neighbours' median height is the wrong scale, not a style.
export const SCALE_RANGE = [0.4, 2.5];
const NEAR_M = 150;
const MAX_NEIGHBOURS = 6;

export function placedPrefabs(spec) {
  const f = buildingFiles(spec);
  return [f.store, f.building].filter(Boolean);
}

export function findPlacement(world, spec, entity) {
  const prefabs = placedPrefabs(spec);
  return world.find((e) => (entity ? e.name === entity : prefabs.some((p) => (e.prefab || "").endsWith(p)))) || null;
}

// The nearest Everon buildings, one per prefab, nearest first.
export function neighbours(world, place, near = NEAR_M, max = MAX_NEIGHBOURS) {
  const seen = new Set();
  return world
    .filter((e) => e !== place && EVERON_BUILDING.test(e.prefab || ""))
    .map((e) => ({ ...e, dist: Math.hypot(e.x - place.x, e.z - place.z) }))
    .filter((e) => e.dist <= near)
    .sort((a, b) => a.dist - b.dist)
    .filter((e) => !seen.has(e.prefab) && seen.add(e.prefab))
    .slice(0, max);
}

const size = (b) => [b.max[0] - b.min[0], b.max[2] - b.min[2], b.max[1] - Math.max(b.min[1], 0)];

// Pure: the scale verdict. ours and theirs are [w, d, h] in metres.
export function scaleVerdict(ours, theirs) {
  if (!theirs.length) return { ok: true, note: `no Everon building within ${NEAR_M} m to compare with; compare with the reference sizes in docs/building-pipeline/polish.md` };
  const hs = theirs.map((t) => t[2]).sort((a, b) => a - b);
  const median = hs[Math.floor(hs.length / 2)];
  const ratio = ours[2] / median;
  const ok = ratio >= SCALE_RANGE[0] && ratio <= SCALE_RANGE[1];
  return { ok, ratio, median, note: `height ${ours[2].toFixed(1)} m is ${ratio.toFixed(2)}x the neighbours' median ${median.toFixed(1)} m (allowed ${SCALE_RANGE.join("..")})` };
}

export async function cmdProof(root, slug, flags = []) {
  const flag = (n) => {
    const i = flags.indexOf(n);
    return i >= 0 ? flags[i + 1] : undefined;
  };
  const t0 = Date.now();
  const spec = JSON.parse(readFileSync(join(root, "tools", "buildings", slug, "spec.json"), "utf8"));
  const { loadWorld } = await import("../../../agent-box/layers.mjs");
  const { measureOne } = await import("../../commands/measure.mjs");
  const fails = [];
  const say = (ok, msg) => {
    console.log(`${ok ? "ok   " : "FAIL "} ${msg}`);
    if (!ok) fails.push(msg);
  };

  // The world layers plus the pack's grid world (a pack building's cell, tools\cli grid place --pack): a building there
  // is audited with --grid and proved in the grid mission.
  const { PACK_GRID_LAYERS } = await import("../../lib/world-zones.mjs");
  const packGrid = join(root, ...PACK_GRID_LAYERS.split("/"));
  const world = [...loadWorld(root), ...(existsSync(packGrid) ? loadWorld(root, packGrid).map((e) => ({ ...e, packGrid: true })) : [])];
  const place = findPlacement(world, spec, flag("--entity"));
  if (!place) {
    say(false, `${placedPrefabs(spec).join(" or ")} is placed in no world layer: tools\\cli place ground <x,z> <prefab> --layer <L>.layer --name <Entity>`);
    return 1;
  }
  console.log(`building proof ${slug}: ${place.name} (${place.prefab}) in ${place.layer} at ${place.x} ${place.y} ${place.z}${place.yaw ? ` yaw ${place.yaw}` : ""}`);

  // The prefab loads: its chain reaches a model, and the model has bounds and colliders.
  let ours = null;
  let bounds = null;
  try {
    const m = await measureOne(root, place.prefab);
    bounds = m.bounds;
    ours = size(m.bounds);
    say(m.colliders > 0, `prefab ${m.ref ?? m.path} -> ${m.mesh}: ${ours.map((v) => v.toFixed(1)).join(" x ")} m, ${m.colliders} collider(s)`);
  } catch (e) {
    say(false, `prefab ${place.prefab}: ${e.message}`);
  }

  // Scale against the Everon buildings standing near it.
  if (ours) {
    const near = neighbours(world, place);
    const theirs = [];
    for (const n of near) {
      try {
        const s = size((await measureOne(root, n.prefab)).bounds);
        theirs.push(s);
        console.log(`       near ${n.prefab.split("/").pop()} ${n.dist.toFixed(0)} m: ${s.map((v) => v.toFixed(1)).join(" x ")} m`);
      } catch {}
    }
    const v = scaleVerdict(ours, theirs);
    say(v.ok, `scale: ${v.note}`);
  }

  const scenario = loadScenario(root, slug);
  if (scenario) {
    const p = scenarioProblems(scenario);
    say(!p.length, `proof.json: ${scenario.steps.length} step(s)${p.length ? `: ${p.join("; ")}` : ""}`);
  } else console.log(`       no tools/buildings/${slug}/proof.json: the generic checks only`);

  const cli = join(root, "tools", "cli.mjs");
  const node = (args) => spawnSync(process.execPath, args, { cwd: root, stdio: "inherit", windowsHide: true }).status;
  if (!flags.includes("--skip-audit") && !fails.length) {
    const audit = place.packGrid ? ["--grid", "--strict"] : ["--strict", "--layer", place.layer];
    say(node([cli, "audit", "placement", ...audit]) === 0, `audit placement ${audit.join(" ")}`);
  }

  const out = join(root, "tmp", "buildings", slug, "proof");
  if (!flags.includes("--no-session") && !fails.length) {
    const session = join(root, "agent-box", "session.mjs");
    // A DebugWorld layer boots the debug mission; anything else is the town the playtest mission loads.
    const debugLayer = existsSync(join(root, "addons", "LifeFramework", "Worlds", "DebugWorld", "DebugWorld_Layers", place.layer));
    const mission = flag("--mission") || (place.packGrid ? "grid" : null) || scenario?.session?.mission || (debugLayer ? "debug" : "playtest");
    const up = node([session, "up", "--profile", "full", "--mission", mission, "--wait"]);
    if (up !== 0) say(false, "session up --profile full");
    else {
      try {
        const box = bounds ? [bounds.min[0], bounds.max[0], bounds.min[2], bounds.max[2], bounds.max[1]].map((v) => v.toFixed(2)).join(",") : "";
        const args = [session, "building-proof", "--slug", slug, "--entity", place.name, "--out", out, ...(box ? ["--box", box] : [])];
        say(node(args) === 0, `session: doors, pictures${scenario ? " and the scenario" : ""} (building-proof)`);
      } finally {
        node([session, "down"]);
      }
    }
  }
  console.log(`SUMMARY building-proof ${fails.length ? "fail" : "ok"} ${slug} ${fails.length} failed in ${((Date.now() - t0) / 1000).toFixed(0)} s pictures=${out}`);
  return fails.length ? 1 : 0;
}
