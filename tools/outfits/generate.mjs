// tools\cli outfits generate: NPC roles from tools/assets/outfits/roles.json.
//
// Writes, all idempotent and with stable GUIDs (a hash of the path), so a rerun changes nothing:
//   Prefabs/Characters/Npcs/Outfits/LF_Npc_<Role>_<n>[_Civ].et (+ .meta)  one prefab per outfit
//   Scripts/Game/Feature/Npc/LF_RolesTable.generated.c                     the role table the game reads
//   Language/everonlife_localization.st                                   talk strings (then regen-localization)
//   DebugWorld_Layers/*.layer                                              each placed NPC's prefab reference
//
// An outfit is a vanilla Npc_Default_01 (or the NPC's own base: bank teller, fence, clerk, civilian)
// with its loadout slots and its role id overridden. Which outfit a placed NPC wears is
// hash(entity name) % outfit count: stable for that NPC, different between NPCs of one role.
// LF_Roles.PickOutfit in script uses the same hash for an NPC spawned by code.
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { upsertStrings } from "../assets/pipeline/dialogue.mjs";

export const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const ADDON = join(ROOT, "addons/LifeFramework");
const SPEC = join(ROOT, "tools/assets/outfits/roles.json");
const OUT_DIR = "Prefabs/Characters/Npcs/Outfits";
const TABLE = join(ADDON, "Scripts/Game/Feature/Npc/LF_RolesTable.generated.c");
const ST = join(ADDON, "Language/everonlife_localization.st");
const LAYERS = join(ADDON, "Worlds/DebugWorld/DebugWorld_Layers");
// The role component's id in Npc_Base.et; a variant overrides the component by this GUID.
export const ROLE_COMPONENT_GUID = "7A31C05E94D2B861";
const LOADOUT_COMPONENT_GUID = "5A4B0B84C86D5843";
const SLOT_ORDER = ["Jacket", "Pants", "Boots", "Hat"];

// The seed hash LF_Roles.PickOutfit uses too (kept small so it never overflows a script int).
const SEED_ALPHABET = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
export function seedHash(text) {
  let h = 0;
  for (const c of text) h = (h * 31 + SEED_ALPHABET.indexOf(c) + 1) % 1000003;
  return h;
}

export const pascal = (id) => id.split("_").map((p) => p[0].toUpperCase() + p.slice(1)).join("");

function guidOf(path) {
  return createHash("sha1").update("lf-npc-outfit:" + path).digest("hex").slice(0, 16).toUpperCase();
}

export function loadSpec() {
  return JSON.parse(readFileSync(SPEC, "utf8"));
}

// Everything wrong with the spec, one line each.
export function problems(spec) {
  const out = [];
  const ids = new Set();
  for (const r of spec.roles) {
    if (ids.has(r.id)) out.push(`role ${r.id} is listed twice`);
    ids.add(r.id);
    if (!spec.bases[r.base]) out.push(`role ${r.id}: unknown base ${r.base}`);
    for (const b of r.also || []) if (!spec.bases[b]) out.push(`role ${r.id}: unknown extra base ${b}`);
    if (!r.outfits?.length) out.push(`role ${r.id} has no outfit`);
    for (const o of r.outfits || []) {
      const slots = new Set();
      for (const piece of o) {
        const c = spec.clothes[piece];
        if (!c) { out.push(`role ${r.id}: unknown clothing ${piece}`); continue; }
        for (const s of Object.keys(c)) {
          if (slots.has(s)) out.push(`role ${r.id}: outfit [${o}] fills ${s} twice`);
          slots.add(s);
        }
      }
      for (const need of ["Jacket", "Pants", "Boots"]) if (!slots.has(need)) out.push(`role ${r.id}: outfit [${o}] has no ${need}`);
    }
    if (!r.own_dialogue) for (const k of ["name", "greet", "what", "where", "rumour", "bye"]) if (!r[k]) out.push(`role ${r.id} has no ${k} text`);
  }
  for (const [npc, role] of Object.entries(spec.assign)) if (!ids.has(role)) out.push(`${npc} is assigned to unknown role ${role}`);
  return out;
}

// Variants of a role: [{ file, base, outfit }]; the primary base first.
export function variants(spec, role) {
  const out = [];
  const bases = [role.base, ...(role.also || [])];
  role.outfits.forEach((outfit, i) => {
    for (const base of bases) {
      const suffix = base === role.base ? "" : "_" + pascal(base);
      out.push({ role: role.id, index: i, base, outfit, name: `LF_Npc_${pascal(role.id)}_${i + 1}${suffix}`, primary: base === role.base });
    }
  });
  return out;
}

export function resourcePath(v) {
  return `${OUT_DIR}/${v.name}.et`;
}

export function resourceName(v) {
  return `{${guidOf(resourcePath(v))}}${resourcePath(v)}`;
}

export function prefabText(spec, v) {
  const slots = {};
  for (const piece of v.outfit) Object.assign(slots, spec.clothes[piece]);
  const lines = [];
  lines.push(`LF_NpcBaseEntity : "${spec.bases[v.base]}" {`);
  lines.push(` ID "${guidOf(resourcePath(v) + "#id")}"`);
  lines.push(" components {");
  lines.push(`  LF_NpcRoleComponent "{${ROLE_COMPONENT_GUID}}" {`);
  lines.push(`   m_sRole "${v.role}"`);
  lines.push("  }");
  lines.push(`  BaseLoadoutManagerComponent "{${LOADOUT_COMPONENT_GUID}}" {`);
  lines.push("   Slots {");
  for (const slot of SLOT_ORDER) {
    if (!slots[slot]) continue;
    lines.push(`    LoadoutSlotInfo ${slot} {`);
    if (slot === "Hat") lines.push("     Enabled 1");
    lines.push(`     Prefab "${slots[slot]}"`);
    lines.push("    }");
  }
  lines.push("   }");
  lines.push("  }");
  lines.push(" }");
  lines.push("}");
  return lines.join("\n") + "\n";
}

export function metaText(path) {
  return `MetaFileClass {
 Name "{${guidOf(path)}}${path}"
 Configurations {
  EntityTemplateResourceClass PC {
  }
  EntityTemplateResourceClass XBOX_ONE : PC {
  }
  EntityTemplateResourceClass XBOX_SERIES : PC {
  }
  EntityTemplateResourceClass PS4 : PC {
  }
  EntityTemplateResourceClass HEADLESS : PC {
  }
 }
}`;
}

export function tableText(spec) {
  const out = [];
  out.push("// GENERATED by `tools\\cli outfits generate` from tools/assets/outfits/roles.json. Do not edit.");
  out.push("// One row per NPC role: its outfits (prefabs to place) and whether it talks through its own dialogue.");
  out.push("");
  out.push("class LF_RolesTable");
  out.push("{");
  out.push("\t//------------------------------------------------------------------------------------------------");
  out.push("\tstatic void Fill(notnull array<ref LF_RoleDef> roles)");
  out.push("\t{");
  out.push("\t\tLF_RoleDef role;");
  for (const r of spec.roles) {
    out.push(`\t\trole = new LF_RoleDef("${r.id}", ${r.own_dialogue ? "true" : "false"});`);
    for (const v of variants(spec, r)) {
      out.push(`\t\trole.AddOutfit("${resourceName(v)}", ${v.primary ? "true" : "false"});`);
    }
    out.push("\t\troles.Insert(role);");
  }
  out.push("\t}");
  out.push("}");
  return out.join("\n") + "\n";
}

export function stringEntries(spec) {
  const e = [];
  const c = spec.common;
  e.push({ id: "LF-NpcTalk_Action", text: c.action, comment: "F-key action on an NPC that talks (tools/assets/outfits/roles.json)." });
  e.push({ id: "LF-NpcTalk_AskWhat", text: c.ask_what, comment: "NPC role talk: reply asking what the NPC does." });
  e.push({ id: "LF-NpcTalk_AskWhere", text: c.ask_where, comment: "NPC role talk: reply asking for directions." });
  e.push({ id: "LF-NpcTalk_AskRumour", text: c.ask_rumour, comment: "NPC role talk: reply asking for a rumour." });
  e.push({ id: "LF-NpcTalk_More", text: c.more, comment: "NPC role talk: reply to ask something else." });
  e.push({ id: "LF-NpcTalk_Bye", text: c.bye, comment: "NPC role talk: goodbye reply." });
  for (const r of spec.roles) {
    if (r.own_dialogue) continue;
    const comment = `NPC role talk (${r.id}), from tools/assets/outfits/roles.json.`;
    e.push({ id: `LF-NpcRole_${r.id}_Name`, text: r.name, comment });
    e.push({ id: `LF-NpcRole_${r.id}_Greet`, text: r.greet, comment });
    e.push({ id: `LF-NpcRole_${r.id}_What`, text: r.what, comment });
    e.push({ id: `LF-NpcRole_${r.id}_Where`, text: r.where, comment });
    e.push({ id: `LF-NpcRole_${r.id}_Rumour`, text: r.rumour, comment });
    e.push({ id: `LF-NpcRole_${r.id}_Bye`, text: r.bye, comment });
  }
  return e;
}

function writeIfChanged(path, text) {
  if (existsSync(path) && readFileSync(path, "utf8") === text) return false;
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, text);
  return true;
}

// Point each assigned NPC at its outfit prefab.
export function assignLayers(spec, dryRun = false) {
  const byGuid = new Map();
  for (const r of spec.roles) for (const v of variants(spec, r)) byGuid.set(guidOf(resourcePath(v)), v);
  const baseOf = new Map(Object.entries(spec.bases).map(([k, ref]) => [/\{([0-9A-F]{16})\}/.exec(ref)[1], k]));
  const roles = new Map(spec.roles.map((r) => [r.id, r]));
  const seen = new Set();
  const changed = [];
  for (const file of readdirSync(LAYERS).filter((f) => f.endsWith(".layer"))) {
    const path = join(LAYERS, file);
    const text = readFileSync(path, "utf8");
    const next = text.replace(/^(\s*LF_NpcBaseEntity (\S+) : )"\{([0-9A-F]{16})\}[^"]+"/gm, (whole, head, name, guid) => {
      const roleId = spec.assign[name];
      if (!roleId) return whole;
      const role = roles.get(roleId);
      const current = byGuid.get(guid)?.base ?? baseOf.get(guid);
      if (!current) throw new Error(`${file}: ${name} has a prefab this tool does not know (${guid})`);
      const all = variants(spec, role);
      const mine = all.filter((v) => v.base === current);
      if (!mine.length) throw new Error(`${file}: ${name} is a ${current} NPC but role ${roleId} has no ${current} outfits`);
      const pick = mine[seedHash(name) % role.outfits.length];
      seen.add(name);
      return `${head}"${resourceName(pick)}"`;
    });
    if (next !== text) {
      changed.push(file);
      if (!dryRun) writeFileSync(path, next);
    }
  }
  const missing = Object.keys(spec.assign).filter((n) => !seen.has(n));
  if (missing.length) throw new Error(`assigned NPCs not found in any layer: ${missing.join(", ")}`);
  return changed;
}

export function main(args) {
  const spec = loadSpec();
  const bad = problems(spec);
  if (bad.length) {
    for (const b of bad) console.error("outfits: " + b);
    return 1;
  }
  let n = 0;
  for (const r of spec.roles) {
    for (const v of variants(spec, r)) {
      const path = resourcePath(v);
      if (writeIfChanged(join(ADDON, path), prefabText(spec, v))) n++;
      if (writeIfChanged(join(ADDON, path + ".meta"), metaText(path))) n++;
    }
  }
  if (writeIfChanged(TABLE, tableText(spec))) n++;
  const st = readFileSync(ST, "utf8");
  const nextSt = upsertStrings(st, stringEntries(spec));
  if (nextSt !== st) { writeFileSync(ST, nextSt); n++; }
  const layers = assignLayers(spec, args.includes("--dry-run"));
  console.log(`outfits: ${n} file(s) written, ${layers.length} layer(s) updated${layers.length ? " (" + layers.join(", ") + ")" : ""}`);
  if (nextSt !== st) console.log("outfits: strings changed; run tools\\cli regen-localization");
  return 0;
}
