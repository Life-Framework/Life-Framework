// tools/validation/release-ready-selftest.mjs
// inputs: tools/**, agent-box/**, .githooks/**, addons/**, docs/**, website/**, video/**
//
// Proves validate-release-ready fails on each missing field and names it. Each
// case builds a fixture repo (clean core, pack and kits), plants one defect and
// asserts the report names it. The clean tree must pass; a pack with no kit
// folder only warns.
// red-proof: with the TITLE check removed in validate-release-ready.mjs this
// printed "FAIL core gproj without TITLE: expected /LifeFramework.gproj: missing
// TITLE/" and exited 1 (2026-11-11).

import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { checkRelease, CORE, PACK } from "./validate-release-ready.mjs";

const CORE_GUID = "79636E668EA37AC9";
const PACK_GUID = "B1E84D5687E11724";
const BASE = "58D0FB3206B6F859";
const KIT = "docs/marketing/workshop";
const PACK_KIT = `${KIT}/everon-1989`;

function put(root, path, text) {
  const full = join(root, path);
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
}

function gproj(id, guid, title, deps) {
  const t = title ? ` TITLE "${title}"\n` : "";
  return `GameProject {\n ID "${id}"\n GUID "${guid}"\n${t} Dependencies {\n  ${deps.map((d) => `"${d}"`).join(" ")}\n }\n}\n`;
}

function kit(root, dir, licence = "Arma Public License (APL)", version = "`0.1.0`") {
  put(root, `${dir}/summary.txt`, "A roleplay framework.\n");
  put(root, `${dir}/description.bbcode`, "[b]Life Framework[/b]\n");
  put(root, `${dir}/preview.jpg`, "x".repeat(2048));
  put(root, `${dir}/README.md`, `| Field | Value |\n|---|---|\n| Version | ${version} |\n| Licence | ${licence} |\n`);
}

function cleanTree(withPackKit = true) {
  const root = mkdtempSync(join(tmpdir(), "lf-release-"));
  put(root, `addons/${CORE}/${CORE}.gproj`, gproj(CORE, CORE_GUID, "Life Framework", [BASE]));
  put(root, `addons/${CORE}/Scripts/Game/LF_Core.c`, "class LF_Core\n{\n}\n");
  put(root, `addons/${PACK}/${PACK}.gproj`, gproj(PACK, PACK_GUID, "Life Framework: Everon 1989", [BASE, CORE_GUID]));
  put(root, `addons/${PACK}/README.md`, "# pack\n");
  kit(root, KIT);
  if (withPackKit) kit(root, PACK_KIT);
  return root;
}

const cases = [
  { name: "core gproj without TITLE", plant: (r) => put(r, `addons/${CORE}/${CORE}.gproj`, gproj(CORE, CORE_GUID, "", [BASE])), expect: /LifeFramework\.gproj: missing TITLE/ },
  { name: "core gproj without GUID", plant: (r) => put(r, `addons/${CORE}/${CORE}.gproj`, gproj(CORE, "", "T", [BASE])), expect: /LifeFramework\.gproj: missing or malformed GUID/ },
  { name: "pack gproj ID is not the folder", plant: (r) => put(r, `addons/${PACK}/${PACK}.gproj`, gproj("Other", PACK_GUID, "T", [BASE, CORE_GUID])), expect: /ID "Other" is not the folder name/ },
  { name: "pack gproj lacks the core dependency", plant: (r) => put(r, `addons/${PACK}/${PACK}.gproj`, gproj(PACK, PACK_GUID, "T", [BASE])), expect: /LF_Everon1989\.gproj: Dependencies lacks the core LifeFramework GUID/ },
  { name: "core gproj lacks the base game", plant: (r) => put(r, `addons/${CORE}/${CORE}.gproj`, gproj(CORE, CORE_GUID, "T", [])), expect: /LifeFramework\.gproj: Dependencies lacks the base game/ },
  { name: "pack gproj missing", plant: (r) => rmSync(join(r, `addons/${PACK}/${PACK}.gproj`)), expect: /LF_Everon1989\.gproj: missing/ },
  { name: "generated rdb in the tree", plant: (r) => put(r, `addons/${CORE}/resourceDatabase.rdb`, "junk"), expect: /resourceDatabase\.rdb: generated file/ },
  { name: "lock file in the pack", plant: (r) => put(r, `addons/${PACK}/x.rdb.lock`, ""), expect: /LF_Everon1989\/x\.rdb\.lock: generated file/ },
  { name: "log folder in the core", plant: (r) => put(r, `addons/${CORE}/log/console.log`, "x"), expect: /LifeFramework\/log\/console\.log: generated file/ },
  { name: "EL_ identifier in a script", plant: (r) => put(r, `addons/${CORE}/Scripts/Game/LF_Bad.c`, "class EL_Bad\n{\n}\n"), expect: /LF_Bad\.c:1: EL_ identifier EL_Bad/ },
  { name: "kit lacks the summary", plant: (r) => rmSync(join(r, KIT, "summary.txt")), expect: /LifeFramework: Workshop kit lacks the summary/ },
  { name: "kit lacks the description", plant: (r) => rmSync(join(r, KIT, "description.bbcode")), expect: /LifeFramework: Workshop kit lacks the description/ },
  { name: "kit lacks the preview image", plant: (r) => rmSync(join(r, KIT, "preview.jpg")), expect: /LifeFramework: Workshop kit lacks the preview image/ },
  { name: "kit version is not semver", plant: (r) => kit(r, KIT, undefined, "latest"), expect: /LifeFramework: Workshop kit README has no semver Version row/ },
  { name: "kit licence is MIT", plant: (r) => kit(r, KIT, "MIT"), expect: /LifeFramework: Workshop kit Licence row "MIT" is not APL/ },
  { name: "pack kit lacks the preview image", plant: (r) => rmSync(join(r, PACK_KIT, "preview.jpg")), expect: /LF_Everon1989: Workshop kit lacks the preview image/ },
];

let failed = 0;
const roots = [];
try {
  const clean = cleanTree();
  roots.push(clean);
  const res = checkRelease(clean);
  if (res.errors.length || res.warnings.length) {
    failed++;
    console.log(`FAIL clean tree reported: ${[...res.errors, ...res.warnings].join("; ")}`);
  }
  const noKit = cleanTree(false);
  roots.push(noKit);
  const nk = checkRelease(noKit);
  if (nk.errors.length || nk.warnings.length !== 1 || !/LF_Everon1989: no Workshop kit/.test(nk.warnings[0])) {
    failed++;
    console.log(`FAIL a pack with no kit folder should only warn once, got errors=${nk.errors.join("; ")} warnings=${nk.warnings.join("; ")}`);
  }
  for (const c of cases) {
    const root = cleanTree();
    roots.push(root);
    c.plant(root);
    const { errors } = checkRelease(root);
    if (!errors.some((e) => c.expect.test(e))) {
      failed++;
      console.log(`FAIL ${c.name}: expected ${c.expect}, got:\n  ${errors.join("\n  ") || "(no errors)"}`);
    }
  }
} finally {
  for (const r of roots) rmSync(r, { recursive: true, force: true });
}

if (failed) {
  console.log(`release-ready-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log(`release-ready-selftest: OK (clean tree, pack without kit warns, ${cases.length} planted defects caught)`);
