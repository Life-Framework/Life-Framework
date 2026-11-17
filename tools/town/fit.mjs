// tools/town/fit.mjs - `tools\cli town fit <place> <slot>`: apply a slot's kit to the building the approved mapping names
// (docs/architecture/everon-world.md 3.3 and 3.4, card #2205).
//
//   town fit <place> <slot> [--dry] [--check] [--allow-clip] [--no-fetch] [--no-audit] [--no-look] [--building-prefab {GUID}path]
//            [--destruction-guid {GUID}] [--dossier f] [--mapping f] [--template-file f] [--kits-dir d] [--out-dir d]
//
// Reads  docs/everon/places/<place>/dossier.json and mapping.json, tools/town/templates/<kind>.json, tools/town/kits/<kit>.json
// Writes addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_<place>_<slot>.layer   (pack content, never a core layer)
//        tools/town/claims/<place>-<slot>.json
// Then runs `audit placement --strict --world MainWorld --layer <file>` and takes the `look` pictures (--no-audit, --no-look skip).
//
// No step types a coordinate: every number comes from the dossier (the building's centre, floor and entrance) and the kit's
// building-local offsets (`kit apply`). A host that shows the kit's own building model (the .xob of the kit's
// `building.vanilla`) takes the kit at its origin and yaw: the kit's offsets are already in that frame. Any other host takes
// it at its footprint centre and floor, turned so the kit's `front` faces the entrance, in quarter turns from the building's yaw.
// Then every row is seated on the HOST, read offline from its model colliders (tools/town/seat.mjs): a floor row on the
// host's top under it, a wall-mounted row on the nearest wall plane along its facing; a row that still cuts the shell
// refuses the fit (--allow-clip writes it anyway). `--check` prints the per-row corrections against the layer on disk and
// writes nothing.
// A slot's layer holds the slot's LF_PlaceComponent marker(s) (kind from the template, id `<place>.<slot>`) and one entity per kit row.
// A slot whose mapping entry is a free plot (D9) also gets the purpose-built building, written with its destruction off (D15).
// A vanilla building stands in the terrain, not in a layer, so a layer cannot switch its destruction off: the slot's place
// marker carries LF_FittedBuildingComponent, which turns the building's damage off on the server, and the claim records
// `destruction.mode: "hooked"`. A slot with no place marker keeps `needs-hook`.
// Pure functions (planFit and what it calls) are what tools/validation/town-fit-selftest.mjs runs; main() reads and writes files.
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { basename, dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { applyKit, checkKit, entitiesOf, parseBlocks } from "./kit.mjs";
import { checkMapping, loadTemplate } from "./map.mjs";
import { sameShell, seatReport, seatRows, shellReader } from "./seat.mjs";

export const help = "fit <place> <slot> [--dry] [--check] [--allow-clip] [--no-audit] [--no-look]   apply the slot's kit into its mapped building, rows seated on the host's floors and walls: Place_<place>_<slot>.layer (marker + kit rows) and a claim";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const LAYER_DIR = "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers";
export const CLAIMS_DIR = "tools/town/claims";
const SLUG_RX = /^[a-z0-9][a-z0-9-]*$/;
const REF_RE = /^\{([0-9A-Fa-f]{16})\}(.+)$/;
const FLAGS = new Set(["dry", "check", "allow-clip", "no-fetch", "no-audit", "no-look", "building-prefab", "destruction-guid", "dossier", "mapping", "template-file", "kits-dir", "out-dir", "help"]);
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
// `base` is the height kit offsets count from: a kit's row offsets already hold its own floor (`anchoring.floorY`), so
// base = the host floor - that floor (adding both put every row one floor height too high: 0.2 m on the island shops, #2369).
// sameShell: the host shows the kit's own building model, so the kit sits at the host's origin and yaw, unturned.
export function anchorFor(building, kit, { sameShell = false } = {}) {
  if (sameShell) {
    const fy = r3(building.y + (building.firstFloor ?? 0));
    return { x: building.x, y: fy, z: building.z, yaw: r3(norm(building.yaw ?? 0)), base: building.y, marker: [building.x, fy, building.z], note: "the host shows the kit's own building model: the kit at its origin and yaw" };
  }
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
  return { x, y, z, yaw: r3(norm(yaw)), base: r3(y - (kit.anchoring?.floorY ?? 0)), marker: [x, y, z], note };
}

// ------------------------------------------------------------------ prefab component guids (config overrides)

function prefabFile(ref, root) {
  const m = REF_RE.exec(ref ?? "");
  if (!m) return null;
  const addons = join(root, "addons");
  const names = existsSync(addons) ? readdirSync(addons) : [];
  for (const a of ["LifeFramework", "LF_Everon1989", ...names]) {
    const f = join(addons, a, m[2]);
    if (existsSync(f)) return f;
  }
  return null;
}

// A prefab's text: an addon file, else the game's own paks through `readGame(path)` (a vanilla prefab is not in addons/), else null.
function prefabText(ref, root, readGame) {
  const f = prefabFile(ref, root);
  if (f) return readFileSync(f, "utf8");
  const m = REF_RE.exec(ref ?? "");
  if (!m || !readGame) return null;
  try {
    return readGame(m[2]) ?? null;
  } catch {
    return null;
  }
}

// The GUID a component has in a prefab or in the prefab it inherits (addons, then the game's paks), so a layer override reuses
// it (a second component of the same class would shadow it).
export function componentGuid(ref, cls, root, hops = 6, readGame = null) {
  let cur = ref;
  for (let i = 0; i < hops && cur; i++) {
    const text = prefabText(cur, root, readGame);
    if (text == null) return null;
    const hit = new RegExp(`^\\s*${cls}\\s+"\\{([0-9A-Fa-f]{16})\\}"`, "m").exec(text);
    if (hit) return hit[1].toUpperCase();
    const up = /^\s*\w+\s*:\s*"(\{[0-9A-Fa-f]{16}\}[^"]+)"/m.exec(text);
    cur = up ? up[1] : null;
  }
  return null;
}

// ------------------------------------------------------------------ the kit's source (nested blocks, new components)

const pathOfRef = (p) => String(p ?? "").replace(REF_RE, "$2").toLowerCase();
const compClass = (c) => c.head.split(/\s+/)[0];
const walkEntities = (e) => [e, ...e.kids.flatMap(walkEntities)];
const guidOfHead = (comp) => /"\{([0-9A-Fa-f]{16})\}"/.exec(comp.head)?.[1]?.toUpperCase() ?? null;
const scalarSet = (comp) => {
  const set = {};
  for (const p of comp.props) {
    const i = p.search(/\s/);
    if (i > 0) set[p.slice(0, i)] = p.slice(i + 1).trim();
  }
  return set;
};

// Every entity of the files a kit was lifted from (its `source.file` and `source.also`), nested ones included.
function sourceEntities(kit, root, cache) {
  if (cache.has(kit)) return cache.get(kit);
  const out = [];
  for (const rel of [kit.source?.file, ...(kit.source?.also ?? [])]) {
    const f = rel ? join(root, rel) : null;
    if (!f || !existsSync(f) || !/\.(et|layer)$/.test(f)) continue;
    try {
      out.push(...entitiesOf(parseBlocks(readFileSync(f, "utf8"))).flatMap(walkEntities));
    } catch {
      // an unreadable source file is the same as no source: the caller warns
    }
  }
  cache.set(kit, out);
  return out;
}

// The component of a kit row in the kit's source: its own GUID and its nested blocks (Materials, shop items, stock lines),
// which the kit names but does not copy. Among the source entities of the row's prefab, the one whose scalar overrides and
// block names agree with the kit wins; a looser match still gives its GUID but no blocks.
export function sourceComponent(kit, row, c, root, cache = new Map()) {
  const want = pathOfRef(row.prefab);
  const names = JSON.stringify(c.blocks ?? []);
  const cands = [];
  for (const e of sourceEntities(kit, root, cache)) {
    if (pathOfRef(e.prefab) !== want) continue;
    for (const comp of e.comps) {
      if (compClass(comp) !== c.component) continue;
      const set = scalarSet(comp);
      const setOk = Object.entries(c.set ?? {}).every(([k, v]) => set[k] === v);
      const blocksOk = JSON.stringify(comp.kids.map(compClass)) === names;
      cands.push({ comp, exact: setOk && blocksOk, score: (setOk ? 2 : 0) + (blocksOk ? 1 : 0) });
    }
  }
  cands.sort((x, y) => y.score - x.score);
  const best = cands[0];
  if (!best) return null;
  return { guid: guidOfHead(best.comp), kids: best.exact ? best.comp.kids : null };
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

// A parsed block back to text: its properties, then its nested blocks, one space deeper each.
function renderBlock(b, indent) {
  const pad = " ".repeat(indent);
  const out = [`${pad}${b.head ? `${b.head} {` : "{"}`];
  for (const p of b.props) out.push(`${pad} ${p}`);
  for (const k of b.kids) out.push(...renderBlock(k, indent + 1));
  out.push(`${pad}}`);
  return out;
}

function entityText({ cls = "GenericEntity", name, ref, comps = [], coords, yaw }) {
  const lines = [`${cls} ${name}${ref ? ` : "${ref}"` : ""} {`];
  if (comps.length) {
    lines.push(" components {");
    for (const c of comps) {
      lines.push(`  ${c.cls} "{${c.guid}}" {`);
      for (const [k, v] of Object.entries(c.set)) lines.push(`   ${k} ${v}`);
      for (const b of c.blocks ?? []) lines.push(...renderBlock(b, 3));
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
  // `--check` writes nothing, so it judges a slot whatever claims it (the draft that holds the slot is what it checks).
  const claimReads = opts.skipClaims ? [] : [mainTownClaims(exec, { fetch: opts.fetch !== false }), openPrTownClaims(exec, { branch: opts.branch ?? null }), worktreeTownClaims(exec, root)];
  for (const r of claimReads) {
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
    destruction = null; // decided below: hooked through the place marker, or needs-hook without one
  } else {
    if (!opts.buildingPrefab || !REF_RE.test(opts.buildingPrefab)) return (errors.push(`slot ${slotId} is a free plot: the purpose-built building must be resolved first (asset_search) and passed as --building-prefab {GUID}path`), { ...fail(), notes });
    const guid = opts.destructionGuid ?? componentGuid(opts.buildingPrefab, "SCR_DestructionMultiPhaseComponent", root, 6, ctx.readGame);
    if (!guid) return (errors.push(`cannot find the SCR_DestructionMultiPhaseComponent guid of ${opts.buildingPrefab}; pass --destruction-guid {GUID}`), { ...fail(), notes });
    host = { x: plotE.x, y: plotE.ground ? r3((plotE.ground.min + plotE.ground.max) / 2) : plotE.y ?? 0, z: plotE.z, yaw: 0, entrances: [], footprint: null, prefab: opts.buildingPrefab };
    destruction = { mode: "layer-override", component: "SCR_DestructionMultiPhaseComponent" };
    out.push({ name: `Fit_${ident(place)}_${ident(slotId)}_Building`, ref: opts.buildingPrefab, coords: [host.x, host.y, host.z], yaw: 0, comps: [{ cls: "SCR_DestructionMultiPhaseComponent", guid: guid.replace(/[{}]/g, "").toUpperCase(), set: { Enabled: "0" } }] });
  }

  const anchor = anchorFor(host, kit, { sameShell: Boolean(opts.sameShell) });
  const rows = applyKit(kit, anchor.x, anchor.z, anchor.yaw, anchor.base ?? anchor.y);

  // The slot's markers: the first kind carries `<place>.<slot>`; each further kind is a marker of its own.
  const kinds = slot.places?.length ? slot.places : [];
  const markers = kinds.map((kind, i) => ({
    name: `Fit_${ident(place)}_${ident(slotId)}_Place${i ? `_${ident(kind)}` : ""}`,
    comps: [{ cls: "LF_PlaceComponent", guid: guidOf(place, slotId, kind), set: { m_sKind: `"${kind}"`, m_sId: `"${place}.${slotId}"` } }],
    coords: anchor.marker,
    yaw: 0,
  }));
  if (!markers.length) warnings.push(`slot ${slotId} lists no LF_Places kind: the layer carries no place marker`);
  // A vanilla building stands in the terrain, so a layer cannot switch its destruction off. The core hook does it
  // (LF_FittedBuildingComponent on the place marker inside the building, D15); without a marker it stays open.
  if (!destruction) {
    if (markers.length) {
      markers[0].comps.push({ cls: "LF_FittedBuildingComponent", guid: guidOf(place, slotId, "fitted-building"), set: {} });
      destruction = { mode: "hooked", component: "LF_FittedBuildingComponent", marker: markers[0].name };
    } else {
      destruction = { mode: "needs-hook", reason: "a vanilla building stands in the terrain and the slot has no place marker to carry LF_FittedBuildingComponent (D15)" };
      warnings.push(`slot ${slotId}: no place marker, so the vanilla building stays destructible (needs-hook)`);
    }
  }

  const sourceCache = new Map();
  const srcRows = new Map(kit.rows.map((r) => [r.id, r]));
  const rowEnts = rows.map((r) => {
    const src = srcRows.get(r.id);
    const comps = [];
    for (const c of src.config ?? []) {
      const hasSet = Object.keys(c.set ?? {}).length > 0;
      const named = (c.blocks ?? []).length > 0;
      if (!hasSet && !named) continue;
      // Nested blocks, and a component the prefabs do not have, come from the kit's source. An inherited GUID always wins:
      // the layer overrides that component, it never adds a second one.
      const fromSrc = sourceComponent(kit, src, c, root, sourceCache);
      const guid = componentGuid(r.prefab, c.component, root, 6, ctx.readGame) ?? fromSrc?.guid ?? null;
      if (!guid) {
        warnings.push(`row ${r.id}: no guid for ${c.component} in ${r.prefab}, its parents or the kit's source; its overrides were not written`);
        continue;
      }
      let blocks = [];
      if (named) {
        if (fromSrc?.kids) blocks = fromSrc.kids;
        else warnings.push(`row ${r.id}: ${c.component} names the blocks ${c.blocks.join(", ")} but the kit's source holds no matching component; they were not written (re-extract the kit)`);
      }
      comps.push({ cls: c.component, guid, set: c.set ?? {}, blocks });
    }
    return { id: r.id, cls: src.class && /^\w+$/.test(src.class) ? src.class : "GenericEntity", name: `Fit_${ident(place)}_${ident(slotId)}_${ident(r.id)}`, ref: r.prefab, comps, coords: r.coords, yaw: r.yaw };
  });

  const layerName = layerNameOf(place, slotId);
  const layerText = renderLayer({ markers, out, rowEnts });
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
  return { ok: true, errors, warnings, notes, layerName, layerText, claimFile: claimFileOf(place, slotId), claim, anchor, rows, host, kit, ents: { markers, out, rowEnts } };
}

const renderLayer = ({ markers, out, rowEnts }) => `${[...markers, ...out, ...rowEnts].map(entityText).join("\n")}\n`;

// The host prefab the dossier (or the --building-prefab of a plot) gives, before planFit runs; null when the slot has none.
function hostRefOf(opts) {
  const entry = opts.mapping?.slots?.[opts.slot];
  if (!entry) return null;
  if (entry.building) return opts.dossier?.buildings?.find((b) => b.id === entry.building)?.prefab ?? null;
  return opts.buildingPrefab ?? null;
}

// planFit, then every row seated on the host (tools/town/seat.mjs) and the layer text rewritten from the seated rows.
// ctx.shell is the reader (seat.mjs shellReader, or a selftest fixture); without one this is planFit. A row that still cuts
// the host shell after the nudge fails the plan unless opts.allowClip.
export async function planFitSeated(opts, ctx = {}) {
  const reader = ctx.shell;
  if (!reader) return planFit(opts, ctx);
  const tmpl = opts.template?.slots?.find((s) => s.id === opts.slot);
  const kit = tmpl ? opts.kits?.[tmpl.kit] : null;
  const ref = hostRefOf(opts);
  const same = kit && ref ? await sameShell(kit, ref, reader) : false;
  const plan = planFit({ ...opts, sameShell: same }, ctx);
  if (!plan.ok) return plan;
  plan.anchor.sameShell = same;
  if (!plan.host?.prefab) {
    plan.warnings.push("the host has no prefab in the dossier: rows were not seated on it");
    return plan;
  }
  const seat = await seatRows(plan.rows, plan.kit.rows, plan.host, { reader, allowClip: Boolean(opts.allowClip) });
  plan.seat = seat;
  if (!seat.host) {
    plan.warnings.push(seat.note);
    return plan;
  }
  const byId = new Map(plan.rows.map((r) => [r.id, r]));
  for (const e of plan.ents.rowEnts) if (byId.has(e.id)) e.coords = byId.get(e.id).coords;
  const skip = new Set(seat.skipped.map((r) => r.id));
  plan.ents.rowEnts = plan.ents.rowEnts.filter((e) => !skip.has(e.id));
  for (const r of seat.skipped) plan.warnings.push(`row ${r.id} ${r.skip}`);
  for (const r of seat.furniture) plan.warnings.push(`row ${r.id} ${r.furniture}`);
  plan.claim.rows = plan.ents.rowEnts.length;
  plan.layerText = renderLayer(plan.ents);
  for (const r of seat.unseated) plan.warnings.push(`row ${r.id}: ${r.notes.filter((n) => /^no /.test(n)).join("; ")}`);
  const hostPath = String(plan.host.prefab).replace(/^\{[0-9A-Fa-f]{16}\}/, "");
  for (const c of seat.clips) {
    if (opts.allowClip) plan.warnings.push(`row ${c.id} ${c.clip.text} of the host ${hostPath} (written: --allow-clip)`);
    else plan.errors.push(`row ${c.id} ${c.clip.text} of the host ${hostPath}; move it in the kit, or pass --allow-clip`);
  }
  if (plan.errors.length) plan.ok = false;
  return plan;
}

// The coords of the Fit_ rows in a layer already on disk, by kit row id (for --check).
export function layerCoords(text, place, slotId) {
  const pre = `Fit_${ident(place)}_${ident(slotId)}_`;
  const out = new Map();
  for (const m of String(text).matchAll(/^\w+ (Fit_\w+)(?: : "[^"]+")? \{[\s\S]*?\n coords ([^\n]+)/gm)) {
    if (m[1].startsWith(pre)) out.set(m[1].slice(pre.length), m[2].trim().split(/\s+/).map(Number));
  }
  return out;
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

const USAGE = "usage: tools\\cli town fit <place> <slot> [--dry] [--check] [--allow-clip] [--no-fetch] [--no-audit] [--no-look] [--building-prefab {GUID}path] [--destruction-guid {GUID}]";
const readJson = (f, what) => {
  if (!existsSync(f)) throw new Error(`no ${what}: ${f} does not exist`);
  return JSON.parse(readFileSync(f, "utf8"));
};

// The game's own prefab text, for the inherited component GUIDs of a vanilla prefab; null when no game install is readable.
async function gameReader() {
  try {
    const vfs = await (await import("../lib/game-prefab.mjs")).gameVfs();
    return (rel) => (vfs.exists(rel) ? vfs.readTextFile(rel) : null);
  } catch {
    return null;
  }
}

// The verify steps after a write: [name, cli args]. The audit is scoped to the slot's own layer (the playtest town's other
// layers are not this slot's to judge); `look place:` opens the island at the slot (a plain x,z opens DebugWorld, where an
// island slot is sky). Nothing is pinned to a machine: the pool picks one.
export function verifySteps(place, slotId, layerName, flags = {}) {
  const steps = [];
  if (!flags["no-audit"]) steps.push(["audit", ["audit", "placement", "--strict", "--world", "MainWorld", "--layer", layerName, "--hard-layer", layerName]]);
  if (!flags["no-look"]) {
    steps.push(["look", ["look", `place:${place}/${slotId}`]]);
    steps.push(["look-interior", ["look", `place:${place}/${slotId}`, "--interior", "auto"]]);
  }
  return steps;
}

// `town fit --check`: per-row corrections, against the slot's layer on disk when there is one (a draft branch), else
// against the kit's unseated positions. Writes nothing. Exit 1 on a clip or when the host cannot be read.
function checkReport(plan, { root, place, slotId, layerFile }) {
  const shown = relative(root, layerFile).replaceAll("\\", "/");
  if (!plan.seat?.host) {
    for (const e of plan.errors ?? []) console.log(`REFUSE  ${e}`);
    for (const w of plan.warnings ?? []) console.log(`warn  ${w}`);
    console.log(`SUMMARY town-fit check fail ${place}.${slotId}: ${plan.seat ? "the host's model could not be read" : `${(plan.errors ?? []).length} problem(s)`}; nothing written`);
    return 1;
  }
  const written = existsSync(layerFile) ? layerCoords(readFileSync(layerFile, "utf8"), place, slotId) : null;
  // the layer names a row by ident(id) (dashes become underscores); a row the layer lacks is judged against its unseated spot
  const onDisk = written ? new Map(plan.seat.rows.map((r) => [r.id, written.get(ident(r.id)) ?? r.from])) : null;
  const a = plan.anchor;
  console.log(`anchor ${a.x} ${a.y} ${a.z} yaw ${a.yaw} (${a.note}); host model ${plan.seat.host.mesh}`);
  console.log(onDisk ? `corrections against ${shown}:` : "no layer on disk: corrections against the kit's unseated positions:");
  for (const l of seatReport(plan.seat, { against: onDisk })) console.log(l);
  for (const w of plan.warnings ?? []) console.log(`warn  ${w}`);
  const moved = plan.seat.rows.filter((r) => {
    const was = onDisk?.get(r.id) ?? r.from;
    return Math.hypot(...r.to.map((v, i) => v - was[i])) > 0.02;
  }).length;
  const clips = plan.seat.clips.length;
  console.log(`SUMMARY town-fit check ${clips ? "fail" : "ok"} ${place}.${slotId}: ${plan.seat.rows.length} row(s), ${moved} to correct, ${clips} clip(s)${onDisk ? "" : " (no layer on disk)"}; nothing written`);
  return clips ? 1 : 0;
}

export async function main(args, { root = ROOT, run, shell: ctxShell = null } = {}) {
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
  if (!flags.dry && !flags.check && !flags["out-dir"] && !new RegExp(`^ws/town-draft-${place}-${slotId}$`).test(branch ?? "")) {
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
    const shell = ctxShell ?? shellReader(root);
    const fitOpts = { place, slot: slotId, dossier, mapping, template, kits, buildingPrefab: flags["building-prefab"], destructionGuid: flags["destruction-guid"], branch, fetch: !flags["no-fetch"] && !flags.check, allowClip: Boolean(flags["allow-clip"]), skipClaims: Boolean(flags.check) };
    plan = await planFitSeated(fitOpts, { root, run, readGame: await gameReader(), shell });
  } catch (e) {
    console.log(`REFUSE  ${e.message}`);
    return 1;
  }
  if (flags.check) return checkReport(plan, { root, place, slotId, layerFile: join(resolve(root, flags["out-dir"] ?? LAYER_DIR), layerNameOf(place, slotId)) });
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
  let ok = true;
  for (const [name, a] of verifySteps(place, slotId, plan.layerName, flags)) {
    const r = spawnSync(cli, a, { cwd: root, encoding: "utf8", shell: process.platform === "win32", timeout: 600000 });
    steps.push(`${name} ${r.status === 0 ? "ok" : "fail"}`);
    ok = r.status === 0 && ok;
  }
  console.log(`SUMMARY town-fit ${ok ? "ok" : "fail"} ${place}.${slotId}: ${shown(layerFile)}, ${shown(claimFile)}${steps.length ? `; ${steps.join(", ")}` : "; audit and look skipped"}`);
  return ok ? 0 : 1;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) process.exit(await main(process.argv.slice(2), { root: ROOT }));
