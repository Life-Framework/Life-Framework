// tools/town/fit.mjs - `tools\cli town fit <place> <slot>`: apply a slot's kit to the building the approved mapping names
// (docs/architecture/everon-world.md 3.3 and 3.4, card #2205).
//
//   town fit <place> <slot> [--dry] [--check] [--refit] [--allow-clip] [--no-fetch] [--no-audit] [--no-look] [--building-prefab {GUID}path]
//            [--destruction-guid {GUID}] [--dossier f] [--mapping f] [--template-file f] [--kits-dir d] [--out-dir d]
//
// Reads  docs/everon/places/<place>/dossier.json and mapping.json, tools/town/templates/<kind>.json, tools/town/kits/<kit>.json
// Writes addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_<place>_<slot>.layer   (pack content, never a core layer)
//        tools/town/claims/<place>-<slot>.json
// Then runs `audit placement --strict --world MainWorld --layer <file>` and takes the `look` pictures (--no-audit, --no-look skip).
// Open-ground kit rows (`"mount": "terrain"`) rest on terrain heights kept in tools/town/terrain/<place>-<slot>.json;
// `--terrain` probes them on the island's test server first (tools/town/terrain.mjs), a plain fit and `--check` read the file.
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
import { onWater } from "../scout/analyse.mjs";
import { applyKit, checkKit, entitiesOf, parseBlocks, toWorld } from "./kit.mjs";
import { checkMapping, loadTemplate } from "./map.mjs";
import { mountOf, sameShell, seatReport, seatRows, shellReader } from "./seat.mjs";
import { probeTerrain, readPins, terrainFromPins, writePins } from "./terrain.mjs";
import { frameOf, gameVfsSync } from "../lib/height-frame.mjs";

export const help = "fit <place> <slot> [--dry] [--check] [--refit] [--allow-clip] [--no-audit] [--no-look]   apply the slot's kit into its mapped building, rows seated on the host's floors and walls: Place_<place>_<slot>.layer (marker + kit rows) and a claim";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const LAYER_DIR = "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers";
export const CLAIMS_DIR = "tools/town/claims";
const SLUG_RX = /^[a-z0-9][a-z0-9-]*$/;
// The kit a slot takes: the template's, unless the mapping entry names another (`"kit": "village-bar-1i04"`): the same slot fitted into
// a building the template's kit was not cut for (the Levie bar's house is not Chotain's, #4444).
export const kitNameOf = (templateSlot, mappingEntry) => (typeof mappingEntry?.kit === "string" && mappingEntry.kit ? mappingEntry.kit : templateSlot?.kit);
const REF_RE = /^\{([0-9A-Fa-f]{16})\}(.+)$/;
const FLAGS = new Set(["dry", "check", "refit", "allow-clip", "no-fetch", "no-audit", "no-look", "terrain", "building-prefab", "destruction-guid", "dossier", "mapping", "template-file", "kits-dir", "out-dir", "help"]);
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
// The vanilla props a kit hides inside its building (card #3882/#3924): kit.stamp.clear (what `town author` writes), or a top-level
// kit.clear for a kit that is fitted rather than stamped. An entry is a resource name or { prefab }.
// An entry with a `box` hides that prefab only inside a small box in the kit's frame (clearBoxesOf), never across the whole building.
const clearEntries = (kit) => [...(kit?.stamp?.clear ?? []), ...(kit?.clear ?? [])];
export const clearListOf = (kit) => [...new Set(clearEntries(kit).filter((c) => !c?.box).map((c) => (typeof c === "string" ? c : c?.prefab)).filter(Boolean))].sort();

// The boxed entries: { prefab, box: { offset: [x, z], half: [hx, hz], height? } }, grouped by box, offset in the kit's frame like a row's x and z. One
// vanilla door of a prefab the building uses many times (the bank's interior door behind the vault door) is hidden this way.
export const clearBoxesOf = (kit) => {
  const groups = new Map();
  for (const c of clearEntries(kit)) {
    if (!c?.box || !c.prefab) continue;
    const offset = [Number(c.box.offset?.[0]) || 0, Number(c.box.offset?.[1]) || 0];
    const half = [Number(c.box.half?.[0]) || 0, Number(c.box.half?.[1]) || 0];
    const height = Number(c.box.height) || 4;
    const key = [...offset, ...half, height].join("|");
    if (!groups.has(key)) groups.set(key, { prefabs: [], offset, half, height });
    if (!groups.get(key).prefabs.includes(c.prefab)) groups.get(key).prefabs.push(c.prefab);
  }
  return [...groups.values()].map((g) => ({ ...g, prefabs: g.prefabs.sort() })).sort((p, q) => `${p.offset}|${p.half}|${p.height}`.localeCompare(`${q.offset}|${q.half}|${q.height}`));
};

// One box-mode carrier per distinct box (the entries that share one box list their prefabs together): no building, a box (half size on x and z) centred on the carrier and turned by its yaw, standing
// on the terrain under it. The centre is the entry's kit-frame offset moved to the world by the anchor, the yaw the anchor's.
export function clearBoxCarriers({ place, slot, anchor, boxes }) {
  return boxes.map((bx, i) => {
    const [wx, wz] = toWorld(bx.offset[0], bx.offset[1], anchor.yaw);
    const list = ["{", ...bx.prefabs.map((p) => `    "${p}"`), "   }"].join("\n");
    return {
      name: `Fit_${ident(place)}_${ident(slot)}_clear_box${i + 1}`,
      comps: [{ cls: "LF_BaseWorldClearComponent", guid: guidOf(place, slot, "base-world-clear-box", i + 1), set: { m_vBoxHalfM: `${bx.half[0]} 0 ${bx.half[1]}`, m_fBoxHeightM: String(bx.height), m_aPrefabs: list } }],
      coords: [r3(anchor.x + wx), anchor.y, r3(anchor.z + wz)],
      yaw: anchor.yaw,
    };
  });
}

// The base-world clear carrier of one stamped copy: a GenericEntity at the building's x and z with LF_BaseWorldClearComponent.
// It names the building prefab and the clear list; the game finds the building by prefab within 3 m on x and z and takes the
// height from it, so the row's height is only a placeholder (the building origin), never a measured value.
export function clearCarrier({ place, slot, host, clear }) {
  if (!clear.length || !host?.prefab) return null;
  const list = ["{", ...clear.map((p) => `    "${p}"`), "   }"].join("\n");
  return {
    name: `Fit_${ident(place)}_${ident(slot)}_clear`,
    comps: [{ cls: "LF_BaseWorldClearComponent", guid: guidOf(place, slot, "base-world-clear"), set: { m_sBuilding: `"${host.prefab}"`, m_aPrefabs: list } }],
    coords: [host.x, host.y, host.z],
    yaw: 0,
  };
}

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

// A further marker kind may stand away from the building: `anchoring.markerOffsets["<Kind>"] = [dx, dz]` in the kit frame
// (the orchard's waypoint kind stands on its first apple tree, not on the barn). The first kind never moves: it carries
// LF_FittedBuildingComponent, which needs the marker inside the building.
export function markerCoords(kit, kind, index, anchor) {
  const off = index > 0 ? kit.anchoring?.markerOffsets?.[kind] : null;
  if (!Array.isArray(off) || off.length !== 2) return anchor.marker;
  const [wx, wz] = toWorld(off[0], off[1], anchor.yaw);
  return [r3(anchor.marker[0] + wx), anchor.marker[1], r3(anchor.marker[2] + wz)];
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
      // A kit row has no prefab when its entities are bare ones, so every one of them matches; the row's own source entity wins.
      const own = e.name && e.name === row.name ? 4 : 0;
      cands.push({ comp, exact: setOk && blocksOk, score: own + (setOk ? 2 : 0) + (blocksOk ? 1 : 0) });
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

// A claim without where it was read from: what two copies of one claim file share.
export const claimBody = (c) => JSON.stringify(Object.fromEntries(Object.entries(c).filter(([k]) => k !== "where" && k !== "file").sort(([a], [b]) => (a < b ? -1 : 1))));

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

// Pure: an integration worktree (ws/island-integration, #3076) merges other drafts' claim files to try them together and is never
// pushed, so its copies are not claims of its own and must not block the drafts they came from.
export const isIntegrationBranch = (branch) => /^ws\/island-integration(-[a-z0-9-]+)?$/.test(String(branch ?? ""));

// Pure: whether a claim belongs to the draft this branch is stacked on. A follow-up branch ws/town-draft-<place>-<slot>-<word> builds on
// the open draft ws/town-draft-<place>-<slot> and changes that slot's layer on top of it, so the draft's own claim (its open PR, its
// worktree) is not a rival. Another slot's claim, or a PR of any other branch, still blocks.
export function claimOfBase(c, { branch, place, slot }) {
  const base = `ws/town-draft-${place}-${slot}`;
  if (!branch || !String(branch).startsWith(`${base}-`)) return false;
  if (c.place !== place || c.slot !== slot) return false;
  return new RegExp(`^open PR #[0-9]+ \\(${base}\\)$`).test(c.where) || c.where === `worktree Life-Framework-ws-town-draft-${place}-${slot}`;
}

// The blob ids claim file `file` has had on `ref`: a sibling worktree whose copy is one of these is cut from some main, not a rival.
export function mainClaimBlobs(exec, file, ref = "origin/main") {
  const log = exec("git", ["log", ref, "--format=%H", "--", file]);
  if (log.status !== 0) return new Set();
  const blobs = new Set();
  for (const sha of String(log.stdout).split(/\r?\n/).filter(Boolean)) {
    const b = exec("git", ["rev-parse", `${sha}:${file}`]);
    if (b.status === 0) blobs.add(String(b.stdout).trim());
  }
  return blobs;
}

// Uncommitted claim files in sibling local worktrees: no network, read from `git worktree list --porcelain`.
export function worktreeTownClaims(exec, root) {
  const ls = exec("git", ["worktree", "list", "--porcelain"]);
  if (ls.status !== 0) return { claims: [], note: "could not list the worktrees; claims in sibling worktrees were not checked" };
  const claims = [];
  for (const block of String(ls.stdout).split(/\r?\n\r?\n/)) {
    const path = /^worktree (.+)$/m.exec(block)?.[1];
    if (!path || resolve(path) === resolve(root)) continue;
    if (isIntegrationBranch(/^branch refs\/heads\/(.+)$/m.exec(block)?.[1])) continue;
    const dir = join(path, CLAIMS_DIR);
    if (!existsSync(dir)) continue;
    for (const f of readdirSync(dir).filter((n) => n.endsWith(".json"))) {
      const c = asClaim(readFileSync(join(dir, f), "utf8"), { file: `${CLAIMS_DIR}/${f}`, where: `worktree ${basename(path)}` });
      if (!c) continue;
      // Where that copy lives, to hash it later: a worktree cut from an older main holds an older main version (#3566).
      Object.defineProperty(c, "path", { value: join(dir, f), enumerable: false });
      claims.push(c);
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

function entityText({ cls = "GenericEntity", name, ref, comps = [], coords, yaw, pitch = 0, roll = 0, scale = 1 }) {
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
  if (pitch) lines.push(` angleX ${pitch}`);
  if (yaw) lines.push(` angleY ${yaw}`);
  if (roll) lines.push(` angleZ ${roll}`);
  // A kit row's `scale` (a gravel pile at 0.5) is the layer's own `scale` line, which the audit and the game read; fit dropped it before #4778.
  if (Number.isFinite(scale) && scale > 0 && scale !== 1) lines.push(` scale ${scale}`);
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
  if (slot.host === "outposts") return (errors.push(`slot ${slotId} stands in other places' buildings (an outposts slot): run tools\\cli town drops ${place} ${slotId}`), fail());
  const entry = mapping.slots?.[slotId];
  if (!entry) return (errors.push(`slot ${slotId} is not in the mapping`), fail());
  if (entry.none) return (errors.push(`slot ${slotId} is mapped to none${entry.reason ? ` (${entry.reason})` : ""}: nothing to fit`), fail());
  // A slot whose building is in ANOTHER place's survey (`dossier` on its entry): `dossier` here is that survey, the place's own
  // is `mappingDossier`, and the building is claimed under "<survey slug>/<id>" so it never meets this place's own b14.
  const buildingKey = entry.dossier ? `${entry.dossier}/${entry.building}` : entry.building;
  for (const p of checkMapping(mapping, opts.mappingDossier ?? dossier, template, entry.dossier ? { [entry.dossier]: dossier } : null)) if (!/^slot /.test(p) || p.startsWith(`slot ${slotId}:`)) errors.push(`mapping: ${p}`);
  if (errors.length) return fail();

  const kitName = kitNameOf(slot, entry);
  const kit = opts.kits?.[kitName];
  if (!kit) return (errors.push(`kit '${kitName}' (slot ${slotId}) was not found`), fail());
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
  // `--refit`: this slot's own claim, already committed on origin/main (every worktree carries it), does not block its re-fit.
  // The same slot held by an open PR or a sibling worktree, and any other slot's claim on the building, still refuse.
  for (let i = taken.length - 1; i >= 0; i--) if (claimOfBase(taken[i], { branch: opts.branch, place, slot: slotId })) taken.splice(i, 1);
  // A sibling worktree that has not touched the claim holds byte-for-byte what main holds (every worktree is cut from main), so under
  // `--refit` that copy is main's own claim and not a rival either (all sixteen worktrees blocked a re-fit of the supermarket, #3516).
  const mainOwn = taken.find((c) => c.place === place && c.slot === slotId && c.where === "origin/main");
  // A worktree cut from an older main carries an older version of the file (police: 56 rows on main's earlier head, 57 now): its blob is
  // in main's history for that file, so it is main's too (#3566).
  const historyBlobs = new Map();
  const inMainHistory = (c) => {
    if (!c.path || !/^worktree /.test(c.where)) return false;
    const h = exec("git", ["hash-object", c.path]);
    if (h.status !== 0) return false;
    if (!historyBlobs.has(c.file)) historyBlobs.set(c.file, mainClaimBlobs(exec, c.file));
    return historyBlobs.get(c.file).has(String(h.stdout).trim());
  };
  const sameAsMain = (c) => /^worktree /.test(c.where) && ((Boolean(mainOwn) && claimBody(c) === claimBody(mainOwn)) || inMainHistory(c));
  const blocking = opts.refit ? taken.filter((c) => !(c.place === place && c.slot === slotId && (c.where === "origin/main" || sameAsMain(c)))) : taken;
  errors.push(...conflicts(blocking, { place, slot: slotId, building: buildingKey, plot: entry.plot, shared: entry.shared }));
  if (errors.length) return { ...fail(), notes };

  // The building: a mapped one stands where the dossier says; a plot gets the purpose-built building first (D9); a hostless
  // plot (a lake, a pond, a field: `"hostless": true` on a plot entry, #3666) has no building at all, the kit stands on the open ground.
  let host;
  const out = [];
  let destruction;
  if (entry.hostless) {
    if (!plotE) return (errors.push(`slot ${slotId}: a hostless slot needs a plot of the dossier (${entry.plot ?? "none named"})`), { ...fail(), notes });
    const wet = onWater(dossier.water?.bodies, plotE.x, plotE.z);
    if (wet) return (errors.push(`slot ${slotId}: plot ${plotE.id} lies on the water ${wet.id} (${wet.name}); anchor it on a dry plot or a shore point`), { ...fail(), notes });
    host = { x: plotE.x, y: plotE.ground ? r3((plotE.ground.min + plotE.ground.max) / 2) : plotE.y ?? 0, z: plotE.z, yaw: 0, entrances: [], footprint: null, prefab: null };
    destruction = { mode: "none", reason: "hostless plot: no building stands in the slot" };
  } else if (building) {
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
    coords: markerCoords(kit, kind, i, anchor),
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

  const carrier = clearCarrier({ place, slot: slotId, host, clear: clearListOf(kit) });
  if (carrier) out.push(carrier);
  out.push(...clearBoxCarriers({ place, slot: slotId, anchor, boxes: clearBoxesOf(kit) }));

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
      const guid = componentGuid(r.prefab, c.component, root, 6, ctx.readGame) ?? (/^[0-9A-Fa-f]{16}$/.test(c.guid ?? "") ? c.guid.toUpperCase() : null) ?? fromSrc?.guid ?? null;
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
    return { id: r.id, cls: src.class && /^\w+$/.test(src.class) ? src.class : "GenericEntity", name: `Fit_${ident(place)}_${ident(slotId)}_${ident(r.id)}`, ref: r.prefab, comps, coords: r.coords, yaw: r.yaw, pitch: Number(src.pitch) || 0, roll: Number(src.roll) || 0, scale: Number(src.scale) > 0 ? Number(src.scale) : 1 };
  });

  const layerName = layerNameOf(place, slotId);
  const layerText = renderLayer({ markers, out, rowEnts });
  const claim = {
    schema: 1,
    place,
    slot: slotId,
    ...(entry.building ? { building: buildingKey } : { plot: entry.plot, ...(entry.hostless ? { hostless: true } : {}) }),
    ...(entry.shared ? { shared: true } : {}),
    kit: kitName,
    layer: `${LAYER_DIR}/${layerName}`,
    kinds,
    rows: rows.length,
    destruction,
    date: (ctx.now ? ctx.now() : new Date()).toISOString().slice(0, 10),
  };
  const plan = { ok: true, errors, warnings, notes, layerName, layerText, claimFile: claimFileOf(place, slotId), claim, anchor, rows, host, kit, ents: { markers, out, rowEnts } };
  renderFramed(plan, ctx);
  return plan;
}

const renderLayer = ({ markers, out, rowEnts }) => `${[...markers, ...out, ...rowEnts].map(entityText).join("\n")}\n`;

// The rows as the layer must hold them. Every row is planned in world heights (a host floor, the probed ground), or, for a
// "ground" row (seat.mjs), as its height over the terrain. The engine reads a row's y in the frame its prefab's flags set
// (tools/lib/height-frame.mjs: RELATIVE_Y means the height over the terrain, else a world height), so each row is turned
// into that frame here, the one place the layer text is made. A conversion that needs the ground height under the row takes
// it from the slot's probe pins (`--terrain`); without one the row is pending and the fit refuses.
// Returns { rowEnts, pending: [id], points: [[x, z]], frames: { id: frame } }.
export function framedRows(rowEnts, { root = ROOT, terrain = null, overGround = new Set(), frameOfRef = null } = {}) {
  const fr = frameOfRef ?? ((ref) => frameOf(root, gameVfsSync(), ref));
  const pending = [];
  const points = [];
  const frames = {};
  const rows = rowEnts.map((e) => {
    const frame = fr(e.ref);
    frames[e.id] = frame;
    const terrainFrame = frame === "terrain";
    if (terrainFrame === overGround.has(e.id) || !e.coords) return e; // already in its frame
    const [x, y, z] = e.coords;
    points.push([x, z]);
    const t = terrain?.at(x, z);
    if (!Number.isFinite(t)) {
      pending.push(e.id);
      return e;
    }
    return { ...e, coords: [x, r3(terrainFrame ? y - t : y + t), z] };
  });
  return { rowEnts: rows, pending, points, frames };
}

// Render the plan's layer with every row in its prefab's frame; the pending rows and the ground points they need go on the plan.
function renderFramed(plan, ctx, overGround = new Set()) {
  const f = framedRows(plan.ents.rowEnts, { root: ctx.root ?? ROOT, terrain: ctx.terrain ?? null, overGround, frameOfRef: ctx.frameOf ?? null });
  plan.framePending = f.pending;
  plan.framePoints = f.points;
  plan.frames = f.frames;
  plan.layerText = renderLayer({ ...plan.ents, rowEnts: f.rowEnts });
}

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
  const kit = tmpl ? opts.kits?.[kitNameOf(tmpl, opts.mapping?.slots?.[opts.slot])] : null;
  const ref = hostRefOf(opts);
  // opts.sameShell (`town stamp`: the kit was authored around this very prefab) is taken as given; `town fit` asks the models.
  const same = opts.sameShell != null ? Boolean(opts.sameShell) : kit && ref ? await sameShell(kit, ref, reader) : false;
  const plan = planFit({ ...opts, sameShell: same }, ctx);
  if (!plan.ok) return plan;
  plan.anchor.sameShell = same;
  if (opts.mapping?.slots?.[opts.slot]?.hostless) {
    // No host shell to seat on: an open-ground row rests on the ground the server probed under its point (`--terrain`), else it keeps the plot's height.
    plan.terrainPoints = plan.rows.filter((r) => plan.kit.rows.find((q) => q.id === r.id)?.mount === "terrain").map((r) => [r.coords[0], r.coords[2]]);
    for (const r of plan.rows) {
      const src = plan.kit.rows.find((q) => q.id === r.id);
      const h = src?.mount === "terrain" ? ctx.terrain?.at(r.coords[0], r.coords[2]) : null;
      // A kit row's `lift` raises it over the probed ground, as on a host's floor (#3685): a trader on its counter top.
      if (Number.isFinite(h)) r.coords[1] = r3(h + (Number.isFinite(src?.lift) ? src.lift : 0));
    }
    for (const e of plan.ents.rowEnts) {
      const r = plan.rows.find((q) => q.id === e.id);
      if (r) e.coords = r.coords;
    }
    renderFramed(plan, ctx);
    plan.terrainPoints = [...plan.terrainPoints, ...plan.framePoints];
    return plan;
  }
  if (!plan.host?.prefab) {
    plan.warnings.push("the host has no prefab in the dossier: rows were not seated on it");
    return plan;
  }
  // opts.seatOnly (`town stamp`): seat just the rows held that way (["wall"]); the others stand where the grid proved them.
  const only = Array.isArray(opts.seatOnly) ? new Set(opts.seatOnly) : null;
  const srcOf = new Map(plan.kit.rows.map((q) => [q.id, q]));
  const toSeat = only ? plan.rows.filter((r) => only.has(mountOf({ ...(srcOf.get(r.id) ?? {}), prefab: r.prefab }).mount)) : plan.rows;
  const seat = await seatRows(toSeat, plan.kit.rows, plan.host, { reader, allowClip: Boolean(opts.allowClip), terrain: ctx.terrain ?? null, flatWalls: Boolean(opts.flatWalls) });
  plan.seat = seat;
  plan.terrainPoints = seat.terrainPoints ?? [];
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
  renderFramed(plan, ctx, new Set(seat.rows.filter((r) => r.overGround).map((r) => r.id)));
  plan.terrainPoints = [...plan.terrainPoints, ...plan.framePoints];
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

const USAGE = "usage: tools\\cli town fit <place> <slot> [--dry] [--check] [--refit] [--allow-clip] [--no-fetch] [--no-audit] [--no-look] [--terrain] [--building-prefab {GUID}path] [--destruction-guid {GUID}]";
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
function checkReport(plan, { root, place, slotId, layerFile, terrain = null }) {
  const shown = relative(root, layerFile).replaceAll("\\", "/");
  if (!plan.seat?.host) {
    for (const e of plan.errors ?? []) console.log(`REFUSE  ${e}`);
    for (const w of plan.warnings ?? []) console.log(`warn  ${w}`);
    console.log(`SUMMARY town-fit check fail ${place}.${slotId}: ${plan.seat ? "the host's model could not be read" : `${(plan.errors ?? []).length} problem(s)`}; nothing written`);
    return 1;
  }
  const written = existsSync(layerFile) ? layerCoords(readFileSync(layerFile, "utf8"), place, slotId) : null;
  // the layer names a row by ident(id) (dashes become underscores); a row the layer lacks is judged against its unseated spot
  // The disk holds each row in its prefab's frame (tools/lib/height-frame.mjs); the seat plans it in world heights, or over
  // the ground for a "ground" row. Compared in the seat's frame when the pins know the ground under it.
  const worldOf = (id, c) => {
    const diskOver = plan.frames?.[id] === "terrain";
    const seatOver = Boolean(plan.seat.rows.find((r) => r.id === id)?.overGround);
    if (!c || diskOver === seatOver) return c;
    const t = terrain?.at(c[0], c[2]);
    return Number.isFinite(t) ? [c[0], diskOver ? c[1] + t : c[1] - t, c[2]] : c;
  };
  const onDisk = written ? new Map(plan.seat.rows.map((r) => [r.id, written.has(ident(r.id)) ? worldOf(r.id, written.get(ident(r.id))) : r.from])) : null;
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
  if (!flags.dry && !flags.check && !flags["out-dir"] && !new RegExp(`^ws/town-draft-${place}-${slotId}(-[a-z0-9-]+)?$`).test(branch ?? "")) {
    console.log(`REFUSE  town fit writes an island layer only from the worktree ws/town-draft-${place}-${slotId} (this is ${branch ?? "unknown"}); create it with tools\\cli wt new town-draft-${place}-${slotId}, or pass --dry`);
    return 1;
  }

  let plan;
  let fitOpts0;
  let fitCtx;
  try {
    const placeDir = join(root, "docs", "everon", "places", place);
    const mappingDossier = readJson(resolve(root, flags.dossier ?? join(placeDir, "dossier.json")), "dossier");
    const mapping = readJson(resolve(root, flags.mapping ?? join(placeDir, "mapping.json")), "mapping");
    // A slot whose entry names another place's survey (`dossier`) fits against that dossier.
    const survey = mapping.slots?.[slotId]?.dossier;
    const dossier = survey && SLUG_RX.test(survey) ? readJson(join(root, "docs", "everon", "places", survey, "dossier.json"), "dossier") : mappingDossier;
    const template = flags["template-file"] ? readJson(resolve(root, flags["template-file"]), "template") : loadTemplate(mapping.template, root);
    const entry = template.slots.find((s) => s.id === slotId);
    const kits = {};
    if (entry) {
      const kitsDir = resolve(root, flags["kits-dir"] ?? "tools/town/kits");
      const kitName = kitNameOf(entry, mapping.slots?.[slotId]);
      const kf = join(kitsDir, `${kitName}.json`);
      if (existsSync(kf)) kits[kitName] = readJson(kf, "kit");
    }
    const shell = ctxShell ?? shellReader(root);
    const fitOpts = { place, slot: slotId, dossier, mappingDossier, mapping, template, kits, buildingPrefab: flags["building-prefab"], destructionGuid: flags["destruction-guid"], branch, fetch: !flags["no-fetch"] && !flags.check, refit: Boolean(flags.refit), allowClip: Boolean(flags["allow-clip"]), skipClaims: Boolean(flags.check) };
    fitOpts0 = fitOpts;
    fitCtx = { root, run, readGame: await gameReader(), shell, terrain: terrainFromPins(readPins(root, place, slotId)) };
    plan = await planFitSeated(fitOpts, fitCtx);
  } catch (e) {
    console.log(`REFUSE  ${e.message}`);
    return 1;
  }
  if (flags.check) return checkReport(plan, { root, place, slotId, terrain: fitCtx.terrain, layerFile: join(resolve(root, flags["out-dir"] ?? LAYER_DIR), layerNameOf(place, slotId)) });
  for (const n of plan.notes ?? []) console.log(`note  ${n}`);
  for (const w of plan.warnings ?? []) console.log(`warn  ${w}`);
  if (!plan.ok) {
    for (const e of plan.errors) console.log(`REFUSE  ${e}`);
    console.log(`SUMMARY town-fit fail ${place}.${slotId}: ${plan.errors.length} problem(s)`);
    return 1;
  }
  // A row whose frame needs the ground height under it and has none would be written in the wrong frame: refuse, unless
  // --terrain is about to probe it (the first write only gives the probe server the world to boot around).
  if (plan.framePending?.length && !flags.terrain) {
    console.log(`REFUSE  ${plan.framePending.length} row(s) need the ground height under them to be written in their prefab's frame (tools/lib/height-frame.mjs): ${plan.framePending.slice(0, 6).join(", ")}${plan.framePending.length > 6 ? ", ..." : ""}; run town fit ${place} ${slotId} --terrain`);
    console.log(`SUMMARY town-fit fail ${place}.${slotId}: ${plan.framePending.length} row(s) without a probed ground height`);
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

  // --terrain: the island server measures the ground under every open-ground row (the layer just written gives it the
  // world to boot around), the answers are kept in tools/town/terrain/, and the layer is written again on them.
  if (flags.terrain && plan.terrainPoints?.length) {
    try {
      const probed = await probeTerrain(plan.terrainPoints, { layerName: plan.layerName, aroundY: plan.anchor.y });
      if (probed.missing.length) console.log(`warn  the server gave no ground height at ${probed.missing.length} point(s): ${probed.missing.join(" ")}`);
      const pinFile = writePins(root, place, slotId, probed.points, (fitCtx.now ? fitCtx.now() : new Date()).toISOString().slice(0, 10));
      console.log(`terrain  ${Object.keys(probed.points).length} point(s) probed -> ${shown(pinFile)}`);
      fitCtx.terrain = terrainFromPins({ points: probed.points });
      plan = await planFitSeated(fitOpts0, fitCtx);
      for (const w of plan.warnings ?? []) console.log(`warn  ${w}`);
      if (plan.framePending?.length) plan = { ...plan, ok: false, errors: [...plan.errors, `no ground height came back for ${plan.framePending.join(", ")}: they cannot be written in their prefab's frame`] };
      if (!plan.ok) {
        for (const e of plan.errors) console.log(`REFUSE  ${e}`);
        console.log(`SUMMARY town-fit fail ${place}.${slotId}: ${plan.errors.length} problem(s) after the terrain probe`);
        return 1;
      }
      writeFileSync(layerFile, plan.layerText);
      writeFileSync(claimFile, `${JSON.stringify(plan.claim, null, 2)}\n`);
    } catch (e) {
      console.log(`REFUSE  terrain probe: ${e.message}`);
      return 1;
    }
  } else if (flags.terrain) console.log("note  --terrain: the kit has no open-ground row, nothing to probe");

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
