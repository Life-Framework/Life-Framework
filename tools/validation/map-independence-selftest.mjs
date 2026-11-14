// tools/validation/map-independence-selftest.mjs
// inputs: tools/**, agent-box/**, .githooks/**, addons/**, docs/**, website/**, video/**
// Plants each kind of map knowledge in a temp tree and asserts validate-map-independence
// fails it; a comment, a Tests/ file, the harness folder, an offset literal and a
// baselined finding pass; a stale or issue-less or new baseline entry fails.

import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { SCRIPTS, checkMapIndependence } from "./validate-map-independence.mjs";

function put(root, path, text) {
  const full = join(root, SCRIPTS, path);
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
}

const script = (body) => `class LF_X\n{\n\tstatic void Run()\n\t{\n${body}\n\t}\n}\n`;

function tree() {
  const root = mkdtempSync(join(tmpdir(), "lf-mapindep-"));
  put(root, "Feature/Crime/LF_Good.c", script([
    "\t\t// FindEntityByName(\"Roadmap_Bank\") and \"300 0 300\" are the old way",
    "\t\t/* Sign_SafeZone_Pub */",
    "\t\tvector hand = pos + \"8 0 0\";",
    "\t\tvector lift = Vector(0, 1.6, 0);",
    "\t\tstring s = \"a // Roadmap-ish\";",
  ].join("\n")));
  put(root, "Tests/LF_Test_Uses.c", script("\t\tIEntity e = w.FindEntityByName(\"Roadmap_Bank\");\n\t\tvector v = \"300 0 300\";"));
  put(root, "Feature/AgentHarness/LF_Harness.c", script("\t\tIEntity e = w.FindEntityByName(name);\n\t\tvector v = \"268 0 255\";"));
  return root;
}

const bad = (line) => (r) => put(r, "Feature/Crime/LF_Bad.c", script(line));
const cases = [
  { name: "a new FindEntityByName(\"X\") in a feature", plant: bad("\t\tIEntity e = GetGame().GetWorld().FindEntityByName(\"X\");"), expect: /Feature\/Crime\/LF_Bad\.c: find-by-name `"X"`/ },
  { name: "FindEntityByName with a variable", plant: bad("\t\tIEntity e = w.FindEntityByName(m_sName);"), expect: /find-by-name `m_sName`/ },
  { name: "a world position string", plant: bad("\t\tvector v = \"256 0 256\";"), expect: /world-coords `256 0 256`/ },
  { name: "a world position Vector()", plant: bad("\t\tvector v = Vector(180, 0, 244);"), expect: /world-coords `180 0 244`/ },
  { name: "a Roadmap_ name", plant: bad("\t\tstring id = \"Roadmap_Hideout\";"), expect: /place-name `Roadmap_Hideout`/ },
  { name: "a Sign_SafeZone_ name", plant: bad("\t\tstring id = \"Sign_SafeZone_Pub\";"), expect: /place-name `Sign_SafeZone_Pub`/ },
];

let failed = 0;
const roots = [];
const mk = () => { const r = tree(); roots.push(r); return r; };
try {
  const clean = checkMapIndependence(mk(), { entries: [] });
  if (clean.errors.length) {
    failed++;
    console.log(`FAIL clean tree reported: ${clean.errors.join("; ")}`);
  }

  for (const c of cases) {
    const root = mk();
    c.plant(root);
    const { errors } = checkMapIndependence(root, { entries: [] });
    if (!errors.some((e) => c.expect.test(e))) {
      failed++;
      console.log(`FAIL ${c.name}: expected ${c.expect}, got:\n  ${errors.join("\n  ") || "(no errors)"}`);
    }
  }

  const entry = { file: "Feature/Crime/LF_Bad.c", rule: "find-by-name", token: "\"X\"", issue: "#1045" };
  const based = mk();
  cases[0].plant(based);
  const ok = checkMapIndependence(based, { entries: [entry] });
  if (ok.errors.length) { failed++; console.log(`FAIL a baselined finding was flagged: ${ok.errors.join("; ")}`); }

  const stale = checkMapIndependence(mk(), { entries: [entry] });
  if (!stale.errors.some((e) => /is gone from the code/.test(e))) { failed++; console.log("FAIL a stale baseline entry passed"); }

  const noIssue = checkMapIndependence(based, { entries: [{ ...entry, issue: "" }] });
  if (!noIssue.errors.some((e) => /has no issue/.test(e))) { failed++; console.log("FAIL a baseline entry with no issue passed"); }

  const grown = checkMapIndependence(based, { entries: [entry] }, new Set());
  if (!grown.errors.some((e) => /may only shrink/.test(e))) { failed++; console.log("FAIL a baseline entry the main branch lacks passed"); }
} finally {
  for (const r of roots) rmSync(r, { recursive: true, force: true });
}

if (failed) {
  console.log(`map-independence-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log(`map-independence-selftest: OK (${cases.length} kinds caught; comments, Tests/, AgentHarness/, offsets and baselined findings pass; stale, issue-less and grown entries fail)`);
