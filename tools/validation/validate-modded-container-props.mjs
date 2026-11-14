// A `modded class` of a base-game config class repeats the base's container attribute.
// inputs: addons/**/Scripts/**, tools/mcp-call.mjs
//
// Enfusion reads `[BaseContainerProps()]` / `[BaseContainerCustomTitle...]` from the class a config
// entry names. A modded copy that drops the attribute replaces the base class without it, so every
// config entry of that class fails to load with "Unknown class 'X'" (the bandage consumable, the
// emote radial). This check reads each modded class's base from the game scripts through the MCP
// tools (`asset_search`, `game_read`) and flags a modded copy missing an attribute the base has.
// Without the game install and MCP (CI) it prints SKIP and exits 0; CI lists it in LF_VALIDATE_SKIP.
//
// red-proof: 2026-11-14, with `[BaseContainerProps()]` deleted above `modded class SCR_ConsumableBandage`
// in LF_FirstAid.c this printed "FAIL ... SCR_ConsumableBandage lacks [BaseContainerProps()]" and exited 1.
import { execFileSync, spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const CONTAINER = /BaseContainer(Props|Custom)/;

// The attribute lines directly above `<keyword> class <name>`: [ "[BaseContainerProps()]", ... ].
function attrsAbove(lines, idx) {
  const out = [];
  for (let i = idx - 1; i >= 0; i--) {
    const t = lines[i].trim();
    if (t.startsWith("[") && t.endsWith("]")) out.push(t);
    else if (t === "" || t.startsWith("//")) continue;
    else break;
  }
  return out.reverse();
}

// Each `[ A, B(x) ]` line split into its attributes, top level commas only.
export function attrList(lines) {
  const out = [];
  for (const l of lines) {
    const inner = l.slice(1, -1);
    let depth = 0, cur = "", inStr = false;
    for (const ch of inner) {
      if (ch === '"') inStr = !inStr;
      if (!inStr && (ch === "(" || ch === "[")) depth++;
      if (!inStr && (ch === ")" || ch === "]")) depth--;
      if (ch === "," && depth === 0 && !inStr) { out.push(cur.trim()); cur = ""; } else cur += ch;
    }
    if (cur.trim()) out.push(cur.trim());
  }
  return out;
}

const attrName = (a) => /^[\w.]+/.exec(a)?.[0] ?? a;
const isContainerAttr = (a) => CONTAINER.test(attrName(a));

export function classAttrs(src, kind, name) {
  const lines = src.split("\n");
  const re = new RegExp("^\\s*" + kind + "\\s*class\\s+" + name + "\\b");
  const idx = lines.findIndex((l) => re.test(l));
  return idx < 0 ? null : attrList(attrsAbove(lines, idx));
}

// Container attributes the base class has and the modded copy lacks (compared by attribute name).
export function missingContainerAttrs(baseSrc, moddedSrc, name) {
  const base = classAttrs(baseSrc, "", name);
  const mod = classAttrs(moddedSrc, "modded ", name);
  if (!base || !mod) return [];
  const have = new Set(mod.map(attrName));
  return base.filter((a) => isContainerAttr(a) && !have.has(attrName(a)));
}

function main() {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const files = execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard", "addons"], {
    cwd: root, encoding: "utf8", maxBuffer: 1 << 28,
  }).split("\0").filter((f) => /^addons\/[^/]+\/Scripts\/.*\.c$/.test(f) && existsSync(join(root, f)));
  const modded = []; // { file, name }
  for (const f of files) {
    const src = readFileSync(join(root, f), "utf8");
    for (const m of src.matchAll(/^\s*modded\s+class\s+(\w+)/gm)) modded.push({ file: f, name: m[1], src });
  }
  const names = [...new Set(modded.map((m) => m.name))];
  const tmp = join(root, "tmp");
  mkdirSync(tmp, { recursive: true });
  const run = (calls) => {
    const p = join(tmp, "modded-container-props-batch.json");
    writeFileSync(p, JSON.stringify(calls));
    const r = spawnSync(process.execPath, [join(root, "tools", "mcp-call.mjs"), "--batch", "@" + p], {
      cwd: root, encoding: "utf8", maxBuffer: 1 << 28, timeout: 180000,
    });
    return r.status === 0 ? r.stdout : null;
  };
  const sections = (out) => out.split(/^=== \w+ ===$/m).slice(1);

  const found = run(names.map((n) => ({ tool: "asset_search", args: { query: n, type: "script" } })));
  if (!found) {
    console.log("validate-modded-container-props: SKIP (game scripts not reachable through the MCP tools)");
    return;
  }
  const paths = new Map();
  sections(found).forEach((s, i) => {
    const hit = s.split("\n").map((l) => l.trim()).find((l) => l.endsWith(`/${names[i]}.c`));
    if (hit) paths.set(names[i], hit);
  });
  const resolved = [...paths.keys()];
  const read = resolved.length ? run(resolved.map((n) => ({ tool: "game_read", args: { path: paths.get(n) } }))) : "";
  if (read === null) {
    console.log("validate-modded-container-props: SKIP (game_read failed)");
    return;
  }
  const bad = [];
  sections(read).forEach((s, i) => {
    const name = resolved[i];
    for (const m of modded.filter((x) => x.name === name)) {
      for (const a of missingContainerAttrs(s, m.src, name)) {
        bad.push(`FAIL ${m.file}: modded class ${name} lacks [${a}] (the base has it); config entries of ${name} fail with "Unknown class"`);
      }
    }
  });
  if (bad.length) {
    console.log(bad.join("\n"));
    console.log(`validate-modded-container-props: ${bad.length} missing attribute(s). Repeat the base's attribute above the modded class.`);
    process.exit(1);
  }
  console.log(`validate-modded-container-props: OK (${names.length} modded class(es), ${resolved.length} base(s) read)`);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main();
