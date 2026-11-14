// Upgrade page generator (#294): pure functions that diff two releases' machine-readable facts and
// fill docs/upgrades/TEMPLATE.md. No git, no network here; tools/commands/upgrade-page.mjs reads the
// facts from two git refs and hands them in. Facts: { schema, migrations, shapes, manifests }.

// Flatten the owner schema to dotted paths. A map of records (additionalProperties) shows as `<key>`,
// an array of records as `[]`. `section` marks a top-level object.
export function flattenSchema(schema) {
  const out = new Map();
  const leaf = (n) => ({
    type: Array.isArray(n.type) ? n.type.join("|") : (n.type ?? (n.enum ? "enum" : "any")),
    default: n.default, minimum: n.minimum, maximum: n.maximum, enum: n.enum,
  });
  const walk = (n, path, depth) => {
    if (!n || typeof n !== "object") return;
    const props = n.properties;
    const extra = n.additionalProperties && typeof n.additionalProperties === "object" ? n.additionalProperties : null;
    const items = n.type === "array" && n.items && typeof n.items === "object" ? n.items : null;
    if (path) out.set(path, { ...leaf(n), section: depth === 1 && !!(props || extra) });
    if (props) for (const [k, v] of Object.entries(props)) walk(v, path ? `${path}.${k}` : k, depth + 1);
    if (extra) walk(extra, `${path}.<key>`, depth + 1);
    if (items) walk(items, `${path}[]`, depth + 1);
  };
  walk(schema, "", 0);
  return out;
}

const show = (v) => (v === undefined ? "(none)" : JSON.stringify(v));
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);

function rangeText(n) {
  const parts = [];
  if (n.minimum !== undefined) parts.push(`min ${n.minimum}`);
  if (n.maximum !== undefined) parts.push(`max ${n.maximum}`);
  if (n.enum) parts.push(`one of ${n.enum.map((e) => JSON.stringify(e)).join(", ")}`);
  return parts.length ? parts.join(", ") : "(unbounded)";
}

// { addedSections, removedSections, added, removed, retyped, defaults, ranges } over two schemas.
export function diffSchemas(oldSchema, newSchema) {
  const a = flattenSchema(oldSchema ?? {}), b = flattenSchema(newSchema ?? {});
  const d = { addedSections: [], removedSections: [], added: [], removed: [], retyped: [], defaults: [], ranges: [] };
  for (const [p, n] of b) if (!a.has(p) && n.section) d.addedSections.push(p);
  for (const [p, n] of a) if (!b.has(p) && n.section) d.removedSections.push(p);
  const inside = (p, secs) => secs.some((s) => p.startsWith(`${s}.`));
  for (const [p, n] of b) {
    if (!a.has(p)) {
      if (!n.section && !inside(p, d.addedSections)) d.added.push({ path: p, type: n.type, default: n.default });
      continue;
    }
    const o = a.get(p);
    if (o.type !== n.type) d.retyped.push({ path: p, was: o.type, now: n.type });
    if (!same(o.default, n.default)) d.defaults.push({ path: p, was: o.default, now: n.default });
    if (!same([o.minimum, o.maximum, o.enum], [n.minimum, n.maximum, n.enum])) d.ranges.push({ path: p, was: rangeText(o), now: rangeText(n) });
  }
  for (const [p, n] of a) if (!b.has(p) && !n.section && !inside(p, d.removedSections)) d.removed.push({ path: p });
  return d;
}

// Migrations in `now` and not in `was` (by from + name), in order.
export function diffMigrations(was, now) {
  const key = (m) => `${m.from}:${m.name}`;
  const have = new Set((was?.migrations ?? []).map(key));
  return (now?.migrations ?? []).filter((m) => !have.has(key(m))).sort((x, y) => x.from - y.from);
}

// Save version and payload classes from tools/validation/record-shapes.json.
export function diffShapes(was, now) {
  const w = was?.payloads ?? {}, n = now?.payloads ?? {};
  return {
    from: was?.saveVersion ?? null, to: now?.saveVersion ?? null,
    payloadsAdded: Object.keys(n).filter((k) => !(k in w)),
    payloadsRemoved: Object.keys(w).filter((k) => !(k in n)),
    payloadsChanged: Object.keys(n).filter((k) => k in w && !same(w[k], n[k])).map((k) => ({ key: k, was: w[k], now: n[k] })),
  };
}

// manifests: { <packId>: { min, below } }. A pack that appears, goes, or moves its range.
export function diffManifests(was, now) {
  const out = [];
  for (const id of Object.keys(now ?? {})) {
    const o = was?.[id], n = now[id];
    if (!o) out.push({ pack: id, text: `new pack, core [${n.min}, ${n.below})` });
    else if (o.min !== n.min || o.below !== n.below) out.push({ pack: id, text: `core range [${o.min}, ${o.below}) -> [${n.min}, ${n.below})` });
  }
  for (const id of Object.keys(was ?? {})) if (!(now ?? {})[id]) out.push({ pack: id, text: "pack removed" });
  return out;
}

export function parseManifest(text) {
  const f = (name) => text.match(new RegExp(`^\\s*${name}\\s+"([^"]*)"`, "m"))?.[1];
  const id = f("m_sPackId");
  return id ? { id, min: f("m_sCoreMin") ?? "", below: f("m_sCoreBelow") ?? "" } : null;
}

export function diffAll(was, now) {
  return {
    schema: diffSchemas(was.schema, now.schema),
    migrations: diffMigrations(was.migrations, now.migrations),
    shapes: diffShapes(was.shapes, now.shapes),
    manifests: diffManifests(was.manifests, now.manifests),
  };
}

const row = (k, a, b) => `| \`${k}\` | ${a} | ${b} | TODO: one line. |`;

export function renderConfig(s) {
  const rows = [];
  for (const r of s.retyped) rows.push(row(r.path, `type ${r.was}`, `type ${r.now}`));
  for (const r of s.defaults) rows.push(row(r.path, `default ${show(r.was)}`, `default ${show(r.now)}`));
  for (const r of s.ranges) rows.push(row(r.path, r.was, r.now));
  const lines = [];
  if (rows.length) lines.push("| Key | Was | Now | Why it changed |", "|---|---|---|---|", ...rows, "");
  for (const p of s.addedSections) lines.push(`- Added section: \`${p}\`. Leave it out to keep every default.`);
  for (const a of s.added) lines.push(`- Added: \`${a.path}\` (${a.type})${a.default === undefined ? "" : ` default \`${show(a.default)}\``}. Leave it out to keep the default.`);
  for (const p of s.removedSections) lines.push(`- Removed section: \`${p}\`. The server logs it as ignored; delete the block.`);
  for (const r of s.removed) lines.push(`- Removed: \`${r.path}\`. The server logs it as ignored; delete the line.`);
  return lines.length ? lines.join("\n").trimEnd() : "No config changes.";
}

export function renderSave(sh) {
  if (sh.from == null) return `Save version ${sh.to}. The older ref has no record-shapes lock, so the payload diff is not available: TODO (what changed, and whether old saves load).`;
  if (sh.from === sh.to && !sh.payloadsAdded.length && !sh.payloadsRemoved.length && !sh.payloadsChanged.length) {
    return `No save version changes (save version ${sh.to}).`;
  }
  const what = [
    ...sh.payloadsAdded.map((k) => `new payload \`${k}\``),
    ...sh.payloadsRemoved.map((k) => `payload \`${k}\` removed`),
    ...sh.payloadsChanged.map((c) => `payload \`${c.key}\` record \`${c.was}\` -> \`${c.now}\``),
  ];
  const keys = [...sh.payloadsAdded, ...sh.payloadsRemoved, ...sh.payloadsChanged.map((c) => c.key)].map((k) => `\`${k}\``).join(", ") || "(none)";
  return [
    "| Save version | Payload | What changed | Old saves |", "|---|---|---|---|",
    `| \`${sh.from}\` -> \`${sh.to}\` | ${keys} | ${what.join("; ") || "version bump only"} | TODO: load whole / load with a default / reset |`,
    "",
    "- Saves from before this release load: TODO (yes, every payload / no: list).",
    "- A reset is needed: TODO (no / yes: the one announced reset, and what it wipes).",
    `- After the upgrade, one restart must write and reload a save (manual). Watch for \`save loaded: version ${sh.to}\` in the console log.`,
  ].join("\n");
}

export function renderMigrations(list) {
  if (!list.length) return "None.";
  const lines = [];
  for (const m of list) {
    lines.push(`- \`${m.name}\` (schemaVersion ${m.from} -> ${m.from + 1}): ${m.description ?? ""}`.trimEnd());
    for (const c of m.changes ?? []) {
      lines.push(c.op === "rename" ? `  - rename \`${c.path}\` -> \`${c.to}\`` : c.op === "drop" ? `  - drop \`${c.path}\`` : `  - default \`${c.path}\` = ${show(c.value)}`);
    }
  }
  lines.push("", "The original file is backed up first. Preview on your file: `tools\\cli config-check <path\\to\\old-config.json> --no-loops`.");
  return lines.join("\n");
}

// Replace the body between a `## Heading` and the next `## ` (or the end) in the template.
function section(tpl, heading, body) {
  const re = new RegExp(`(^## ${heading}\\n)[\\s\\S]*?(?=^## |$(?![\\s\\S]))`, "m");
  return tpl.replace(re, (_m, h) => `${h}\n${body}\n\n`);
}

export function renderPage(template, { fromRef, toRef, version, d }) {
  let t = template.replace("# Upgrade to <x.y.z>", `# Upgrade to ${version || toRef}`);
  t = t.replace("Upgrading from `<previous x.y.z>`.", `Upgrading from \`${fromRef}\`.`);
  t = t.split("<previous x.y.z>").join(fromRef);
  if (version) t = t.split("<x.y.z>").join(version);
  const packs = d.manifests.length ? `\n\nPack manifest ranges: ${d.manifests.map((m) => `\`${m.pack}\` ${m.text}`).join("; ")}. A pack built for the old range does not load on this core.` : "";
  t = section(t, "Summary", `TODO: one or two sentences: what a server owner notices first. (Generated from \`${fromRef}\`..\`${toRef}\`.)${packs}`);
  t = section(t, "Config changes", `Edit your \`config.json\` (\`$profile:LifeFramework/config.json\`) by hand, then run \`tools\\cli config-check <path>\` (manual). Nothing here is required unless your server uses the feature.\n\n${renderConfig(d.schema)}`);
  t = section(t, "Save version changes", renderSave(d.shapes));
  t = section(t, "Migrations", renderMigrations(d.migrations));
  return t;
}
