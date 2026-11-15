// No secret in a tracked file (issue #1463).
// inputs: **
//
// The live server config is public in this repo (server/flagship), so a committed
// secret is a leaked one. A tracked file fails when it holds:
//   - a Discord webhook URL (id and token both present),
//   - a non-empty `password`, `passwordAdmin` or `rconPassword` value in a server*.json
//     (the RCON password is the `password` key under `rcon`),
//   - or it is a `secrets.json` / `*.secrets.json` at all.
// Placeholders pass: "", "<set on host>" (any <...>) and CHANGE-ME. The failure names
// file:line and the key, never the value.
//
// `--selftest` runs the scanner over tools/validation/fixtures/no-secrets/ (every `fail/`
// folder must be rejected, every `pass/` folder accepted) and then over the real tree.
//
// red-proof: 2026-11-15, with scanFile returning no findings `--selftest` printed
// `FAIL selftest: fail/... not rejected` for all five fail fixtures and exited 1.
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { basename, dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIXTURES = "tools/validation/fixtures/no-secrets";

// Returns [{ line, what }] for one file; `rel` only supplies the basename.
const WEBHOOK = /https:\/\/(?:(?:ptb|canary)\.)?discord(?:app)?\.com\/api(?:\/v\d+)?\/webhooks\/\d+\/[\w-]+/;
const SERVER_JSON = /^server([-.][\w.-]*)?\.json$/i;
const SECRETS_JSON = /^(?:[\w.-]+\.)?secrets\.json$/i;
const PASSWORD_KEY = /"(password|passwordAdmin|rconPassword)"\s*:\s*"((?:[^"\\]|\\.)*)"/g;

// "", any <placeholder> and CHANGE-ME are not secrets.
function isPlaceholder(value) {
  const v = value.trim();
  return v === "" || /^<[^<>]*>$/.test(v) || /^change[-_ ]?me$/i.test(v);
}

export function scanFile(rel, text) {
  const name = basename(rel);
  const hits = [];
  if (SECRETS_JSON.test(name)) hits.push({ line: 1, what: "secrets file is tracked (keep it on the host, gitignored)" });
  const lines = text.split("\n");
  lines.forEach((line, i) => {
    if (WEBHOOK.test(line)) hits.push({ line: i + 1, what: "Discord webhook URL" });
    if (!SERVER_JSON.test(name)) return;
    for (const m of line.matchAll(PASSWORD_KEY)) {
      if (!isPlaceholder(m[2])) hits.push({ line: i + 1, what: `non-empty "${m[1]}"` });
    }
  });
  return hits;
}

function tracked() {
  return execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard"], {
    cwd: root,
    encoding: "utf8",
    maxBuffer: 1 << 28,
  }).split("\0").filter(Boolean);
}

function scanTree() {
  const bad = [];
  let scanned = 0;
  for (const f of tracked()) {
    if (f.startsWith(FIXTURES + "/") || f.includes("node_modules/")) continue;
    // Validator fixtures and selftests hold fake secrets on purpose.
    if (f.startsWith("tools/validation/fixtures/") || /^tools\/validation\/[\w.-]*-selftest\.mjs$/.test(f)) continue;
    const abs = join(root, f);
    if (!existsSync(abs)) continue;
    const text = readText(abs);
    if (text === null) continue;
    scanned++;
    for (const h of scanFile(f, text)) bad.push(`FAIL ${f}:${h.line} ${h.what}`);
  }
  return { bad, scanned };
}

function readText(abs) {
  if (statSync(abs).size > 4 << 20) return null;
  const buf = readFileSync(abs);
  if (buf.includes(0)) return null;
  return buf.toString("utf8");
}

function selftest() {
  const problems = [];
  for (const kind of ["fail", "pass"]) {
    const dir = join(root, FIXTURES, kind);
    for (const sub of readdirSync(dir)) {
      for (const name of readdirSync(join(dir, sub))) {
        const rel = `${FIXTURES}/${kind}/${sub}/${name}`;
        const hits = scanFile(rel, readFileSync(join(root, rel), "utf8"));
        if (kind === "fail" && hits.length === 0) problems.push(`FAIL selftest: ${kind}/${sub}/${name} not rejected`);
        if (kind === "pass" && hits.length > 0) problems.push(`FAIL selftest: ${kind}/${sub}/${name} rejected: ${hits.map((h) => h.what).join("; ")}`);
      }
    }
  }
  const tree = scanTree();
  problems.push(...tree.bad);
  if (problems.length) {
    console.log(problems.join("\n"));
    console.log(`validate-no-secrets --selftest: ${problems.length} problem(s)`);
    process.exit(1);
  }
  console.log(`validate-no-secrets --selftest: OK (fixtures behave; ${tree.scanned} tracked file(s) clean)`);
}

if (process.argv.includes("--selftest")) selftest();
else {
  const { bad, scanned } = scanTree();
  if (bad.length) {
    console.log(bad.slice(0, 60).join("\n"));
    if (bad.length > 60) console.log(`... ${bad.length - 60} more`);
    console.log(`validate-no-secrets: ${bad.length} secret(s) in tracked files. Remove them, rotate the secret, keep the real value on the host.`);
    process.exit(1);
  }
  console.log(`validate-no-secrets: OK (${scanned} tracked file(s))`);
}
