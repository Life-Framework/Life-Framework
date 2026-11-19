// tools\cli rebuild-fbx <path.fbx>... [--addon <name>]
// Rebuilds the .xob (or .edds) beside each registered source in a hidden Workbench, the way `asset kit` does for its
// models, for a hand-written resource that has no asset spec: the sign carrier kit (tools/assets/sign-carriers, #3307).
// The .xob.meta beside the source must exist already. A path is absolute or relative to the repo root. One Workbench run per
// source: with -exitAfterInit only the first of a batch is built (tools/assets/pipeline/import.mjs).
import { copyFileSync, existsSync, mkdirSync, readdirSync, statSync } from "node:fs";
import { dirname, isAbsolute, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { addonOf, pluginDir } from "../assets/pipeline/addon.mjs";
import { logProblems, runLog } from "../assets/pipeline/import.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const PLUGIN = "LF_AssetImportPlugin";

export function main(args, { root, workbench }) {
  const flagAt = args.indexOf("--addon");
  const addon = flagAt >= 0 ? args[flagAt + 1] : undefined;
  const paths = args.filter((a, i) => !a.startsWith("--") && !(flagAt >= 0 && i === flagAt + 1));
  const sources = paths.map((a) => (isAbsolute(a) ? a : resolve(root, a)));
  if (!sources.length) {
    console.log("usage: tools\\cli rebuild-fbx <path.fbx>... [--addon <name>]");
    return 2;
  }
  const plugin = pluginDir(root);
  mkdirSync(plugin, { recursive: true });
  copyFileSync(join(HERE, "..", "assets", "pipeline", "wb", `${PLUGIN}.c`), join(plugin, `${PLUGIN}.c`));
  let failed = 0;
  for (const src of sources) {
    if (!existsSync(src) || !existsSync(src.replace(/\.fbx$/i, ".xob") + ".meta")) {
      console.log(`FAIL  ${src}: the source or its .xob.meta is missing`);
      failed++;
      continue;
    }
    const logFile = join(root, "tmp", "rebuild-fbx.log");
    mkdirSync(dirname(logFile), { recursive: true });
    const { profile } = workbench(root, PLUGIN, [`-lfAsset0=${src}`], { timeoutMs: 180000, logFile, addon: addonOf({ addon }) });
    const logs = join(profile, "logs");
    const dirs = readdirSync(logs).map((d) => join(logs, d)).filter((d) => statSync(d).isDirectory()).sort((a, b) => statSync(b).mtimeMs - statSync(a).mtimeMs);
    const log = dirs.length ? runLog(dirs[0]) : "";
    const out = src.replace(/\.fbx$/i, ".xob");
    const problems = logProblems(log, src.split(/[\\/]/).pop());
    const built = /LF_ASSET_IMPORT done/.test(log) && existsSync(out) && statSync(out).mtimeMs >= statSync(src).mtimeMs;
    for (const p of problems) console.log(`FAIL  ${p}`);
    console.log(`  ${src.split(/[\\/]/).pop()} ${built && !problems.length ? "ok" : "FAIL"}`);
    if (!built || problems.length) failed++;
  }
  return failed ? 1 : 0;
}
