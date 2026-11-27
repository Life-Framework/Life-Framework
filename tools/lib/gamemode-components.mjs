// Which components a game mode prefab carries, resolved through its parent prefabs (#5154 follow-up).
// validate-gamemode-components.mjs and its selftest share this.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";

// Vanilla components that vanilla scripts fetch from the game mode with NO null check on a path that runs in
// our game. The roleplay game mode is built from scratch (no GameMode_Base parent), so it carries only what its
// prefab lists. Add a row only with the vanilla file:line that dereferences the fetch.
//
// Scanned for #5154's follow-up (2026-11-27, build 26639613): every component of GameMode_Base / Plain / Editor /
// Campaign that ours lacks. Everything else vanilla fetches is null-checked, or runs only in campaign, editor,
// tutorial or group-UI code our game never loads. The rest of that scan is in docs/debug-log-triage.md
// ("Game mode components vanilla reads").
export const REQUIRED = {
  SCR_PossessingManagerComponent:
    "SCR_CharacterHelper.GetCharacterControlType calls SCR_PossessingManagerComponent.GetInstance().IsPossessing with no null check; vanilla death handling runs it for every player killer (#5154)",
};

// Component class names listed directly on one prefab: entries of its top-level `components { }` block.
export function componentsOf(text) {
  const found = [];
  let depth = 0;
  let componentsDepth = -1;
  for (const raw of text.split(/\r?\n/)) {
    // A component row is `Class "{GUID}"`; swap every string for a brace-free marker so only real braces count.
    const line = raw.replace(/"[^"]*"/g, (s) => (/^"\{[0-9A-Fa-f]{16}\}"$/.test(s) ? '"@G@"' : '""'));
    const entry = line.match(/^\s*([A-Za-z_][A-Za-z0-9_]*)\s+"@G@"/);
    if (entry && componentsDepth >= 0 && depth === componentsDepth + 1) found.push(entry[1]);
    if (componentsDepth < 0 && depth === 1 && /^\s*components\s*\{/.test(line)) componentsDepth = depth;
    for (const ch of line) {
      if (ch === "{") depth++;
      else if (ch === "}") {
        depth--;
        if (depth <= componentsDepth) componentsDepth = -1;
      }
    }
  }
  return found;
}

// `Class : "{GUID}path/to/parent.et" {` on the first line, or null for a prefab with no parent.
export function parentPath(text) {
  const m = text.split(/\r?\n/, 1)[0].match(/:\s*"\{[0-9A-Fa-f]{16}\}([^"]+)"/);
  return m ? m[1] : null;
}

export function isGameModeRoot(text) {
  return /^[A-Za-z_]*GameMode[A-Za-z_]*\b/.test(text.split(/\r?\n/, 1)[0]);
}

export function findPrefabs(root) {
  const out = [];
  const addons = join(root, "addons");
  const walk = (dir, rel) => {
    for (const e of readdirSync(dir, { withFileTypes: true })) {
      if (e.isDirectory()) walk(join(dir, e.name), `${rel}/${e.name}`);
      else if (e.name.endsWith(".et")) out.push(`${rel}/${e.name}`);
    }
  };
  for (const a of existsSync(addons) ? readdirSync(addons) : []) {
    const prefabs = join(addons, a, "Prefabs");
    if (existsSync(prefabs)) walk(prefabs, `addons/${a}/Prefabs`);
  }
  return out;
}

// Every component class a prefab carries, parents included. `vanillaParent` is true when the chain ends in a
// prefab that is not in our addons (the base game's own, which brings its own components).
export function resolve(root, rel, seen = new Set()) {
  if (seen.has(rel)) return { classes: new Set(), vanillaParent: false };
  seen.add(rel);
  const text = readFileSync(join(root, rel), "utf8");
  const classes = new Set(componentsOf(text));
  const parent = parentPath(text);
  if (!parent) return { classes, vanillaParent: false };
  const addons = join(root, "addons");
  for (const a of existsSync(addons) ? readdirSync(addons) : []) {
    const candidate = `addons/${a}/${parent}`;
    if (!existsSync(join(root, candidate))) continue;
    const up = resolve(root, candidate, seen);
    for (const c of up.classes) classes.add(c);
    return { classes, vanillaParent: up.vanillaParent };
  }
  return { classes, vanillaParent: true };
}
