// tools/validation/validate-town-hand-tools.mjs
// inputs: addons/**
//
// No hand tool lies loose in a world players walk through. A hand tool (a prefab with
// LF_HandToolComponent, or one inheriting such a prefab: the vault drill, the courier parcel) is a
// working item. Placed as dressing it is a free tool for whoever finds it, and its pick-up prompt
// covers whatever it stands on. #4156: the Workshop photo drill (#370) stood on the playtest rob kit
// chest, so the F key on the chest offered the drill's "Pick up" (refused as too heavy once the kit
// drill was carried) instead of the kit.
//
// Fails when a town layer (the DebugWorld village, the pack's Everon1989Town) or an island layer
// (the pack's MainWorld) declares an entity whose prefab is a hand tool. The debug grid and the
// annex may hold one as a fixture. A picture that needs a drill in a hand uses a film actor.
// red-proof: tools/validation/town-hand-tools-selftest.mjs plants a town drill and a grid drill and
// asserts only the town one is reported; on the tree before #4156's fix this check named
// Village_PhotoScenes/PhotoScene_Bank_Drill.

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { DEBUG_LAYERS, PACK_TOWN_LAYERS, zoneOfLayer } from "../lib/world-zones.mjs";

export const ISLAND_LAYERS = "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers";

function walk(dir, out = []) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (name.endsWith(".et")) out.push(p);
  }
  return out;
}

// GUIDs of every hand-tool prefab: those carrying the component, then those inheriting one.
// `prefabs` is [{ guid, parent, text }].
export function handToolGuids(prefabs) {
  const tools = new Set(prefabs.filter((p) => p.guid && /\bLF_HandToolComponent\b/.test(p.text)).map((p) => p.guid));
  let grew = true;
  while (grew) {
    grew = false;
    for (const p of prefabs) {
      if (p.guid && !tools.has(p.guid) && p.parent && tools.has(p.parent)) {
        tools.add(p.guid);
        grew = true;
      }
    }
  }
  return tools;
}

// Every .et under each addon's Prefabs folder, with its GUID (from the .meta) and parent GUID.
export function readPrefabs(root) {
  const out = [];
  const addons = join(root, "addons");
  if (!existsSync(addons)) return out;
  for (const addon of readdirSync(addons)) {
    for (const file of walk(join(addons, addon, "Prefabs"))) {
      const meta = `${file}.meta`;
      const guid = existsSync(meta) ? (/\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8")) ?? [])[1] : undefined;
      const text = readFileSync(file, "utf8");
      const parent = (/^[A-Za-z_][A-Za-z0-9_]* : "\{([0-9A-F]{16})\}/.exec(text) ?? [])[1];
      out.push({ guid, parent, text });
    }
  }
  return out;
}

// Top-level layer entities whose prefab GUID is in `tools`: [{ name, prefab }].
export function looseTools(text, tools) {
  const out = [];
  for (const m of String(text).matchAll(/^[A-Za-z_][A-Za-z0-9_]* ([A-Za-z_][A-Za-z0-9_]*) : "\{([0-9A-F]{16})\}([^"]+)"/gm)) {
    if (tools.has(m[2])) out.push({ name: m[1], prefab: m[3] });
  }
  return out;
}

export function scan(root, tools) {
  const found = [];
  for (const [rel, world] of [[DEBUG_LAYERS, "debug"], [PACK_TOWN_LAYERS, "pack"], [ISLAND_LAYERS, "pack"]]) {
    const dir = join(root, rel);
    if (!existsSync(dir)) continue;
    for (const f of readdirSync(dir).filter((n) => n.endsWith(".layer")).sort()) {
      if (zoneOfLayer(f, world) !== "town") continue;
      for (const hit of looseTools(readFileSync(join(dir, f), "utf8"), tools)) found.push({ layer: `${rel}/${f}`, ...hit });
    }
  }
  return found;
}

const isMain = process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1];
if (isMain) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const tools = handToolGuids(readPrefabs(root));
  const found = scan(root, tools);
  for (const f of found) console.log(`FAIL ${f.layer}: ${f.name} places the hand tool ${f.prefab} loose in a world players walk through; put it in a grid cell or remove it`);
  console.log(found.length ? `validate-town-hand-tools: ${found.length} loose hand tool(s)` : `validate-town-hand-tools: OK (${tools.size} hand-tool prefab(s), none loose in a town or island layer)`);
  process.exit(found.length ? 1 : 0);
}
