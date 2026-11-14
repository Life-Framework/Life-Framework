// Every system can be turned on or off from config (#960). The list of systems is
// addons/LifeFramework/Configs/Systems/LF_SystemCatalog.conf; LF_Systems reads it at boot.
//
// Fails when:
//   - a Feature/<Area> folder has no entry in the catalog (a new area must say how it switches);
//   - an id is listed twice, or a parent or requirement names no listed system;
//   - script names a system id the catalog does not list (`SYSTEM = "X"`, `LF_Systems.IsEnabled("X")`,
//     `Refuses("X"`, `SwitchOff("X"`);
//   - a SWITCHABLE system has no gate in script (no `SYSTEM = "<id>"` constant and no literal
//     IsEnabled/Refuses call), so the switch would be refused-free but do nothing.
//
// red-proof: 2026-11-14, removing the "Toys" entry from the catalog printed
// `FAIL Feature/Toys has no entry in Configs/Systems/LF_SystemCatalog.conf` and exited 1.
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const addon = join(root, "addons", "LifeFramework");
const catalogPath = join(addon, "Configs", "Systems", "LF_SystemCatalog.conf");
const featureDir = join(addon, "Scripts", "Game", "Feature");
const scriptsDir = join(addon, "Scripts", "Game");
const bad = [];

if (!existsSync(catalogPath)) {
  console.log("FAIL addons/LifeFramework/Configs/Systems/LF_SystemCatalog.conf is missing");
  process.exit(1);
}

// The catalog: one LF_SystemDef block per system.
const text = readFileSync(catalogPath, "utf8");
const systems = new Map();
for (const block of text.split(/LF_SystemDef\s+"\{[0-9A-F]+\}"\s*\{/).slice(1)) {
  const id = /m_sId\s+"([^"]*)"/.exec(block)?.[1];
  if (!id) { bad.push("FAIL a catalog entry has no m_sId"); continue; }
  const sw = /m_eSwitch\s+(\w+)/.exec(block)?.[1] ?? "NOT_YET";
  const parent = /m_sParent\s+"([^"]*)"/.exec(block)?.[1] ?? "";
  const reqBlock = /m_aRequires\s*\{([^}]*)\}/.exec(block)?.[1] ?? "";
  const requires = [...reqBlock.matchAll(/"([^"]+)"/g)].map((m) => m[1]);
  if (systems.has(id)) bad.push(`FAIL system ${id} is listed twice in the catalog`);
  systems.set(id, { sw, parent, requires });
}

for (const [id, s] of systems) {
  if (!["NOT_YET", "SWITCHABLE", "LOCKED"].includes(s.sw)) bad.push(`FAIL system ${id}: m_eSwitch ${s.sw} is not NOT_YET, SWITCHABLE or LOCKED`);
  if (s.parent && !systems.has(s.parent)) bad.push(`FAIL system ${id}: parent ${s.parent} is not in the catalog`);
  for (const r of s.requires) if (!systems.has(r)) bad.push(`FAIL system ${id}: requires ${r}, which is not in the catalog`);
}

for (const area of readdirSync(featureDir).sort()) {
  if (!statSync(join(featureDir, area)).isDirectory()) continue;
  if (!systems.has(area)) bad.push(`FAIL Feature/${area} has no entry in Configs/Systems/LF_SystemCatalog.conf`);
}

// Script references.
function walk(dir, out = []) {
  for (const e of readdirSync(dir)) {
    const p = join(dir, e);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (p.endsWith(".c")) out.push(p);
  }
  return out;
}
const gated = new Set();
for (const file of walk(scriptsDir)) {
  if (/[\\/]Tests[\\/]/.test(file)) continue;
  const rel = relative(root, file).replace(/\\/g, "/");
  readFileSync(file, "utf8").split("\n").forEach((line, i) => {
    if (/^\s*\/\//.test(line)) return;
    const refs = [
      ...line.matchAll(/static\s+const\s+string\s+SYSTEM\s*=\s*"([^"]*)"/g),
      ...line.matchAll(/LF_Systems\.(?:IsEnabled|Refuses|SwitchOff)\(\s*"([^"]*)"/g),
    ];
    for (const m of refs) {
      if (!systems.has(m[1])) bad.push(`FAIL ${rel}:${i + 1} names system "${m[1]}", which is not in the catalog`);
      else gated.add(m[1]);
    }
  });
}
for (const [id, s] of systems) {
  if (s.sw === "SWITCHABLE" && !gated.has(id)) bad.push(`FAIL system ${id} is SWITCHABLE but no script checks it (add a SYSTEM = "${id}" constant at its entry points)`);
}

for (const b of bad) console.log(b);
if (bad.length) process.exit(1);
console.log(`systems-list: OK (${systems.size} systems, ${[...systems.values()].filter((s) => s.sw === "SWITCHABLE").length} switchable)`);
