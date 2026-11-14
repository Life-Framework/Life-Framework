// The website's content step (`npm run content` in website/) reads files out of addons/ and docs/ by
// path (the config generator's scenario headers, the gproj ids, the presets). A move such as #323
// batch 1 broke main's website `build` check for every PR with an ENOENT nobody saw until CI. This runs
// the same step offline (LF_SITE_OFFLINE, a few seconds) so a moved file fails here first.
// Skips with a hint when website/node_modules is missing (CI's `validate` job has none; the website
// `build` check covers it there). Proof: tools/validation/website-content-selftest.mjs
// inputs: addons/**, docs/**, tools/test/config-fixtures/presets/**, website/**
import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

// Returns { skipped: "<why>" } or { problems: [..] } (empty when the step ran clean).
export function websiteContent(root, run = spawnSync) {
  const site = join(root, "website");
  if (!existsSync(join(site, "scripts", "build-content.mjs"))) return { skipped: "no website/scripts/build-content.mjs" };
  if (!existsSync(join(site, "node_modules"))) {
    return { skipped: "website/node_modules is missing; run `npm ci` in website/ and then `tools\cli validate --all` to check it" };
  }
  const r = run(process.execPath, ["scripts/build-content.mjs"], {
    cwd: site,
    env: { ...process.env, LF_SITE_OFFLINE: "1" },
    encoding: "utf8",
    timeout: 120000,
  });
  if (r.status === 0) return { problems: [] };
  const text = `${r.stderr || ""}\n${r.stdout || ""}`;
  const lines = text.split(/\r?\n/).map((l) => l.trim()).filter(Boolean);
  const hit = lines.find((l) => /ENOENT|no such file|Cannot find|Error:/.test(l)) || lines.find((l) => /at .*build-content/.test(l)) || lines[0] || `exit ${r.status}`;
  const why = r.error ? ` (${r.error.message})` : "";
  return { problems: [`website content step failed: ${hit}${why}`] };
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const root = resolve(fileURLToPath(import.meta.url), "..", "..", "..");
  const res = websiteContent(root);
  if (res.skipped) console.log(`SKIP website-content: ${res.skipped}`);
  else if (res.problems.length) {
    for (const p of res.problems) console.log(`FAIL website-content: ${p}`);
    console.log("  a path build-content.mjs reads (website/scripts/) moved or was renamed; point it at the new place (a core scenario is Missions/EL_DebugTest.conf)");
    process.exit(1);
  } else console.log("OK website-content: the website's content step reads every path it needs");
}
