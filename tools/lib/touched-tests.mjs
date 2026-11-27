// Which `LF_Test_*` classes a diff should run before it merges (docs/testing/revert-audit-2026-11-27.md).
//
// `wt ship` has no pre-merge gate and GitHub's `validate` runs no game, so a test that pins a conf, a prefab or a
// string is first run by the main guard after the merge, and a red there reverts the PR. Of the 22 reverts up to
// 2026-11-27, 8 were a data change whose pinned test nobody ran (everon/job-tables after #5074, everon/mannequins
// after #5096, everon/reputation after #3767 and #3805, signs/next after #4899, ...) and 4 more were a persistence or
// test-world change the guard's e2e and full-suite legs caught. This module turns the changed file list into the
// exact `tools\cli test --no-build --only ...` commands that would have caught them. It only reads files and
// prints; it never fails a ship.
//
// Rules, cheapest first:
//   1. a changed test file runs its own class;
//   2. a changed non-test file runs every test whose source names the file's stem, or a class the file declares,
//      or (for a localization table) an Id the diff added;
//   2b. a changed .conf/.et/.layout also runs the tests that read it: the ones that name its file or {GUID}, the ones
//      that name a class whose script holds that path in a string (LF_SignCatalog.CONFIG_PATH, one helper deeper
//      for a fixture under Tests/), and for a conf the ones that name a top-level array member (m_aSigns). #5173 added
//      sign rows and signs/next, which walks them, only matched through a word in an assertion message. A symbol more
//      than 30 tests name is a base class: it prints a count and `--tier fast`, not the list;
//   3. a pack data change (addons/LF_Everon1989 Configs, Prefabs, Missions, Language, UI, Scripts) runs the whole pack
//      leg: a pack test reads the merged config, so it rarely names the file it pins (everon/job-tables never
//      names LF_Jobs_Everon1989.conf). The pack leg is one boot of about 40 s;
//   4. persistence files add `--tier persistence`; island or grid layers add the placement audit; the test world
//      pin lists add `--tier all`.

import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";

export const PACK_ROOT = "addons/LF_Everon1989/";
export const CORE_ROOT = "addons/LifeFramework/";
const TESTS_RE = /\/Scripts\/Game\/Tests\/LF_Test_[A-Za-z0-9_]+\.c$/;
const DATA_DIRS = ["Configs/", "Prefabs/", "Missions/", "Language/", "UI/", "Scripts/"];
const MIN_TOKEN = 6; // shorter stems ("Jobs", "Shop") name half the suite

const BROAD = 30; // a class more tests than this name is a base class: listing them hides the ones that matter
const RESOURCE_RE = /\.(conf|et|layout|st)$/i;

const norm = (p) => String(p).replace(/\\/g, "/");

// A resource a script loads by path or GUID. A string table is matched by the Ids the diff adds instead.
export const isResource = (p) => RESOURCE_RE.test(norm(p)) && !/\/Language\//.test(norm(p));
const declaredClasses = (text) => [...String(text).matchAll(/^\s*(?:modded\s+)?class\s+(LF_[A-Za-z0-9_]+)/gm)].map((m) => m[1]).filter((c) => c.length >= MIN_TOKEN);

export const isTestFile = (p) => TESTS_RE.test(norm(p));
export const classOfTestFile = (p) => norm(p).split("/").pop().replace(/\.c$/, "");

// The test classes of a checkout: [{ cls, path, pack, text }], from addons/*/Scripts/Game/Tests/LF_Test_*.c.
export function scanTests(root) {
  const out = [];
  const addons = join(root, "addons");
  if (!existsSync(addons)) return out;
  for (const a of readdirSync(addons)) {
    const dir = join(addons, a, "Scripts", "Game", "Tests");
    if (!existsSync(dir)) continue;
    for (const f of readdirSync(dir)) {
      if (!/^LF_Test_[A-Za-z0-9_]+\.c$/.test(f)) continue;
      const rel = `addons/${a}/Scripts/Game/Tests/${f}`;
      const text = readFileSync(join(dir, f), "utf8");
      // A helper or fixture (LF_Test_SignCatalog's neighbours) has no [LF_TestInfo]; `--only` cannot run it.
      out.push({ cls: f.replace(/\.c$/, ""), path: rel, pack: rel.startsWith(PACK_ROOT), runnable: /\[LF_TestInfo\(/.test(text), text });
    }
  }
  return out.sort((x, y) => x.cls.localeCompare(y.cls));
}

// Every non-test script of a checkout: [{ path, text }]. A changed resource is found in the one that loads it.
export function scanSources(root) {
  const out = [];
  const walk = (dir, rel) => {
    for (const e of readdirSync(dir, { withFileTypes: true })) {
      if (e.isDirectory()) { if (e.name !== "Tests") walk(join(dir, e.name), `${rel}/${e.name}`); continue; }
      if (e.name.endsWith(".c")) out.push({ path: `${rel}/${e.name}`, text: readFileSync(join(dir, e.name), "utf8") });
    }
  };
  const addons = join(root, "addons");
  if (!existsSync(addons)) return out;
  for (const a of readdirSync(addons)) {
    const dir = join(addons, a, "Scripts");
    if (existsSync(dir)) walk(dir, `addons/${a}/Scripts`);
  }
  return out;
}

// "data" the pack leg reads: not a test, not a .meta, not a sound or image.
export function isPackData(p) {
  p = norm(p);
  if (!p.startsWith(PACK_ROOT) || isTestFile(p) || p.endsWith(".meta")) return false;
  const rel = p.slice(PACK_ROOT.length);
  return DATA_DIRS.some((d) => rel.startsWith(d));
}

export function isCoreData(p) {
  p = norm(p);
  if (!p.startsWith(CORE_ROOT) || isTestFile(p) || p.endsWith(".meta")) return false;
  const rel = p.slice(CORE_ROOT.length);
  return DATA_DIRS.some((d) => rel.startsWith(d));
}

export const isPersistenceFile = (p) => /\/Persistence\/|SavePayload|LF_TestSaveVersion|record-shapes/.test(norm(p));
export const isLayer = (p) => /\/Worlds\/[^/]+\/[^/]+_Layers\/.+\.layer$/.test(norm(p));
export const isWorldPinFile = (p) => /^tools\/test\/(world-pins|town-tests)\.json$/.test(norm(p));

// The words in a changed file that a test could name.
export function tokensFor(file, text) {
  const f = norm(file);
  if (f.endsWith(".meta")) return [];
  if (/\/Language\//.test(f)) return []; // a string table is matched by the Ids the diff adds (addedStringIds), not its file name
  const base = f.split("/").pop();
  const toks = new Set();
  const stem = base.replace(/\.(c|conf|et|layout|st|json|mjs|md)$/i, "");
  if (stem.length >= MIN_TOKEN && !/^(Feature|README)$/i.test(stem)) toks.add(stem);
  if (text && f.endsWith(".c")) {
    for (const m of text.matchAll(/^\s*(?:modded\s+)?class\s+(LF_[A-Za-z0-9_]+)/gm)) if (m[1].length >= MIN_TOKEN) toks.add(m[1]);
  }
  return [...toks];
}

// The Ids a localization diff adds: `+     Id "LF-Foo"` lines of `git diff -U0`.
export function addedStringIds(diffText) {
  const ids = new Set();
  for (const m of String(diffText ?? "").matchAll(/^\+\s*Id "([^"]+)"/gm)) ids.add(m[1]);
  return [...ids];
}

const has = (text, tok) => new RegExp(`(^|[^A-Za-z0-9_-])${tok.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}($|[^A-Za-z0-9_-])`).test(text);

/**
 * @param files changed paths, repo-relative
 * @param tests scanTests() rows
 * @param readFile (path) -> text | null, for the class names a changed .c declares
 * @param addedIds localization Ids the diff added
 * @returns { named: Map<cls, string[]>, packLeg: string[]|null, persistence: string[], layers: string[], worldPins: string[], coreTouched: boolean }
 */
export function pickTests({ files, tests, readFile = () => null, addedIds = [], sources = [] }) {
  const fs = [...new Set(files.map(norm))];
  const byCls = new Map(tests.map((t) => [t.cls, t]));
  const runnable = tests.filter((t) => t.runnable !== false);
  const named = new Map();
  const dataCls = new Set(); // tests named because they read a changed resource, shown first
  const broad = []; // [{ file, via, count }] a symbol too many tests name to list one by one
  const add = (cls, why, data = false) => {
    if (!named.has(cls)) named.set(cls, []);
    const w = named.get(cls);
    if (!w.includes(why)) w.push(why);
    if (data) dataCls.add(cls);
  };
  let srcs = null;
  const allSources = () => (srcs ??= typeof sources === "function" ? sources() : sources);
  for (const f of fs) {
    if (isTestFile(f)) {
      const cls = classOfTestFile(f);
      if (byCls.has(cls) && byCls.get(cls).runnable !== false) add(cls, "its own file changed");
      continue;
    }
    if (!f.startsWith("addons/") || f.endsWith(".meta")) continue;
    const resource = isResource(f);
    const base = f.split("/").pop();
    const toks = tokensFor(f, f.endsWith(".c") ? readFile(f) : null);
    for (const t of runnable) for (const tok of toks) if (has(t.text, tok)) add(t.cls, `names ${tok} (${base})`, resource);
    if (resource) resourceReaders(f, base);
  }
  // A changed conf / prefab / layout is read by the tests that name it (by file name or {GUID}), by the tests that name
  // a class whose script names it (LF_SignCatalog.CONFIG_PATH), one helper deeper, and for a conf, by the tests that
  // name one of its top-level array members (m_aSigns: "every sign says what comes next").
  function resourceReaders(f, base) {
    const guid = (readFile(`${f}.meta`) ?? "").match(/Name "\{([0-9A-F]{16})\}/)?.[1] ?? "";
    const names = (text) => has(text, base) || (guid && text.includes(`{${guid}}`));
    const direct = new Set();
    for (const t of runnable) if (names(t.text)) { add(t.cls, `reads ${base}`, true); direct.add(t.cls); }
    // A loader holds the path in a string literal (`"{GUID}Configs/Signs/LF_Signs.conf"`); a comment that names the file is not a read.
    const quoted = (text) => text.split("\n").some((l) => !/^\s*\/\//.test(l) && l.split('"').some((seg, k) => k % 2 === 1 && (seg.includes(base) || (guid && seg.includes(`{${guid}}`)))));
    const loaders = allSources().filter((s) => quoted(s.text));
    const symbols = new Map(); // symbol -> via text
    for (const s of loaders) for (const c of declaredClasses(s.text)) symbols.set(c, c);
    for (const h of tests) { // a helper or fixture under Tests/ that names a loader class
      if (h.runnable !== false || !([...symbols.keys()].some((c) => has(h.text, c)) || quoted(h.text))) continue;
      for (const c of [h.cls, ...declaredClasses(h.text)]) if (!symbols.has(c)) symbols.set(c, `helper ${c}`);
    }
    for (const [sym, via] of symbols) {
      const hit = runnable.filter((t) => !direct.has(t.cls) && has(t.text, sym));
      if (hit.length > BROAD) { broad.push({ file: base, via: sym, count: hit.length }); continue; }
      for (const t of hit) add(t.cls, `${base} via ${via}`, true);
    }
    if (f.endsWith(".conf")) {
      const members = new Set();
      for (const m of String(readFile(f) ?? "").matchAll(/^ (m_a[A-Za-z0-9_]{4,})\s*\{/gm)) members.add(m[1]);
      for (const mem of members) {
        const hit = runnable.filter((t) => !direct.has(t.cls) && has(t.text, mem));
        if (hit.length > BROAD) { broad.push({ file: base, via: mem, count: hit.length }); continue; }
        for (const t of hit) add(t.cls, `${base} rows via ${mem}`, true);
      }
    }
  }
  if (addedIds.length) {
    for (const t of runnable) for (const id of addedIds) if (has(t.text, id)) add(t.cls, `names string ${id}`);
  }
  const packTouched = fs.filter(isPackData);
  const packLeg = packTouched.length ? runnable.filter((t) => t.pack).map((t) => t.cls) : null;
  return {
    named,
    dataCls,
    broad,
    packLeg,
    packTouched,
    coreTouched: fs.some(isCoreData),
    persistence: fs.filter(isPersistenceFile),
    layers: fs.filter(isLayer),
    worldPins: fs.filter(isWorldPinFile),
  };
}

// The lines `wt ship` prints. [] when the diff touches nothing a test could pin.
export function renderLines(r) {
  const packSet = new Set(r.packLeg ?? []);
  const lines = [];
  // Named tests the pack leg already covers are not listed twice.
  const rest = [...r.named.keys()].filter((c) => !packSet.has(c));
  // Tests that read a changed conf, prefab or layout come first: they are the ones a data edit breaks. Then the tests
  // that only name a changed class.
  const dataSet = r.dataCls ?? new Set();
  const group = (cs, label) => {
    if (!cs.length) return;
    if (label) lines.push(`  ${label}`);
    lines.push(`  tools\\cli test --no-build --only ${cs.join(",")}`);
    // One line per reason, not per test: a base class such as LF_UIScreen is named by dozens.
    const byWhy = new Map();
    for (const c of cs) for (const w of r.named.get(c)) byWhy.set(w, [...(byWhy.get(w) ?? []), c]);
    const dataWhy = (w) => (/^reads | via /.test(w) ? 0 : 1); // reasons that name the data itself first
    for (const [w, ts] of [...byWhy].sort((x, y) => dataWhy(x[0]) - dataWhy(y[0])).slice(0, 6)) lines.push(`    ${w}: ${ts.slice(0, 3).join(", ")}${ts.length > 3 ? ` and ${ts.length - 3} more` : ""}`);
    if (byWhy.size > 6) lines.push(`    ... and ${byWhy.size - 6} more reasons`);
  };
  group(rest.filter((c) => dataSet.has(c)), "tests that read the changed data:");
  group(rest.filter((c) => !dataSet.has(c)), rest.some((c) => dataSet.has(c)) && rest.some((c) => !dataSet.has(c)) ? "tests that name the changed code:" : "");
  for (const b of (r.broad ?? []).slice(0, 3)) lines.push(`    ${b.count} more tests name ${b.via}, which loads ${b.file}: too many to list; run tools\\cli test --no-build --tier fast`);
  if (r.packLeg?.length) {
    const named = [...r.named.keys()].filter((c) => packSet.has(c));
    lines.push(`  pack data changed (${r.packTouched.slice(0, 3).map((p) => p.split("/").pop()).join(", ")}${r.packTouched.length > 3 ? ", ..." : ""}); a pack test reads the merged config and GitHub validate runs no game, so run the whole pack leg (${r.packLeg.length} tests, one boot):`);
    lines.push(`  tools\\cli test --no-build --only ${r.packLeg.join(",")}`);
    if (named.length) lines.push(`    pack tests that name a changed file: ${named.slice(0, 8).join(", ")}${named.length > 8 ? ", ..." : ""}`);
  }
  if (r.persistence.length) {
    lines.push(`  persistence files changed (${r.persistence.slice(0, 3).map((p) => p.split("/").pop()).join(", ")}${r.persistence.length > 3 ? ", ..." : ""}); the main guard's pre-move e2e and restart proofs read them:`);
    lines.push("  tools\\cli test --no-build --tier persistence   and   tools\\cli wt e2e <slug>");
  }
  if (r.layers.length) {
    lines.push(`  world layer changed (${r.layers.slice(0, 3).map((p) => p.split("/").pop()).join(", ")}${r.layers.length > 3 ? ", ..." : ""}):`);
    lines.push(`  tools\\cli audit placement --strict${r.layers.some((p) => /\/GridWorld\//.test(p)) ? "   and   tools\\cli audit placement --grid --strict" : ""}`);
  }
  if (r.worldPins.length) {
    lines.push(`  ${r.worldPins.map((p) => p.split("/").pop()).join(", ")} decides which world each test boots in:`);
    lines.push("  tools\\cli test --no-build --tier all");
  }
  if (!lines.length && r.coreTouched) lines.push("  core script or data changed and no test names it: tools\\cli test --no-build --tier fast");
  if (!lines.length) return [];
  return ["ship: the main guard runs the whole suite after the merge and reverts a red PR; run what this diff can break first (advice only, nothing blocks):", ...lines];
}
