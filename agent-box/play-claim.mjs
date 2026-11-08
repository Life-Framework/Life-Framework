// Who owns the play-session command channel.
//
// A play session polls $profile:agent-box/command.txt. Every Workbench, and
// every game client on the default profile, shares that profile folder, so two
// worktrees driving play sessions at once would move each other's character.
// The machine-wide `agent-box-play` lock (tools/wt.mjs, tmp/wt/locks in the
// main checkout) records which checkout's rendering process owns the channel.
// The lock belongs to that Workbench or game process, not to the short-lived
// harness command, so it frees itself when the session closes.

import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import * as wt from "../tools/wt.mjs";

export const PLAY_LOCK = "agent-box-play";

const RENDERERS = ["ArmaReforgerWorkbenchSteamDiag.exe", "ArmaReforgerWorkbenchSteam.exe", "ArmaReforgerSteam.exe", "ArmaReforgerSteamDiag.exe"];

function norm(path) {
  return resolve(path).replaceAll("\\", "/").toLowerCase();
}

function alive(pid) {
  try {
    process.kill(pid, 0);
    return true;
  } catch (e) {
    return e.code === "EPERM";
  }
}

/**
 * Pure decision. `held` is the parsed lock ({ pid, root }) or null.
 * Returns { ok, claim, reason }: claim=true means this checkout should take the lock.
 */
export function decideClaim(held, root, isAlive) {
  if (held && held.pid && isAlive(held.pid)) {
    if (held.root && norm(held.root) !== norm(root)) {
      return {
        ok: false,
        claim: false,
        reason: `play_owned_by_other_checkout ${held.root} (pid ${held.pid}). Close that session first; the command file is shared machine-wide.`,
      };
    }
    return { ok: true, claim: false, reason: "" };
  }
  return { ok: true, claim: true, reason: "" };
}

/** A rendering process whose command line names this checkout (its -gproj or -addonsDir). */
export function rendererFor(root, list = renderingProcesses()) {
  const want = norm(root);
  const hit = list.find((p) => (p.cmd || "").replaceAll("\\", "/").toLowerCase().includes(want + "/addons"));
  return hit ? hit.pid : 0;
}

function renderingProcesses() {
  if (process.platform !== "win32") return [];
  const filter = RENDERERS.map((n) => `Name='${n}'`).join(" or ");
  const res = spawnSync(
    "powershell.exe",
    ["-NoProfile", "-Command", `Get-CimInstance Win32_Process -Filter "${filter}" | Select-Object ProcessId,CommandLine | ConvertTo-Json -Compress`],
    { windowsHide: true, encoding: "utf8" },
  );
  const text = (res.stdout || "").trim();
  if (!text) return [];
  try {
    const parsed = JSON.parse(text);
    return (Array.isArray(parsed) ? parsed : [parsed]).map((p) => ({ pid: p.ProcessId, cmd: p.CommandLine || "" }));
  } catch {
    return [];
  }
}

function lockFile(root) {
  return join(wt.hubDirOf(root), "locks", `${PLAY_LOCK}.lock`);
}

function readHeld(root) {
  const file = lockFile(root);
  if (!existsSync(file)) return null;
  try {
    return JSON.parse(readFileSync(file, "utf8"));
  } catch {
    return null;
  }
}

/**
 * Check that this checkout may drive the play session, and claim the channel
 * for this checkout's Workbench or game process when one is running.
 */
export function claimPlay(root) {
  const decision = decideClaim(readHeld(root), root, alive);
  if (!decision.ok || !decision.claim) return decision;
  const pid = rendererFor(root);
  // A Workbench opened from the launcher has no -gproj on its command line.
  // Nothing to tie the lock to, so drive it unclaimed.
  if (!pid) return { ok: true, claim: false, reason: "unclaimed" };
  try {
    wt.acquireLock(root, PLAY_LOCK, { waitMs: 0 });
  } catch (e) {
    const again = decideClaim(readHeld(root), root, alive);
    return again.ok ? { ok: true, claim: false, reason: "" } : again;
  }
  wt.transferLock(root, PLAY_LOCK, pid);
  const file = lockFile(root);
  const info = JSON.parse(readFileSync(file, "utf8"));
  writeFileSync(file, JSON.stringify({ ...info, root }));
  return { ok: true, claim: true, reason: `claimed for pid ${pid}` };
}
