// The parcel kiosk's pack prefab, from the base game's NewspaperBooth_E_01_redyellow (Saint-Philippe b21).
//
//   node tools/buildings/parcel-kiosk/prune.mjs          write the prefab below
//   node tools/buildings/parcel-kiosk/prune.mjs --check  exit 1 when the file is not what this script writes
//
// ParcelKiosk.et  a SCR_DestructibleBuildingEntity that inherits the vanilla redyellow booth and adds nothing yet.
//                 The booth is one model with its shelves, counter and hatch built in: the vanilla prefab (and its
//                 Base) carry no Furniture child to flatten or prune, only glass and one door. This script throws
//                 if a game update adds one, so the shell never silently carries repeats.
// Writing twice gives the same bytes; a missing `.meta` is made with a fresh GUID and kept after that (rule 2).
// The kiosk is fitted indestructible through the place layer (D15), not here.
//
// Space: front +z. Room rectangles are in the prefab's own frame (spec.json "rooms").
import { createHash, randomBytes } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gameVfs, readText } from "../../lib/game-prefab.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const REL = "Prefabs/Buildings/ParcelKiosk";
const DIR = join(ROOT, "addons", "LF_Everon1989", ...REL.split("/"));
const DIR_V = "Prefabs/Structures/Commercial/Booth/NewspaperBooth_E_01";
const HOST = `{DA33ED8E94FD68DF}${DIR_V}/NewspaperBooth_E_01_redyellow.et`;
const CHECK = process.argv.includes("--check");


const metaText = (guid, path) => `MetaFileClass {
 Name "{${guid}}${path}"
 Configurations {
  EntityTemplateResourceClass PC {
  }
  EntityTemplateResourceClass XBOX_ONE : PC {
  }
  EntityTemplateResourceClass XBOX_SERIES : PC {
  }
  EntityTemplateResourceClass PS4 : PC {
  }
  EntityTemplateResourceClass PS5 : PC {
  }
  EntityTemplateResourceClass HEADLESS : PC {
  }
 }
}
`;

function guidOf(rel) {
  const meta = join(DIR, `${rel}.meta`);
  if (existsSync(meta)) return /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))?.[1];
  if (CHECK) throw new Error(`${rel}.meta is missing`);
  const guid = randomBytes(8).toString("hex").toUpperCase();
  mkdirSync(dirname(meta), { recursive: true });
  writeFileSync(meta, metaText(guid, `${REL}/${rel}`));
  return guid;
}

async function main() {
  const vfs = await gameVfs();
  const vanilla = [`${DIR_V}/NewspaperBooth_E_01_redyellow.et`, `${DIR_V}/NewspaperBooth_E_01_Base.et`].map((p) => readText(ROOT, vfs, p));
  for (const text of vanilla) {
    if (/Furniture|Decorations/.test(text)) throw new Error("the vanilla booth now carries furniture: add a flatten/prune step (see fuel-station/prune.mjs)");
  }
  guidOf("ParcelKiosk.et");
  const id = createHash("sha256").update("ParcelKiosk.et").digest("hex").slice(0, 16).toUpperCase();
  const text = `SCR_DestructibleBuildingEntity : "${HOST}" {\n ID "${id}"\n}\n`;
  const file = join(DIR, "ParcelKiosk.et");
  const drift = !existsSync(file) || readFileSync(file, "utf8") !== text;
  if (drift && CHECK) console.log("  differs: ParcelKiosk.et");
  else if (drift) {
    mkdirSync(DIR, { recursive: true });
    writeFileSync(file, text);
  }
  console.log(`parcel-kiosk prune: ParcelKiosk.et, 0 vanilla props to prune${CHECK ? (drift ? " DRIFT" : " current") : " written"}`);
  if (CHECK && drift) process.exit(1);
}
main().catch((e) => {
  console.error(`parcel-kiosk prune: ${e.message}`);
  process.exit(1);
});
