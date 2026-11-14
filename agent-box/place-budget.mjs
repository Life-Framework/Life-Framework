// Static placement checks on the layer entries alone (no server, no measurements): an entity budget per
// world cell and duplicates (same prefab, same spot). docs/placement-audit.md "World checks".
// Floating and buried (sunk) props are measured in-game by LF_PlacementAudit and ranked by place-audit.mjs.

// A world cell is 50 m square. Limit: what a client keeps loaded without terrain occlusion hiding it;
// the densest cell of the shipped town (the market) is well under it. A cell over the limit is reported
// with its heaviest prefabs so the fix (thin a pile, merge props) is obvious.
export const BUDGET = { cell: 50, limit: 150, dup: 0.05, dupYaw: 5 };

const baseOf = (e) => String(e.prefab || e.mesh || "").split("/").pop().replace(/\.(et|xob)$/, "");
// A record counts as a world entity when it carries a prefab or a mesh; the layer's property blocks
// (m_PrefabsToSpawn and the like, which only name a prefab to spawn later) do not.
export const isWorldEntity = (e) => Boolean(e.prefab || e.mesh) && !/^m_[A-Z]/.test(e.name || "");

export const cellOf = (e, cell = BUDGET.cell) => `${Math.floor(e.x / cell)},${Math.floor(e.z / cell)}`;

// Entities per cell, over the limit first. Each row: { cell, x, z (cell centre), count, top: [[prefab, n]...] }.
export function cellBudget(records, { cell = BUDGET.cell, limit = BUDGET.limit } = {}) {
  const cells = new Map();
  for (const e of records.filter(isWorldEntity)) {
    const k = cellOf(e, cell);
    if (!cells.has(k)) cells.set(k, { cell: k, n: 0, by: new Map() });
    const c = cells.get(k);
    c.n++;
    c.by.set(baseOf(e), (c.by.get(baseOf(e)) ?? 0) + 1);
  }
  return [...cells.values()]
    .map((c) => {
      const [i, j] = c.cell.split(",").map(Number);
      return { cell: c.cell, x: (i + 0.5) * cell, z: (j + 0.5) * cell, count: c.n, over: c.n > limit, top: [...c.by.entries()].sort((a, b) => b[1] - a[1]).slice(0, 3) };
    })
    .sort((a, b) => b.count - a.count);
}

const yawDiff = (a, b) => {
  const d = Math.abs((((a ?? 0) - (b ?? 0)) % 360) + 360) % 360;
  return Math.min(d, 360 - d);
};

// Same prefab (or mesh), within `dup` metres on all three axes and the same facing: the second one only
// costs frames and z-fights. Returns [{ a, b }] with a the first placed.
export function duplicates(records, { dup = BUDGET.dup, dupYaw = BUDGET.dupYaw } = {}) {
  const grid = new Map();
  const out = [];
  for (const e of records.filter(isWorldEntity)) {
    const key = `${e.prefab || e.mesh}`;
    const gx = Math.floor(e.x / 1);
    const gz = Math.floor(e.z / 1);
    let hit = null;
    for (let dx = -1; dx <= 1 && !hit; dx++) {
      for (let dz = -1; dz <= 1 && !hit; dz++) {
        for (const o of grid.get(`${key}|${gx + dx}|${gz + dz}`) ?? []) {
          if (Math.abs(o.x - e.x) <= dup && Math.abs(o.y - e.y) <= dup && Math.abs(o.z - e.z) <= dup && yawDiff(o.yaw, e.yaw) <= dupYaw) {
            hit = o;
            break;
          }
        }
      }
    }
    if (hit) out.push({ a: hit, b: e });
    const gk = `${key}|${gx}|${gz}`;
    if (!grid.has(gk)) grid.set(gk, []);
    grid.get(gk).push(e);
  }
  return out;
}

const nm = (e) => e.name || baseOf(e);
const allowed = (allow, e, kind) => allow.some((a) => a.kind === kind && new RegExp(a.match, "i").test(`${nm(e)} ${e.prefab || ""}`) && (!a.layer || a.layer === e.layer));

// Findings as report lines: { kind: "duplicate"|"budget", text }. `allow` is place-audit-allow.json's list
// (kind "duplicate" matches either entity; kind "budget" matches a cell as "cell:<i>,<j>").
export function worldFindings(records, { allow = [], cell = BUDGET.cell, limit = BUDGET.limit } = {}) {
  const out = [];
  for (const { a, b } of duplicates(records)) {
    if (allowed(allow, a, "duplicate") || allowed(allow, b, "duplicate")) continue;
    out.push({ kind: "duplicate", text: `duplicate: ${nm(b)} [${b.layer}] sits on ${nm(a)} [${a.layer}] (${baseOf(b)} @${b.x},${b.y},${b.z})` });
  }
  for (const c of cellBudget(records, { cell, limit })) {
    if (!c.over) continue;
    if (allow.some((a) => a.kind === "budget" && new RegExp(a.match).test(`cell:${c.cell}`))) continue;
    out.push({ kind: "budget", text: `budget: cell ${c.cell} (${c.x},${c.z}) holds ${c.count} entities, limit ${limit}; most: ${c.top.map(([p, n]) => `${p} x${n}`).join(", ")}` });
  }
  return out;
}
