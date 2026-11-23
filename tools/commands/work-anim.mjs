// tools\cli work-anim [--check] [--no-compile]
// Builds the resident work clips (#4396): tools/assets/work-anim/clips/<job>_loop.json becomes
// Anims/LF/Work/Work_<job>_{in,loop,out}.txa (+ .meta), compiled to .anm by a hidden Workbench, and an anim set
// instance Anims/LF/Work/LF_Work<Job>.asi on the work graph's template (Anims/Workspaces/Work/LF_WorkPick.ast).
// The in and out clips hold the loop's first pose, so the loiter blends in and out of it. Same pose solver and
// checks as `tools\cli asset anim` (docs/asset-pipeline/animation.md); the work graph is the lockpick's, so no
// workspace of its own is written. --check solves and checks the clips and writes nothing.
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync, copyFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { newestLog } from "../assets/pipeline/import.mjs";
import { clipProblems, plainMeta } from "../assets/pipeline/anim.mjs";
import { freshGuid, readGuid } from "../assets/pipeline/meta.mjs";
import { addonOf, addonPath, guidsInUse, pluginDir } from "../assets/pipeline/addon.mjs";
import * as pose from "../assets/pipeline/pose.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const PLUGIN = "LF_AssetImportPlugin";
const OUT = "Anims/LF/Work";
const TEMPLATE = "{9A43A8BDDF59B93C}Anims/Workspaces/Work/LF_WorkPick.ast";
const EDGE_FRAMES = 12;

const cap = (s) => s[0].toUpperCase() + s.slice(1);

// The in or out clip of a job: its first pose, held, so it joins the loop without a jump.
export function edgeClip(loop, which) {
  const key = { ...loop.keys[0], frame: 0 };
  return { name: `${loop.name.replace(/_loop$/, "")}_${which}`, fps: loop.fps, frames: EDGE_FRAMES, loop: false, bones: loop.bones, keys: [key, { ...key, frame: EDGE_FRAMES }] };
}

export function asiText(res) {
  const line = (anim, r) => `  AnimSetInstanceSource_Line "Work.Pick.${anim}" {\n   Resource "${r}"\n  }`;
  return `AnimSetInstanceSource {\n Template "${TEMPLATE}"\n Lines {\n${line("In", res.in)}\n${line("Loop", res.loop)}\n${line("Out", res.out)}\n }\n}\n`;
}

export function main(args, { root, workbench } = {}) {
  const check = args.includes("--check");
  const compile = !args.includes("--no-compile") && !check;
  const clipsDir = join(root, "tools", "assets", "work-anim", "clips");
  const jobs = readdirSync(clipsDir).filter((f) => f.endsWith("_loop.json")).map((f) => f.replace(/_loop\.json$/, ""));
  const sk = pose.loadSkeleton();
  const addon = addonPath(root, {});
  const used = guidsInUse(root);
  const toCompile = [];
  let failed = 0;
  for (const job of jobs) {
    const loop = JSON.parse(readFileSync(join(clipsDir, `${job}_loop.json`), "utf8"));
    const clips = { in: edgeClip(loop, "in"), loop, out: edgeClip(loop, "out") };
    const res = {};
    for (const [which, clip] of Object.entries(clips)) {
      const built = pose.buildClip(sk, clip);
      const bad = clipProblems(sk, clip, built);
      if (bad.length) {
        console.log(`FAIL  ${clip.name}:\n  ${bad.join("\n  ")}`);
        failed++;
        continue;
      }
      const rel = `${OUT}/Work_${clip.name}.anm`;
      const txaRel = rel.replace(/\.anm$/, ".txa");
      const txa = pose.writeTxa(sk, clip, built);
      const guid = readGuid(join(addon, rel + ".meta")) || freshGuid(used);
      res[which] = `{${guid}}${rel}`;
      if (check) continue;
      mkdirSync(dirname(join(addon, rel)), { recursive: true });
      const before = existsSync(join(addon, txaRel)) ? readFileSync(join(addon, txaRel), "utf8") : null;
      if (before !== txa) writeFileSync(join(addon, txaRel), txa);
      if (!existsSync(join(addon, rel + ".meta"))) writeFileSync(join(addon, rel + ".meta"), plainMeta(rel, guid, "TXAResourceClass", true));
      if (before !== txa || !existsSync(join(addon, rel))) toCompile.push(join(addon, txaRel));
    }
    if (check) continue;
    const asiRel = `${OUT}/LF_Work${cap(job)}.asi`;
    writeFileSync(join(addon, asiRel), asiText(res));
    if (!existsSync(join(addon, asiRel + ".meta"))) writeFileSync(join(addon, asiRel + ".meta"), plainMeta(asiRel, freshGuid(used), "AnimSetInstanceResourceClass"));
    console.log(`work-anim: ${job} -> ${asiRel} {${readGuid(join(addon, asiRel + ".meta"))}}`);
  }
  if (failed) return 1;
  if (check) {
    console.log(`work-anim: ${jobs.length} job(s) solve and pass the checks`);
    return 0;
  }
  if (compile && toCompile.length) {
    const plugins = pluginDir(root);
    mkdirSync(plugins, { recursive: true });
    copyFileSync(join(HERE, "..", "assets", "pipeline", "wb", `${PLUGIN}.c`), join(plugins, `${PLUGIN}.c`));
    for (const txa of toCompile) {
      const anm = txa.replace(/\.txa$/, ".anm");
      const before = existsSync(anm) ? statSync(anm).mtimeMs : 0;
      const logFile = join(root, "tmp", "work-anim.log");
      mkdirSync(dirname(logFile), { recursive: true });
      const { profile } = workbench(root, PLUGIN, [`-lfAsset0=${txa}`], { timeoutMs: 90000, logFile, addon: addonOf({}) });
      if (!existsSync(anm) || statSync(anm).mtimeMs <= before) {
        console.log(`FAIL  Workbench did not compile ${anm} (log: ${logFile})`);
        const wb = newestLog(profile);
        const rows = String(wb).split(/\r?\n/).filter((l) => /LF_ASSET_IMPORT|ERROR|rror/.test(l)).slice(-12);
        for (const l of rows) console.log(`  ${l.trim().slice(0, 220)}`);
        return 1;
      }
      console.log(`work-anim: compiled ${anm} (${statSync(anm).size} bytes)`);
    }
  }
  return 0;
}
