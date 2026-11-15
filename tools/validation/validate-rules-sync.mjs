// tools/validation/validate-rules-sync.mjs
// inputs: tools/commands/rules.mjs, addons/LF_Everon1989/Language/lf_everon1989_localization.st, website/content/rules.json
//
// The published town rules have one source, the pack's LF-Sign_Rules_* strings (#1281). The board sign, the /rules
// page and the Discord block all come from `rules export`, so website/content/rules.json must be exactly what the
// export writes now. This runs the real exporter into a temp folder and fails, naming the rule, when the committed
// file differs. The fix is `tools\cli rules export` and committing website/content/rules.json (#1513, beat B3).
// red-proof: `--selftest` plants a changed rule string against a stale rules.json and expects a failure naming
// that rule; with driftReport stubbed to return no problems it failed on "a stale rules.json is reported".
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

const clip = (s) => (s.length > 60 ? s.slice(0, 57) + "..." : s);

// Problems (strings naming the rule that differs) between the exported rules and the committed ones; [] when in sync.
export function driftReport(exported, committed) {
  const out = [];
  for (const k of ["title", "intro", "next"]) {
    if (exported[k] !== committed[k]) out.push(`${k} differs: table has "${clip(exported[k] ?? "")}", rules.json has "${clip(committed[k] ?? "")}"`);
  }
  const a = exported.rules || [];
  const b = committed.rules || [];
  for (let i = 0; i < Math.max(a.length, b.length); i++) {
    if (a[i] === b[i]) continue;
    if (a[i] === undefined) out.push(`rule ${i + 1} is in rules.json but no longer in the table: "${clip(b[i])}"`);
    else if (b[i] === undefined) out.push(`rule ${i + 1} is new in the table and missing from rules.json: "${clip(a[i])}"`);
    else out.push(`rule ${i + 1} differs: table has "${clip(a[i])}", rules.json has "${clip(b[i])}"`);
  }
  return out;
}

// Runs `rules export` for `table` into a fresh temp folder and compares the JSON it writes with `jsonFile`.
// Returns the list of problems, empty when the committed file is what the export writes.
export async function checkSync(table, jsonFile, root = ROOT) {
  const { main } = await import("../commands/rules.mjs");
  const tmp = mkdtempSync(join(tmpdir(), "lf-rules-sync-"));
  try {
    const outJson = join(tmp, "rules.json");
    const log = console.log;
    const err = console.error;
    let code;
    const said = [];
    console.log = (...a) => said.push(a.join(" "));
    console.error = (...a) => said.push(a.join(" "));
    try {
      code = await main(["export", "--table", table, "--json", outJson, "--md", join(tmp, "rules-discord.md")], { root });
    } finally {
      console.log = log;
      console.error = err;
    }
    if (code !== 0) return [`rules export failed: ${said.join(" ").trim()}`];
    if (!existsSync(jsonFile)) return [`${jsonFile} is missing`];
    const fresh = readFileSync(outJson, "utf8");
    const committedText = readFileSync(jsonFile, "utf8");
    if (fresh === committedText) return [];
    let committed;
    try {
      committed = JSON.parse(committedText);
    } catch (e) {
      return [`${jsonFile} is not valid JSON: ${e.message}`];
    }
    const problems = driftReport(JSON.parse(fresh), committed);
    // Same rules, different bytes (formatting): still drift, still fixed by re-exporting.
    return problems.length ? problems : ["rules.json has the same text as the export but different formatting"];
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }
}

const row = (id, text) => `    CustomStringTableItem "{0000000000000000}" {\n     Id "${id}"\n     Target_en_us "${text}"\n     Comment "fixture"\n    }\n`;
const tableOf = (...rows) => `StringTable {\n ItemClassName "CustomStringTableItem"\n Items {\n${rows.join("")} }\n}\n`;

async function selftest() {
  const failures = [];
  const check = (ok, msg) => {
    if (!ok) failures.push(msg);
  };
  const dir = mkdtempSync(join(tmpdir(), "lf-rules-sync-self-"));
  try {
    const mk = (rules) =>
      tableOf(
        row("LF-Sign_Rules_Title", "Town rules"),
        row("LF-Sign_Rules_What", "Fixture Town."),
        row("LF-Sign_Rules_Rules", rules.join("|")),
        row("LF-Sign_Rules_Next", "Tell staff."),
      );
    const stale = { title: "Town rules", intro: "Fixture Town.", rules: ["No theft.", "No random violence."], next: "Tell staff." };
    mkdirSync(dir, { recursive: true });
    const jsonFile = join(dir, "rules.json");
    writeFileSync(jsonFile, JSON.stringify(stale, null, 2) + "\n");

    const same = join(dir, "same.st");
    writeFileSync(same, mk(["No theft.", "No random violence."]));
    const ok = await checkSync(same, jsonFile);
    check(ok.length === 0, `a rules.json that matches its table passes (got ${JSON.stringify(ok)})`);

    const changed = join(dir, "changed.st");
    writeFileSync(changed, mk(["No theft.", "No violence without a reason."]));
    const bad = await checkSync(changed, jsonFile);
    check(bad.length > 0, "a stale rules.json is reported");
    check(bad.some((p) => /rule 2/.test(p) && /No violence without a reason/.test(p)), `the report names the changed rule (got ${JSON.stringify(bad)})`);
    check(!bad.some((p) => /rule 1/.test(p)), "the report leaves the unchanged rule out");

    const added = join(dir, "added.st");
    writeFileSync(added, mk(["No theft.", "No random violence.", "Be kind."]));
    const more = await checkSync(added, jsonFile);
    check(more.some((p) => /rule 3/.test(p) && /new in the table/.test(p)), `a rule added to the table is reported (got ${JSON.stringify(more)})`);

    const retitled = join(dir, "retitled.st");
    writeFileSync(retitled, mk(["No theft.", "No random violence."]).replace("Town rules", "House rules"));
    const title = await checkSync(retitled, jsonFile);
    check(title.some((p) => /^title differs/.test(p)), `a changed title is reported (got ${JSON.stringify(title)})`);

    const noFile = await checkSync(same, join(dir, "absent.json"));
    check(noFile.length === 1 && /missing/.test(noFile[0]), "a missing rules.json is reported");

    const broken = await checkSync(join(dir, "nope.st"), jsonFile);
    check(broken.length === 1 && /rules export failed/.test(broken[0]), "an export that fails is reported");
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }

  const real = await checkSync(join(ROOT, "addons/LF_Everon1989/Language/lf_everon1989_localization.st"), join(ROOT, "website/content/rules.json"));
  check(real.length === 0, `the real tree is in sync (got ${JSON.stringify(real)})`);

  if (failures.length) {
    for (const f of failures) console.error("FAIL " + f);
    console.error(`validate-rules-sync --selftest: ${failures.length} problem(s)`);
    return 1;
  }
  console.log("validate-rules-sync --selftest: OK (changed, added and retitled rules named, missing file and failed export reported, real tree in sync)");
  return 0;
}

if (process.argv[1] && process.argv[1].split("\\").join("/").endsWith("validate-rules-sync.mjs")) {
  if (process.argv.includes("--selftest")) {
    process.exit(await selftest());
  }
  const problems = await checkSync(join(ROOT, "addons/LF_Everon1989/Language/lf_everon1989_localization.st"), join(ROOT, "website/content/rules.json"));
  if (problems.length) {
    for (const p of problems) console.error(`website/content/rules.json: ${p}`);
    console.error("validate-rules-sync: the published rules drifted from the pack strings. Run `tools\\cli rules export` and commit website/content/rules.json.");
    process.exit(1);
  }
  console.log("validate-rules-sync: OK (website/content/rules.json is what `rules export` writes now)");
}
