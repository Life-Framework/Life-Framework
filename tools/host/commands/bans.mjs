// `tools\cli host bans [--player <uid>] [--open] [--file <path>]`: reads the host's
// $profile:LifeFramework/ban_record.txt (written by LF_BanRecord.c, #1285) and prints one row per ban
// and unban with reason, staff uid and incident ids; `--open` shows only bans with no later unban.
// A malformed row is reported with its line number and skipped. The reader is tools/host/bans.mjs.
import { existsSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { hostPaths } from "../host.mjs";
import { parseBanRecord, listBans, formatEntry } from "../bans.mjs";

export const help =
  "bans [--player <uid>] [--open] [--file <path>]   every ban and unban in the host's ban record with reason, staff uid and incident ids; --open: only bans no later unban closed; --player: one uid";

function option(args, name) {
  const i = args.indexOf(name);
  if (i < 0) return null;
  const v = args[i + 1];
  return v && !v.startsWith("--") ? v : undefined;
}

export async function main(args, ctx) {
  const player = option(args, "--player");
  const fileArg = option(args, "--file");
  if (player === undefined || fileArg === undefined) {
    console.log("[LFHost:bans] usage: host bans [--player <uid>] [--open] [--file <path>]");
    return 2;
  }
  const file = fileArg ? resolve(fileArg) : join(hostPaths(ctx.hostDir).profile, "LifeFramework", "ban_record.txt");
  if (!existsSync(file)) {
    console.log(`[LFHost:bans] no ban record at ${file} (no ban has been recorded, or pass --file)`);
    return 1;
  }
  const { rows, skipped } = parseBanRecord(readFileSync(file, "utf8"));
  console.log(`[LFHost:bans] ${file}`);
  for (const s of skipped) console.log(`[LFHost:bans] WARN line ${s.line} skipped: ${s.reason}`);
  const entries = listBans(rows, { player: player ?? undefined, open: args.includes("--open") });
  for (const e of entries) console.log(formatEntry(e));
  if (!entries.length) console.log("(no matching rows)");
  console.log(`SUMMARY host-bans ok rows=${rows.length} skipped=${skipped.length} shown=${entries.length} file=${file}`);
  return 0;
}