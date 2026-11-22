// Shared by validate-sign-ids and its selftest: a placed sign with no sign id.
//
// A sign prefab whose own mesh samples `$rendertarget` draws solid black until a face is printed on it. The
// face is printed from the sign id, so a placed instance whose id is empty in its whole prefab chain was a
// black slab (the bakery and butcher fascias on the island and the taxi stand plate, 2026-11-22). The component
// now prints a blank painted board for such a sign, but the sign still reads nothing: the id is required.

// files: Map<repo path with forward slashes, text> of every .et, .layer and .meta under addons/.
// Returns [{ path, line, name, prefab }].
export function signsWithoutId(files) {
  const byGuid = new Map();
  for (const [p, t] of files) {
    if (!p.endsWith(".meta")) continue;
    const m = /Name "\{([0-9A-Fa-f]{16})\}/.exec(t);
    if (m) byGuid.set(m[1].toUpperCase(), p.slice(0, -5));
  }

  const parentOf = (text) => {
    const m = /^\w+[^\n{:]*:\s*"\{([0-9A-Fa-f]{16})\}/.exec(text);
    return m ? byGuid.get(m[1].toUpperCase()) || null : null;
  };
  const chainCache = new Map();
  const chainTexts = (path) => {
    if (chainCache.has(path)) return chainCache.get(path);
    const out = [];
    for (let p = path, i = 0; p && files.has(p) && i < 24; i++) {
      out.push(files.get(p));
      p = parentOf(files.get(p));
    }
    chainCache.set(path, out);
    return out;
  };
  const firstId = (texts) => {
    for (const t of texts) {
      const m = /m_sSignId "([^"]*)"/.exec(t);
      if (m) return m[1];
    }
    return null;
  };
  const faceOff = (texts) => {
    for (const t of texts) {
      const m = /m_bShowFace (\d)/.exec(t);
      if (m) return m[1] === "0";
    }
    return false;
  };
  const isSign = (path) => chainTexts(path).some((t) => t.includes("LF_SignComponent"));

  const bad = [];
  const check = (path, entity, prefab) => {
    if (!prefab || !isSign(prefab)) return;
    const texts = [entity.body, ...chainTexts(prefab)];
    const id = firstId(texts);
    if ((id === null || id === "") && !faceOff(texts)) bad.push({ path, line: entity.line, name: entity.name, prefab });
  };

  for (const [path, text] of files) {
    if (!path.endsWith(".layer")) continue;
    const lines = text.split("\n");
    let depth = 0;
    let group = null;
    let cur = null;
    for (let i = 0; i < lines.length; i++) {
      const l = lines[i];
      if (depth === 0) {
        group = null;
        const g = /^\$grp\s+\w+\s*:\s*"\{([0-9A-Fa-f]{16})\}/.exec(l);
        const e = /^\w+\s+(\w+)\s*:\s*"\{([0-9A-Fa-f]{16})\}[^"]*"\s*\{/.exec(l);
        if (g) group = byGuid.get(g[1].toUpperCase()) || null;
        else if (e) cur = { name: e[1], prefab: byGuid.get(e[2].toUpperCase()) || null, line: i + 1, lines: [] };
      } else if (depth === 1 && cur === null && /^\s*\w+\s*\{\s*$/.test(l)) {
        cur = { name: l.trim().split(/\s+/)[0], prefab: group, line: i + 1, lines: [] };
      }
      if (cur) cur.lines.push(l);
      for (const ch of l) {
        if (ch === "{") depth++;
        else if (ch === "}") depth--;
      }
      const top = cur && group === null && depth === 0;
      const inGroup = cur && group !== null && depth === 1;
      if (top || inGroup) {
        cur.body = cur.lines.join("\n");
        check(path, cur, cur.prefab);
        cur = null;
      }
    }
  }
  return bad;
}
