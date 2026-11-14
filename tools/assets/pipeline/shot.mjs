// tools\cli asset shot <slug> [--view iso|front|right|top|...] [--size WxH] [--live] [--discard]
// Pictures of the cleaned model (the LOD0 object of tmp/assets/<slug>/clean/<Name>.blend) in
// tmp/assets/<slug>/shots/<view>.png, from the clean stage's own .blend. Headless by default (the
// Workbench engine); --live opens that .blend in the user's Blender over the bridge and shoots it
// with the viewport renderer (docs/blender-bridge.md).
import { existsSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { bridge, runHeadless } from "../../lib/blender.mjs";
import { cacheDir, loadSpec } from "./asset.mjs";

const DEFAULT_VIEWS = ["iso", "front", "right", "top"];
const SHOT_SCRIPT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "blender", "lf_shot.py");

const valueOf = (flags, name) => {
  const i = flags.indexOf(name);
  return i >= 0 ? flags[i + 1] : undefined;
};

export async function main(root, flags) {
  const view = valueOf(flags, "--view");
  const sizeFlag = valueOf(flags, "--size");
  const slug = flags.find((f, i) => !f.startsWith("--") && flags[i - 1] !== "--view" && flags[i - 1] !== "--size");
  if (!slug) {
    console.log("usage: tools\\cli asset shot <slug> [--view iso|front|right|top|back|left|bottom] [--size 1280x720] [--live] [--discard]");
    return 2;
  }
  const spec = loadSpec(root, slug);
  const blend = join(cacheDir(root, slug), "clean", `${spec.name}.blend`);
  if (!existsSync(blend)) throw new Error(`no ${blend}; run: tools\\cli asset run ${slug} --only clean`);
  const size = sizeFlag ? sizeFlag.split("x").map(Number) : [1280, 720];
  if (size.length !== 2 || !size.every((n) => n >= 16 && n <= 8192)) throw new Error("--size is WIDTHxHEIGHT, e.g. 1280x720");
  const outDir = join(cacheDir(root, slug), "shots");
  mkdirSync(outDir, { recursive: true });
  const views = view ? [view] : DEFAULT_VIEWS;
  const target = [`${spec.name}_LOD0`];
  const jobs = views.map((v) => ({ out: join(outDir, `${v}.png`), view: v, size, target }));

  if (flags.includes("--live")) {
    const st = await bridge.status();
    if (st.state !== "listening") throw new Error(`the live Blender is not reachable: ${st.detail || st.state}`);
    if (st.info.dirty && !flags.includes("--discard")) throw new Error(`the open Blender file (${st.info.file || "untitled"}) has unsaved changes and ${blend} would replace it; save it, or pass --discard`);
    const opened = await bridge.exec(`bpy.ops.wm.open_mainfile(filepath=${JSON.stringify(blend)})`);
    if (!opened.ok) throw new Error(`opening ${blend} in the live Blender failed\n${opened.error}`);
    for (const j of jobs) {
      const r = await bridge.shot(j.out, { view: j.view, size, target });
      if (!r.ok) throw new Error(`shot ${j.view}: ${r.error}`);
    }
    console.log(`asset shot ${slug}: ${jobs.length} picture(s) in ${outDir} (live, Blender ${st.info.blender}: ${views.join(", ")})`);
    return 0;
  }
  const job = join(cacheDir(root, slug), "shots-job.json");
  writeFileSync(job, JSON.stringify(jobs, null, 1));
  const r = runHeadless(SHOT_SCRIPT, [job], { blend, log: join(outDir, "shots.log") });
  console.log(`asset shot ${slug}: ${jobs.length} picture(s) in ${outDir} (headless, ${views.join(", ")})`);
  for (const l of r.stdout.split("\n").filter((x) => x.startsWith("LF_SHOT"))) console.log("  " + l.slice(8));
  return 0;
}
