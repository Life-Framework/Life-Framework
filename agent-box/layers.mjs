// DebugWorld interactables. Each feature layer is a .layer file. An entity is
// useful to the harness when it owns a *Action and has a world position, either
// its own `coords` or an ancestor's. The scan is the catalog: no second list
// to keep in sync.

import { readdirSync, readFileSync } from "node:fs";
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
  if (action) return { entity: false, name: action[1], actionName: action[1] };

  const prefab = line.match(/^([A-Za-z_][A-Za-z0-9_]*)\s+([A-Za-z_][A-Za-z0-9_]*)\s*:\s*"/);
  if (prefab) return { entity: true, name: prefab[2], actionName: "" };

  const generic = line.match(/^GenericEntity\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{/);
  if (generic) return { entity: true, name: generic[1], actionName: "" };

  const simple = line.match(/^([A-Za-z_][A-Za-z0-9_]*)\s*\{$/);
  if (
    simple &&
    !STRUCTURAL.has(simple[1]) &&
    !simple[1].endsWith("Component") &&
    !simple[1].endsWith("Info")
  ) {
    return { entity: true, name: simple[1], actionName: "" };
  }

  return { entity: false, name: "", actionName: "" };
}

export function parseLayer(text, layerFile) {
  const pads = [];
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
      if (frame?.entity && frame.actions.length) {
        // Nested `coords` are local to the parent. World position is the sum.
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
        if (any) {
          pads.push({
            name: frame.name,
            layer: layerFile,
            actions: [...new Set(frame.actions)],
            x: Math.round(x * 1000) / 1000,
            y: Math.round(y * 1000) / 1000,
            z: Math.round(z * 1000) / 1000,
          });
        }
      }
    }
  }

  return pads;
}

export function loadPads(root) {
  const dir = join(root, "addons", "LifeFramework", "Worlds", "DebugWorld", "DebugWorld_Layers");
  const pads = [];
  for (const file of readdirSync(dir)) {
    if (!file.endsWith(".layer")) continue;
    const text = readFileSync(join(dir, file), "utf8");
    pads.push(...parseLayer(text, file));
  }
  pads.sort((a, b) => a.name.localeCompare(b.name) || a.layer.localeCompare(b.layer));
  return pads;
}

export function findPad(pads, name) {
  return pads.find((pad) => pad.name === name) ?? null;
}
