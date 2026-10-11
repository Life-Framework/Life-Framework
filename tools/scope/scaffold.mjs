// Scope scaffold. A new feature starts from these four files.
//
// tools\cli scope <name> <dir> writes into <dir> only. It does not run
// regen-tests, does not edit the string table, and does not edit docs/features.md.
// The selftest runs that command in a temp directory.

import { spawnSync } from "node:child_process";
import { randomBytes } from "node:crypto";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, isAbsolute, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

export const FEATURES_ROW_FILE = "features-row.md";
export const LOCALIZATION_KEYS_FILE = "localization-keys.st";
export const STREET_MANIFEST_FILE = "street-manifest.json";

const USAGE = `usage: tools\\cli scope <name> <dir>
       tools\\cli scope <name> --out <dir>
       tools\\cli scope --selftest`;

const CLI_PATH = join(dirname(fileURLToPath(import.meta.url)), "..", "cli.mjs");

function guid() {
  return randomBytes(8).toString("hex").toUpperCase();
}

function inside(root, target) {
  const rel = relative(resolve(root), resolve(target));
  return rel === "" || (!rel.startsWith("..") && !isAbsolute(rel));
}

export function parseScopeName(raw) {
  const text = String(raw ?? "").trim();
  if (!text) return null;
  const split = text.replace(/([a-z0-9])([A-Z])/g, "$1 $2");
  let parts = split.split(/[^A-Za-z0-9]+/).filter(Boolean);
  if (parts.length > 1 && parts[0].toLowerCase() === "roadmap") parts = parts.slice(1);
  if (!parts.length) return null;
  const setup = parts.map((part) => part.toLowerCase()).join("-");
  const Setup = parts.map((part) => part.charAt(0).toUpperCase() + part.slice(1).toLowerCase()).join("");
  const title = parts.map((part) => part.charAt(0).toUpperCase() + part.slice(1).toLowerCase()).join(" ");
  return {
    setup,
    Setup,
    title,
    className: `EL_Test_Roadmap_${Setup}`,
    testFile: `EL_Test_Roadmap_${Setup}.c`,
    logicName: `roadmap/${setup}`,
    entityName: `Roadmap_${Setup}`,
  };
}

function testStub(identity) {
  return [
    "// tier: LOGIC",
    "// red-proof: scaffold stub. Break the assertion, run the fast suite, and record the observed [ELTEST] line before trusting this test.",
    "",
    `class ${identity.className} : EL_Test`,
    "{",
    "\toverride string GetName()",
    "\t{",
    `\t\treturn "${identity.logicName}";`,
    "\t}",
    "",
    "\toverride void Run(EL_TestContext ctx)",
    "\t{",
    `\t\tctx.True(false, "scaffold assertion for ${identity.logicName}");`,
    "\t}",
    "}",
    "",
  ].join("\n");
}

function featuresRow(identity) {
  return [
    `### \`LF_${identity.Setup}\` — scaffold`,
    `- ⚠️ Not implemented. Logic test \`${identity.logicName}\` in \`${identity.testFile}\` (\`// tier: LOGIC\`, \`// red-proof:\`). Street site \`${identity.entityName}\` is a placeholder slot.`,
    "",
  ].join("\n");
}

function localizationKeys(identity) {
  const action = guid();
  const denied = guid();
  return [
    `    CustomStringTableItem "{${action}}" {`,
    `     Id "EL-${identity.Setup}_Action"`,
    `     Target_en_us "${identity.title}"`,
    `     Comment "Scaffold action name for ${identity.logicName}. Append this item; do not rewrite the table."`,
    "    }",
    `    CustomStringTableItem "{${denied}}" {`,
    `     Id "EL-${identity.Setup}_Denied"`,
    `     Target_en_us "${identity.title} is not available"`,
    `     Comment "Scaffold refusal for ${identity.logicName}. Append this item; do not rewrite the table."`,
    "    }",
    "",
  ].join("\n");
}

function streetRow(identity) {
  return `${JSON.stringify({ name: identity.entityName, slot: "placeholder" })}\n`;
}

export function writeScope(name, dir) {
  const identity = parseScopeName(name);
  if (!identity) return { ok: false, error: "name must contain a letter or digit" };
  if (!dir || !String(dir).trim()) return { ok: false, error: "directory is required" };
  const root = resolve(String(dir));
  mkdirSync(root, { recursive: true });
  const outputs = [
    [identity.testFile, testStub(identity)],
    [FEATURES_ROW_FILE, featuresRow(identity)],
    [LOCALIZATION_KEYS_FILE, localizationKeys(identity)],
    [STREET_MANIFEST_FILE, streetRow(identity)],
  ];
  const files = [];
  for (const [fileName, body] of outputs) {
    const path = join(root, fileName);
    if (!inside(root, path)) return { ok: false, error: `refusing to write outside ${root}` };
    writeFileSync(path, body, "utf8");
    files.push(path);
  }
  return { ok: true, files, identity };
}

function hasLinePrefix(text, prefix) {
  return text.split(/\r?\n/).some((line) => line.startsWith(prefix));
}

export function selftestScope(cliPath = CLI_PATH) {
  const failures = [];
  const dir = mkdtempSync(join(tmpdir(), "lf-scope-"));
  const name = "sample";
  try {
    const res = spawnSync(process.execPath, [cliPath, "scope", name, dir], { encoding: "utf8" });
    if (res.status !== 0) {
      const detail = `${res.stdout || ""}${res.stderr || ""}`.trim();
      failures.push(`scope command exited ${res.status}${detail ? `: ${detail}` : ""}`);
    }
    const identity = parseScopeName(name);
    const testPath = join(dir, identity.testFile);
    const expected = [testPath, join(dir, FEATURES_ROW_FILE), join(dir, LOCALIZATION_KEYS_FILE), join(dir, STREET_MANIFEST_FILE)];
    for (const file of expected) {
      if (!existsSync(file)) failures.push(`missing ${file}`);
    }
    if (existsSync(testPath)) {
      const text = readFileSync(testPath, "utf8");
      if (!hasLinePrefix(text, "// tier:")) failures.push("test stub is missing a // tier: line");
      if (!hasLinePrefix(text, "// red-proof:")) failures.push("test stub is missing a // red-proof: line");
      if (!text.includes(`"${identity.logicName}"`)) failures.push(`test stub is missing logic name ${identity.logicName}`);
    }
    const streetPath = join(dir, STREET_MANIFEST_FILE);
    if (existsSync(streetPath)) {
      let row = null;
      try {
        row = JSON.parse(readFileSync(streetPath, "utf8"));
      } catch {
        failures.push("street manifest row is not json");
      }
      if (row && (row.name !== identity.entityName || row.slot !== "placeholder")) {
        failures.push("street manifest row needs the site name and a placeholder slot");
      }
    }
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
  return failures;
}

export function runScopeCli(args) {
  const list = args ?? [];
  if (list.includes("--selftest")) {
    const failures = selftestScope();
    if (failures.length) {
      console.log("scope: FAIL");
      for (const failure of failures) console.log(`  ${failure}`);
      return 1;
    }
    console.log("scope: OK");
    return 0;
  }

  let out = null;
  const positional = [];
  for (let i = 0; i < list.length; i++) {
    const arg = list[i];
    if (arg === "--out") {
      out = list[i + 1] ?? "";
      i++;
      continue;
    }
    if (arg.startsWith("--")) {
      console.log(`scope: unknown flag ${arg}`);
      console.log(USAGE);
      return 1;
    }
    positional.push(arg);
  }
  const name = positional[0];
  if (!out) out = positional[1];
  if (!name || !out || positional.length > 2) {
    console.log(USAGE);
    return 1;
  }
  const written = writeScope(name, out);
  if (!written.ok) {
    console.log(`scope: FAIL ${written.error}`);
    return 1;
  }
  for (const file of written.files) console.log(file);
  return 0;
}
