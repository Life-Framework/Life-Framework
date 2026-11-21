// tools/town/stamp.mjs - `tools\cli town stamp <kit> [--place p] | --all`: write a grid-authored, building-local kit onto every
// island building the place mappings assign it, and let the island audit judge the result (card #3881,
// docs/architecture/placement-system.md "Author in the grid, stamp into the world").
//
//   town stamp <kit> [--place p] [--dry] [--no-audit] [--no-claims]       every mapped copy of <kit> (or the one place)
//   town stamp --all [--dry] [--no-audit]                                 every kit that carries a `stamp` block (`town author extract`)
//   test overrides: [--places-dir d] [--templates-dir d] [--kits-dir d] [--layer-dir d] [--claims-dir d]
//
// A stamped copy is the kit at the building's own origin and yaw (`planFit` with sameShell): the kit was authored around a copy of
// the SAME vanilla building in the grid, so its building-local offsets, height included, land on the same floors and walls. The
// stamp refuses a building whose prefab differs from the kit's (`stamp.prefab`, else `building.vanilla`). Rows go out sorted by kit
// row id and rounded to the mm, so stamping twice writes the same bytes and a moved kit row moves in every copy.
// Then `audit placement --strict --world MainWorld --layer <the stamped layers>` runs, and a hard finding on a stamped row
// (STAMP_HARD) puts the previous files back and exits 1: the island audit is the stamp's verdict, not a warning.
// Pure parts (stampTargets, planStamp, stampVerdict) are what tools/town/stamp.selftest.mjs runs on tools/town/fixtures/stamp/.
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { checkKit } from "./kit.mjs";
import { CLAIMS_DIR, LAYER_DIR, planFit } from "./fit.mjs";

export const help = "stamp <kit> [--place p] [--dry] [--no-audit] | stamp --all   write a grid-authored building-local kit onto every island building mapped to it (same prefab only), then the island audit judges it: a hard finding refuses the stamp";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const REF_RE = /^\{([0-9A-Fa-f]{16})\}(.+)$/;
// Audit kinds that refuse a stamp: the layout must sit on the floor, out of door swings and out of walls and other props.
export const STAMP_HARD = new Set(["floating", "sunk", "blocks-door", "clips", "overlaps"]);

const readJson = (f) => JSON.parse(readFileSync(f, "utf8"));
const refKey = (ref) => {
  const m = REF_RE.exec(String(ref ?? ""));
  return m ? `${m[1].toUpperCase()}|${m[2].toLowerCase()}` : String(ref ?? "").toLowerCase();
};
export const kitPrefab = (kit) => kit.stamp?.prefab ?? kit.building?.vanilla ?? null;

// A kit with its rows in id order: the layer is written in that order, so the bytes never depend on the order a grid edit left.
function sortedKit(kit) {
  return { ...kit, rows: [...kit.rows].sort((a, b) => (a.id < b.id ? -1 : a.id > b.id ? 1 : 0)) };
}

// Every place whose template gives <kit> to a slot that the mapping puts in a building: [{ place, slot, building, dossier, mapping, template }].
export function stampTargets(kitName, { placesDir, templatesDir, place = null }) {
  const out = [];
  if (!existsSync(placesDir)) return out;
  for (const p of readdirSync(placesDir).sort()) {
    if (place && p !== place) continue;
    const mf = join(placesDir, p, "mapping.json");
    const df = join(placesDir, p, "dossier.json");
    if (!existsSync(mf) || !existsSync(df)) continue;
    const mapping = readJson(mf);
    const tf = join(templatesDir, `${mapping.template}.json`);
    if (!existsSync(tf)) continue;
    const template = readJson(tf);
    const dossier = readJson(df);
    for (const slot of template.slots ?? []) {
      if (slot.kit !== kitName) continue;
      const entry = mapping.slots?.[slot.id];
      if (!entry || entry.none || !entry.building || entry.dossier) continue;
      const building = (dossier.buildings ?? []).find((b) => b.id === entry.building) ?? null;
      out.push({ place: p, slot: slot.id, building, dossier, mapping, template });
    }
  }
  return out;
}

// Pure but for the prefab reads planFit makes. Returns { ok, errors, copies: [{ place, slot, layerFile, layerText, claimFile, claim }] }.
export function planStamp(kit, targets, { root = ROOT, layerDir = LAYER_DIR, claimsDir = CLAIMS_DIR, skipClaims = false, branch = null, now = null, readGame = null, outRoot = root } = {}) {
  const errors = [...checkKit(kit, { root }).map((e) => `kit: ${e}`)];
  const want = kitPrefab(kit);
  if (!want) errors.push(`kit ${kit.kit}: names no building prefab (stamp.prefab or building.vanilla); author it around a vanilla building first (town author)`);
  if (!targets.length) errors.push(`kit ${kit.kit}: no place mapping assigns it to a building`);
  if (errors.length) return { ok: false, errors, copies: [] };
  const sk = sortedKit(kit);
  const copies = [];
  for (const t of targets) {
    const where = `${t.place}.${t.slot}`;
    if (!t.building) {
      errors.push(`${where}: building ${t.mapping.slots[t.slot].building} is not in the dossier`);
      continue;
    }
    if (refKey(t.building.prefab) !== refKey(want)) {
      errors.push(`${where}: building ${t.building.id} is ${t.building.prefab}, the kit was authored around ${want}; a stamp only goes onto the same prefab (author another kit for it)`);
      continue;
    }
    const plan = planFit(
      { place: t.place, slot: t.slot, dossier: t.dossier, mapping: t.mapping, template: t.template, kits: { [t.template.slots.find((s) => s.id === t.slot).kit]: sk }, sameShell: true, refit: true, skipClaims, branch, fetch: false },
      { root, now, readGame },
    );
    if (!plan.ok) {
      errors.push(...plan.errors.map((e) => `${where}: ${e}`));
      continue;
    }
    // A row whose prefab reads y over the terrain (RELATIVE_Y, tools/lib/height-frame.mjs) cannot be stamped from building-local
    // heights alone: it needs the ground under it, which only `town fit --terrain` probes.
    if (plan.framePending?.length) {
      errors.push(`${where}: ${plan.framePending.join(", ")} read y over the terrain (their prefab flags carry RELATIVE_Y); a stamp writes world heights only: fit the slot with town fit --terrain, or use a prefab without the flag`);
      continue;
    }
    const layerFile = `${layerDir}/${plan.layerName}`;
    const claimFile = `${claimsDir}/${t.place}-${t.slot}.json`;
    // The claim keeps the date it was first stamped with while nothing else changed, so a re-stamp is byte-equal.
    const stampHash = createHash("sha1").update(plan.layerText).digest("hex").slice(0, 12);
    const claim = { ...plan.claim, layer: layerFile, stamp: { kit: kit.kit, prefab: want, layer: stampHash } };
    const prevFile = join(outRoot, claimFile);
    if (existsSync(prevFile)) {
      try {
        const prev = readJson(prevFile);
        if (prev.stamp?.layer === stampHash && prev.date) claim.date = prev.date;
      } catch {
        /* a broken claim is rewritten */
      }
    }
    copies.push({ place: t.place, slot: t.slot, building: t.building.id, layerFile, layerText: plan.layerText, claimFile, claim: JSON.stringify(claim, null, 2) + "\n", warnings: plan.warnings });
  }
  return { ok: errors.length === 0, errors, copies };
}

// The audit's report.json against the stamped layers: every hard finding on a stamped row, as text.
export function stampVerdict(report, layerNames) {
  const names = new Set(layerNames.map((l) => l.split("/").pop()));
  const hard = [];
  for (const r of report?.ranked ?? []) {
    if (!names.has(String(r.layer ?? "").split("/").pop())) continue;
    for (const p of r.problems ?? []) if (STAMP_HARD.has(p.kind)) hard.push(`${r.name} [${r.layer}]: ${p.kind}${p.text ? ` (${p.text})` : ""}`);
  }
  return hard;
}

// Write the copies, audit them, and put the previous bytes back on a hard finding. ctx.audit(layers) -> { report } | { error }.
export function applyStamp(plan, { root = ROOT, audit = null, dry = false, log = console.log } = {}) {
  if (!plan.ok) {
    for (const e of plan.errors) log(`  refused: ${e}`);
    return 1;
  }
  const files = plan.copies.flatMap((c) => [[c.layerFile, c.layerText], [c.claimFile, c.claim]]);
  const changed = files.filter(([f, text]) => !existsSync(join(root, f)) || readFileSync(join(root, f), "utf8") !== text);
  for (const c of plan.copies) log(`  ${c.place}.${c.slot} -> ${c.building}: ${c.layerFile}${changed.some(([f]) => f === c.layerFile) ? "" : " (unchanged)"}`);
  if (dry) return 0;
  const before = new Map(changed.map(([f]) => [f, existsSync(join(root, f)) ? readFileSync(join(root, f), "utf8") : null]));
  for (const [f, text] of changed) {
    mkdirSync(dirname(join(root, f)), { recursive: true });
    writeFileSync(join(root, f), text);
  }
  if (!audit) return 0;
  const layers = plan.copies.map((c) => c.layerFile);
  const res = audit(layers);
  const hard = res.error ? [`the island audit did not run: ${res.error}`] : stampVerdict(res.report, layers);
  if (!hard.length) {
    log(`  island audit: no hard finding on ${layers.length} stamped layer(s)`);
    return 0;
  }
  for (const [f, text] of before) {
    if (text === null) rmSync(join(root, f), { force: true });
    else writeFileSync(join(root, f), text);
  }
  log(`  stamp refused, previous files restored; fix the kit in the grid (town author) and stamp again:`);
  for (const h of hard) log(`    ${h}`);
  return 1;
}

function islandAudit(root, kitName) {
  return (layers) => {
    const out = join(root, "tmp", "stamp-audit", kitName);
    const names = layers.map((l) => l.split("/").pop()).join(",");
    const r = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "audit", "placement", "--strict", "--world", "MainWorld", "--layer", names, "--hard-layer", names, "--out", out], { cwd: root, encoding: "utf8", timeout: 600000, windowsHide: true });
    const rep = join(out, "report.json");
    if (!existsSync(rep)) return { error: `exit ${r.status}, no report (${String(r.stdout ?? "").trim().split("\n").pop()})` };
    return { report: readJson(rep) };
  };
}

function parseArgs(args) {
  const o = { _: [] };
  const valued = new Set(["place", "places-dir", "templates-dir", "kits-dir", "layer-dir", "claims-dir"]);
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a.startsWith("--")) {
      const k = a.slice(2);
      o[k] = valued.has(k) ? args[++i] : true;
    } else o._.push(a);
  }
  return o;
}

export async function main(args, { root = ROOT } = {}) {
  const a = parseArgs(args);
  if (a.help || (!a._[0] && !a.all)) {
    console.log("usage: tools\\cli town stamp <kit> [--place p] [--dry] [--no-audit] [--no-claims] | town stamp --all\n(tools/town/stamp.mjs header; docs/starter-packs/stamp.md)");
    return a.help ? 0 : 2;
  }
  const kitsDir = resolve(root, a["kits-dir"] ?? "tools/town/kits");
  const opts = { placesDir: resolve(root, a["places-dir"] ?? "docs/everon/places"), templatesDir: resolve(root, a["templates-dir"] ?? "tools/town/templates"), place: a.place ?? null };
  const names = a.all
    ? readdirSync(kitsDir).filter((f) => f.endsWith(".json")).map((f) => readJson(join(kitsDir, f))).filter((k) => k.stamp).map((k) => k.kit).sort()
    : [a._[0]];
  const branch = spawnSync("git", ["rev-parse", "--abbrev-ref", "HEAD"], { cwd: root, encoding: "utf8" }).stdout?.trim() || null;
  let fails = 0;
  for (const name of names) {
    const f = join(kitsDir, `${name}.json`);
    if (!existsSync(f)) {
      console.log(`town stamp: no kit ${name} (${f})`);
      fails++;
      continue;
    }
    const kit = readJson(f);
    console.log(`town stamp ${name}:`);
    const plan = planStamp(kit, stampTargets(name, opts), { root, layerDir: a["layer-dir"] ?? LAYER_DIR, claimsDir: a["claims-dir"] ?? CLAIMS_DIR, skipClaims: Boolean(a["no-claims"]), branch });
    for (const c of plan.copies) for (const w of c.warnings ?? []) console.log(`  warning ${c.place}.${c.slot}: ${w}`);
    if (applyStamp(plan, { root, dry: Boolean(a.dry), audit: a["no-audit"] ? null : islandAudit(root, name) })) fails++;
  }
  console.log(`SUMMARY town-stamp ${fails ? "fail" : "ok"} kits=${names.length}${fails ? ` refused=${fails}` : ""}`);
  return fails ? 1 : 0;
}
