// Semantic diff of DebugWorld .layer files (tools\cli world diff, docs/worktrees.md "Maintainer world edits").
//
// A .layer is a list of entities. Each is `Class [Name] [: "{GUID}prefab"] { ... }`, or a group
// `$grp Class [: "prefab"] { Name { ... } { ... } }` whose items share the class and prefab. Inside an
// entity body, a bare `{ ... }` block is its children (again an entity list); every other block
// (components, ...) and every `key value` line is a property.
//
// Entities are matched by name, or when unnamed by prefab (or class) plus parent path plus their index
// among unnamed siblings of that prefab. A matched pair is MOVED (coords or angles changed), PROP CHANGED
// (any other property, the property named), or equal. What Workbench rewrites on save without changing
// anything is NOISE: the BSP line, an entity at another file position, `angleY 180` vs `angles 0 180 0`,
// an explicit class prefix dropped or added, whitespace.
//
// Pure functions; proven by tools/validation/world-diff-selftest.mjs on tools/test/fixtures/world-diff/.

const EPS = 1e-4;

function tokenizeHeader(h) {
  // `$grp GenericEntity : "{G}x.et"` -> { grp, cls, name, prefab }
  let s = h.trim();
  const out = { grp: false, cls: null, name: null, prefab: null };
  if (s.startsWith("$grp ")) {
    out.grp = true;
    s = s.slice(5).trim();
  }
  const pm = /:\s*"([^"]*)"\s*$/.exec(s);
  if (pm) {
    out.prefab = pm[1];
    s = s.slice(0, pm.index).trim();
  }
  const words = s.split(/\s+/).filter(Boolean);
  if (out.grp) {
    out.cls = words[0] ?? null;
  } else if (words.length >= 2) {
    out.cls = words[0];
    out.name = words[1];
  } else if (words.length === 1) {
    // One word: a class (`GenericEntity {`) or, with the class dropped, a name. A class name is CamelCase
    // with no underscore-led prefix we use for names; Enfusion entity classes end in Entity/Effect/... but
    // names in this repo carry an underscore or a lower-case letter run. Treat a word ending in a known
    // class suffix as a class.
    if (/(Entity|Effect|Manager|Area|Trigger|Light|Volume|Component|Camera|Sound|Decal|Particle|Prefab|Spawn\w*)$/.test(words[0]) && !/_/.test(words[0])) out.cls = words[0];
    else out.name = words[0];
  }
  return out;
}

// Parse text into lines of { t: trimmed, n: line number }.
function lines(text) {
  return String(text).replace(/\r\n/g, "\n").split("\n").map((l, i) => ({ t: l.trim(), n: i + 1 })).filter((l) => l.t !== "");
}

// Reads a block body from lines[i] (the line after the `{`) to its closing `}`. Returns { body: [lines], next }.
function blockBody(L, i) {
  let depth = 1;
  const start = i;
  for (; i < L.length; i++) {
    const t = L[i].t;
    if (t.endsWith("{")) depth++;
    else if (t === "}" || t.startsWith("}")) {
      depth--;
      if (depth === 0) return { body: L.slice(start, i), next: i + 1 };
    }
  }
  throw new Error(`unclosed block opened before line ${L[start - 1]?.n ?? "?"}`);
}

function vec(v) {
  return String(v).replace(/"/g, "").trim().split(/\s+/).map(Number);
}

function canonBlock(body) {
  return body.map((l) => l.t.replace(/\s+/g, " ")).join("\n");
}

// One entity body -> { props: Map, blocks: Map, children: [entity] }.
function parseBody(L, ctx) {
  const props = new Map();
  const blocks = new Map();
  let children = [];
  for (let i = 0; i < L.length; ) {
    const t = L[i].t;
    if (t === "{") {
      const b = blockBody(L, i + 1);
      children = children.concat(parseList(b.body, ctx));
      i = b.next;
      continue;
    }
    if (t.endsWith("{")) {
      const b = blockBody(L, i + 1);
      const key = t.slice(0, -1).trim();
      blocks.set(key, canonBlock(b.body));
      i = b.next;
      continue;
    }
    const m = /^(\S+)\s*(.*)$/.exec(t);
    if (m) props.set(m[1], m[2]);
    i++;
  }
  return { props, blocks, children };
}

function makeEntity(h, bodyLines, line, ctx) {
  const { props, blocks, children } = parseBody(bodyLines, ctx);
  const angles = [0, 0, 0];
  if (props.has("angles")) vec(props.get("angles")).forEach((v, k) => { angles[k] = v; });
  for (const [k, ax] of [["angleX", 0], ["angleY", 1], ["angleZ", 2]]) if (props.has(k)) angles[ax] = Number(props.get(k));
  const coords = props.has("coords") ? vec(props.get("coords")) : [0, 0, 0];
  const rest = new Map([...props].filter(([k]) => !["coords", "angles", "angleX", "angleY", "angleZ"].includes(k)));
  return {
    cls: h.cls, name: h.name, prefab: h.prefab, line, coords, angles,
    angleForm: [...props.keys()].filter((k) => k.startsWith("angle")).sort().join(","),
    props: rest, blocks, children,
  };
}

// An entity list (file top level, or a children block).
function parseList(L, ctx) {
  const out = [];
  for (let i = 0; i < L.length; ) {
    const t = L[i].t;
    if (!t.endsWith("{")) {
      i++; // a stray line in a list: nothing an entity owns
      continue;
    }
    const h = tokenizeHeader(t.slice(0, -1));
    const b = blockBody(L, i + 1);
    if (h.grp) {
      const G = b.body;
      for (let j = 0; j < G.length; ) {
        const gt = G[j].t;
        if (!gt.endsWith("{")) { j++; continue; }
        const nm = gt.slice(0, -1).trim();
        const gb = blockBody(G, j + 1);
        out.push(makeEntity({ cls: h.cls, name: nm || null, prefab: h.prefab, grp: true }, gb.body, G[j].n, ctx));
        j = gb.next;
      }
    } else {
      out.push(makeEntity(h, b.body, L[i].n, ctx));
    }
    i = b.next;
  }
  return out;
}

export function parseLayer(text) {
  return parseList(lines(text), {});
}

function shortPrefab(p) {
  if (!p) return null;
  return p.replace(/^\{[0-9A-Fa-f]{16}\}/, "").split("/").pop();
}

export function label(e) {
  return e.name || shortPrefab(e.prefab) || e.cls || "(entity)";
}

// Flattens an entity tree to Map<key, { e, parent, pos }>. Unnamed: parent/prefab#k.
export function flatten(list, parent = "", out = new Map()) {
  const seen = new Map();
  list.forEach((e, pos) => {
    let id;
    if (e.name) id = e.name;
    else {
      const base = e.prefab || e.cls || "?";
      const k = seen.get(base) ?? 0;
      seen.set(base, k + 1);
      id = `${base}#${k}`;
    }
    const key = parent ? `${parent}/${id}` : id;
    out.set(key, { e, parent, pos });
    flatten(e.children, key, out);
  });
  return out;
}

const near = (a, b) => a.length === b.length && a.every((v, k) => Math.abs(v - b[k]) < EPS);
const nearAngle = (a, b) => a.every((v, k) => {
  const d = (((v - b[k]) % 360) + 540) % 360 - 180;
  return Math.abs(d) < EPS;
});
const fmt = (v) => v.map((x) => +x.toFixed(3)).join(" ");

function where(key, parent) {
  if (!parent) return "";
  return ` at ${parent.split("/").pop()}`;
}

// Diff one file's text. Returns { changes: [{ kind, key, label, ... }], noise: [string] }.
export function diffLayer(path, before, after) {
  const changes = [];
  const noise = new Set();
  if (before === after) return { changes, noise: [] };
  if (before != null && after != null && before.replace(/\s+/g, "") === after.replace(/\s+/g, "")) return { changes, noise: ["whitespace"] };
  let A;
  let B;
  try {
    A = flatten(before == null ? [] : parseLayer(before));
    B = flatten(after == null ? [] : parseLayer(after));
  } catch (err) {
    return { changes: [{ kind: "UNPARSED", key: path, label: path, text: `could not parse: ${err.message}` }], noise: [] };
  }
  // Order among siblings, adds and removes taken out: a different relative order is a re-order.
  const order = (M, other) => {
    const byParent = new Map();
    for (const [k, v] of M) if (other.has(k)) {
      if (!byParent.has(v.parent)) byParent.set(v.parent, []);
      byParent.get(v.parent).push(k);
    }
    return byParent;
  };
  const oa = order(A, B);
  const ob = order(B, A);
  for (const [p, ks] of oa) if ((ob.get(p) || []).join("\n") !== ks.join("\n")) noise.add("entities re-ordered (same values, another position in the file)");

  for (const [k, { e, parent }] of B) {
    const o = A.get(k);
    if (!o) {
      changes.push({ kind: "ADDED", key: k, name: e.name, label: label(e) + where(k, parent), prefab: e.prefab, cls: e.cls, coords: e.coords, angles: e.angles, hasBody: e.blocks.size > 0 });
      continue;
    }
    const a = o.e;
    const moved = !near(a.coords, e.coords) || !nearAngle(a.angles, e.angles);
    if (moved) {
      const d = Math.hypot(...e.coords.map((v, i) => v - (a.coords[i] ?? 0)));
      changes.push({ kind: "MOVED", key: k, name: e.name, prefab: e.prefab, label: label(e) + where(k, parent), from: { coords: a.coords, angles: a.angles }, to: { coords: e.coords, angles: e.angles }, metres: d });
    } else if (a.angleForm !== e.angleForm || (a.props.has("coords") !== e.props.has("coords"))) {
      noise.add("coords/angles re-serialized with equal values (e.g. angleY 180 -> angles 0 180 0)");
    }
    const props = [];
    for (const key of new Set([...a.props.keys(), ...e.props.keys()])) {
      if (key === "BSP") {
        if (a.props.get(key) !== e.props.get(key)) noise.add("BSP block rewritten by Workbench");
        continue;
      }
      if ((a.props.get(key) ?? null) !== (e.props.get(key) ?? null)) props.push(key);
    }
    for (const key of new Set([...a.blocks.keys(), ...e.blocks.keys()])) if ((a.blocks.get(key) ?? null) !== (e.blocks.get(key) ?? null)) props.push(key);
    if ((a.prefab ?? null) !== (e.prefab ?? null)) props.push("prefab");
    if (a.cls && e.cls && a.cls !== e.cls) props.push("class");
    else if ((a.cls ?? null) !== (e.cls ?? null)) noise.add("explicit class prefix dropped or added (same entity)");
    if (props.length) changes.push({ kind: "PROP CHANGED", key: k, name: e.name, prefab: e.prefab, label: label(e) + where(k, parent), props });
  }
  for (const [k, { e, parent }] of A) if (!B.has(k)) changes.push({ kind: "REMOVED", key: k, label: label(e) + where(k, parent), prefab: e.prefab, coords: e.coords });
  if (!changes.length && !noise.size) noise.add("whitespace or formatting only");
  return { changes, noise: [...noise] };
}

export function changeLine(c) {
  if (c.kind === "MOVED") {
    const angs = nearAngle(c.from.angles, c.to.angles) ? "" : `, angles ${fmt(c.from.angles)} -> ${fmt(c.to.angles)}`;
    return `MOVED ${c.label}: ${fmt(c.from.coords)} -> ${fmt(c.to.coords)}${angs} (${c.metres.toFixed(2)} m)`;
  }
  if (c.kind === "ADDED") return `ADDED ${c.label}${c.prefab ? ` (${c.prefab})` : ""} @ ${fmt(c.coords)}`;
  if (c.kind === "REMOVED") return `REMOVED ${c.label}${c.prefab ? ` (${c.prefab})` : ""} @ ${fmt(c.coords)}`;
  if (c.kind === "PROP CHANGED") return `PROP CHANGED ${c.label}: ${c.props.join(", ")}`;
  return `${c.kind} ${c.label}: ${c.text ?? ""}`;
}

// files: [{ path, before, after }] (null for absent). Returns per-file results and totals.
export function diffFiles(files) {
  const out = [];
  for (const f of files) {
    const r = diffLayer(f.path, f.before, f.after);
    out.push({ path: f.path, status: f.before == null ? "A" : f.after == null ? "D" : "M", ...r, noiseOnly: r.changes.length === 0 });
  }
  const count = (k) => out.reduce((n, f) => n + f.changes.filter((c) => c.kind === k).length, 0);
  return { files: out, totals: { moved: count("MOVED"), added: count("ADDED"), removed: count("REMOVED"), prop: count("PROP CHANGED"), unparsed: count("UNPARSED"), noiseFiles: out.filter((f) => f.noiseOnly).length } };
}

export function summaryLine(d) {
  const t = d.totals;
  const parts = [];
  if (t.moved) parts.push(`${t.moved} MOVED`);
  if (t.added) parts.push(`${t.added} ADDED`);
  if (t.removed) parts.push(`${t.removed} REMOVED`);
  if (t.prop) parts.push(`${t.prop} PROP CHANGED`);
  if (t.unparsed) parts.push(`${t.unparsed} UNPARSED`);
  const noisy = d.files.some((f) => f.noise.length);
  return `${parts.join(" + ") || "no real change"}${noisy ? " + noise" : ""}`;
}

// Is this diff a maintainer hand edit the fast tier may take: only MOVED, and ADDED / REMOVED of
// existing prefabs (knownPrefab(ref) says the {GUID}path is already used), in modified .layer files.
// Returns null when it is, else the reason.
export function handEditProblem(d, knownPrefab = () => false) {
  for (const f of d.files) {
    if (f.status !== "M") return `${f.path}: layer ${f.status === "A" ? "added" : "deleted"}`;
    for (const c of f.changes) {
      if (c.kind === "MOVED" || c.kind === "REMOVED") continue;
      if (c.kind === "ADDED") {
        if (!c.prefab) return `${f.path}: ADDED ${c.label} has no prefab (an inline entity)`;
        if (c.hasBody) return `${f.path}: ADDED ${c.label} carries component overrides`;
        if (!knownPrefab(c.prefab)) return `${f.path}: ADDED ${c.label} uses ${c.prefab}, which no layer used before`;
        continue;
      }
      return `${f.path}: ${changeLine(c)}`;
    }
  }
  return null;
}

// What a test source names when it depends on a touched layer: the layer stems and the touched entity
// names (prefab file names and short names left out: too common to pick tests by).
export function testTokens(layers, touched) {
  return [...new Set([...layers.map((l) => l.replace(/\.layer$/, "")), ...touched.map((t) => t.name).filter((n) => n.length >= 6 && !/\.et$/.test(n) && !/[{}#]/.test(n))])];
}

// The names (or labels) of every entity a real change touched: what the placement audit and the test pick read.
export function touchedNames(d) {
  const out = [];
  for (const f of d.files) for (const c of f.changes) if (c.kind !== "REMOVED") out.push({ layer: f.path.split("/").pop(), key: c.key, name: c.name || c.label.split(" at ")[0], named: !!c.name, prefab: c.prefab ?? null, label: c.label });
  return out;
}
