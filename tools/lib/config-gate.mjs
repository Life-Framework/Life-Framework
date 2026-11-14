// The config harness step of `wt gate` / `wt ship` / `ci` (#301): the static part (parse + validate over
// every fixture) always, the game-session parts only when the change can move what a config does.
// The release runs all of it (tools/release/release.mjs). Trigger proven offline by
// tools/validation/config-gate-selftest.mjs.

// A changed file that can change how an owner config is read, validated, served or shown.
export function configRelevant(file) {
  const f = String(file).split("\\").join("/");
  if (f.startsWith("tools/config/") || f.startsWith("tools/test/config-fixtures/")) return true;
  if (f === "docs/config/config.schema.json" || f === "docs/config/config.migrations.json") return true;
  if (/^agent-box\/proofs\/config-[^/]+\.mjs$/.test(f) || f === "tools/lib/config-gate.mjs" || f === "tools/commands/config-gate.mjs") return true;
  if (!f.startsWith("addons/")) return false;
  if (f.endsWith(".conf")) return true;
  if (/\/Scripts\/Game\/.*\/(?:Config|Configs)\/[^/]+\.c$/.test(f)) return true;
  if (/\/Scripts\/Game\/.*Config[^/]*\.c$/.test(f)) return true;
  return false;
}

export function configTrigger(files) {
  return files.filter(configRelevant);
}

// `stages.static()` and `stages.sessions()` each return an exit code; injected so the selftest runs offline.
export async function runConfigGate(files, stages, log = console.log) {
  const sCode = await stages.static();
  if (sCode !== 0) { log("config harness (static) FAILED: a fixture no longer matches its expected result"); return 1; }
  const hit = configTrigger(files);
  if (!hit.length) {
    log("=== ci: config harness sessions skipped (no config code, .conf, schema or config fixture changed) ===");
    return 0;
  }
  log(`=== ci: config harness sessions (${hit.length} file(s) touch config, e.g. ${hit[0]}) ===`);
  const code = await stages.sessions();
  if (code !== 0) log("config harness sessions FAILED: see the FAIL lines above (tools\cli config-gate --sessions reruns them)");
  return code === 0 ? 0 : 1;
}
