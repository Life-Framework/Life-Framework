// tools/validation/guid-paths-selftest.mjs
// inputs: tools/**, addons/**
// Plants GUID-less resource paths in a temp addon and asserts validate-guid-paths fails them;
// a GUID path, a comment, log text, a path builder, a $profile: path, a Tests/ file and a
// baselined finding pass; a stale, issue-less or new baseline entry fails.

import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { checkGuidPaths } from "./validate-guid-paths.mjs";

function put(root, path, text) {
  const full = join(root, "addons", path);
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
}

const script = (body) => `class LF_X\n{\n\tstatic void Run()\n\t{\n${body}\n\t}\n}\n`;
const GOOD = "Pack/Scripts/Game/Feature/Crime/LF_Good.c";
const BAD = "Pack/Scripts/Game/Feature/Crime/LF_Bad.c";

function tree() {
  const root = mkdtempSync(join(tmpdir(), "lf-guidpaths-"));
  put(root, GOOD, script([
    '\t\tResourceName a = "{D6DE32D1C0FCC1C7}Prefabs/Editor/Camera/ManualCameraBase.et";',
    '\t\t// ResourceName b = "Prefabs/Old/Commented.et";',
    '\t\t/* "Configs/Block/Commented.conf" */',
    '\t\tLF_Debug.Error("Shop", "missing Prefabs/Items/Apple.et in the catalog");',
    '\t\tstring c = "Prefabs/" + name + ".et";',
    '\t\tstring d = "{D6DE32D1C0FCC1C7}Prefabs/Items/%1.et";',
    '\t\tstring e = "$profile:LifeFramework/dev/dialogue.json";',
    '\t\tstring f = ".et";',
    '\t\tstring g = "Role id, a row of tools/assets/outfits/roles.json";',
    '\t\tstring h = "a // Prefabs/Fake.et";',
  ].join("\n")));
  put(root, "Pack/Scripts/Game/Tests/LF_Test_Uses.c", script('\t\tstring p = "Prefabs/Characters/Core/Character_Roleplay.et";'));
  return root;
}

const bad = (line) => (r) => put(r, BAD, script(line));
const cases = [
  { name: "a bare Prefabs path", plant: bad('\t\tResourceName p = "Prefabs/Editor/Camera/ManualCameraBase.et";'), expect: /Pack\/Scripts\/Game\/Feature\/Crime\/LF_Bad\.c: `Prefabs\/Editor\/Camera\/ManualCameraBase\.et`/ },
  { name: "the null GUID", plant: bad('\t\tResourceName p = "{0000000000000000}Prefabs/Editor/Camera/ManualCameraBase.et";'), expect: /`\{0000000000000000\}Prefabs/ },
  { name: "a short GUID", plant: bad('\t\tResourceName p = "{D6DE32D1}Prefabs/X/Y.et";'), expect: /`\{D6DE32D1\}Prefabs\/X\/Y\.et`/ },
  { name: "a bare Configs path", plant: bad('\t\tResourceName p = "Configs/Block/Things.conf";'), expect: /`Configs\/Block\/Things\.conf`/ },
  { name: "a bare layout path", plant: bad('\t\tResourceName p = "UI/layouts/Hud/Thing.layout";'), expect: /`UI\/layouts\/Hud\/Thing\.layout`/ },
  { name: "a bare string table path", plant: bad('\t\tResourceName p = "Language/everonlife_localization.st";'), expect: /Language\/everonlife_localization\.st/ },
  { name: "a bare path in a second addon", plant: (r) => put(r, "Other/Scripts/Game/LF_O.c", script('\t\tResourceName p = "Worlds/Debug/Debug.ent";')), expect: /addons\/Other\/Scripts\/Game\/LF_O\.c: `Worlds\/Debug\/Debug\.ent`/ },
];

let failed = 0;
const roots = [];
const mk = () => { const r = tree(); roots.push(r); return r; };
try {
  const clean = checkGuidPaths(mk(), { entries: [] });
  if (clean.errors.length) {
    failed++;
    console.log(`FAIL clean tree reported: ${clean.errors.join("; ")}`);
  }

  for (const c of cases) {
    const root = mk();
    c.plant(root);
    const { errors } = checkGuidPaths(root, { entries: [] });
    if (!errors.some((e) => c.expect.test(e))) {
      failed++;
      console.log(`FAIL ${c.name}: expected ${c.expect}, got:\n  ${errors.join("\n  ") || "(no errors)"}`);
    }
  }

  const entry = { file: BAD, rule: "no-guid", token: "Prefabs/Editor/Camera/ManualCameraBase.et", issue: "#506" };
  const based = mk();
  cases[0].plant(based);
  const ok = checkGuidPaths(based, { entries: [entry] });
  if (ok.errors.length) { failed++; console.log(`FAIL a baselined finding was flagged: ${ok.errors.join("; ")}`); }

  const stale = checkGuidPaths(mk(), { entries: [entry] });
  if (!stale.errors.some((e) => /is gone from the code/.test(e))) { failed++; console.log("FAIL a stale baseline entry passed"); }

  const noIssue = checkGuidPaths(based, { entries: [{ ...entry, issue: "" }] });
  if (!noIssue.errors.some((e) => /has no issue/.test(e))) { failed++; console.log("FAIL a baseline entry with no issue passed"); }

  const grown = checkGuidPaths(based, { entries: [entry] }, new Set());
  if (!grown.errors.some((e) => /may only shrink/.test(e))) { failed++; console.log("FAIL a baseline entry the main branch lacks passed"); }
} finally {
  for (const r of roots) rmSync(r, { recursive: true, force: true });
}

if (failed) {
  console.log(`guid-paths-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log(`guid-paths-selftest: OK (${cases.length} kinds caught; GUID paths, comments, log text, path builders, $profile:, Tests/ and baselined findings pass; stale, issue-less and grown entries fail)`);
