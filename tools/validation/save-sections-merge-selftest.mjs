// tools/validation/save-sections-merge-selftest.mjs
// inputs: tools/validation/save-sections-merge-selftest.mjs, tools/validation/validate-record-shapes.mjs, addons/LifeFramework/Scripts/Game/Persistence/**, tools/validation/record-shapes.json
//
// #2841: two PRs that each add a save payload merge without a conflict. In a throwaway repo seeded with
// the real serializer, LF_SavePayload.c and the record-shapes lock, two branches each add one section the
// documented way (a section class and its row class in a feature folder, and its lock file). The merge
// must be clean, and the merged tree must hold both sections with no name or lock error.
// The old way (each branch appends an Insert to RegisterPersistencePayloads) is merged too and must
// conflict: that is the collision the sections remove, and it shows the scratch repo can see one.
//
//   node tools/validation/save-sections-merge-selftest.mjs
//
// red-proof (2026-11-18): with branch B naming its section "alpha" too, it failed `two section PRs merge
// cleanly` (CONFLICT (add/add) in tools/validation/record-shapes/alpha.json): a name collision still shows.
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gitMust, removeScratch, runGit, useHermeticGit } from "../lib/selftest-git.mjs";
import { scratchDir } from "../lib/scratch.mjs";
import { SECTION_LOCKS, SERIALIZER, buildSectionLock, checkSectionNames, collectShapes, compareSection, indexClasses, parseSections, parseSerializer } from "./validate-record-shapes.mjs";

useHermeticGit();
const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const SAVE_PAYLOAD = "addons/LifeFramework/Scripts/Game/Persistence/LF_SavePayload.c";
const LEGACY_LOCK = "tools/validation/record-shapes.json";
let failed = 0;
const check = (ok, label, detail = "") => {
  if (!ok) {
    failed++;
    console.log(`FAIL ${label}${detail ? `: ${detail}` : ""}`);
  }
};

const put = (dir, rel, text) => {
  mkdirSync(dirname(join(dir, rel)), { recursive: true });
  writeFileSync(join(dir, rel), text);
};

// The files one section PR adds, named after its area.
function sectionFiles(area, name) {
  const cls = `LF_${area}SavePayload`;
  const row = `LF_${area}Row`;
  const feature = `addons/LifeFramework/Scripts/Game/Feature/${area}`;
  const rowText = `class ${row}\n{\n\tint m_iVersion = 1;\n\tstring m_sKey;\n\tint m_iCount;\n}\n`;
  const sectionText = [
    `class ${cls} : LF_SavePayload`,
    "{",
    `\tstatic const string NAME = "${name}";`,
    "\toverride string Name()\n\t{\n\t\treturn NAME;\n\t}",
    "\toverride void WriteSection(notnull SaveContext context)\n\t{",
    `\t\tarray<ref ${row}> rows = {};`,
    '\t\tcontext.WriteValue("rows", rows);',
    "\t}",
    "\toverride bool ReadSection(notnull LoadContext context, int savedVersion)\n\t{",
    `\t\tarray<ref ${row}> rows = {};`,
    '\t\treturn context.ReadValue("rows", rows);',
    "\t}",
    "}",
    "",
  ].join("\n");
  const files = { [`${feature}/${row}.c`]: rowText, [`${feature}/${cls}.c`]: sectionText };
  const fileMap = new Map(Object.entries(files));
  const [sec] = parseSections(fileMap);
  const { shapes } = collectShapes(sec.roots.map((r) => ({ payload: name, root: r })), indexClasses(fileMap).classes);
  files[`${SECTION_LOCKS}/${name}.json`] = JSON.stringify(buildSectionLock(sec, shapes), null, 2) + "\n";
  return files;
}

const temp = scratchDir("lf-save-sections-merge-");
try {
  const repo = join(temp, "repo");
  mkdirSync(repo, { recursive: true });
  gitMust(repo, ["init", "-q"]);
  for (const rel of [SERIALIZER, SAVE_PAYLOAD, LEGACY_LOCK]) put(repo, rel, readFileSync(join(root, rel), "utf8"));
  gitMust(repo, ["add", "-A"]);
  gitMust(repo, ["commit", "-q", "-m", "base"]);
  const base = gitMust(repo, ["rev-parse", "HEAD"]);

  const branch = (name, files) => {
    gitMust(repo, ["checkout", "-q", "-b", name, base]);
    for (const [rel, text] of Object.entries(files)) put(repo, rel, text);
    gitMust(repo, ["add", "-A"]);
    gitMust(repo, ["commit", "-q", "-m", name]);
  };

  // The new way: two sections, no shared file.
  branch("ws/alpha", sectionFiles("Alpha", "alpha"));
  branch("ws/beta", sectionFiles("Beta", "beta"));
  gitMust(repo, ["checkout", "-q", "ws/alpha"]);
  const merge = runGit(repo, ["merge", "--no-edit", "-q", "ws/beta"]);
  check(merge.status === 0, "two section PRs merge cleanly", `${merge.stdout}${merge.stderr}`);
  if (merge.status !== 0) runGit(repo, ["merge", "--abort"]);
  if (merge.status === 0) {
    const files = new Map();
    for (const rel of gitMust(repo, ["ls-files", "addons"]).split("\n").filter((f) => f.endsWith(".c"))) {
      files.set(rel, readFileSync(join(repo, rel), "utf8"));
    }
    const sections = parseSections(files);
    const names = sections.map((s) => s.name).join(",");
    check(names === "alpha,beta", "the merged tree holds both sections", names);
    const legacy = parseSerializer(files.get(SERIALIZER)).order;
    const lockNames = gitMust(repo, ["ls-files", SECTION_LOCKS]).split("\n").filter(Boolean).map((f) => f.split("/").pop().replace(/\.json$/, ""));
    const nameErrors = checkSectionNames(sections, legacy, lockNames);
    check(!nameErrors.length, "the merged sections have unique names and a lock each", nameErrors.join(" | "));
    const { classes } = indexClasses(files);
    for (const sec of sections) {
      const lock = JSON.parse(readFileSync(join(repo, SECTION_LOCKS, `${sec.name}.json`), "utf8"));
      const { shapes } = collectShapes(sec.roots.map((r) => ({ payload: sec.name, root: r })), classes);
      const errors = compareSection(lock, sec, shapes);
      check(!errors.length, `section ${sec.name} matches its lock after the merge`, errors.map((e) => e.msg).join(" | "));
    }
  }

  // The old way: each branch appends to the one payload list. Both touch the same lines.
  const appendLegacy = (name) => {
    const text = readFileSync(join(root, SERIALIZER), "utf8");
    const last = [...text.matchAll(/\t\torder\.Insert\("(\w+)"\);\n/g)].pop();
    return { [SERIALIZER]: text.slice(0, last.index + last[0].length) + `\t\torder.Insert("${name}");\n` + text.slice(last.index + last[0].length) };
  };
  branch("ws/legacy-alpha", appendLegacy("alpha"));
  branch("ws/legacy-beta", appendLegacy("beta"));
  gitMust(repo, ["checkout", "-q", "ws/legacy-alpha"]);
  const legacyMerge = runGit(repo, ["merge", "--no-edit", "-q", "ws/legacy-beta"]);
  check(legacyMerge.status !== 0, "two PRs appending to the legacy payload list conflict (the scratch repo can see a collision)");
  if (legacyMerge.status !== 0) runGit(repo, ["merge", "--abort"]);
} finally {
  removeScratch(temp);
}

if (failed) {
  console.log(`save-sections merge selftest: ${failed} check(s) failed`);
  process.exit(1);
}
console.log("save-sections merge selftest: two section PRs merge cleanly; two legacy appends conflict");
