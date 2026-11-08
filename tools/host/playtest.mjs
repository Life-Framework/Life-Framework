// Helpers for `tools\cli playtest [start|stop|status|shortcut]`: one command
// that puts the maintainer in the game on the hosted playtest server.
//
// - The host runs from a dedicated checkout pinned to origin/main
//   (<parent>\Life-Framework-playtest, a detached git worktree), fast-forwarded
//   on each launch while no host runs, so an agent's work in progress never
//   reaches the playtest. `host start` in that checkout does the rest (rdb
//   refresh, save continue, server-2001 lock).
// - The client is the release game exe (ArmaReforgerSteam.exe), on the user's
//   desktop, with its own persistent profile. The host runs -noBackend, so the
//   account is keyed by the Steam name (EL_Utils.ChooseIdentityFallback); the
//   client carries no -scrDefine, or the script checksum differs from the host.
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

export const CHECKOUT_NAME = "Life-Framework-playtest";
export const JOIN_LINE = /\[ELDebug:Whitelist\] join allowed/g;

export function playtestRoot(mainRoot) {
  return join(dirname(mainRoot), CHECKOUT_NAME);
}

// The maintainer's client profile: settings, keybinds, and client logs.
// Outside every worktree, like the host dir. LF_PLAYER_DIR moves it.
export function playerDir(env = process.env) {
  if (env.LF_PLAYER_DIR) return env.LF_PLAYER_DIR;
  const base = env.LOCALAPPDATA || join(env.USERPROFILE || ".", "AppData", "Local");
  return join(base, "LifeFramework", "player");
}

export function playerPaths(dir) {
  return { dir, profile: join(dir, "profile"), state: join(dir, "client.json") };
}

// A player's client: no -window/-noFocus (the human's own full session), no
// -noBackend (a real Steam identity), no -scrDefine (same checksum as the host).
export function playerClientArgs({ address = "127.0.0.1", addonsDir, profile }) {
  return ["-client", address, "-addonsDir", addonsDir, "-addons", "LifeFramework", "-profile", profile];
}

export function readClientState(dir) {
  try {
    return JSON.parse(readFileSync(playerPaths(dir).state, "utf8"));
  } catch {
    return null;
  }
}

export function writeClientState(dir, s) {
  mkdirSync(dir, { recursive: true });
  writeFileSync(playerPaths(dir).state, JSON.stringify(s, null, 2));
}

export function countJoins(text) {
  return (String(text).match(JOIN_LINE) ?? []).length;
}

// What a friend does to join (printed after a launch).
export function friendLines({ lan, port, commit }) {
  const out = [];
  out.push("friends (optional): they need Arma Reforger and a clone of the repo at the same commit");
  out.push(`  1. git clone https://github.com/Life-Framework/Life-Framework.git && git -C Life-Framework checkout ${commit}`);
  out.push("  2. from their Arma Reforger folder:");
  const addr = lan[0] ?? "<this PC's address>";
  out.push(`     ArmaReforgerSteam.exe -client ${addr} -addonsDir "<their clone>\\addons" -addons LifeFramework`);
  for (const a of lan.slice(1)) out.push(`     (other LAN address: ${a})`);
  out.push(`  over the internet: your public IP, with UDP ${port} forwarded to this PC (docs/playtest.md)`);
  out.push("  each player needs a distinct Steam name: the name keys the character");
  return out;
}

export function selftestPlaytest() {
  const failures = [];
  if (playtestRoot(join("C:", "R", "Life-Framework")) !== join("C:", "R", CHECKOUT_NAME)) failures.push("playtest checkout is not a sibling of the main checkout");
  if (playerDir({ LOCALAPPDATA: "L" }) !== join("L", "LifeFramework", "player")) failures.push("default player dir moved");
  if (playerDir({ LF_PLAYER_DIR: "X", LOCALAPPDATA: "L" }) !== "X") failures.push("LF_PLAYER_DIR does not override the player dir");
  const a = playerClientArgs({ addonsDir: "A", profile: "P" });
  if (a[0] !== "-client" || a[1] !== "127.0.0.1") failures.push("client does not join 127.0.0.1");
  if (a[a.indexOf("-profile") + 1] !== "P") failures.push("client does not use the player profile");
  if (a[a.indexOf("-addons") + 1] !== "LifeFramework") failures.push("client does not load LifeFramework");
  for (const bad of ["-window", "-noFocus", "-noBackend", "-scrDefine", "-noSound", "-lfSession"]) {
    if (a.includes(bad)) failures.push(`player client passes ${bad}`);
  }
  if (countJoins("x [ELDebug:Whitelist] join allowed: uuid=a\ny\n[ELDebug:Whitelist] join allowed: uuid=b") !== 2) failures.push("countJoins miscounts");
  const f = friendLines({ lan: ["192.168.1.5"], port: 2001, commit: "abc" }).join("\n");
  if (!f.includes("-client 192.168.1.5") || !f.includes("checkout abc")) failures.push("friend lines miss the address or commit");
  return failures;
}
