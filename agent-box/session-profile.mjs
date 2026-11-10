// Session resource profiles: `session up --profile full|lowspec`.
//
// A session boots a dedicated server and N game clients on one PC, and memory is what limits how
// many sessions run at once (docs/worktrees.md "Session profiles"). Most proofs read logs and the
// harness's structured results, never pixels, so they do not need a good picture. `lowspec` keeps
// everything a proof reads (world, replication, script) and cuts what only a picture needs.
// Shots, `shots area`, filming, and any proof that captures images use `full`.
//
// A profile is a list of levers. Each lever is one measured change (RAM per lever:
// docs/worktrees.md "Session profiles"), so `--levers a,b` runs any subset: that is how the
// numbers were measured, and how one is dropped if a proof turns out to need it.
//
// Where each lever comes from (never guessed; ArmaReforger.gproj DefaultSettings and the game's
// own SCR_VideoSettingsSubMenu, read with game_read; BI wiki "Startup Parameters"):
//  - engine settings: the per-user ReforgerEngineSettings.conf modules the options menu edits
//    (ResourceManagerUserSettings.TextureDetail, PipelineUserSettings.ShadowQuality, ...). The
//    low values are the ones the game's own LOW preset and PS4 platform config use.
//  - startup parameters: -maxFPS, -nds, -streamingBudget (server), window size.

import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";

export const PROFILES = ["full", "lowspec"];
export const DEFAULT_PROFILE = "lowspec";

// Each lever: client { res, maxFps, args, engine, game }, server { maxFps, args }.
// engine: a tree written to ReforgerEngineSettings.conf; an object value is a block whose class
// name is its key unless it carries _cls. game: a ReforgerGameSettings.conf tree, same shape.
export const LEVERS = {
  // 640x360 window: the render targets and swap chain shrink with it.
  res: { client: { res: { width: 640, height: 360 } } },
  // Frame rate cap. Fewer frames per second is CPU, not RAM, but it is what lets two sessions share cores.
  fps: { client: { maxFps: 15 } },
  // The texture streaming budget tier: MemoryBudgetTextures is "768 1024 1280 1536 2048" MB and
  // TextureDetail picks the tier (the game's LOW preset is 3, ULTRA 0; 4 is the smallest).
  tex: { client: { engine: { ResourceManagerUserSettings: { TextureDetail: 4 } } } },
  // Mesh LOD memory tier: MemoryBudgetMeshes "204 224 256 304 384" MB; ULTRA is 4, LOW preset 1.
  geom: { client: { engine: { ResourceManagerUserSettings: { GeometricDetail: 0 } } } },
  // Post-processing off: Postprocess 0 and the PS4 platform config's PPQuality.
  post: {
    client: {
      engine: {
        PostprocessUserSettings: { Postprocess: 0 },
        DisplayUserSettings: { PPQuality: { _cls: "PPEffectsSettings", HBAO: 0, SSDO: 0, SSR: 0, PPAA: 1, Rain: 0 } },
        VideoUserSettings: { Fsaa: "NONE", Atoc: "NONE" },
      },
    },
  },
  // Shadows, distant shadows, sky and clouds at their lowest named level.
  shadow: {
    client: {
      engine: {
        PipelineUserSettings: { ShadowQuality: "Low" },
        VideoUserSettings: { DistantShadowsQuality: "LOW", EnvironmentQuality: "LOW" },
      },
    },
  },
  // Fewer drawn objects: object draw distance, grass and terrain detail, texture filtering.
  view: {
    client: {
      engine: {
        GraphicsQualitySettings: { ObjectDrawDistanceScale: "Low" },
        GrassMaterialSettings: { Distance: 60, Lod: 1 },
        TerrainGenMaterialSettings: { Detail: 0 },
        MaterialSystemUserSettings: { TextureFilter: 1, MaxAniso: 1 },
      },
    },
  },
  // Server: fewer replicated cells per client (-nds, default 2) and a smaller streaming budget.
  nds: { server: { args: ["-nds", "1"] } },
  stream: { server: { args: ["-streamingBudget", "200"] } },
  // One update thread instead of the job system's 22 (-singleThreadedUpdate, BI wiki "Startup Parameters"): each job thread has its own allocator and stack.
  st: { client: { args: ["-singleThreadedUpdate"] }, server: { args: ["-singleThreadedUpdate"] } },
  // Server frame cap (CPU only).
  sfps: { server: { maxFps: 20 } },
};

// What each profile turns on. `full` is the session as it always was.
export const PROFILE_LEVERS = {
  full: [],
  lowspec: ["res", "fps", "tex", "geom", "post", "shadow", "view", "nds"],
};

// Flags that only make sense for a picture or a recording: without an explicit --profile they mean full.
export const PICTURE_FLAGS = ["--res", "--visible", "--sound"];

export function parseProfile(args, env = process.env) {
  const i = args.indexOf("--profile");
  const picture = PICTURE_FLAGS.some((f) => args.includes(f));
  const name = i === -1 ? String(env.LF_SESSION_PROFILE || (picture ? "full" : DEFAULT_PROFILE)) : String(args[i + 1] ?? "");
  if (!PROFILES.includes(name)) throw new Error(`--profile must be ${PROFILES.join(" or ")} (got '${name}')`);
  return name;
}

// --levers a,b,c replaces the profile's list (measuring one lever, or dropping one).
export function parseLevers(args, profile) {
  const i = args.indexOf("--levers");
  if (i === -1) return [...PROFILE_LEVERS[profile]];
  const list = String(args[i + 1] ?? "").split(",").map((s) => s.trim()).filter(Boolean);
  for (const l of list) if (!LEVERS[l]) throw new Error(`--levers: unknown lever '${l}' (${Object.keys(LEVERS).join(", ")})`);
  return list;
}

function merge(into, from) {
  for (const [k, v] of Object.entries(from)) {
    if (v && typeof v === "object" && !Array.isArray(v)) into[k] = merge(into[k] && typeof into[k] === "object" ? into[k] : {}, v);
    else into[k] = v;
  }
  return into;
}

// The resolved profile a session records in session.json and passes to serverArgs/clientArgs:
// { profile, levers, client: { res?, maxFps?, args, engine, game }, server: { maxFps?, args } }.
export function resolveProfile(profile = "full", levers = PROFILE_LEVERS[profile] ?? []) {
  const out = { profile, levers: [...levers], client: { args: [], engine: {}, game: {} }, server: { args: [] } };
  for (const name of levers) {
    const l = LEVERS[name];
    if (!l) continue;
    const c = l.client ?? {};
    if (c.res) out.client.res = c.res;
    if (c.maxFps) out.client.maxFps = c.maxFps;
    if (c.args) out.client.args.push(...c.args);
    if (c.engine) merge(out.client.engine, c.engine);
    if (c.game) merge(out.client.game, c.game);
    const s = l.server ?? {};
    if (s.maxFps) out.server.maxFps = s.maxFps;
    if (s.args) out.server.args.push(...s.args);
  }
  return out;
}

export function isLowspec(rp) {
  return !!rp && rp.profile === "lowspec";
}

// A settings tree as Enfusion text: `Class Name {` blocks, `Key value` lines. A string value that is
// not a plain word is quoted.
export function confText(rootClass, tree) {
  const lines = [`${rootClass} {`];
  const walk = (node, depth) => {
    const pad = " ".repeat(depth);
    for (const [key, value] of Object.entries(node)) {
      if (key === "_cls") continue;
      if (value && typeof value === "object") {
        lines.push(`${pad}${key} ${value._cls ?? key} {`);
        walk(value, depth + 1);
        lines.push(`${pad}}`);
      } else if (typeof value === "string" && !/^[A-Za-z0-9_]+$/.test(value)) lines.push(`${pad}${key} "${value}"`);
      else lines.push(`${pad}${key} ${value}`);
    }
  };
  walk(tree, 1);
  lines.push("}");
  return lines.join("\n") + "\n";
}

export function engineConf(rp) {
  return Object.keys(rp?.client?.engine ?? {}).length ? confText("EngineUserSettings", rp.client.engine) : "";
}

export function gameConf(rp) {
  return Object.keys(rp?.client?.game ?? {}).length ? confText("GameUserSettings", rp.client.game) : "";
}

// ---- client settings on disk -------------------------------------------------------------------

const APP_ID = "1874880";

// The Steam user the engine keys a profile's settings by: `.save/app<id>_user<steamid64>/settings`.
// Read from the profile when it has run before, else from Steam's loginusers.vdf (MostRecent).
export function steamUserDirs(profileDir, steamRoot = process.env.LF_STEAM_DIR || "C:/Program Files (x86)/Steam") {
  const save = join(profileDir, "profile", ".save");
  const found = [];
  try {
    for (const d of readdirSync(save)) if (d.startsWith(`app${APP_ID}_user`)) found.push(d);
  } catch {}
  if (found.length) return found;
  try {
    const text = readFileSync(join(steamRoot, "config", "loginusers.vdf"), "utf8");
    const users = [...text.matchAll(/"(\d{17})"\s*\{([^}]*)\}/g)];
    const recent = users.find((m) => /"MostRecent"\s*"1"/.test(m[2])) ?? users[0];
    if (recent) return [`app${APP_ID}_user${recent[1]}`];
  } catch {}
  return [];
}

// Writes this profile's engine settings into the client profile, replacing whatever an earlier
// session left there (the engine saves its settings back, so a lowspec run would otherwise leak
// into the next full one). `full` leaves no file: the game's own defaults apply.
export function applyClientSettings(profileDir, rp, steamRoot) {
  const roots = ["", ...steamUserDirs(profileDir, steamRoot)];
  const engine = engineConf(rp);
  const game = gameConf(rp);
  const written = [];
  for (const sub of roots) {
    const dir = join(profileDir, "profile", ".save", sub, "settings");
    // A fresh profile has no settings dir yet: create it only when there is something to write.
    if (!existsSync(dir) && !engine && !game) continue;
    mkdirSync(dir, { recursive: true });
    for (const [file, text] of [["ReforgerEngineSettings.conf", engine], ["ReforgerGameSettings.conf", game]]) {
      const p = join(dir, file);
      if (text) {
        writeFileSync(p, text);
        written.push(p);
      } else if (existsSync(p)) rmSync(p, { force: true });
    }
  }
  return written;
}
