// tools/lib/rpl-actions.mjs - find prefabs whose user actions cannot reach the server.
//
// A user action on a world entity is performed through that entity's replication: the engine sends the
// request to the server over the entity's RplComponent. With no RplComponent on the root entity the request
// is dropped ("Attempting to send an RPC through unregistred item", docs/architecture/rpc-routing.md), so a
// client's action silently does nothing while a host's works. A prefab whose root has an
// ActionsManagerComponent with actions and no RplComponent, anywhere on its addon ancestry, is broken
// unless every action class is local-effect-only on purpose (`override bool HasLocalEffectOnlyScript()` in
// its script: the action runs on the acting machine and asks the server some other way, as the plant
// stage meshes do through the player controller, #3177).
//
// Ancestry is followed through addon prefabs only. A parent outside the addons is a base-game prefab, which
// CI cannot read: it is assumed to carry what the child inherits, so the check never guesses about vanilla.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";

const HEADER = /^\s*\w+\s*(?::\s*"\{([0-9A-Fa-f]{16})\}[^"]*")?\s*\{\s*$/;
const COMPONENT = /^\s*(\w+)\s+"\{[0-9A-Fa-f]{16}\}"\s*\{\s*$/;
const ACTION = /^\s*(\w+)\s+"\{[0-9A-Fa-f]{16}\}"\s*\{\s*$/;

function walk(dir, ext, out) {
  if (!existsSync(dir)) return;
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory()) walk(p, ext, out);
    else if (e.name.endsWith(ext)) out.push(p);
  }
}

/** What one prefab text says about its own root entity: parent GUID, root component classes, action classes. */
export function parsePrefab(text) {
  const lines = text.replace(/\r/g, "").split("\n");
  let parent = null;
  let header = false;
  const components = new Set();
  const actions = [];
  let depth = 0;
  let inActions = false;
  let actionsDepth = -1;
  for (const line of lines) {
    if (!header) {
      const h = HEADER.exec(line);
      if (h) {
        header = true;
        parent = h[1] ? h[1].toUpperCase() : null;
        depth = 1;
      }
      continue;
    }
    // Root components sit at depth 2 (root { components { <here> } }); a child entity is deeper.
    if (depth === 2) {
      const c = COMPONENT.exec(line);
      if (c) components.add(c[1]);
    }
    if (inActions && depth === actionsDepth + 1) {
      const a = ACTION.exec(line);
      if (a) actions.push(a[1]);
    }
    if (/^\s*additionalActions\s*\{\s*$/.test(line) && depth === 3) {
      inActions = true;
      actionsDepth = depth;
    }
    for (const ch of line) {
      if (ch === "{") depth++;
      else if (ch === "}") depth--;
    }
    if (inActions && depth <= actionsDepth) inActions = false;
  }
  return { parent, components, actions };
}

/** Action classes whose script overrides HasLocalEffectOnlyScript, from every addon .c file. */
export function localOnlyActionClasses(root) {
  const files = [];
  const addons = join(root, "addons");
  if (existsSync(addons)) for (const a of readdirSync(addons)) walk(join(addons, a, "Scripts"), ".c", files);
  const out = new Set();
  for (const f of files) {
    const text = readFileSync(f, "utf8");
    if (!text.includes("HasLocalEffectOnlyScript")) continue;
    let cls = null;
    for (const line of text.split("\n")) {
      const c = /^\s*(?:modded\s+)?class\s+(\w+)/.exec(line);
      if (c) cls = c[1];
      if (cls && /override\s+bool\s+HasLocalEffectOnlyScript/.test(line)) out.add(cls);
    }
  }
  return out;
}

/** Every prefab under addons/: [{ path, guid, parent, components, actions }]. GUIDs come from the .meta files. */
export function readPrefabs(root) {
  const files = [];
  const addons = join(root, "addons");
  if (existsSync(addons)) for (const a of readdirSync(addons)) walk(join(addons, a, "Prefabs"), ".et", files);
  const prefabs = [];
  for (const f of files) {
    let guid = null;
    const meta = f + ".meta";
    if (existsSync(meta)) {
      const g = /Name\s+"\{([0-9A-Fa-f]{16})\}/.exec(readFileSync(meta, "utf8"));
      if (g) guid = g[1].toUpperCase();
    }
    prefabs.push({ path: f.slice(root.length + 1).replace(/\\/g, "/"), guid, ...parsePrefab(readFileSync(f, "utf8")) });
  }
  return prefabs;
}

/**
 * Prefabs with actions and no RplComponent on their addon ancestry. `localOnly` is the set of action classes
 * that run on the acting machine only. \return [{ path, actions }] (actions: the classes that need the entity).
 */
export function findUnreachableActions(prefabs, localOnly) {
  const byGuid = new Map();
  for (const p of prefabs) if (p.guid) byGuid.set(p.guid, p);

  const chainState = (p, seen = new Set()) => {
    // { rpl, actions, vanilla }: does an RplComponent exist on the chain, the action classes, is the chain open at a vanilla base
    if (seen.has(p)) return { rpl: false, actions: [], vanilla: false };
    seen.add(p);
    let rpl = p.components.has("RplComponent");
    let actions = p.components.has("ActionsManagerComponent") ? [...p.actions] : [];
    let vanilla = false;
    if (p.parent) {
      const base = byGuid.get(p.parent);
      if (base) {
        const s = chainState(base, seen);
        rpl = rpl || s.rpl;
        actions = actions.concat(s.actions);
        vanilla = s.vanilla;
      } else {
        vanilla = true;
      }
    }
    return { rpl, actions, vanilla };
  };

  const bad = [];
  for (const p of prefabs) {
    const s = chainState(p);
    if (s.rpl || s.vanilla) continue;
    const needing = s.actions.filter((a) => !localOnly.has(a));
    if (needing.length) bad.push({ path: p.path, actions: [...new Set(needing)] });
  }
  return bad;
}
