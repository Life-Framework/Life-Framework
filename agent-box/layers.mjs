// DebugWorld interactables. Each feature layer is a .layer file. An entity is
// useful to the harness when it owns a *Action and has a world position, either
// its own `coords` or an ancestor's. The scan is the catalog: no second list
// to keep in sync.

import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";

const STRUCTURAL = new Set([
  "components",
  "ActionContexts",
  "additionalActions",
  "ParentContextList",
  "UIInfo",
  "Sources",
  "m_aShopItems",
  "Identity",
]);

function classify(line) {
  const action = line.match(/^([A-Za-z_][A-Za-z0-9_]*Action)\s+"\{/);
  if (action) return { entity: false, name: action[1], actionName: action[1], prefab: "" };

  const namedPrefab = line.match(
    /^(?:\$grp\s+)?([A-Za-z_][A-Za-z0-9_]*)\s+([A-Za-z_][A-Za-z0-9_]*)\s*:\s*"\{[0-9A-Fa-f]{16}\}([^"]+)"/,
  );
  if (namedPrefab && !line.startsWith("$grp")) {
    return { entity: true, name: namedPrefab[2], actionName: "", prefab: namedPrefab[3] };
  }

  const groupPrefab = line.match(/^\$grp\s+[A-Za-z_][A-Za-z0-9_]*\s*:\s*"\{[0-9A-Fa-f]{16}\}([^"]+)"/);
  if (groupPrefab) return { entity: false, name: "", actionName: "", prefab: groupPrefab[1] };

  const generic = line.match(/^GenericEntity\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{/);
  if (generic) return { entity: true, name: generic[1], actionName: "", prefab: "" };

  const simple = line.match(/^([A-Za-z_][A-Za-z0-9_]*)\s*\{$/);
  if (
    simple &&
    !STRUCTURAL.has(simple[1]) &&
    !simple[1].endsWith("Component") &&
    !simple[1].endsWith("Info")
  ) {
    return { entity: true, name: simple[1], actionName: "", prefab: "" };
  }

  return { entity: false, name: "", actionName: "", prefab: "" };
}

function prefabActions(root, prefabPath, seen = new Set()) {
  if (!prefabPath || seen.has(prefabPath)) return [];
  seen.add(prefabPath);
  const rel = prefabPath.replaceAll("\\", "/");
  if (rel.includes("..")) return [];
  const full = join(root, "addons", "LifeFramework", rel);
  if (!existsSync(full)) return [];
  const text = readFileSync(full, "utf8");
  const actions = [];
  for (const raw of text.split(/\r?\n/)) {
    const action = raw.trim().match(/^([A-Za-z_][A-Za-z0-9_]*Action)\s+"\{/);
    if (action) actions.push(action[1]);
  }
  const parent = text.match(/:\s*"\{[0-9A-Fa-f]{16}\}([^"]+\.et)"/);
  if (parent) actions.push(...prefabActions(root, parent[1], seen));
  return [...new Set(actions)];
}

function worldPosition(stack, frame) {
  let x = 0;
  let y = 0;
  let z = 0;
  let any = false;
  for (const item of [...stack, frame]) {
    if (!item.coords) continue;
    any = true;
    x += item.coords[0];
    y += item.coords[1];
    z += item.coords[2];
  }
  if (!any) return null;
  return {
    x: Math.round(x * 1000) / 1000,
    y: Math.round(y * 1000) / 1000,
    z: Math.round(z * 1000) / 1000,
  };
}

function walkLayer(text, layerFile, root) {
  const pads = [];
  const placed = [];
  const stack = [];

  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line) continue;

    const opens = line.endsWith("{");
    const closes = line === "}";

    if (opens) {
      const frame = classify(line);
      frame.actions = [];
      frame.coords = null;
      if (frame.actionName) {
        for (let i = stack.length - 1; i >= 0; i--) {
          if (stack[i].entity) {
            stack[i].actions.push(frame.actionName);
            break;
          }
        }
      }
      stack.push(frame);
    }

    const coords = line.match(/^coords\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)$/);
    if (coords && stack.length) {
      stack[stack.length - 1].coords = [Number(coords[1]), Number(coords[2]), Number(coords[3])];
    }

    if (closes) {
      const frame = stack.pop();
      if (frame?.entity && frame.name && root) {
        let prefab = frame.prefab;
        if (!prefab) {
          for (let i = stack.length - 1; i >= 0; i--) {
            if (stack[i].prefab) {
              prefab = stack[i].prefab;
              break;
            }
          }
        }
        if (prefab) frame.actions.push(...prefabActions(root, prefab));
      }
      if (frame?.entity && frame.name) {
        const world = worldPosition(stack, frame);
        if (world) {
          const record = {
            name: frame.name,
            layer: layerFile,
            actions: [...new Set(frame.actions)],
            x: world.x,
            y: world.y,
            z: world.z,
          };
          placed.push(record);
          if (frame.actions.length) pads.push(record);
        }
      }
    }
  }

  return { pads, placed };
}

export function parseLayer(text, layerFile, root) {
  return walkLayer(text, layerFile, root).pads;
}

export function parsePlaced(text, layerFile, root) {
  return walkLayer(text, layerFile, root).placed;
}

export function loadPads(root) {
  const dir = join(root, "addons", "LifeFramework", "Worlds", "DebugWorld", "DebugWorld_Layers");
  const pads = [];
  for (const file of readdirSync(dir)) {
    if (!file.endsWith(".layer")) continue;
    const text = readFileSync(join(dir, file), "utf8");
    pads.push(...parseLayer(text, file, root));
  }
  pads.sort((a, b) => a.name.localeCompare(b.name) || a.layer.localeCompare(b.layer));
  return pads;
}

export function loadPlaced(root) {
  const dir = join(root, "addons", "LifeFramework", "Worlds", "DebugWorld", "DebugWorld_Layers");
  const placed = [];
  for (const file of readdirSync(dir)) {
    if (!file.endsWith(".layer")) continue;
    const text = readFileSync(join(dir, file), "utf8");
    placed.push(...parsePlaced(text, file, root));
  }
  placed.sort((a, b) => a.x - b.x || a.z - b.z || a.name.localeCompare(b.name));
  return placed;
}

export function findPad(pads, name) {
  return pads.find((pad) => pad.name === name) ?? null;
}
