// Every ammo prefab an addon's magazine config uses is in the addon's AmmoList.conf (issue #1643).
// inputs: addons/**/*.conf, addons/**/*.conf.meta, addons/**/*.et.meta
//
// The base game's magazine system looks each round of a magazine up in `Configs/Weapons/Ammo/AmmoList.conf`
// when the magazine is created. A round that is not listed throws a Virtual Machine Exception ("unable to
// find <prefab> in the SCR_MagazineRepackingSystem ammunition list") at the first frame after a save that
// holds the magazine loads. The addon lists its own rounds in an appending override of that file.
//
// Checked here, with no game install (a round that lives in the base game is the base list's business):
//   - each ammo prefab a `MagazineConfig` (`AmmoResourceArray`) names that exists in an addon is listed,
//     with the GUID the prefab's own .meta carries, in some addon's AmmoList.conf;
//   - the override is `m_aAmmoList + { ... }`: a plain `m_aAmmoList { ... }` would stand in for the game's
//     whole list and drop every vanilla round.
//
// red-proof: 2026-11-15, the taser line removed from addons/LifeFramework/Configs/Weapons/Ammo/AmmoList.conf
// printed `FAIL ... Ammo_Taser.et is used by ... LF_Ammo_Taser.conf but is not in AmmoList.conf` and exited 1;
// with the line restored it prints OK.
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const files = execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard", "addons"], {
  cwd: root,
  encoding: "utf8",
  maxBuffer: 1 << 28,
}).split("\0").filter((f) => f && existsSync(join(root, f)));

const REF = /"\{([0-9A-F]{16})\}([^"]+)"/g;
const read = (f) => readFileSync(join(root, f), "utf8");
const bad = [];

// Every addon-relative resource path, by addon: addons/<Addon>/<rel>.
const addonPaths = new Map(); // rel path -> repo file
for (const f of files) {
  const m = /^addons\/[^/]+\/(.+)$/.exec(f);
  if (m && !f.endsWith(".meta")) addonPaths.set(m[1], f);
}

const listed = new Set(); // GUID + path of every round an AmmoList.conf names
let lists = 0;
for (const f of files) {
  if (!/^addons\/[^/]+\/Configs\/Weapons\/Ammo\/AmmoList\.conf$/.test(f)) continue;
  lists++;
  const text = read(f);
  if (!/m_aAmmoList\s*\+\s*\{/.test(text)) {
    bad.push(`FAIL ${f}: m_aAmmoList is not the appending form \`m_aAmmoList + {\`; it would replace the game's list`);
  }
  for (const m of text.matchAll(REF)) listed.add(`${m[1]}${m[2]}`);
}

let rounds = 0;
for (const f of files) {
  if (!f.endsWith(".conf")) continue;
  const text = read(f);
  // The array closes on a line of its own; a `}` inside a "{GUID}path" string is not its end.
  const array = /AmmoResourceArray\s*\{([\s\S]*?)\n\s*\}/.exec(text);
  if (!array) continue;
  for (const m of array[1].matchAll(REF)) {
    const [, guid, path] = m;
    const file = addonPaths.get(path);
    if (!file) continue; // a base-game round: the game's own list holds it
    rounds++;
    const metaFile = file + ".meta";
    const meta = existsSync(join(root, metaFile)) ? read(metaFile) : "";
    const real = /Name "\{([0-9A-F]{16})\}/.exec(meta)?.[1];
    if (real && real !== guid) {
      bad.push(`FAIL ${f}: ${path} is referenced as {${guid}} but its .meta says {${real}}`);
      continue;
    }
    if (!listed.has(`${guid}${path}`)) {
      bad.push(`FAIL ${file}: ${path} is used by ${f} but is not in AmmoList.conf; add "{${guid}}${path}" to the addon's Configs/Weapons/Ammo/AmmoList.conf (append form)`);
    }
  }
}

if (bad.length) {
  console.log(bad.join("\n"));
  console.log(`validate-ammo-list: ${bad.length} problem(s). A magazine whose round is not listed throws a VME when a save holding it loads (#1643).`);
  process.exit(1);
}
console.log(`validate-ammo-list: OK (${rounds} addon round(s) used by a magazine config, ${lists} AmmoList.conf override(s))`);
