// tools/lib/host-box.mjs - box mode for `tools\cli host` on a rented machine (#1642).
//
// A rented box is a plain `git clone`, which is a main checkout, and `host start|watch|autostart` refuse
// in a main checkout because on the maintainer's PC that is the world-editor copy. Box mode lifts that
// refusal for the host commands only: `LF_HOST_BOX=1` in the environment, or a `.lf-host-box` file in the
// repo root (`tools\cli host box on` writes it). The marker exists because the scheduled task that
// `host autostart` registers carries no environment.
//
// `missingForHostStart` names what a clone lacks before `host start` boots anything, so the operator gets
// one clear line instead of a stack trace from the launch-addons scan.

import { existsSync } from "node:fs";
import { join } from "node:path";

export const BOX_MARKER = ".lf-host-box";

/** { on, why }: box mode is on for this clone when the env says so or the marker file exists. */
export function boxMode(root, env = process.env) {
  const v = String(env.LF_HOST_BOX ?? "").trim().toLowerCase();
  if (v && !["0", "false", "no", "off"].includes(v)) return { on: true, why: "LF_HOST_BOX is set" };
  if (existsSync(join(root, BOX_MARKER))) return { on: true, why: `${BOX_MARKER} exists in the repo root` };
  return { on: false, why: "" };
}

/** What `host start` needs and this clone lacks: one sentence per missing piece, empty when it can start. */
export function missingForHostStart(root, { exe }) {
  const out = [];
  if (!existsSync(join(root, "addons", "LifeFramework"))) {
    out.push("addons/ (the server loads the mods from <clone>/addons; clone the whole repo, not a sparse one)");
  }
  if (!existsSync(exe)) {
    out.push(`the dedicated server at ${exe} (install Arma Reforger Server, or set ENFUSION_SERVER_PATH to the folder that holds ArmaReforgerServer.exe)`);
  }
  return out;
}

/** Pieces a box clone has beside the start path: not needed to boot, needed by one command. */
export function optionalPieces(root) {
  const out = [];
  if (!existsSync(join(root, "website"))) out.push("website/ (only `host announce` needs it: it writes website/content/sessions.json)");
  return out;
}
