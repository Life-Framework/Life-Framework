// tools\cli host announce --at <iso time> [--build <v>] [--wipe] [--address <text>] [--rules <url>]
//                         [--channel <id>] [--reply <messageId>] [--send]
//
// Drafts the session announcement from docs/community/announce-template.md, writes the next session
// to website/content/sessions.json and prints the post. A draft is { next: { at, build, wipe } };
// a successful --send adds `announced`, the ISO time it was posted (#1537), which `host wipe` ages.
// It posts to Discord only with --send, through tools/discord-mcp/say.mjs; agents never run --send,
// the maintainer or a moderator does. --wipe refuses an --at less than 24 hours from now.
// Without --build the build is the one that wrote the newest host save (host preflight's stamp).
// Env (selftest and ops): LF_ANNOUNCE_NOW (epoch ms), LF_ANNOUNCE_SESSIONS (file), LF_ANNOUNCE_CHANNEL,
// LF_ANNOUNCE_SAY (a stand-in for say.mjs; the selftest only).
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { checkAt, buildPost, sessionRow, sessionsJson, DEFAULT_ADDRESS, DEFAULT_RULES } from "../announce.mjs";
import { hostPaths, newestSaveBuild } from "../host.mjs";

export const help = "announce --at <iso> [--build v] [--wipe] [--send]   draft the session post and record the next session for the site; --send posts it (maintainer only)";

function parse(args) {
  const o = { wipe: false, send: false };
  const valued = new Set(["at", "build", "address", "rules", "channel", "reply"]);
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === "--wipe") o.wipe = true;
    else if (a === "--send") o.send = true;
    else if (a.startsWith("--") && valued.has(a.slice(2))) o[a.slice(2)] = args[++i];
    else return { error: `unknown argument ${a}` };
  }
  return o;
}

export async function main(args, ctx) {
  const o = parse(args);
  if (o.error || !o.at) {
    console.log(`[LFDebug:HostAnnounce] ${o.error || "--at is required"}\nusage: ${help}`);
    return 2;
  }
  const now = process.env.LF_ANNOUNCE_NOW ? Number(process.env.LF_ANNOUNCE_NOW) : Date.now();
  const chk = checkAt({ at: o.at, wipe: o.wipe, now });
  if (!chk.ok) {
    console.log(`[LFDebug:HostAnnounce] refused: ${chk.error}`);
    return 1;
  }
  const build = o.build || newestSaveBuild(hostPaths(ctx.hostDir).profile);
  if (!build) {
    console.log("[LFDebug:HostAnnounce] no build: pass --build <version> (no host save to read it from)");
    return 1;
  }
  const channel = o.channel || process.env.LF_ANNOUNCE_CHANNEL;
  if (o.send && !channel) {
    console.log("[LFDebug:HostAnnounce] --send needs --channel <id> (or LF_ANNOUNCE_CHANNEL); nothing was posted");
    return 1;
  }

  const template = readFileSync(join(ctx.root, "docs", "community", "announce-template.md"), "utf8");
  const post = buildPost({ template, at: o.at, build, wipe: o.wipe, address: o.address || DEFAULT_ADDRESS, rules: o.rules || DEFAULT_RULES });
  const file = process.env.LF_ANNOUNCE_SESSIONS || join(ctx.root, "website", "content", "sessions.json");

  console.log(post);
  writeFileSync(file, sessionsJson(sessionRow({ at: o.at, build, wipe: o.wipe })));
  console.log(`[LFDebug:HostAnnounce] next session recorded in ${file} (draft: no announced time)`);

  if (!o.send) {
    console.log("[LFDebug:HostAnnounce] draft only; add --send to post it");
    return 0;
  }
  const dir = mkdtempSync(join(tmpdir(), "lf-announce-"));
  try {
    const textFile = join(dir, "post.txt");
    writeFileSync(textFile, post);
    const sayScript = process.env.LF_ANNOUNCE_SAY || join(ctx.root, "tools", "discord-mcp", "say.mjs");
    const say = spawnSync(process.execPath, [sayScript, channel, o.reply || "-", textFile], {
      encoding: "utf8",
      timeout: 30000,
      windowsHide: true,
    });
    console.log(`${say.stdout ?? ""}${say.stderr ?? ""}`.trim());
    if (say.status !== 0) {
      console.log("[LFDebug:HostAnnounce] post failed; sessions.json stays a draft (no announced time)");
      return 1;
    }
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
  // The post time is when the post succeeded: `host wipe` counts its 24 hours from here.
  const postedAt = new Date(process.env.LF_ANNOUNCE_NOW ? Number(process.env.LF_ANNOUNCE_NOW) : Date.now()).toISOString();
  writeFileSync(file, sessionsJson(sessionRow({ at: o.at, build, wipe: o.wipe, announced: postedAt })));
  console.log(`[LFDebug:HostAnnounce] posted; sessions.json announced=${postedAt}`);
  return 0;
}
