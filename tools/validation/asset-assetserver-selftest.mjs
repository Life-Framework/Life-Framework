// Proves the 3d-asset-server rung (tools/assets/pipeline/assetserver.mjs) offline on fixture JSON: the licence gate
// (CGTrader Royalty Free allowed, Editorial refused), the result mapping, the scoring against a spec, the shortlist,
// the purchased/ source and that no buying path exists. No network, no install, no Blender.
// red-proof: 2026-11-14, three lines broken, each FAIL seen, all restored:
//   licenceGate editorial test disabled          -> FAIL gate: Editorial is refused
//   scoreCandidate not returning 0 when refused  -> FAIL score: a refused licence never ranks
//   sourceProblems skipping the risk_accepted    -> FAIL purchased: a CGTrader source needs risk_accepted
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import * as as from "../assets/pipeline/assetserver.mjs";
import * as src from "../assets/pipeline/source.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");

// Shaped like 3d-asset-server's Asset (src/core/types.ts); values from a live search of 2026-11-14.
const asset = (o) => ({ id: "x:1", provider: "x", nativeId: "1", title: "Thing", type: "model", tags: [], url: "https://x/1", downloadable: false, ...o });
const FIX = {
  cc0: asset({ id: "threedassets:radio", provider: "threedassets", title: "Radio charging rack", license: { name: "CC0", commercialUse: true }, price: { free: true }, formats: ["glb"], polyCount: 3224, downloadable: true, score: 0.7 }),
  by: asset({ id: "sketchfab:a", provider: "sketchfab", title: "Police radio scanner", license: { name: "CC-BY-4.0", attributionRequired: true }, price: { free: true }, formats: ["glb", "fbx"], polyCount: 5000, tags: ["pbr"], downloadable: true }),
  sa: asset({ id: "x:sa", title: "Scanner sa", license: { name: "CC-BY-SA-4.0" }, price: { free: true }, formats: ["glb"] }),
  rf: asset({ id: "cgtrader:scanner", provider: "cgtrader", title: "Police radio scanner", license: { name: "Royalty Free", commercialUse: true }, price: { free: false, amount: 19, currency: "USD" }, formats: ["fbx", "obj", "blend"], polyCount: 7800, tags: ["pbr"] }),
  ed: asset({ id: "cgtrader:ed", provider: "cgtrader", title: "Police radio scanner editorial", license: { name: "Editorial", commercialUse: false }, price: { free: false, amount: 9, currency: "USD" }, formats: ["fbx"], polyCount: 1000 }),
  nc: asset({ id: "x:nc", title: "Scanner nc", license: { name: "CC-BY-NC-4.0" }, price: { free: true }, formats: ["glb"] }),
  nolic: asset({ id: "x:none", title: "Scanner none", formats: ["glb"] }),
};

export async function main() {
  let failed = 0;
  const ok = (cond, what) => { if (!cond) { failed++; console.log(`FAIL  ${what}`); } };
  const gate = (k) => as.licenceGate(FIX[k].license, { price: FIX[k].price, provider: FIX[k].provider }).verdict;

  ok(gate("cc0") === "ok" && gate("by") === "credit" && as.licenceGate({ name: "CC-BY-4.0" }).credit === true, "gate: CC0 is ok and CC-BY needs its credit");
  ok(gate("rf") === "paid" && !!as.licenceGate(FIX.rf.license, { provider: "cgtrader" }).caveat, "gate: CGTrader Royalty Free is allowed as a paid purchase, with the extraction caveat");
  ok(gate("ed") === "refuse" && as.licenceGate({ name: "Editorial" }).verdict === "refuse", "gate: Editorial is refused");
  ok(gate("nc") === "refuse" && gate("nolic") === "refuse", "gate: non-commercial and an unstated licence are refused");
  ok(gate("sa") === "review", "gate: share-alike needs a human look");
  ok(as.licenceGate({ name: "Royalty Free (no AI)" }, { provider: "cgtrader" }).verdict === "paid" && as.licenceGate({ name: "No redistribution" }).verdict === "review" && as.licenceGate({ name: "CC BY-ND" }).verdict === "refuse", "gate: variants of Royalty Free, no-redistribution and no-derivatives");

  const c = as.mapAsset(FIX.rf);
  ok(c.free === false && c.price === "19 USD" && c.formats.join() === "fbx,obj,blend" && c.tris === 7800 && c.pbr && !c.direct && c.gate.verdict === "paid", `map: a paid CGTrader result keeps price, formats, polys, PBR and is not a direct download (${JSON.stringify(c)})`);
  const d = as.mapAsset(FIX.cc0);
  ok(d.direct && d.free && d.price === "free" && d.credit === false, "map: a free downloadable CC0 result is a direct download");

  const spec = { name: "PoliceScanner", search: "police radio scanner", budget: { lod0_tris: 6000 } };
  const ranked = as.rank(Object.values(FIX).map((a) => as.mapAsset(a)), spec);
  ok(!ranked.some((r) => ["cgtrader:ed", "x:nc", "x:none"].includes(r.id)), "score: a refused licence never ranks");
  ok(ranked[0].id === "sketchfab:a" && ranked[1].id === "cgtrader:scanner", `score: matching words, importable formats and the polygon budget put the CC-BY scanner first, the CGTrader one second (${ranked.map((r) => r.id + ":" + r.score)})`);
  ok(as.scoreCandidate(as.mapAsset(FIX.sa), spec).score <= 50, "score: a licence under review is capped");
  ok(as.scoreCandidate(as.mapAsset(FIX.ed), spec).score === 0, "score: Editorial scores 0");

  const md = as.shortlistMarkdown("scanner", spec, ranked, [as.linkEntry("cgtrader", "CGTrader", as.cgtraderSearchUrl("police radio scanner"))], { query: "police radio scanner" });
  ok(md.includes("never buys") && md.includes("purchased/") && md.includes("Royalty Free (paid)") && md.includes("19 USD") && md.includes("Incorporated Product") && md.includes("cgtrader.com/3d-models?keywords=police%20radio%20scanner"), "shortlist: link, price, licence, caveat, the human buy step and the CGTrader search URL are all in the file");

  // search() through a fake service: CGTrader is never queried by us, its search URL is handed to the human.
  const seen = [];
  const service = { async search(req) { seen.push(req); return { results: [FIX.cc0, FIX.rf], providers: [{ provider: "fab", name: "Fab", status: "link", count: 0, searchUrl: "https://www.fab.com/search?q=x" }, { provider: "threedassets", status: "ok", count: 1 }] }; } };
  const found = await as.search(ROOT, { query: "scanner", service });
  ok(found.links[0].provider === "cgtrader" && found.links.some((l) => l.provider === "fab") && found.candidates.length === 2 && seen[0].types.join() === "model", "search: CGTrader and link-only markets come back as links, the rest as candidates");
  ok(as.SKIP_PROVIDERS.has("cgtrader") && !as.DEFAULT_ACCESS.has("scrape"), "search: CGTrader (robots.txt Disallow /search) and unaudited scrapers are not queried");

  // the purchased source
  const base = { slug: "s", name: "S" };
  const rf = { provider: "purchased", url: "https://www.cgtrader.com/3d-models/electronics/audio/police-scanner", licence: "Royalty Free" };
  ok(src.sourceProblems({ ...base, source: rf }).some((p) => p.includes("risk_accepted")), "purchased: a CGTrader source needs risk_accepted");
  ok(src.sourceProblems({ ...base, source: { ...rf, risk_accepted: true } }).length === 0, "purchased: Royalty Free with the human's yes passes");
  ok(src.sourceProblems({ ...base, source: { ...rf, licence: "Editorial", risk_accepted: true } }).length > 0 && src.sourceProblems({ ...base, source: { ...rf, licence: "CC-BY-SA-4.0" } }).some((p) => p.includes("licence_checked")), "purchased: Editorial is refused and share-alike needs licence_checked");
  ok(src.sourceProblems({ ...base, source: { provider: "purchased", licence: "CC0" } }).length > 0 && src.sourceProblems({ ...base, source: { provider: "download", id: "nocolon" } }).length > 0 && src.sourceProblems({ ...base, source: { provider: "download", id: "threedassets:radio" } }).length === 0, "purchased: a store URL is required; a download id is provider:id");
  ok(!!src.skipsStage({ source: rf }, "generate") && !src.skipsStage({ source: rf }, "clean") && !!src.skipsStage({ source: { provider: "download", id: "a:b" } }, "concept"), "purchased: no generation, but clean and import still run");

  // findModel prefers glb over fbx, skips LOD copies
  const tmp = mkdtempSync(join(tmpdir(), "lf-as-selftest-"));
  const dir = join(tmp, "purchased", "pack");
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "scanner.fbx"), "x".repeat(50));
  writeFileSync(join(dir, "scanner_lod1.fbx"), "x".repeat(500));
  ok(as.findModel(join(tmp, "purchased")).endsWith("scanner.fbx") && as.findModel(join(tmp, "nothing")) === null, "findModel: picks the model and skips LOD copies");
  writeFileSync(join(dir, "scanner.glb"), "x");
  ok(as.findModel(join(tmp, "purchased")).endsWith("scanner.glb"), "findModel: glb before fbx");

  // credits row for the library section
  const credits = join(tmp, "credits.md");
  writeFileSync(credits, `${src.INCOMING_HEADING}\n\n| Model | Author | Source | Licence | Recorded in |\n|---|---|---|---|---|\n| old | row |\n\n## Next\n`);
  const row = "| Scanner | Seller | https://www.cgtrader.com/3d-models/x | Royalty Free | `tools/assets/s/source.json` |";
  ok(src.appendCredit(credits, row, "https://www.cgtrader.com/3d-models/x", src.INCOMING_HEADING) && !src.appendCredit(credits, row, "https://www.cgtrader.com/3d-models/x", src.INCOMING_HEADING), "credits: a library or purchased row is added once");
  ok(readFileSync(join(ROOT, "docs", "credits.md"), "utf8").includes(src.INCOMING_HEADING), "credits: docs/credits.md has the library and purchased section");

  // The rung has no purchase code: no store login, cart or payment call.
  const code = ["assetserver.mjs", "source.mjs", "sources.mjs"].map((f) => readFileSync(join(ROOT, "tools", "assets", "pipeline", f), "utf8")).join("\n");
  ok(!/\/(cart|checkout|orders|users\/login|sign_in)\b|cardNumber/i.test(code), "never buys: no cart, checkout, login or payment code in the source rung");

  if (failed) { console.log(`asset-assetserver-selftest: FAILED (${failed})`); return 1; }
  console.log("asset-assetserver-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
