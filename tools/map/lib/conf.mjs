// tools/map/lib/conf.mjs - a reader for Enfusion text confs, enough for the map data:
//   ClassName [instance] {      opens a block        }                       closes it
//   Key value value ...         a scalar line        Key { a b c }           a one-line array
// Quoted strings lose their quotes. Returns { class, name, props, children } for the top block.
export function parseConf(text) {
  const root = { class: "", name: "", props: {}, children: [] };
  const stack = [root];
  for (const raw of text.split("\n")) {
    const line = raw.replace(/\/\/.*$/, "").trim();
    if (!line) continue;
    const cur = stack[stack.length - 1];
    if (line === "}") {
      if (stack.length > 1) {
        const done = stack.pop();
        // A multi-line scalar array (`m_aPoints {` then one quoted value per line) is a prop, not a block.
        if (done.items && !done.children.length && !Object.keys(done.props).length) {
          const parent = stack[stack.length - 1];
          parent.children.splice(parent.children.indexOf(done), 1);
          parent.props[done.class] = done.items;
        }
      }
      continue;
    }
    const arr = line.match(/^([A-Za-z_]\w*)\s*\{\s*(.*?)\s*\}$/);
    if (arr) {
      cur.props[arr[1]] = tokens(arr[2]);
      continue;
    }
    const open = line.match(/^([A-Za-z_]\w*)(?:\s+("[^"]*"|[^\s{]+))?\s*\{$/);
    if (open) {
      const b = { class: open[1], name: (open[2] || "").replace(/^"|"$/g, ""), props: {}, children: [] };
      cur.children.push(b);
      stack.push(b);
      continue;
    }
    const kv = line.match(/^([A-Za-z_]\w*)\s+(.*)$/);
    if (kv) {
      const t = tokens(kv[2]);
      cur.props[kv[1]] = t.length === 1 ? t[0] : t;
      continue;
    }
    // A bare value line inside a block: an item of a multi-line scalar array.
    if (stack.length > 1) (cur.items ||= []).push(...tokens(line));
  }
  return root.children.length === 1 && !Object.keys(root.props).length ? root.children[0] : root;
}

function tokens(s) {
  const out = [];
  for (const m of s.matchAll(/"([^"]*)"|(\S+)/g)) out.push(m[1] !== undefined ? m[1] : m[2]);
  return out;
}

// Every block of one class anywhere under `node`.
export function findBlocks(node, cls, out = []) {
  for (const c of node.children || []) {
    if (c.class === cls) out.push(c);
    findBlocks(c, cls, out);
  }
  return out;
}

export const num = (v, d = 0) => {
  const n = parseFloat(Array.isArray(v) ? v[0] : v);
  return Number.isFinite(n) ? n : d;
};
export const str = (v, d = "") => (v === undefined ? d : Array.isArray(v) ? v.join(" ") : String(v));
// An array<vector> value -> [[x, z], ...]. The engine reads it as one flat run of numbers, three per vector
// (`m_aPoints { 1 0 2 3 0 4 }`); a quoted "x y z" per entry loads as zeros in game (the map proof pass), but
// the old quoted form still parses here.
export const vecListXZ = (v) => {
  const items = Array.isArray(v) ? v : v === undefined ? [] : [v];
  if (items.length && items.every((t) => !/\s/.test(String(t).trim()))) {
    const out = [];
    for (let i = 0; i + 2 < items.length; i += 3) out.push([Number(items[i]), Number(items[i + 2])]);
    return out;
  }
  return items.map((t) => vecXZ(t));
};
// "x y z" -> [x, z]
export const vecXZ = (v) => {
  const a = (Array.isArray(v) ? v.join(" ") : String(v)).trim().split(/\s+/).map(Number);
  return a.length >= 3 ? [a[0], a[2]] : [a[0], a[1]];
};
