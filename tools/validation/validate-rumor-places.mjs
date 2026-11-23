// validate-rumor-places - a rumor row points at a place that exists, and a pack's rumor file is loaded (#4569).
// inputs: addons/*/Configs/Residents/LF_Rumors*.conf, addons/*/Missions/*.conf, addons/*/Worlds/**/*.layer, tools/map/lib/*.mjs
//
// The 1989 rumor rows (#4476) stood unheard because no mission header listed their file, and the pack
// test checked their place ids against a table copied by hand. This reads the real thing. It fails when:
//   place    a row's `m_sPlaceId` is no place the island layers (or the playtest town's) register through
//            `LF_PlaceComponent` or a dedicated place component (tools/map/lib/places.mjs reads them the way the game does);
//   id       two rows of one file share an `m_sId`, or a row has no id, place or line key;
//   header   a pack rumor file is not listed in `m_aLF_ConfigOverlays` of the pack's playable mission headers
//            (LF_Everon1989.conf, LF_PlaytestTown.conf), so no player would ever hear it;
//   voice    a `m_aVoices` entry names a file that does not exist.
// The core `LF_Rumors.conf` ships no rows and is skipped.
//
// red-proof: tools/validation/rumor-places-selftest.mjs plants each violation in a scratch tree.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { loadPlaces } from "../map/lib/places.mjs";

const CORE_RUMORS = "addons/LifeFramework/Configs/Residents/LF_Rumors.conf";
const HEADERS = ["Missions/LF_Everon1989.conf", "Missions/LF_PlaytestTown.conf"];

function walk(dir, test, out = []) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, test, out);
    else if (test(name)) out.push(p);
  }
  return out;
}

const guidOf = (text) => (/^\s*Name "\{([0-9A-Fa-f]{16})\}/m.exec(text) || [])[1]?.toUpperCase() || "";

/** Rows of one rumor conf: [{ id, place, line, at }] by a scan of its `LF_RumorDef` blocks. */
export function rumorRows(text) {
  const rows = [];
  const lines = text.split("\n");
  for (let i = 0; i < lines.length; i++) {
    if (!/^\s*LF_RumorDef\b/.test(lines[i])) continue;
    const row = { id: "", place: "", line: "", at: i + 1 };
    for (let j = i + 1; j < lines.length && !/^\s*LF_RumorDef\b/.test(lines[j]); j++) {
      const m = /^\s*(m_sId|m_sPlaceId|m_sLineKey)\s+"([^"]*)"/.exec(lines[j]);
      if (m) row[{ m_sId: "id", m_sPlaceId: "place", m_sLineKey: "line" }[m[1]]] = m[2];
    }
    rows.push(row);
  }
  return rows;
}

/** Every `m_aVoices` resource of one rumor conf, as written ("{GUID}path"). */
export function rumorVoices(text) {
  // A resource name holds braces ("{GUID}path"), so the block ends at the first line that is only a brace.
  const m = /m_aVoices\s*\{[ \t]*\r?\n([\s\S]*?)^[ \t]*\}[ \t]*$/m.exec(text);
  return m ? [...m[1].matchAll(/"([^"]+)"/g)].map((v) => v[1]) : [];
}

/** Returns { fails: [string], rows, files } for the tree under `root`. */
export function checkRumorPlaces(root) {
  const fails = [];
  const addons = join(root, "addons");
  const confs = existsSync(addons)
    ? readdirSync(addons).flatMap((a) => walk(join(addons, a, "Configs", "Residents"), (n) => /^LF_Rumors.*\.conf$/.test(n)))
    : [];
  const packConfs = confs.filter((f) => relative(root, f).split("\\").join("/") !== CORE_RUMORS);

  const known = new Set();
  for (const world of ["main", "town"]) {
    let places = [];
    try {
      places = loadPlaces(root, { world }).places;
    } catch {
      places = []; // a tree with no such world: nothing registers there
    }
    for (const p of places) {
      known.add(p.id);
      known.add(p.id.replace(/#\d+$/, ""));
    }
  }

  let rows = 0;
  for (const file of packConfs) {
    const rel = relative(root, file).split("\\").join("/");
    const text = readFileSync(file, "utf8");
    const seen = new Set();
    for (const row of rumorRows(text)) {
      rows++;
      const where = `${rel}:${row.at}`;
      if (!row.id || !row.place || !row.line) fails.push(`id: ${where} a rumor row needs an id, a place and a line key (id "${row.id}")`);
      else if (seen.has(row.id)) fails.push(`id: ${where} the rumor id ${row.id} is already used in this file`);
      seen.add(row.id);
      if (row.place && !known.has(row.place)) fails.push(`place: ${where} ${row.id} points at "${row.place}", which no island or town layer registers as a place`);
    }

    for (const voice of rumorVoices(text)) {
      const path = voice.replace(/^\{[0-9A-Fa-f]+\}/, "");
      const addon = rel.split("/")[1];
      const found = [addon, ...readdirSync(addons)].some((a) => existsSync(join(addons, a, path)));
      if (!found) fails.push(`voice: ${rel} lists the recorded conversation ${path}, which does not exist`);
    }

    // The pack's headers must load the file, or its rows are never offered.
    const meta = existsSync(`${file}.meta`) ? readFileSync(`${file}.meta`, "utf8") : "";
    const guid = guidOf(meta);
    const pack = join(addons, rel.split("/")[1]);
    for (const header of HEADERS) {
      const headerFile = join(pack, header);
      if (!existsSync(headerFile)) continue;
      const listed = new RegExp(`"\\{${guid}\\}[^"]*"`).test(readFileSync(headerFile, "utf8"));
      if (!guid || !listed) fails.push(`header: ${rel} is not listed in ${header}'s m_aLF_ConfigOverlays, so no player hears it`);
    }
  }

  return { fails, rows, files: packConfs.length };
}

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const { fails, rows, files } = checkRumorPlaces(process.env.LF_RUMOR_ROOT || root);
  if (fails.length) {
    console.log(fails.map((f) => `FAIL ${f}`).join("\n"));
    console.log(`validate-rumor-places: ${fails.length} problem(s) in ${files} rumor file(s).`);
    process.exit(1);
  }
  console.log(`validate-rumor-places: OK (${rows} row(s) in ${files} rumor file(s))`);
}
