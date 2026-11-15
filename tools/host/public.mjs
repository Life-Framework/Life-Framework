// The public flagship host: `tools\cli host start --public` (docs/owner/host-the-flagship.md).
//
// A `-server <mission> -noBackend` host (hostArgs in host.mjs) is LAN/direct-connect only: it is not
// in the in-game server browser, has no join password (the engine checks it through Bohemia's
// backend, #1520) and a normal Bohemia-online client cannot join it. The public host is a real
// backend-registered dedicated server: `-config <profile>/server.json -profile <profile>`, no
// -noBackend, no -addons. The scenario comes from game.scenarioId and the mods from game.mods,
// which the server downloads from the Workshop (a -config boot drops a local -addons list, #1520).
// So it can only run once the core and the pack are published.
//
// server.json schema (installed game, scripts/GameLib/online/DSConfig.c + the native parser):
//   bindAddress, bindPort, publicAddress, publicPort, a2s { address, port },
//   game { name, password, passwordAdmin, admins[], scenarioId, maxPlayers, visible, crossPlatform,
//          supportedPlatforms[], gameProperties {...}, mods[] { modId, name, version? } }, operating {...}
// A mod without `version` takes the newest Workshop revision.
//
// The Workshop IDs are the .gproj GUIDs (the Workshop keeps a project's GUID as its item ID). They
// live in server/flagship/server.json; after publishing, the maintainer confirms them from the item
// pages with `host config apply --mods <core>,<pack>`, which records <host>/workshop.json. The
// pre-flight refuses a public boot until that confirmation matches the live server.json.
import { existsSync, mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

const HEX16 = /^[0-9A-F]{16}$/;
const SEMVER = /^\d+\.\d+\.\d+$/;

export function workshopConfirmPath(dir) {
  return join(dir, "workshop.json");
}
// Where the server keeps what it downloads from the Workshop: the host dir, outside the profile and
// outside every worktree, so no local repo addon can shadow a Workshop item.
export function workshopDir(dir) {
  return join(dir, "workshop");
}

// A Workshop ID that cannot be a published item: not 16 hex digits, one digit repeated, or a run.
export function isPlaceholderId(id) {
  const s = String(id ?? "").trim().toUpperCase();
  if (!HEX16.test(s)) return true;
  if (/^(.)\1{15}$/.test(s)) return true;
  return s === "0123456789ABCDEF" || s === "FEDCBA9876543210";
}

// `--mods <core>,<pack>`: { ids } upper-cased, or { error }.
export function parseModsFlag(text) {
  const ids = String(text ?? "").split(",").map((s) => s.trim().toUpperCase()).filter(Boolean);
  if (ids.length !== 2) return { error: `--mods takes the core and the pack Workshop IDs, comma-separated (got ${ids.length})` };
  const bad = ids.filter(isPlaceholderId);
  if (bad.length) return { error: `--mods ${bad.join(", ")} is not a published Workshop ID (16 hex digits, from the item page address)` };
  return { ids };
}

export function readWorkshopConfirm(dir) {
  try {
    const j = JSON.parse(readFileSync(workshopConfirmPath(dir), "utf8"));
    return Array.isArray(j?.mods) ? j : null;
  } catch {
    return null;
  }
}

export function writeWorkshopConfirm(dir, ids, now = new Date()) {
  const p = workshopConfirmPath(dir);
  mkdirSync(dirname(p), { recursive: true });
  const tmp = `${p}.tmp-${process.pid}`;
  writeFileSync(tmp, `${JSON.stringify({ mods: ids, confirmedAt: now.toISOString() }, null, 2)}\n`);
  renameSync(tmp, p);
  return p;
}

// The launch flags of the public host. No -server (game.scenarioId), no -addons (game.mods), no
// -noBackend (the backend lists it and checks the password), no -addr (bindPort/publicPort).
export function publicHostArgs({ profile, serverJson, workshopDir: wdir, loadSave }) {
  const args = [
    "-config", serverJson,
    "-profile", profile,
    "-addonsDir", wdir,
    "-maxFPS", "60",
    "-logLevel", "normal",
    "-disableCrashReporter",
  ];
  if (loadSave) args.push("-loadSessionSave");
  return args;
}

// The checks before a public boot. Pure: the caller reads the files.
//   serverJson  the parsed <profile>/server.json, or null when it is missing
//   confirm     the parsed <host>/workshop.json, or null
//   drift       host drift changes against server/flagship (tools/host/drift.mjs), [] when none
//   scenario    the flagship header resource ({GUID}Missions/LF_Everon1989.conf)
// Returns { ok, refusals, warnings }. Never carries a secret value.
export function publicPreflight({ serverJson, confirm, drift = [], scenario }) {
  const refusals = [];
  const warnings = [];
  if (!serverJson || typeof serverJson !== "object") {
    return { ok: false, refusals: ["the host profile has no server.json: run `tools\\cli host config apply` first"], warnings };
  }
  const g = serverJson.game ?? {};
  const mods = Array.isArray(g.mods) ? g.mods : null;
  if (!mods || !mods.length) refusals.push("server.json has no game.mods: the public host loads only Workshop items");
  else {
    if (mods.length < 2) refusals.push(`server.json game.mods has ${mods.length} mod(s): it needs the core and the pack (2 mods)`);
    for (const [i, m] of mods.entries()) {
      if (isPlaceholderId(m?.modId)) refusals.push(`server.json game.mods[${i}].modId "${m?.modId ?? ""}" is a placeholder, not a published Workshop ID`);
      if (m?.version !== undefined && !SEMVER.test(String(m.version))) refusals.push(`server.json game.mods[${i}].version "${m.version}" is not x.y.z (leave it out for the newest revision)`);
    }
    const live = mods.map((m) => String(m?.modId ?? "").toUpperCase());
    const confirmed = (confirm?.mods ?? []).map((s) => String(s).toUpperCase());
    if (!confirm) refusals.push("the Workshop IDs are not confirmed: after publishing 0.1.0, run `tools\\cli host config apply --mods <core-id>,<pack-id>` with the IDs from the two item pages");
    else if (JSON.stringify(live) !== JSON.stringify(confirmed)) refusals.push(`the confirmed Workshop IDs (${confirmed.join(",")}) are not server.json game.mods (${live.join(",")}): run \`tools\\cli host config apply --mods <core-id>,<pack-id>\` again`);
  }
  if (typeof g.password !== "string" || !g.password.trim()) refusals.push("server.json game.password is empty: anyone could join the alpha. Set game.password in the host's secrets.json and run `tools\\cli host config apply`");
  if (g.scenarioId !== scenario) refusals.push(`server.json game.scenarioId is ${JSON.stringify(g.scenarioId ?? null)}, not the flagship ${scenario}`);
  if (g.visible === false) refusals.push("server.json game.visible is false: the server would not show in the server browser");
  if (drift.length) refusals.push(`the host config drifted from server/flagship (${drift.map((c) => `${c.file} ${c.field ?? c.note}`).slice(0, 4).join("; ")}): run \`tools\\cli host config apply\` (\`tools\\cli host drift\` lists it)`);
  if (!Array.isArray(g.admins) || !g.admins.length) warnings.push("server.json game.admins is empty: nobody has admin powers. Join once, read your id with `tools\\cli host attendance`, put it in server/flagship/staff.txt and apply again");
  if (typeof g.passwordAdmin !== "string" || !g.passwordAdmin.trim()) warnings.push("server.json game.passwordAdmin is empty: #login in chat cannot give admin");
  return { ok: refusals.length === 0, refusals, warnings };
}

// Reads the files the pre-flight needs from a host dir.
export function readServerJson(path) {
  if (!existsSync(path)) return null;
  try {
    return JSON.parse(readFileSync(path, "utf8"));
  } catch {
    return { game: {}, _bad: true };
  }
}
