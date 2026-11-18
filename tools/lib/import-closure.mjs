// The files a script loads through relative imports, transitively (tools/validation/validate-check-inputs.mjs).
// Static only: `import x from "./a.mjs"`, `import "./a.mjs"`, `export ... from "./a.mjs"`, `import("./a.mjs")` and
// `require("./a.js")` with a literal path. A computed path (`import(pathToFileURL(...))`) is invisible here, so a
// check that loads code that way must list a broad glob (`tools/**`) by hand.
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";

const SPEC = /(?:\bimport\s*(?:[^'"()]*?\bfrom\s*)?|\bexport\s+(?:\*|\{[^}]*\})\s*from\s*|\bimport\s*\(\s*|\brequire\s*\(\s*)["'](\.{1,2}\/[^"']+)["']/g;
const CODE = /\.(mjs|js|cjs)$/;

// Repo-relative path (forward slashes) of what `spec` names from `fromRel`, with the extension the
// loader would find; null when nothing is there.
export function resolveSpec(root, fromRel, spec) {
  const rel = join(dirname(fromRel), spec).split("\\").join("/");
  for (const cand of [rel, `${rel}.mjs`, `${rel}.js`, `${rel}/index.mjs`]) if (existsSync(join(root, cand)) && CODE.test(cand)) return cand;
  return existsSync(join(root, rel)) ? rel : null;
}

// The repo paths one file imports directly (cached in `cache`, a Map shared across calls of one run).
function importsOf(root, f, cache) {
  let list = cache.get(f);
  if (list) return list;
  list = [];
  if (CODE.test(f) && existsSync(join(root, f))) {
    for (const m of readFileSync(join(root, f), "utf8").matchAll(SPEC)) {
      const r = resolveSpec(root, f, m[1]);
      if (r) list.push(r);
    }
  }
  cache.set(f, list);
  return list;
}

// Every file `rel` loads, itself included, sorted. Pass one `cache` Map across many calls to read each module once.
export function importClosure(root, rel, cache = new Map()) {
  const seen = new Set();
  const queue = [rel];
  while (queue.length) {
    const f = queue.pop();
    if (seen.has(f)) continue;
    seen.add(f);
    queue.push(...importsOf(root, f, cache));
  }
  return [...seen].sort();
}
