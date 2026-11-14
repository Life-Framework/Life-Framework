// tools\cli pack new <Name>: scaffold a content pack (#968).
//
// Writes addons/LF_<Name>/: the .gproj depending on the base game and the core, the folder
// tree (same as the flagship), the pack string table with its language confs, a mission header
// that names the manifest and lists the overlay, an empty config overlay, the manifest holding the
// running core's range, and a README of what to fill in. The output passes `validate`
// (validate-pack-manifest, validate-addon-boundary) with no edits.
//
// GUIDs: the same source as the feature scaffold (8 random bytes, as Workbench writes them),
// checked against every GUID already in addons/ so none repeats. The .meta files are written here
// because a headless Workbench cannot register a file it did not create; opening the project in
// Workbench adopts them unchanged.
// red-proof: tools/validation/pack-new-selftest.mjs.

import { randomBytes } from "node:crypto";
import { existsSync, mkdirSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const BASE_GAME_GUID = "58D0FB3206B6F859";
const CORE_NAME = "LifeFramework";
const FOLDERS = ["Assets", "Configs", "Language", "Missions", "Prefabs", "UI", "Worlds"];
const LANGS = ["en_us", "de_de", "es_es", "fr_fr", "it_it", "pt_br", "pt_pt"];
const PLATFORMS = ["XBOX_ONE", "XBOX_SERIES", "PS4", "HEADLESS"];

const USAGE = `usage: tools\\cli pack new <Name>

  <Name>  PascalCase, becomes addons/LF_<Name> (Everon1989 -> LF_Everon1989)`;

function walk(dir, out = []) {
  if (!existsSync(dir)) return out;
  for (const n of readdirSync(dir)) {
    if (n === "log" || n === ".git") continue;
    const p = join(dir, n);
    if (statSync(p).isDirectory()) walk(p, out);
    else out.push(p);
  }
  return out;
}

function usedGuids(addonsDir) {
  const used = new Set();
  for (const f of walk(addonsDir)) {
    if (!/\.(meta|gproj)$/.test(f)) continue;
    for (const m of readFileSync(f, "utf8").matchAll(/[0-9A-F]{16}/g)) used.add(m[0]);
  }
  return used;
}

// [min, below) around the running core: min is the core, below is its next minor.
export function coreRange(coreDir) {
  let version = null;
  for (const f of walk(join(coreDir, "Scripts")).filter((p) => p.endsWith(".c"))) {
    const m = readFileSync(f, "utf8").match(/CORE_VERSION\s*=\s*"(\d+\.\d+\.\d+)"/);
    if (m) version = m[1];
  }
  if (!version) return null;
  const [maj, min] = version.split(".").map(Number);
  return { min: version, below: `${maj}.${min + 1}.0` };
}

function confMeta(guid, path, cls = "CONFResourceClass") {
  const rest = PLATFORMS.map((p) => `  ${cls} ${p} : PC {\n  }\n`).join("");
  return `MetaFileClass {\n Name "{${guid}}${path}"\n Configurations {\n  ${cls} PC {\n  }\n${rest} }\n}\n`;
}

// Returns { error } or { dir, id, files }. `addonsDir` is addons/ of the repo (or a test tree).
export function scaffoldPack(addonsDir, name) {
  if (!/^[A-Z][A-Za-z0-9]*$/.test(name ?? "")) return { error: `<Name> must be PascalCase, got '${name ?? ""}'` };
  if (/^(LF|EL)_/i.test(name)) return { error: "<Name> comes without the LF_ prefix" };
  const id = `LF_${name}`;
  const dir = join(addonsDir, id);
  if (existsSync(dir)) return { error: `${dir} already exists; pick another name` };
  const coreDir = join(addonsDir, CORE_NAME);
  const coreGproj = join(coreDir, `${CORE_NAME}.gproj`);
  if (!existsSync(coreGproj)) return { error: `no core at ${coreGproj}` };
  const coreGuid = readFileSync(coreGproj, "utf8").match(/GUID\s+"([0-9A-Fa-f]{16})"/)?.[1];
  if (!coreGuid) return { error: "the core .gproj has no GUID" };
  const range = coreRange(coreDir);
  if (!range) return { error: "LF_PackManifest.CORE_VERSION not found in core script" };

  // The boundary check finds a pack by its whole-word id in core text; an id the core already
  // uses (LF_Test is its test base class) would fail it on the first run.
  const idRe = new RegExp(`\\b${id}\\b`);
  const clash = walk(coreDir).find((f) => /\.(c|et|conf|layout|layer|st|gproj)$/.test(f) && idRe.test(readFileSync(f, "utf8")));
  if (clash) return { error: `${id} is already a name in the core (${clash.slice(coreDir.length + 1)}); pick another` };

  const used = usedGuids(addonsDir);
  const guid = () => {
    for (;;) {
      const g = randomBytes(8).toString("hex").toUpperCase();
      if (!used.has(g)) {
        used.add(g);
        return g;
      }
    }
  };

  const files = new Map();
  const put = (path, text) => files.set(path, text);

  const manifestGuid = guid();
  const overlayGuid = guid();
  const missionGuid = guid();
  const stGuid = guid();
  const langs = LANGS.map((code) => ({ code, def: guid(), res: guid() }));
  const prefix = name.toLowerCase();

  const langDefs = langs
    .map((l) => `       LanguageDefinition "{${l.def}}" {\n        Code "${l.code}"\n        StringTableRuntime "{${l.res}}Language/${prefix}_localization.${l.code}.conf"\n       }`)
    .join("\n");
  const otherConfigs = PLATFORMS.map((p) => `  GameProjectConfig ${p} {\n  }`).join("\n");
  put(`${id}.gproj`, `GameProject {
 ID "${id}"
 GUID "${guid()}"
 TITLE "Life Framework: ${name}"
 Dependencies {
  "${BASE_GAME_GUID}" "${coreGuid}"
 }
 Configurations {
  GameProjectConfig PC {
   WidgetManagerSettings WidgetManagerSettings "{${guid()}}" {
    StringTables {
     StringTableDefinition "{${guid()}}" {
      StringTableSource "{${stGuid}}Language/${prefix}_localization.st"
      Languages {
${langDefs}
      }
     }
    }
   }
  }
${otherConfigs}
 }
}
`);

  for (const f of FOLDERS) put(`${f}/.gitkeep`, "");

  put("Configs/LF_PackManifest.conf", `LF_PackManifest {\n m_sPackId "${id}"\n m_sCoreMin "${range.min}"\n m_sCoreBelow "${range.below}"\n}\n`);
  put("Configs/LF_PackManifest.conf.meta", confMeta(manifestGuid, "Configs/LF_PackManifest.conf"));

  // Empty on purpose: an overlay applies only the fields it sets (IsVariableSetDirectly), so this
  // changes no value until a price rule is added. Add more overlays, one per core config class.
  put("Configs/ShopPrices.conf", "LF_ShopPricesConfig {\n}\n");
  put("Configs/ShopPrices.conf.meta", confMeta(overlayGuid, "Configs/ShopPrices.conf"));

  put(`Missions/${id}.conf`, `LF_MissionHeader {
 m_sName "Life Framework: ${name}"
 m_sAuthor ""
 m_sDescription ""
 m_sGameMode "Roleplay"
 m_iPlayerCount 16
 m_LF_PackManifest "{${manifestGuid}}Configs/LF_PackManifest.conf"
 m_aLF_ConfigOverlays {
  "{${overlayGuid}}Configs/ShopPrices.conf"
 }
}
`);
  put(`Missions/${id}.conf.meta`, confMeta(missionGuid, `Missions/${id}.conf`));

  put(`Language/${prefix}_localization.st`, `StringTable {\n ItemClassName "CustomStringTableItem"\n Items {\n }\n}\n`);
  put(`Language/${prefix}_localization.st.meta`, confMeta(stGuid, `Language/${prefix}_localization.st`, "StringTableResourceClass"));
  for (const l of langs) {
    put(`Language/${prefix}_localization.${l.code}.conf`, "StringTableRuntime {\n Ids {\n }\n}\n");
    put(`Language/${prefix}_localization.${l.code}.conf.meta`, confMeta(l.res, `Language/${prefix}_localization.${l.code}.conf`));
  }

  put("README.md", `# ${id}

A Life Framework content pack, scaffolded by \`tools\\cli pack new ${name}\`. The core decides what
the game can do; this pack decides what this server is
([\`docs/architecture/core-vs-pack.md\`](../../docs/architecture/core-vs-pack.md)). It depends on the
core (GUID \`${coreGuid}\`); the core never names it.

The manifest \`Configs/LF_PackManifest.conf\` says this pack supports core ${range.min} up to, not
including, ${range.below}. Raise \`m_sCoreBelow\` once you have tested the pack on a newer core.

## Fill in

- [ ] \`${id}.gproj\`: the title.
- [ ] \`Missions/${id}.conf\`: \`World\`, the description, author, icon and loading screen.
- [ ] \`Configs/ShopPrices.conf\`: the price rules and defaults, or delete it and its line in the mission header.
- [ ] More overlays in \`Configs/\`, one per core config class, each listed in \`m_aLF_ConfigOverlays\`. Only classes in
  [\`public-surface.md\`](../../docs/architecture/public-surface.md) are allowed.
- [ ] \`Language/${prefix}_localization.st\`: the place names, sign titles and flavour copy, as \`${id}-<Thing>\` keys.
- [ ] \`Worlds/\`, \`Prefabs/\`, \`Assets/\`, \`UI/\`: the setting itself. Prefabs inherit core prefabs.
- [ ] Open the project in Workbench once, so it registers the resources.

No script here: a pack that needs code asks for an extension point in the core.
\`validate-addon-boundary\` fails on a \`.c\` file in a pack.
`);

  for (const [path, text] of files) {
    const full = join(dir, path);
    mkdirSync(dirname(full), { recursive: true });
    writeFileSync(full, text);
  }
  return { dir, id, files: [...files.keys()] };
}

export async function main(args, ctx) {
  const [sub, name] = args;
  if (sub !== "new" || !name) {
    console.log(USAGE);
    return 1;
  }
  const root = ctx?.root ?? join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const r = scaffoldPack(join(root, "addons"), name);
  if (r.error) {
    console.error(`pack new: ${r.error}`);
    return 1;
  }
  console.log(`pack new: wrote ${r.files.length} files under addons/${r.id}/`);
  console.log(`next: fill in addons/${r.id}/README.md, then \`tools\\cli validate\``);
  return 0;
}
