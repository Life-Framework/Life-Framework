// tools\cli building plan <slug>: the facts an agent brief needs, read from the spec and the exported prefab,
// as markdown on stdout and in tmp/buildings/<slug>/plan.md. The exact furniture and fixture prefabs as
// {GUID}path with their measured sizes (tools\cli measure), the rooms and doors, the store slots, which
// parts can go to parallel agents, and the proof list. Nothing here is guessed from a picture.
// Rooms: ArchBuilder exports no room outlines, so a room is the box its furniture spans, named by the kind
// of furniture in it (ROOM_KINDS); fittings (lamps, switches, sockets) belong to the nearest room.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { FIXTURES, RACKS, buildingFiles, fixturesOf, placedFixtures, readChildren, storePlan } from "./store-slots.mjs";
import { loadScenario } from "./scenario.mjs";

export const ROOM_KINDS = [
  ["wc", /Toilet|Urinal|Sink|Basin|WashBasin|Mirror/i],
  ["office", /Desk|ChairOffice|Computer|CabinetMetal|FileOffice|Typewriter/i],
  ["storage", /ShelfUniversal|Pallet|CardboardBox|Crate|Bucket|ClockPunch|FireExtinguisher/i],
  ["sales hall", /ShelfShop|CoolingBox|Cashier|CounterShop|ShoppingBasket|Fridge/i],
  ["living", /Bed|Sofa|Armchair|Wardrobe|TableDining|Kitchen|Stove/i],
];
const FITTING_REACH = 3;

const f1 = (n) => (+n).toFixed(1);
const f2 = (n) => (+n).toFixed(2);
const pt = (a) => `(${a.map(f2).join(", ")})`;

export function roomKind(name) {
  return ROOM_KINDS.find(([, re]) => re.test(name))?.[0] ?? null;
}

// Pure: rooms from furniture. items: [{ name, at }]. Returns [{ kind, x: [a,b], z: [a,b], items: [...] }].
export function inferRooms(items) {
  const rooms = new Map();
  const loose = [];
  for (const it of items) {
    const kind = roomKind(it.name);
    if (!kind) {
      loose.push(it);
      continue;
    }
    if (!rooms.has(kind)) rooms.set(kind, { kind, items: [] });
    rooms.get(kind).items.push(it);
  }
  const list = [...rooms.values()];
  for (const it of loose) {
    let best = null;
    let bestD = FITTING_REACH;
    for (const r of list) for (const o of r.items) {
      const d = Math.hypot(o.at[0] - it.at[0], o.at[2] - it.at[2]);
      if (d < bestD) [best, bestD] = [r, d];
    }
    (best ? best.fittings ??= [] : (list.unassigned ??= [])).push(it);
  }
  for (const r of list) {
    r.x = [Math.min(...r.items.map((i) => i.at[0])), Math.max(...r.items.map((i) => i.at[0]))];
    r.z = [Math.min(...r.items.map((i) => i.at[2])), Math.max(...r.items.map((i) => i.at[2]))];
    r.fittings ??= [];
  }
  return list;
}

// Pure: which wall a door or window stands in, from the model bounds.
export function wallOf(at, b, tol = 0.8) {
  if (!b) return "?";
  if (Math.abs(at[2] - b.min[2]) < tol) return "front";
  if (Math.abs(at[2] - b.max[2]) < tol) return "back";
  if (Math.abs(at[0] - b.min[0]) < tol) return "left";
  if (Math.abs(at[0] - b.max[0]) < tol) return "right";
  return "inside";
}

const metaRef = (file, rel) => {
  try {
    const g = /Name "(\{[0-9A-F]{16}\})/.exec(readFileSync(`${file}.meta`, "utf8"))?.[1];
    return g ? `${g}${rel}` : rel;
  } catch {
    return `${rel} (no .meta: not exported yet)`;
  }
};

// Measured sizes through tools\cli measure (cached per game build).
function measureAll(root, refs) {
  if (!refs.length) return new Map();
  const r = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "measure", ...refs, "--json"], { cwd: root, encoding: "utf8", windowsHide: true, maxBuffer: 64 << 20 });
  const text = r.stdout || "";
  const at = text.indexOf("[");
  const out = new Map();
  if (at < 0) return out;
  try {
    for (const m of JSON.parse(text.slice(at))) out.set(m.arg, m);
  } catch {}
  return out;
}

const sizeText = (m) => (m?.bounds ? [0, 2, 1].map((k) => f2(m.bounds.max[k] - m.bounds.min[k])).join(" x ") : "?");
const topsText = (m) => [...new Set((m?.surfaces || []).map((s) => f2(s.y)))].slice(0, 5).join(", ") || "-";

export function buildPlan(root, slug, { measure = (refs) => measureAll(root, refs) } = {}) {
  const spec = JSON.parse(readFileSync(join(root, "tools", "buildings", slug, "spec.json"), "utf8"));
  const addon = spec.addon || "LifeFramework";
  const ad = join(root, "addons", addon);
  const pd = spec.prefab_dir;
  const files = { ...buildingFiles(spec), model: spec.vanilla ? `the vanilla ${spec.vanilla.base}` : `${spec.asset_dir}/${spec.name}.xob` };
  const read = (rel) => (rel && existsSync(join(ad, rel)) ? readFileSync(join(ad, rel), "utf8") : "");
  const furnText = read(files.furniture);
  // A vanilla-based building adds its doors and windows in our prefab; the base's own are the game's.
  const baseText = read(spec.vanilla ? files.building : files.base);
  // The shop fittings the store block places are children too (store.fixtures).
  const furniture = [...readChildren(furnText), ...(spec.store ? placedFixtures(spec.store) : [])];
  const shell = readChildren(baseText);
  const props = (spec.polish?.props || []).map((p) => ({ name: p.prefab.split("/").pop().replace(/\.et$/, ""), ref: p.prefab, at: p.at, yaw: p.yaw ?? 0, why: p.why }));
  const refs = [...new Set([files.building, ...furniture.map((c) => c.ref), ...props.map((p) => p.ref)])];
  const sizes = measure(refs);
  const bounds = sizes.get(files.building)?.bounds;
  // The walls: the spec's width and depth (a canopy or a sign widens the model bounds), else the bounds.
  const ab = spec.archbuilder || {};
  const walls = ab.width && ab.depth ? { min: [-ab.width / 2, 0, -ab.depth / 2], max: [ab.width / 2, 0, ab.depth / 2] } : bounds;
  const L = [];
  const line = (s = "") => L.push(s);

  line(`# Building plan: ${spec.name} (\`${slug}\`)`);
  line();
  line(`Read from \`tools/buildings/${slug}/spec.json\` and the export in \`addons/${addon}/${pd}/\` by \`tools\\cli building plan ${slug}\`. Sizes are \`tools\\cli measure\` (model bounds, w x d x h in metres; tops are the heights things stand at). Building space: +Z is the back, -Z the street front, y from the floor origin.`);
  line();
  line(`- ${spec.archbuilder?.archetype || spec.archbuilder?.landmark || "?"}, ${spec.archbuilder?.storeys ?? "?"} storey(s), red line \`${spec.red_line}\`, addon \`${addon}\``);
  line(`- model bounds ${sizeText(sizes.get(files.building))}${bounds ? `, x ${f2(bounds.min[0])}..${f2(bounds.max[0])}, z ${f2(bounds.min[2])}..${f2(bounds.max[2])}` : " (not measured: export and import it first)"}`);
  line();
  line("## Prefabs");
  line();
  line("| Role | Resource |");
  line("|---|---|");
  line(`| Place this | \`${files.store ? metaRef(join(ad, files.store), files.store) : metaRef(join(ad, files.building), files.building)}\` |`);
  line(`| Building (shell + furniture) | \`${metaRef(join(ad, files.building), files.building)}\` |`);
  line(`| Shell (model, doors, windows) | \`${spec.vanilla ? `${files.base} (vanilla; our prefab adds the doors, windows and sign)` : metaRef(join(ad, files.base), files.base)}\` |`);
  line(`| Furniture (every prop a vanilla child) | \`${files.furniture ? metaRef(join(ad, files.furniture), files.furniture) : "none: the store block places the fittings"}\` |`);
  line(`| Model | \`${files.model}\` |`);
  line();
  line("Generated files: `building run` rewrites them from the spec. Never edit them by hand.");
  line();

  const groups = new Map();
  for (const c of furniture) {
    if (!groups.has(c.ref)) groups.set(c.ref, { ref: c.ref, name: c.name, list: [] });
    groups.get(c.ref).list.push(c);
  }
  // The hand props are children of the furniture prefab too (the export writes them there): rooms count them once.
  const rooms = inferRooms(furniture);
  const roomOf = (c) => rooms.find((r) => [...r.items, ...r.fittings].some((o) => Math.hypot(o.at[0] - c.at[0], o.at[1] - c.at[1], o.at[2] - c.at[2]) < 0.05))?.kind ?? "-";
  line(`## Furniture and fixtures (${furniture.length} children of the furniture prefab)`);
  line();
  line("| Prefab | n | Size (m) | Tops y | Room |");
  line("|---|---|---|---|---|");
  for (const g of [...groups.values()].sort((a, b) => b.list.length - a.list.length || a.name.localeCompare(b.name))) {
    const m = sizes.get(g.ref);
    line(`| \`${g.ref}\` | ${g.list.length} | ${sizeText(m)} | ${topsText(m)} | ${[...new Set(g.list.map(roomOf))].join(", ")} |`);
  }
  line();
  if (props.length) {
    line(`## Hand-placed props (${props.length}, \`spec.polish.props\`)`);
    line();
    line("| Prefab | At | Size (m) | Room | Why |");
    line("|---|---|---|---|---|");
    for (const p of props) line(`| \`${p.ref}\` | ${pt(p.at)}${p.yaw ? ` yaw ${p.yaw}` : ""} | ${sizeText(sizes.get(p.ref))} | ${roomOf(p)} | ${p.why || ""} |`);
    line();
  }

  const openings = shell.filter((c) => /^(DoorSet|Glass|Window)/.test(c.name));
  const doors = openings.filter((c) => /^DoorSet/.test(c.name));
  line(`## Rooms (${rooms.length}, from the furniture in them)`);
  line();
  line("| Room | x | z | Furniture | Fittings | Doors within 1.5 m |");
  line("|---|---|---|---|---|---|");
  for (const r of rooms) {
    const near = doors.map((d, i) => [d, i]).filter(([d]) => d.at[0] >= r.x[0] - 1.5 && d.at[0] <= r.x[1] + 1.5 && d.at[2] >= r.z[0] - 1.5 && d.at[2] <= r.z[1] + 1.5).map(([, i]) => `door ${i}`);
    line(`| ${r.kind} | ${f1(r.x[0])}..${f1(r.x[1])} | ${f1(r.z[0])}..${f1(r.z[1])} | ${r.items.length} | ${r.fittings.length} | ${near.join(", ") || "-"} |`);
  }
  line();
  line(`## Doors and windows (${doors.length} door set(s), ${openings.length - doors.length} window(s))`);
  line();
  line("| # | Prefab | At | Yaw | Wall |");
  line("|---|---|---|---|---|");
  for (const [i, d] of doors.entries()) line(`| door ${i} | \`${d.ref}\` | ${pt(d.at)} | ${d.yaw} | ${wallOf(d.at, walls)}${/_EXT/.test(d.name) ? " (outside door)" : ""} |`);
  const windows = new Map();
  for (const w of openings.filter((c) => !/^DoorSet/.test(c.name))) {
    const k = `${w.ref}|${wallOf(w.at, walls)}`;
    windows.set(k, (windows.get(k) || 0) + 1);
  }
  for (const [k, n] of windows) line(`| ${n} window(s) | \`${k.split("|")[0]}\` | | | ${k.split("|")[1]} |`);
  line();

  if (spec.store) {
    const plan = storePlan(spec.store, furnText);
    line(`## Store slots (\`store\` key \`${spec.store.key}\`, restock ${spec.store.restock_seconds ?? "?"} s)`);
    line();
    line("Fixtures by the index `pick` uses (boards measured in `tools/buildings/pipeline/fixtures.json`):");
    line();
    for (const name of Object.keys(FIXTURES)) {
      const list = fixturesOf(furniture, name);
      if (!list.length) continue;
      line(`- \`${name}\` (${list.length}, boards y ${FIXTURES[name].boards.map((b) => f2(b.y)).join(", ")}): ${list.map((f, i) => `${i} ${pt(f.at)} yaw ${f.yaw}`).join("; ")}`);
    }
    for (const name of Object.keys(RACKS)) {
      const list = fixturesOf(furniture, name);
      if (!list.length) continue;
      const kinds = RACKS[name].slots.reduce((m, s) => ({ ...m, [s.kind]: (m[s.kind] || 0) + 1 }), {});
      line(`- \`${name}\` (${list.length}, display rack, slots ${Object.entries(kinds).map(([k, n]) => `${n} ${k}`).join(", ")}): ${list.map((f, i) => `${i} ${pt(f.at)} yaw ${f.yaw}`).join("; ")}`);
    }
    line();
    line("| Item | Fixture | Pick | Boards / slots | Points | Max | Restock |");
    line("|---|---|---|---|---|---|---|");
    let shelf = 0;
    let shown = 0;
    for (const l of spec.store.lines || []) {
      if (l.display) line(`| \`${l.item}\` | ${l.fixture} (display only) | ${(l.pick || []).join(", ")} | ${l.kind || "long"} ${(l.slots || ["all"]).join(", ")} | ${plan.display[shown++]?.points.length ?? "?"} | - | - |`);
      else line(`| \`${l.item}\` | ${l.fixture} | ${(l.pick || []).join(", ")} | ${(l.boards || []).join(", ")} | ${plan.lines[shelf++]?.slots.length ?? "?"} | ${l.max ?? "-"} | ${l.restock ?? "-"} |`);
    }
    line();
    for (const c of plan.counters) line(`- Counter \`${c.prefab}\` at ${pt(c.at)} yaw ${c.yaw}: sells outlet \`${c.outlet}\` over the counter, behind its catalogue gates.`);
    line(`Tills (${plan.tills.length}): ${plan.tills.map(pt).join("; ") || "none"}. Alarm: ${spec.store.alarm ? `\`${spec.store.alarm.prefab}\` at ${pt(spec.store.alarm.at)}, reach ${spec.store.alarm.reach} m` : "none"}.${plan.problems.length ? ` Problems: ${plan.problems.join("; ")}` : ""}`);
    line();
  }

  line("## Parallel work");
  line();
  line("Independent (one agent each; they touch different rows, so merge `origin/main` before shipping):");
  line();
  for (const r of rooms) line(`- **${r.kind}** (x ${f1(r.x[0])}..${f1(r.x[1])}, z ${f1(r.z[0])}..${f1(r.z[1])}): its \`spec.polish.props\` rows (${props.filter((p) => roomOf(p) === r.kind).length} now), dressing checked with \`tools\\cli look\` on a full session.`);
  if (spec.store) {
    const byFixture = new Map();
    for (const l of spec.store.lines || []) byFixture.set(l.fixture, [...(byFixture.get(l.fixture) || []), l.item.split("/").pop().replace(/\.et$/, "")]);
    for (const [fx, items] of byFixture) line(`- **stock on ${fx}**: its \`store.lines\` rows (${items.join(", ")}), prices in \`Configs/Shop/ShopPrices.conf\` (append), item prefabs.`);
  }
  line(`- **proof scenario**: \`tools/buildings/${slug}/proof.json\` only.`);
  line();
  line("One agent at a time (no merge possible or a shared generator):");
  line();
  line(`- \`tools/buildings/${slug}/polished.blend\`: a binary scene, so walls, openings, canopy, materials and lighting in Blender are one agent's at a time.`);
  line("- `tools/buildings/pipeline/fixtures.json` (board measurements shared by every building) and the generated prefabs above (`building run` writes them).");
  line();

  const sc = loadScenario(root, slug);
  line("## Proof list (`tools\\cli building proof " + slug + "`)");
  line();
  line("1. The placed prefab resolves to the model, with colliders (`measure`).");
  line("2. Its height against the Everon buildings within 150 m.");
  line("3. `tools\\cli audit placement --strict` on its layer (the dedicated server loads it).");
  line(`4. Full-profile session: each of the ${doors.length} door(s) is offered by the F key, opens, leaves a doorway a character wide clear at 0.5, 1.0 and 1.6 m, and closes; a picture of each shut and open.`);
  line("5. Pictures: the four sides, the street corner, the hall from inside.");
  if (sc) {
    line(`6. \`proof.json\` (${sc.steps.length} steps):`);
    for (const s of sc.steps.filter((s) => s.why || s.pickup)) line(`   - ${s.why || `pick up ${s.pickup}`}`);
    for (const c of sc.checks || []) line(`   - check: ${c.why || JSON.stringify(c)}`);
    for (const l of sc.log || []) line(`   - server log: ${l.why || l.re || l.last}`);
  } else line(`6. No \`tools/buildings/${slug}/proof.json\` yet: the generic checks only.`);
  return { text: `${L.join("\n")}\n`, spec, rooms, doors };
}

export function cmdPlan(root, slug) {
  if (!existsSync(join(root, "tools", "buildings", slug, "spec.json"))) {
    console.log(`FAIL  no tools/buildings/${slug}/spec.json`);
    return 1;
  }
  const { text } = buildPlan(root, slug);
  const dir = join(root, "tmp", "buildings", slug);
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "plan.md"), text);
  process.stdout.write(text);
  console.log(`\n(also in ${join(dir, "plan.md")})`);
  return 0;
}
