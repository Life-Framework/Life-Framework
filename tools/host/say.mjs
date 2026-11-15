// The write step of `tools\cli host say "<text>"` (#1453).
//
// Drops $profile:LifeFramework/host_say.txt in the host profile. The server (LF_HostSay) polls
// it every 10 s, toasts every player once, logs `[LFDebug:Ops] host say: ...` and deletes the
// file, so the operator can speak to the town without joining.
import { existsSync, mkdirSync, renameSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { hostPaths } from "./host.mjs";

// LF_HostSay.MAX_LENGTH: the server cuts longer text, so the CLI says so up front.
export const MAX_LENGTH = 200;
export const SAY_FILE = ["LifeFramework", "host_say.txt"];

// Writes the say file under `profile` (the folder `$profile:` points at). Newlines become spaces:
// the server reads the first line. Returns { ok: true, file, text, cut, replaced } or
// { ok: false, reason } with nothing written.
export function writeSay(profile, message) {
  const text = String(message ?? "").replace(/\s+/g, " ").trim();
  if (!text) return { ok: false, reason: "the message is empty: tools\\cli host say \"<text>\"" };
  const file = join(profile, ...SAY_FILE);
  const cut = text.length > MAX_LENGTH;
  const sent = cut ? text.slice(0, MAX_LENGTH) : text;
  const replaced = existsSync(file);
  mkdirSync(join(profile, SAY_FILE[0]), { recursive: true });
  // Write beside the file and rename, so the poll never reads half a message.
  const tmp = `${file}.tmp`;
  writeFileSync(tmp, `${sent}\n`);
  renameSync(tmp, file);
  return { ok: true, file, text: sent, cut, replaced };
}

export function sayHost(dir, message) {
  return writeSay(hostPaths(dir).profile, message);
}
