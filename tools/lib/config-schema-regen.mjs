// `wt ship` regenerates docs/config/ when a branch touches the config surface (#3717). validate-config-schema
// failed on main three times in two days (#3510, #3527, #3716): a config class doc comment or key changed
// without `node tools/config/schema.mjs --write`, and ship's preflight validates only changed inputs. Like the
// language confs, the schema is derived, so ship rebuilds it and commits the result with the branch.
// Pure decisions here; every side effect is passed in, so tools/validation/config-schema-regen-selftest.mjs
// drives it with fakes.

// What schema.mjs reads: the config classes (Enforce Script under a Scripts folder), the pack manifest and
// section .conf files, and its own overrides file. A .conf under Configs/ or Missions/ is a pack manifest candidate.
export function touchesConfigSurface(paths) {
  return (paths ?? []).some((raw) => {
    const p = String(raw).replace(/\\/g, "/");
    if (/^addons\/[^/]+\/Scripts\/.+\.c$/.test(p)) return true;
    if (/^addons\/[^/]+\/(Configs|Missions)\/.+\.conf$/.test(p)) return true;
    return p === "tools/config/schema-overrides.json" || p === "tools/config/schema.mjs" || p === "tools/config/pack-sections.mjs";
  });
}

// deps: changed() -> repo-relative paths the branch changes; write() -> {status, stdout, stderr} from
// `node tools/config/schema.mjs --write`; dirty() -> porcelain lines under docs/config/; commit() -> {status, stdout, stderr}
// (git add docs/config and commit); log(line).
// Returns {ran, committed, ok}. A failed generation is not fatal here: validate then names the real problem.
export function regenConfigSchema(deps) {
  const log = deps.log ?? (() => {});
  if (!touchesConfigSurface(deps.changed())) return { ran: false, committed: false, ok: true };
  const w = deps.write();
  if (w.status !== 0) {
    log(`ship: config schema regeneration failed (${String(w.stderr || w.stdout).trim().split(/\r?\n/)[0]}); validate will report it`);
    return { ran: true, committed: false, ok: false };
  }
  const dirty = deps.dirty();
  if (!dirty.length) return { ran: true, committed: false, ok: true };
  const c = deps.commit();
  if (c.status !== 0) {
    log(`ship: could not commit the regenerated docs/config/ (${String(c.stderr || c.stdout).trim().split(/\r?\n/)[0]}); validate will report it`);
    return { ran: true, committed: false, ok: false };
  }
  log(`ship: regenerated and committed ${dirty.length} file(s) in docs/config/ (the config classes or confs changed)`);
  return { ran: true, committed: true, ok: true };
}
