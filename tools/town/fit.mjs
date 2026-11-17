// tools/town/fit.mjs - `tools\cli town fit <place> <slot>`: apply a slot's kit to the building the approved mapping names
// (docs/architecture/everon-world.md 3.3 and 3.4, card #2205).
//
//   town fit <place> <slot> [--dry] [--no-fetch] [--no-audit] [--no-look] [--building-prefab {GUID}path] [--destruction-guid {GUID}]
//            [--dossier f] [--mapping f] [--template-file f] [--kits-dir d] [--out-dir d]
//
// Reads  docs/everon/places/<place>/dossier.json and mapping.json, tools/town/templates/<kind>.json, tools/town/kits/<kit>.json
// Writes addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_<place>_<slot>.layer   (pack content, never a core layer)
//        tools/town/claims/<place>-<slot>.json
// Then runs `audit placement --strict --world MainWorld --layer <file>` and takes the `look` pictures (--no-audit, --no-look skip).
//
// No step types a coordinate: every number comes from the dossier (the building's centre, floor and entrance) and the kit's
// building-local offsets (`kit apply`). The kit turns so its `front` faces the entrance, in quarter turns from the building's yaw.
// A slot's layer holds the slot's LF_PlaceComponent marker(s) (kind from the template, id `<place>.<slot>`) and one entity per kit row.
// A slot whose mapping entry is a free plot (D9) also gets the purpose-built building, written with its destruction off (D15).
// A vanilla building stands in the terrain, not in a layer, so a layer cannot switch its destruction off: the claim records
// `destruction.mode: "needs-hook"` until the core hook exists (the PR for this card says so).
// Pure functions (planFit and what it calls) are what tools/validation/town-fit-selftest.mjs runs; main() reads and writes files.
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { basename, dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { applyKit, checkKit } from "./kit.mjs";
import { checkMapping, loadTemplate } from "./map.mjs";

export const help = "fit <place> <slot> [--dry] [--no-audit] [--no-look]   apply the slot's kit into its mapped building: Place_<place>_<slot>.layer (marker + kit rows) and a claim";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const LAYER_DIR = "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers";
export const CLAIMS_DIR = "tools/town/claims";
const SLUG_RX = /^[a-z0-9][a-z0-9-]*$/;
const REF_RE = /^\{([0-9A-Fa-f]{16})\}(.+)$/;
const FLAGS = new Set(["dry", "no-fetch", "no-audit", "no-look", "building-prefab", "destruction-guid", "dossier", "mapping", "template-file", "kits-dir", "out-dir", "help"]);
const WITH_VALUE = new Set(["building-prefab", "destruction-guid", "dossier", "mapping", "template-file", "kits-dir", "out-dir"]);
const r3 = (n) => Math.round(n * 1000) / 1000;
const norm = (d) => {
  let x = ((d % 360) + 360) % 360;
  if (x > 180) x -= 360;
  return x;
};
const ident = (s) => String(s).replace(/\W/g, "_");
const FRONT = { "+x": [1, 0], "-x": [-1, 0], "+z": [0, 1], "-z": [0, -1] };

export const layerNameOf = (place, slot) => `Place_${place}_${slot}.layer`;
export const claimFileOf = (place, slot) => `${CLAIMS_DIR}/${place}-${slot}.json`;
const guidOf = (...parts) => createHash("sha1").update(parts.join("|")).digest("hex").slice(0, 16).toUpperCase();

// ------------------------------------------------------------------ geometry from the dossier

// Pure: where the kit sits in a building. x, z and the floor come from the dossier; the yaw turns the kit's front side to
// the entrance, in quarter turns from the building's own yaw (a kit with no front keeps the building's yaw).
export function anchorFor(building, kit) {
  const x = building.footprint?.cx ?? building.x;
  const z = building.footprint?.cz ?? building.z;
  const y = r3(building.y + (building.firstFloor ?? 0)); // the scout's first-floor height above the building origin
  const byaw = building.yaw ?? 0;
  const front = FRONT[kit.anchoring?.front];
  const ent = (building.entrances ?? []).find((e) => e.side === "front") ?? (building.entrances ?? [])[0];
  let yaw = byaw;
  let note = "no entrance in the dossier or no front on the kit: the building's yaw";
  if (front && ent && (ent.x !== x || ent.z !== z)) {
    const heading = (dx, dz) => (Math.atan2(dx, dz) * 180) / Math.PI;
    const raw = heading(ent.x - x, ent.z - z) - heading(front[0], front[1]);
    yaw = byaw + Math.round(norm(raw - byaw) / 90) * 90;
    note = `kit front ${kit.anchoring.front} turned to the entrance`;
  }
  return { x, y, z, yaw: r3(norm(yaw)), marker: [x, y, z], note };
}

// ------------------------------------------------------------------ prefab component guids (config overrides)

function prefabFile(ref, root) {
  const m = REF_RE.exec(ref ?? "");
  if (!m) return null;
  for (const a of ["LifeFramework", "LF_Everon1989"]) {
    const f = join(root, "addons", a, m[2]);
    if (existsSync(f)) return f;
  }
  return null;
}

// The GUID a component has in a prefab or in the prefab it inherits, so a layer override reuses it (a new one would shadow it).
export function componentGuid(ref, cls, root, hops = 6) {
  let cur = ref;
  for (let i = 0; i < hops && cur; i++) {
    const f = prefabFile(cur, root);
    if (!f) return null;
    const text = readFileSync(f, "utf8");
    const hit = new RegExp(`^\\s*${cls}\\s+"\\{([0-9A-Fa-f]{16})\\}"`, "m").exec(text);
    if (hit) return hit[1].toUpperCase();
    const up = /^\s*\w+\s*:\s*"(\{[0-9A-Fa-f]{16}\}[^"]+)"/m.exec(text);
    cur = up ? up[1] : null;
  }
  return null;
}

// ------------------------------------------------------------------ claims (origin/main, open PRs, sibling worktrees)

const sh = (run, root) => (cmd, args, timeout = 30000) => run(cmd, args, { cwd: root, encoding: "utf8", timeout, maxBuffer: 64 * 1024 * 1024 });
const defaultRun = (cmd, args, opts) => spawnSync(cmd, args, opts);
const asClaim = (text, extra) => {
  try {
    const j = JSON.parse(text);
    return j && j.place && j.slot ? { ...j, ...extra } : null;
  } catch {
    return null;
  }
};

// Claim files origin/main holds.
export function mainTownClaims(exec, { ref = "origin/main", fetch = true } = {}) {
  let note = "";
  if (fetch) {
    const f = exec("git", ["fetch", "--quiet", "origin", "main"], 20000);
    if (f.status !== 0) note = "could not fetch origin/main; used the last one this clone has";
  }
  const ls = exec("git", ["ls-tree", "--name-only", "-r", ref, "--", `${CLAIMS_DIR}/`]);
  if (ls.status !== 0) return { claims: [], note: note || `no ${ref} here; claims on main were not checked` };
  const claims = [];
  for (const file of String(ls.stdout).split(/\r?\n/).filter((f) => f.endsWith(".json"))) {
    const show = exec("git", ["show", `${ref}:${file}`]);
    const c = show.status === 0 ? asClaim(show.stdout, { file, where: "origin/main" }) : null;
    if (c) claims.push(c);
  }
  return { claims, note };
}

// Claim files of every open PR (the #2021 read: one `gh pr list`, one raw read per claim file). Fails soft.
export function openPrTownClaims(exec, { branch = null } = {}) {
  const ls = exec("gh", ["pr", "list", "--state", "open", "--limit", "200", "--json", "number,headRefName,headRefOid,files"]);
  if (!ls || ls.status !== 0) return { claims: [], note: "could not read the claims of open PRs (gh failed); claims in flight were not checked" };
  let prs;
  try {
    prs = JSON.parse(ls.stdout);
  } catch {
    return { claims: [], note: "could not read the claims of open PRs (unreadable answer); claims in flight were not checked" };
  }
  const claims = [];
  for (const pr of Array.isArray(prs) ? prs : []) {
    if (branch && pr.headRefName === branch) continue;
    for (const f of pr.files ?? []) {
      const path = String(f.path ?? "");
      if (!path.startsWith(`${CLAIMS_DIR}/`) || !path.endsWith(".json")) continue;
      const raw = exec("gh", ["api", "-H", "Accept: application/vnd.github.raw", `repos/{owner}/{repo}/contents/${path}?ref=${pr.headRefOid || pr.headRefName}`]);
      const c = raw && raw.status === 0 ? asClaim(raw.stdout, { file: path, where: `open PR #${pr.number} (${pr.headRefName})` }) : null;
      if (c) claims.push(c);
    }
  }
  return { claims, note: "" };
}

// Uncommitted claim files in sibling local worktrees: no network, read from `git worktree list --porcelain`.
export function worktreeTownClaims(exec, root) {
  const ls = exec("git", ["worktree", "list", "--porcelain"]);
  if (ls.status !== 0) return { claims: [], note: "could not list the worktrees; claims in sibling worktrees were not checked" };
  const claims = [];
  for (const block of String(ls.stdout).split(/\r?\n\r?\n/)) {
    const path = /^worktree (.+)$/m.exec(block)?.[1];
    if (!path || resolve(path) === resolve(root)) continue;
    const dir = join(path, CLAIMS_DIR);
    if (!existsSync(dir)) continue;
    for (const f of readdirSync(dir).filter((n) => n.endsWith(".json"))) {
      const c = asClaim(readFileSync(join(dir, f), "utf8"), { file: `${CLAIMS_DIR}/${f}`, where: `worktree ${basename(path)}` });
      if (c) claims.push(c);
    }
  }
  return { claims, note: "" };
}

// Pure: why another claim blocks this fit ([] when none does). A street slot shares its building's frontage.
export function conflicts(claims, { place, slot, building, plot, shared }) {
  const out = [];
  for (const c of claims) {
    if (c.place !== place) continue;
    const mine = c.slot === slot;
    const same = !shared && !c.shared && ((building && c.building === building) || (plot && c.plot === plot));
    if (mine) out.push(`slot ${place}.${slot} is already claimed on ${c.where} (${c.file})`);
    else if (same) out.push(`${building ? `building ${building}` : `plot ${plot}`} is already claimed by ${c.place}.${c.slot} on ${c.where} (${c.file})`);
  }
  return out;
}

// ------------------------------------------------------------------ layer text

function entityText({ cls = "GenericEntity", name, ref, comps = [], coords, yaw }) {
  const lines = [`${cls} ${name}${ref ? ` : "${ref}"` : ""} {`];
  if (comps.length) {
    lines.push(" components {");
    for (const c of comps) {
      lines.push(`  ${c.cls} "{${c.guid}}" {`);
      for (const [k, v] of Object.entries(c.set)) lines.push(`   ${k} ${v}`);
      lines.push("  }");
    }
    lines.push(" }");
  }
  lines.push(` coords ${coords.map(String).join(" ")}`);
  if (yaw) lines.push(` angleY ${yaw}`);
  lines.push("}");
  return lines.join("\n");
}

// ------------------------------------------------------------------ the plan

// Pure but for the reads `ctx.run` makes (git, gh) and the prefab files componentGuid reads.
//   opts: { place, slot, dossier, mapping, template, kits: { <name>: kit }, kitFile, buildingPrefab, destructionGuid, branch, fetch }
//   ctx:  { root, run(cmd, args, opts), now() }
// Returns { ok, errors, warnings, notes, layerName, layerText, claimFile, claim, anchor, rows }.
export function planFit(opts, ctx = {}) {
  const root = ctx.root ?? ROOT;
  const exec = sh(ctx.run ?? defaultRun, root);
  const errors = [];
  const warnings = [];
  const fail = () => ({ ok: false, errors, warnings, notes: [] });
  const { place, slot: slotId, dossier, mapping, template } = opts;

  const slot = template.slots.find((s) => s.id === slotId);
  if (!slot) return (errors.push(`the ${template.kind} template has no slot '${slotId}'`), fail());
  const entry = mapping.slots?.[slotId];
  if (!entry) return (errors.push(`slot ${slotId} is not in the mapping`), fail());
  if (entry.none) return (errors.push(`slot ${slotId} is mapped to none${entry.reason ? ` (${entry.reason})` : ""}: nothing to fit`), fail());
  for (const p of checkMapping(mapping, dossier, template)) if (!/^slot /.test(p) || p.startsWith(`slot ${slotId}:`)) errors.push(`mapping: ${p}`);
  if (errors.length) return fail();

  const kit = opts.kits?.[slot.kit];
  if (!kit) return (errors.push(`kit '${slot.kit}' (slot ${slotId}) was not found`), fail());
  const kitProblems = checkKit(kit, { root });
  if (kitProblems.length) return (errors.push(...kitProblems.map((p) => `kit: ${p}`)), fail());

  const building = entry.building ? dossier.buildings.find((b) => b.id === entry.building) : null;
  const plotE = entry.plot ? (dossier.freePlots?.plots ?? []).find((p) => p.id === entry.plot) : null;
  if (!building && !plotE) return (errors.push(`slot ${slotId}: neither building nor plot is in the dossier`), fail());

  // Claims: origin/main, open PRs, sibling worktrees. A read that fails is a note, never a block.
  const notes = [];
  const taken = [];
  for (const r of [mainTownClaims(exec, { fetch: opts.fetch !== false }), openPrTownClaims(exec, { branch: opts.branch ?? null }), worktreeTownClaims(exec, root)]) {
    taken.push(...r.claims);
    if (r.note) notes.push(r.note);
  }
  errors.push(...conflicts(taken, { place, slot: slotId, building: entry.building, plot: entry.plot, shared: entry.shared }));
  if (errors.length) return { ...fail(), notes };

  // The building: a mapped one stands where the dossier says; a plot gets the purpose-built building first (D9).
  let host;
  const out = [];
  let destruction;
  if (building) {
    host = building;
    destruction = { mode: "needs-hook", reason: "a vanilla building stands in the terrain; a layer cannot switch its destruction off (D15 hook)" };
  } else {
    if (!opts.buildingPrefab || !REF_RE.test(opts.buildingPrefab)) return (errors.push(`slot ${slotId} is a free plot: the purpose-built building must be resolved first (asset_search) and passed as --building-prefab {GUID}path`), { ...fail(), notes });
    const guid = opts.destructionGuid ?? componentGuid(opts.buildingPrefab, "SCR_DestructionMultiPhaseComponent", root);
    if (!guid) return (errors.push(`cannot find the SCR_DestructionMultiPhaseComponent guid of ${opts.buildingPrefab}; pass --destruction-guid {GUID}`), { ...fail(), notes });
    host = { x: plotE.x, y: plotE.ground ? r3((plotE.ground.min + plotE.ground.max) / 2) : plotE.y ?? 0, z: plotE.z, yaw: 0, entrances: [], footprint: null };
    destruction = { mode: "layer-override", component: "SCR_DestructionMultiPhaseComponent" };
    out.push({ name: `Fit_${ident(place)}_${ident(slotId)}_Building`, ref: opts.buildingPrefab, coords: [host.x, host.y, host.z], yaw: 0, comps: [{ cls: "SCR_DestructionMultiPhaseComponent", guid: guid.replace(/[{}]/g, "").toUpperCase(), set: { Enabled: "0" } }] });
  }

  const anchor = anchorFor(host, kit);
  const rows = applyKit(kit, anchor.x, anchor.z, anchor.yaw, anchor.y);

  // The slot's markers: the first kind carries `<place>.<slot>`; each further kind is a marker of its own.
  const kinds = slot.places?.length ? slot.places : [];
  const markers = kinds.map((kind, i) => ({
    name: `Fit_${ident(place)}_${ident(slotId)}_Place${i ? `_${ident(kind)}` : ""}`,
    comps: [{ cls: "LF_PlaceComponent", guid: guidOf(place, slotId, kind), set: { m_sKind: `"${kind}"`, m_sId: `"${place}.${slotId}"` } }],
    coords: anchor.marker,
    yaw: 0,
  }));
  if (!markers.length) warnings.push(`slot ${slotId} lists no LF_Places kind: the layer carries no place marker`);

  const srcRows = new Map(kit.rows.map((r) => [r.id, r]));
  const rowEnts = rows.map((r) => {
    const src = srcRows.get(r.id);
    const comps = [];
    for (const c of src.config ?? []) {
      if (!Object.keys(c.set ?? {}).length) continue;
      const guid = componentGuid(r.prefab, c.component, root);
      if (!guid) {
        warnings.push(`row ${r.id}: no guid for ${c.component} in ${r.prefab}; its overrides were not written`);
        continue;
      }
      comps.push({ cls: c.component, guid, set: c.set });
    }
    return { cls: src.class && /^\w+$/.test(src.class) ? src.class : "GenericEntity", name: `Fit_${ident(place)}_${ident(slotId)}_${ident(r.id)}`, ref: r.prefab, comps, coords: r.coords, yaw: r.yaw };
  });

  const layerName = layerNameOf(place, slotId);
  const layerText = `${[...markers, ...out, ...rowEnts].map(entityText).join("\n")}\n`;
  const claim = {
    schema: 1,
    place,
    slot: slotId,
    ...(entry.building ? { building: entry.building } : { plot: entry.plot }),
    ...(entry.shared ? { shared: true } : {}),
    kit: slot.kit,
    layer: `${LAYER_DIR}/${layerName}`,
    kinds,
    rows: rows.length,
    destruction,
    date: (ctx.now ? ctx.now() : new Date()).toISOString().slice(0, 10),
  };
  return { ok: true, errors, warnings, notes, layerName, layerText, claimFile: claimFileOf(place, slotId), claim, anchor, rows };
}

// ------------------------------------------------------------------ CLI

function parseArgs(args) {
  const flags = {};
  const pos = [];
  const bad = [];
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a.startsWith("--")) {
      const k = a.slice(2);
      if (!FLAGS.has(k)) bad.push(a);
      else if (WITH_VALUE.has(k)) flags[k] = args[++i];
      else flags[k] = true;
    } else pos.push(a);
  }
  return { flags, pos, bad };
}

const USAGE = "usage: tools\\cli town fit <place> <slot> [--dry] [--no-fetch] [--no-audit] [--no-look] [--building-prefab {GUID}path] [--destruction-guid {GUID}]";
const readJson = (f, what) => {
  if (!existsSync(f)) throw new Error(`no ${what}: ${f} does not exist`);
  return JSON.parse(readFileSync(f, "utf8"));
};

export async function main(args, { root = ROOT, run } = {}) {
  const { flags, pos, bad } = parseArgs(args);
  if (flags.help) {
    console.log(USAGE);
    return 0;
  }
  if (bad.length) {
    console.log(`REFUSE  ${bad.join(" ")}: fit takes no coordinate and no other flag; every number comes from the dossier and the kit\n${USAGE}`);
    return 2;
  }
  const [place, slotId] = pos;
  if (!place || !slotId || !SLUG_RX.test(place) || !SLUG_RX.test(slotId)) {
    console.log(USAGE);
    return 2;
  }
  const exec = sh(run ?? defaultRun, root);
  const branch = (() => {
    const r = exec("git", ["rev-parse", "--abbrev-ref", "HEAD"], 10000);
    return r.status === 0 ? String(r.stdout).trim() : null;
  })();

  // Only a town-draft-<place>-<slot> worktree writes an island layer (T4 enforces it too); --dry and --out-dir are for trying and tests.
  if (!flags.dry && !flags["out-dir"] && !new RegExp(`^ws/town-draft-${place}-${slotId}$`).test(branch ?? "")) {
    console.log(`REFUSE  town fit writes an island layer only from the worktree ws/town-draft-${place}-${slotId} (this is ${branch ?? "unknown"}); create it with tools\\cli wt new town-draft-${place}-${slotId}, or pass --dry`);
    return 1;
  }

  let plan;
  try {
    const placeDir = join(root, "docs", "everon", "places", place);
    const dossier = readJson(resolve(root, flags.dossier ?? join(placeDir, "dossier.json")), "dossier");
    const mapping = readJson(resolve(root, flags.mapping ?? join(placeDir, "mapping.json")), "mapping");
    const template = flags["template-file"] ? readJson(resolve(root, flags["template-file"]), "template") : loadTemplate(mapping.template, root);
    const entry = template.slots.find((s) => s.id === slotId);
    const kits = {};
    if (entry) {
      const kitsDir = resolve(root, flags["kits-dir"] ?? "tools/town/kits");
      const kf = join(kitsDir, `${entry.kit}.json`);
      if (existsSync(kf)) kits[entry.kit] = readJson(kf, "kit");
    }
    plan = planFit({ place, slot: slotId, dossier, mapping, template, kits, buildingPrefab: flags["building-prefab"], destructionGuid: flags["destruction-guid"], branch, fetch: !flags["no-fetch"] }, { root, run });
  } catch (e) {
    console.log(`REFUSE  ${e.message}`);
    return 1;
  }
  for (const n of plan.notes ?? []) console.log(`note  ${n}`);
  for (const w of plan.warnings ?? []) console.log(`warn  ${w}`);
  if (!plan.ok) {
    for (const e of plan.errors) console.log(`REFUSE  ${e}`);
    console.log(`SUMMARY town-fit fail ${place}.${slotId}: ${plan.errors.length} problem(s)`);
    return 1;
  }

  const layerFile = join(resolve(root, flags["out-dir"] ?? LAYER_DIR), plan.layerName);
  const claimFile = join(resolve(root, flags["out-dir"] ?? dirname(plan.claimFile)), basename(plan.claimFile));
  const shown = (f) => relative(root, f).replaceAll("\\", "/");
  if (flags.dry) {
    console.log(plan.layerText);
    console.log(`${JSON.stringify(plan.claim, null, 2)}`);
    console.log(`SUMMARY town-fit dry ${place}.${slotId}: ${plan.rows.length} kit row(s), anchor ${plan.anchor.x} ${plan.anchor.y} ${plan.anchor.z} yaw ${plan.anchor.yaw} (${plan.anchor.note}); would write ${shown(layerFile)} and ${shown(claimFile)}`);
    return 0;
  }
  mkdirSync(dirname(layerFile), { recursive: true });
  mkdirSync(dirname(claimFile), { recursive: true });
  writeFileSync(layerFile, plan.layerText);
  writeFileSync(claimFile, `${JSON.stringify(plan.claim, null, 2)}\n`);

  // Verify: the placement audit on the new layer, then the pictures. Each runs on a pool machine with a Workbench.
  const steps = [];
  const cli = join(root, "tools", process.platform === "win32" ? "cli.cmd" : "cli");
  const stepRun = (name, a) => {
    const r = spawnSync(cli, a, { cwd: root, encoding: "utf8", shell: process.platform === "win32", timeout: 600000 });
    steps.push(`${name} ${r.status === 0 ? "ok" : "fail"}`);
    return r.status === 0;
  };
  let ok = true;
  if (!flags["no-audit"]) ok = stepRun("audit", ["audit", "placement", "--strict", "--world", "MainWorld", "--layer", plan.layerName]) && ok;
  if (!flags["no-look"]) {
    const at = `${plan.anchor.x},${plan.anchor.z}`;
    ok = stepRun("look", ["look", at, "--close", "12"]) && ok;
    ok = stepRun("look-interior", ["look", at, "--interior"]) && ok;
  }
  console.log(`SUMMARY town-fit ${ok ? "ok" : "fail"} ${place}.${slotId}: ${shown(layerFile)}, ${shown(claimFile)}${steps.length ? `; ${steps.join(", ")}` : "; audit and look skipped"}`);
  return ok ? 0 : 1;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) process.exit(await main(process.argv.slice(2), { root: ROOT }));
