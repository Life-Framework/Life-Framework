// tools/pack/retheme-fixture.mjs: the #1005 integration fixture. `pack new Retheme` writes a pack that
// changes nothing; this applies the four overlays of the "make it yours" chapter to it so one booted
// session can prove them together (agent-box/proofs/retheme-proof.mjs):
//   UI theme    Configs/UITheme.conf     (#966)  paper theme recoloured
//   economy     Configs/ShopPrices.conf  (#962)  drinks priced 31 (core 6)
//               Configs/Catalogue.conf           the M9 item and its gun counter offer removed
//   prefab slot Configs/PrefabSlots.conf (#964)  evidence_marker swapped to the shell casing
//   terms       Configs/Terms.conf       (#958)  police renamed Sheriff, words in the pack's string table
// The generated pack is not committed (AGENTS.md); this fixture is. GUIDs are fixed in fixture.json so a
// re-run writes the same pack.
import { copyFileSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const FIXTURE_DIR = join(ROOT, "tools", "test", "pack-fixtures", "retheme");

export function loadFixture() {
  return JSON.parse(readFileSync(join(FIXTURE_DIR, "fixture.json"), "utf8"));
}

function metaText(name, guid) {
  const configs = ["PC", "XBOX_ONE", "XBOX_SERIES", "PS4", "HEADLESS"].map((c, i) => `  CONFResourceClass ${c}${i ? " : PC" : ""} {\n  }`).join("\n");
  return `MetaFileClass {\n Name "{${guid}}${name}"\n Configurations {\n${configs}\n }\n}\n`;
}

/** Applies the fixture to the scaffolded pack in `addonsDir`. Returns the pack folder. */
export function applyRetheme(addonsDir) {
  const fx = loadFixture();
  const dir = join(addonsDir, `LF_${fx.pack}`);
  const header = join(dir, "Missions", `LF_${fx.pack}.conf`);
  if (!existsSync(header)) throw new Error(`${dir} is not a scaffolded pack (run \`tools\cli pack new ${fx.pack}\` first)`);

  for (const rel of fx.overlays) {
    mkdirSync(dirname(join(dir, rel)), { recursive: true });
    copyFileSync(join(FIXTURE_DIR, rel), join(dir, rel));
    const guid = fx.metaGuids[rel];
    if (guid) writeFileSync(join(dir, `${rel}.meta`), metaText(rel, guid));
  }

  // The header loads every overlay by the GUID of its .meta (the scaffold's ShopPrices.conf keeps its own).
  const guidOfMeta = (rel) => readFileSync(join(dir, `${rel}.meta`), "utf8").match(/\{([0-9A-F]{16})\}/)[1];
  const list = fx.overlays.map((rel) => `  "{${guidOfMeta(rel)}}${rel}"`).join("\n");
  const text = readFileSync(header, "utf8");
  const next = text.replace(/ m_aLF_ConfigOverlays \{\n[\s\S]*?\n \}/, ` m_aLF_ConfigOverlays {\n${list}\n }`);
  if (next === text && !text.includes(list)) throw new Error(`${header}: no m_aLF_ConfigOverlays block to fill`);
  writeFileSync(header, next);

  const st = join(dir, "Language", `${fx.pack.toLowerCase()}_localization.st`);
  let table = readFileSync(st, "utf8");
  const rows = fx.strings
    .filter((s) => !table.includes(`Id "${s.id}"`))
    .map((s) => `  CustomStringTableItem "{${s.guid}}" {\n   Id "${s.id}"\n   Target_en_us "${s.en_us}"\n   Comment "${s.comment}"\n  }\n`)
    .join("");
  table = table.replace(/ Items \{\n/, ` Items {\n${rows}`);
  writeFileSync(st, table);
  return dir;
}
