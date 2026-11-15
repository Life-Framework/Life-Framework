// tools\cli host post session|wipe [--send]   (#1414)
//
// Prints the post `host announce` drafted for the next session (read from website/content/sessions.json,
// where #1273 records it; this command never writes it) and, only with --send, POSTs it to the
// announcements webhook URL in the host's secrets.json (`announceWebhookUrl`). Without the URL --send says
// so and exits 1. The URL is never printed. `session` is the announce draft; `wipe` is the wipe notice and
// needs a wipe session. Tests inject the sender as ctx.send; nothing in the selftest reaches Discord.
// Env: LF_ANNOUNCE_SESSIONS (the sessions file, as in host announce and host wipe).
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { buildPost, SERVER_NAME, DEFAULT_RULES, WIPE_LINE } from "../announce.mjs";
import { isDiscordWebhook, MAX_CONTENT, postToWebhook, readWebhookUrl, scrub } from "../discord-post.mjs";

export const help = "post session|wipe [--send]   print the drafted announcement for the next session; --send posts it to the announcements webhook in secrets.json (maintainer only)";

const TAG = "[LFDebug:HostPost]";

// The wipe notice: the time, the wipe line and the rules. Same placeholders as the announce template.
function wipePost({ at, build }) {
  const epoch = Math.floor(Date.parse(at) / 1000);
  return [
    `**${SERVER_NAME}** wipe notice`,
    "",
    `${WIPE_LINE}`,
    `Wipe session: <t:${epoch}:F> (<t:${epoch}:R>)`,
    `Build: ${build}`,
    `Rules: ${DEFAULT_RULES}`,
  ].join("\n") + "\n";
}

function readNext(file) {
  try {
    const next = JSON.parse(readFileSync(file, "utf8")).next;
    if (next && typeof next === "object" && typeof next.at === "string" && typeof next.build === "string") return { next };
  } catch (e) {
    if (e && e.code !== "ENOENT") return { error: `${file} is not readable JSON` };
  }
  return { error: `no next session in ${file}: run host announce --at <time> first` };
}

export async function main(args, ctx) {
  const kind = args[0];
  const flags = args.slice(1);
  const bad = flags.find((a) => a !== "--send");
  if ((kind !== "session" && kind !== "wipe") || bad) {
    console.log(`${TAG} ${bad ? `unknown argument ${bad}` : "say what to post: session or wipe"}\nusage: ${help}`);
    return 2;
  }
  const send = flags.includes("--send");

  const file = process.env.LF_ANNOUNCE_SESSIONS || join(ctx.root, "website", "content", "sessions.json");
  const r = readNext(file);
  if (r.error) {
    console.log(`${TAG} ${r.error}`);
    return 1;
  }
  const { next } = r;
  if (kind === "wipe" && !next.wipe) {
    console.log(`${TAG} the next session (${next.at}) is not a wipe session; nothing to post. Draft one with host announce --wipe --at <time>`);
    return 1;
  }

  let post;
  if (kind === "wipe") post = wipePost(next);
  else post = buildPost({ template: readFileSync(join(ctx.root, "docs", "community", "announce-template.md"), "utf8"), at: next.at, build: next.build, wipe: !!next.wipe });
  const content = post.trim();

  console.log(post);
  if (content.length >= MAX_CONTENT) {
    console.log(`${TAG} the post is ${content.length} characters; Discord takes under ${MAX_CONTENT}. Nothing was sent`);
    return 1;
  }
  if (!send) {
    console.log(`${TAG} dry run (${content.length} characters); add --send to post it`);
    return 0;
  }

  const found = readWebhookUrl(ctx.hostDir);
  if (found.error) {
    console.log(`${TAG} ${found.error}; nothing was sent`);
    return 1;
  }
  if (!isDiscordWebhook(found.url)) {
    console.log(`${TAG} announceWebhookUrl in secrets.json is not an https Discord webhook URL (https://discord.com/api/webhooks/...); nothing was sent`);
    return 1;
  }
  let result;
  try {
    result = await (ctx.send || postToWebhook)(found.url, content);
  } catch (e) {
    result = { ok: false, error: scrub(String(e && e.message ? e.message : e), found.url) };
  }
  if (!result || !result.ok) {
    console.log(`${TAG} post failed: ${scrub((result && result.error) || "unknown error", found.url)}`);
    return 1;
  }
  console.log(`${TAG} posted the ${kind} notice (${content.length} characters, HTTP ${result.status})`);
  return 0;
}
