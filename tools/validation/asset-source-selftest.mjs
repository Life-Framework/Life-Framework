// Proves the source ladder (tools/assets/pipeline/source.mjs, sources.mjs, docs/asset-pipeline/sources.md)
// inputs: tools/**, agent-box/**, .githooks/**
// offline: no network, no Blender, no Workbench, no Meshy.
// red-proof: 2026-11-14, three lines broken, each FAIL seen, all restored:
//   licenceVerdict accepting by-nc                 -> FAIL source: NC, ND, SA and unlabelled licences are refused
//   searchCatalogue without the size filter        -> FAIL source: the best name match wins and a 9 m cliff is not a candidate
//   appendCredit without the already-there test    -> FAIL source: a credit row is added once
import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { STAGES, specProblems } from "../assets/pipeline/asset.mjs";
import * as src from "../assets/pipeline/source.mjs";
import { VANILLA_SKIP, licenceVerdict, vanillaQuery } from "../assets/pipeline/sources.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");

export async function main() {
  let failed = 0;
  const ok = (cond, what) => {
    if (cond) return;
    failed++;
    console.log(`FAIL  ${what}`);
  };
  const tmp = mkdtempSync(join(tmpdir(), "lf-source-selftest-"));

  ok(licenceVerdict("cc0").ok && licenceVerdict("CC0 1.0").ok && licenceVerdict("by").credit === true, "source: CC0 is free and CC-BY needs its credit");
  ok(["by-nc", "by-nd", "by-sa", "by-nc-sa", "", "all rights reserved"].every((l) => !licenceVerdict(l).ok), "source: NC, ND, SA and unlabelled licences are refused");

  const entry = (o) => ({ categories: ["props"], description: "", authors: { A: "All" }, thumbnail_url: "u", ...o });
  const cat = {
    cement_bag: entry({ name: "Cement Bag", tags: ["cement", "bag"], description: "A 50kg cement bag.", dimensions: [462, 700, 180], polycount: 844 }),
    coast_cliff: entry({ name: "Coastal Cliff", tags: ["cliff", "cement"], dimensions: [9198, 1097, 1033], polycount: 1e6 }),
    trashbag: entry({ name: "Trashbag", tags: ["bag"], dimensions: [530, 460, 570], polycount: 4482 }),
  };
  const found = src.searchCatalogue(cat, src.queryTerms({ name: "CementBag" }), { size: 0.7 });
  ok(found[0]?.id === "cement_bag" && found.some((f) => f.id === "trashbag") && !found.some((f) => f.id === "coast_cliff"), `source: the best name match wins and a 9 m cliff is not a candidate (${found.map((f) => f.id)})`);
  ok(src.rawWords("CementBag, a 50 kg paper sack").join() === "cement,bag,paper,sack", `source: query words split PascalCase and drop numbers and stop words (${src.rawWords("CementBag, a 50 kg paper sack")})`);

  const base = { slug: "x", name: "X" };
  ok(src.sourceProblems({ ...base, source: { provider: "polyhaven", id: "cement_bag", resolution: "2k" } }).length === 0 && src.sourceProblems({ ...base, source: "search" }).length === 0, 'source: a Poly Haven id and "search" pass');
  ok(src.sourceProblems({ ...base, source: { provider: "polyhaven", id: "a b" } }).length > 0
    && src.sourceProblems({ ...base, source: { provider: "polyhaven", id: "a", resolution: "3k" } }).length > 0
    && src.sourceProblems({ ...base, source: { provider: "vanilla", path: "Props/x.xob" } }).length > 0
    && src.sourceProblems({ ...base, source: { provider: "sketchfab", id: "a" } }).length > 0, "source: a bad id, resolution, vanilla path or unsupported provider is refused");
  ok(!!src.skipsStage({ source: { provider: "vanilla", path: "Assets/a.xob" } }, "import") && !src.skipsStage({ source: { provider: "polyhaven", id: "a" } }, "import")
    && !!src.skipsStage({ source: { provider: "polyhaven", id: "a" } }, "generate") && !src.skipsStage({}, "generate"), "source: a vanilla source skips clean and import, Poly Haven skips generate only");

  const files = { gltf: { "2k": { gltf: { url: "https://x/cement_bag_2k.gltf", md5: "m", size: 1, include: { "textures/d.jpg": { url: "https://x/d.jpg", md5: "n", size: 2 } } } } } };
  const picked = src.pickFiles(files, "cement_bag", "2k");
  ok(picked.files.length === 2 && picked.files[0].path === "cement_bag_2k.gltf" && picked.files[1].path === "textures/d.jpg", "source: the glTF and its included maps are listed with their paths");
  let refused = false;
  try { src.pickFiles(files, "cement_bag", "8k"); } catch (e) { refused = /no 8k glTF/.test(e.message); }
  ok(refused, "source: a resolution the asset lacks is refused");
  const tex = (i) => ({ pbrMetallicRoughness: { baseColorTexture: { index: i } } });
  ok(src.gltfProblems({ meshes: [{}], materials: [tex(0), tex(1)] }, "m").some((m) => /2 texture sets/.test(m))
    && src.gltfProblems({ meshes: [{}], materials: [{ alphaMode: "BLEND" }] }, "m").some((m) => /alpha/.test(m))
    && src.gltfProblems({ meshes: [{}], materials: [tex(0)] }, "m").length === 0, "source: more than one texture set or an alpha material is refused");

  const credits = join(tmp, "credits.md");
  writeFileSync(credits, ["# C", "", "## Poly Haven models", "", "text", "", "| A | B |", "|---|---|", "| old | row |", "", "## Next", ""].join("\n"));
  const row = src.creditRow({ slug: "s", asset_dir: "Assets/S", name: "S" }, { name: "Thing", authors: "Au", url: "https://polyhaven.com/a/thing" });
  const first = src.appendCredit(credits, row, "https://polyhaven.com/a/thing");
  const second = src.appendCredit(credits, row, "https://polyhaven.com/a/thing");
  ok(first && !second, "source: a credit row is added once");
  const after = readFileSync(credits, "utf8");
  ok(after.indexOf("| Thing") > after.indexOf("| old | row |") && after.indexOf("| Thing") < after.indexOf("## Next"), "source: the row lands at the end of the Poly Haven table");

  mkdirSync(join(tmp, "tmp"), { recursive: true });
  const fake = () => ({ stdout: ["  Assets/Props/Furniture/Safe_01/Safe_01.xob", "  Assets/Props/Furniture/Safe_01/Dst/Safe_01_dst.xob", "  Prefabs/Props/Furniture/Safe_01.et", "  Assets/Props/X/X_LOD1.xob"].join("\n") });
  ok(vanillaQuery(tmp, "safe", fake).join() === "Props/Furniture/Safe_01/Safe_01.xob" && VANILLA_SKIP.test("A/Dst/b.xob"), "source: the base-game search keeps models and drops damage states, LODs and prefabs");

  const cement = JSON.parse(readFileSync(join(ROOT, "tools", "assets", "cement-bag", "spec.json"), "utf8"));
  ok(specProblems(cement).length === 0, `spec: a Poly Haven spec needs no prompt or credits (${specProblems(cement)[0]})`);
  ok(STAGES[1] === "source", "source: the stage runs right after spec");
  ok(existsSync(join(ROOT, "tools", "assets", "cement-bag", "source.json")) && JSON.parse(readFileSync(join(ROOT, "tools", "assets", "cement-bag", "source.json"), "utf8")).licence.startsWith("CC0"), "source: the proof asset keeps its provenance record");

  if (failed) {
    console.log(`asset-source-selftest: FAILED (${failed})`);
    return 1;
  }
  console.log("asset-source-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
