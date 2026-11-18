// Plot keys and house identifiers share one key space in LF_Properties (a house row wins), so a plot key that
// equals a house identifier would sell that house at the plot price (#2783). Pure readers for the validator.

const lineOf = (text, index) => text.slice(0, index).split("\n").length;

// [{ id, line }] of every plot in a Configs/Property conf: `m_sPlotId "plot-a"`.
export function plotKeys(confText) {
  return [...String(confText).matchAll(/\bm_sPlotId\s+"([^"]*)"/g)].filter((m) => m[1]).map((m) => ({ id: m[1], line: lineOf(confText, m.index) }));
}

// [{ id, line }] of every house identifier a layer or prefab sets on an LF_HouseManagerComponent
// or an LF_LockComponent (a door names the house it belongs to; `m_sDebugIdentifier "x"` inside that component's braces).
// An LF_KeyComponent's identifier only names the house its key opens, so it is not a house.
export function houseIds(text) {
  const out = [];
  const src = String(text);
  const re = /\b(?:LF_HouseManagerComponent|LF_LockComponent)\b/g;
  for (let m = re.exec(src); m; m = re.exec(src)) {
    const open = src.indexOf("{", m.index + m[0].length);
    if (open < 0) break;
    // A header like `LF_HouseManagerComponent "{GUID}" {` has its GUID braces first: take the block's own brace.
    let start = open;
    const guid = /^\s*"\{[0-9A-Fa-f]+\}"\s*\{/.exec(src.slice(m.index + m[0].length));
    if (guid) start = m.index + m[0].length + guid[0].length - 1;
    let depth = 0;
    let end = src.length;
    for (let i = start; i < src.length; i++) {
      if (src[i] === "{") depth++;
      else if (src[i] === "}" && --depth === 0) {
        end = i;
        break;
      }
    }
    const body = src.slice(start, end);
    const id = /\bm_sDebugIdentifier\s+"([^"]*)"/.exec(body);
    if (id && id[1]) out.push({ id: id[1], line: lineOf(src, start + id.index) });
    re.lastIndex = end;
  }
  return out;
}

// plots: [{ file, id, line }], houses: [{ file, id, line }] -> one message per plot key that is also a house id.
export function collisions(plots, houses) {
  const byId = new Map();
  for (const h of houses) if (!byId.has(h.id)) byId.set(h.id, h);
  const out = [];
  for (const p of plots) {
    const h = byId.get(p.id);
    if (h) out.push(`${p.file}:${p.line} plot key "${p.id}" equals the house identifier "${h.id}" set at ${h.file}:${h.line}; buying the plot would sell the house at the plot price`);
  }
  return out;
}
