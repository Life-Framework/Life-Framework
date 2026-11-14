// Proves tools\cli api's pure parts: class extraction keeps the attributes the
// docs dump drops, the build stamp reads the Steam manifest, staleness is flagged.
// red-proof: with extractClass not walking up over attribute lines this printed
// "attribute missing" and exited 1.
import { extractClass, parseManifest, staleNote } from "../lib/install-api.mjs";

const fail = (m) => { console.error(`install-api-selftest: ${m}`); process.exit(1); };

const src = [
  "// comment",
  "[BaseContainerProps(configRoot: true)]",
  "class LF_Demo : Base",
  "{",
  "  int a;",
  "}",
  "class Other {}",
  "modded class LF_Demo",
  "{",
  "}",
].join("\n");
const hits = extractClass(src, "LF_Demo");
if (hits.length !== 2) fail(`expected plain + modded declaration, got ${hits.length}`);
if (!hits[0].attributes.includes("[BaseContainerProps(configRoot: true)]")) fail("attribute missing");
if (hits[0].text.includes("class Other")) fail("body ran past the closing brace");

const m = parseManifest('"buildid"\t\t"26266485"\n"LastUpdated"\t\t"1794080532"');
if (m.buildid !== "26266485" || m.updatedIso !== "2026-11-07") fail(`bad manifest parse ${JSON.stringify(m)}`);

if (!staleNote(m, 1000).startsWith("STALE")) fail("old dump not flagged stale");
if (staleNote(m, m.updated + 1).startsWith("STALE")) fail("fresh dump flagged stale");
console.log("install-api-selftest: OK");
