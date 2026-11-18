// tools/lib/conflict-markers.mjs - find committed git conflict markers (validate-no-conflict-markers).
//
// 2026-11-18: 33884d14b (#2683 site-rent merge) put an ours/base/theirs conflict block into a
// Feature.md on main, and docs/features/map.md carried a lone diff3 base marker for days. No
// validator looked, so CI stayed green.
//
// Pure helpers; the validator owns the file walk.

/** Line kinds git writes: ours, diff3 base, theirs (each followed by a space), and the separator (exactly 7 `=` alone). */
const MARKER = /^(<<<<<<< |\|\|\|\|\|\|\| |>>>>>>> |=======\r?$)/;

/** [{ line, text }] for each marker line (1-based). Accepts a string or Buffer (latin1). */
export function conflictMarkerLines(data) {
  const text = Buffer.isBuffer(data) ? data.toString("latin1") : String(data);
  const hits = [];
  const lines = text.split("\n");
  for (let i = 0; i < lines.length; i++) {
    if (MARKER.test(lines[i])) hits.push({ line: i + 1, text: lines[i].replace(/\r$/, "").slice(0, 40) });
  }
  return hits;
}

/**
 * Compile an allowlist of repo-relative forward-slash globs (`*` stays inside a folder, `**` crosses
 * folders) into a predicate on a path.
 */
export function allowMatcher(globs) {
  const rules = globs.map((g) => {
    const body = g
      .replace(/[.+^${}()|[\]\\]/g, "\\$&")
      .replace(/\*\*/g, "\u0000")
      .replace(/\*/g, "[^/]*")
      .replace(/\u0000/g, ".*");
    return new RegExp(`^${body}$`);
  });
  return (path) => rules.some((r) => r.test(path));
}
