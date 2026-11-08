#!/usr/bin/env node
// Mirror the roadmap into the Discord #roadmap forum: one thread per roadmap issue, where
// people discuss it. GitHub is the source of truth. A roadmap issue is any issue in an M
// milestone; its pillar is the `topic:` label (a forum tag), its system the `Parent:` line.
//
// - An open issue gets a thread. The bot's starter message is the issue card, edited in place.
// - Tags follow the issue: its pillar plus Needs a hand / In progress / Done.
// - A closed issue's thread is tagged Done and archived; replying reopens it.
// - An issue that leaves the roadmap has its thread archived and locked, never deleted.
// - Only threads the bot started, named "#<number> ...", are touched.
//
//   node tools/discord-mcp/sync-roadmap.mjs            sync (env below)
//   node tools/discord-mcp/sync-roadmap.mjs --dry-run  print the cards; needs only GITHUB_REPOSITORY

import { setTimeout as delay } from "node:timers/promises";
import { pathToFileURL } from "node:url";

const GITHUB_API = "https://api.github.com";
const DISCORD_API = "https://discord.com/api/v10";
const FORUM_CHANNEL_TYPE = 15;
const MAX_DESCRIPTION = 3900;
const SITE_ROADMAP = "https://life-framework.org/roadmap";

// Same order as the website (website/src/components/RoadmapTree.tsx).
export const PILLAR_ORDER = [
  "Law & Order",
  "Economy & Work",
  "Life & Identity",
  "Community & Organizations",
  "World & Travel",
  "Framework & Playtest",
  "Flagship Story",
];
const PILLAR_EMOJI = {
  "Law & Order": "🛡️",
  "Economy & Work": "💰",
  "Life & Identity": "❤️",
  "Community & Organizations": "🤝",
  "World & Travel": "🌍",
  "Framework & Playtest": "🔧",
  "Flagship Story": "⭐",
};
export const STATUS = {
  open: { tag: "Needs a hand", emoji: "🙋", color: 0xe3b23c },
  "in-progress": { tag: "In progress", emoji: "🔄", color: 0x5865f2 },
  done: { tag: "Done", emoji: "✅", color: 0x57f287 },
};

// Only characters Discord treats as inline markdown; escaping others leaves visible backslashes.
const escapeMarkdown = (text) => String(text).replace(/([\\`*_~|\[\]])/g, "\\$1");
const logins = (issue) => (issue.assignees ?? []).map((user) => (typeof user === "string" ? user : user.login));

export function issuePillar(issue) {
  const topics = (issue.labels ?? [])
    .map((label) => (typeof label === "string" ? label : label.name))
    .filter((name) => name?.startsWith("topic:"))
    .map((name) => name.slice(6).trim())
    .filter(Boolean);
  return topics.length === 1 ? topics[0] : null;
}

export function issueSystem(issue) {
  const match = /^\s*Parent:\s*(.+)$/im.exec(issue.body ?? "");
  return match?.[1].replace(/[*_`]/g, "").trim() || "General";
}

export function issueState(issue) {
  if (issue.state === "closed") return "done";
  return logins(issue).length ? "in-progress" : "open";
}

export function displayTitle(title) {
  const t = title
    .replace(/^(?:M\d+:\s*)?(?:M\d+-\d+):?\s+/, "")
    .replace(/^M\d+:\s*/, "")
    .replace(/^M\d+\s+(?:epic|flagship):\s*/i, "")
    .replace(/\s*\(M\d+\)$/, "");
  return t.charAt(0).toUpperCase() + t.slice(1);
}

export const milestoneCode = (title) => /^(M\d+)\b/.exec(title)?.[1] ?? null;

// Markdown to one plain line; the same rules as the website's build-content.mjs.
const plain = (s) =>
  s
    .replace(/ÔåÆ/g, "→")
    .replace(/!\[[^\]]*\]\([^)]*\)/g, "")
    .replace(/\[([^\]]+)\]\([^)]*\)/g, "$1")
    .replace(/<[^>]+>/g, "")
    .replace(/[`*]/g, "")
    .replace(/\s+/g, " ")
    .trim();
const clip = (s, max) => (s.length > max ? `${s.slice(0, max - 1).replace(/\s+\S*$/, "")}…` : s);

export function issueSections(body) {
  const out = {};
  const parts = (body ?? "").replace(/\r\n/g, "\n").replace(/```[\s\S]*?```/g, "").split(/^##\s+(.+)$/m);
  for (let k = 1; k < parts.length; k += 2) out[parts[k].trim().toLowerCase()] = parts[k + 1] ?? "";
  return out;
}
const bullets = (section, max, len) =>
  (section ?? "")
    .split("\n")
    .filter((l) => /^[-*]\s+/.test(l))
    .map((l) => clip(plain(l.replace(/^[-*]\s+/, "")), len))
    .filter(Boolean)
    .slice(0, max);

/** Thread name: "#96 Police station on the street…". The number is the key back to the issue. */
export function threadName(issue) {
  return clip(`#${issue.number} ${displayTitle(issue.title)}`, 100);
}
export function threadIssueNumber(name) {
  const match = /^#(\d+)\s/.exec(name ?? "");
  return match ? Number(match[1]) : null;
}

/** The issue card: the bot's starter message in the issue's thread. */
export function renderIssueCard(issue, milestone, byNumber = new Map(), syncedAt = new Date().toISOString()) {
  const state = issueState(issue);
  const status = STATUS[state];
  const s = issueSections(issue.body);
  const pillar = issuePillar(issue) ?? "Unsorted";
  const people = logins(issue);
  const parts = [];

  parts.push(`${status.emoji} **${status.tag}** · ${milestoneCode(milestone.title)} · ${escapeMarkdown(pillar)} / ${escapeMarkdown(issueSystem(issue))}`);
  if (s.why) parts.push(`**Why**\n${escapeMarkdown(clip(plain(s.why.split(/\n\s*[-*]\s/)[0]), 600))}`);
  const work = bullets(s.work ?? s["candidate scopes (not cards yet)"], 6, 200);
  if (work.length) parts.push(`**${state === "done" ? "What was built" : "What to build"}**\n${work.map((w) => `• ${escapeMarkdown(w)}`).join("\n")}`);
  const proof = bullets(s["acceptance proof"], 4, 180);
  if (proof.length) parts.push(`**Done when**\n${proof.map((p) => `• ${escapeMarkdown(p)}`).join("\n")}`);
  const deps = [...new Set([...(s["depends on"] ?? "").matchAll(/#(\d+)/g)].map((m) => Number(m[1])))];
  if (deps.length) {
    const lines = deps.map((n) => {
      const dep = byNumber.get(n);
      if (!dep) return `• #${n}`;
      return `• ${STATUS[issueState(dep)].emoji} [#${n} ${escapeMarkdown(displayTitle(dep.title))}](${dep.html_url})`;
    });
    parts.push(`**Waits on**\n${lines.join("\n")}`);
  }
  if (people.length) {
    parts.push(
      `**${state === "done" ? "Built by" : "On it"}**\n${people.map((login) => `[${escapeMarkdown(login)}](https://github.com/${encodeURIComponent(login)})`).join(", ")}`,
    );
  }
  if (state !== "done") {
    parts.push(
      "**Want in?** Comment `/claim` on the GitHub issue and a bot assigns you; several people can claim one issue, and `/unclaim` steps back. " +
        "Ask questions and talk it through here in this thread.",
    );
  }
  parts.push(`[Open the issue](${issue.html_url}) · [See it on the Tree of Life](${SITE_ROADMAP})`);

  let description = parts.join("\n\n");
  if (description.length > MAX_DESCRIPTION) description = `${description.slice(0, MAX_DESCRIPTION - 1)}…`;
  return {
    name: threadName(issue),
    state,
    pillar,
    embed: {
      title: clip(displayTitle(issue.title), 256),
      url: issue.html_url,
      description,
      color: status.color,
      footer: { text: `GitHub #${issue.number} · synced ${syncedAt.slice(0, 10)}` },
      timestamp: syncedAt,
    },
  };
}

/** The forum's tags: one per pillar, then the three statuses. Existing tags keep their IDs. */
export function wantedTags(existing) {
  const names = [...PILLAR_ORDER, ...Object.values(STATUS).map((st) => st.tag)];
  const kept = new Map((existing ?? []).map((tag) => [tag.name, tag]));
  const tags = names.map((name) => {
    const tag = kept.get(name);
    if (tag) return { id: tag.id, name, moderated: true, emoji_name: tag.emoji_name ?? null, emoji_id: tag.emoji_id ?? null };
    const emoji = PILLAR_EMOJI[name] ?? Object.values(STATUS).find((st) => st.tag === name)?.emoji ?? null;
    return { name, moderated: true, emoji_name: emoji, emoji_id: null };
  });
  // Tags someone added by hand stay, after ours, within Discord's limit of 20.
  for (const tag of existing ?? []) if (!names.includes(tag.name)) tags.push(tag);
  return tags.slice(0, 20);
}

async function githubPages(path, token) {
  let url = `${GITHUB_API}${path}`;
  const out = [];
  while (url) {
    const headers = { Accept: "application/vnd.github+json", "X-GitHub-Api-Version": "2022-11-28", "User-Agent": "life-framework-roadmap-sync" };
    if (token) headers.Authorization = `Bearer ${token}`;
    const response = await fetch(url, { headers });
    if (!response.ok) throw new Error(`GitHub API returned HTTP ${response.status} for ${new URL(url).pathname}`);
    const page = await response.json();
    if (!Array.isArray(page)) throw new Error(`GitHub API returned a non-list for ${new URL(url).pathname}`);
    out.push(...page);
    const next = /<([^>]+)>;\s*rel="next"/.exec(response.headers.get("link") ?? "");
    url = next ? next[1] : null;
  }
  return out;
}

async function discordRequest(method, path, token, body) {
  for (let attempt = 0; attempt < 6; attempt++) {
    const headers = { Authorization: `Bot ${token}`, "User-Agent": "DiscordBot (life-framework-roadmap-sync, 3.0)" };
    if (body !== undefined) headers["Content-Type"] = "application/json";
    const response = await fetch(`${DISCORD_API}${path}`, { method, headers, body: body === undefined ? undefined : JSON.stringify(body) });
    if (response.status === 429) {
      const data = await response.json().catch(() => ({}));
      await delay(Math.ceil((data.retry_after ?? 1) * 1000) + 250);
      continue;
    }
    if (response.status === 204) return null;
    const text = await response.text();
    let data = {};
    if (text) {
      try {
        data = JSON.parse(text);
      } catch {
        throw new Error(`Discord API returned HTTP ${response.status} with an invalid response.`);
      }
    }
    if (!response.ok) throw new Error(`Discord API ${method} ${path.split("?")[0]} returned HTTP ${response.status}: ${JSON.stringify(data)}`);
    return data;
  }
  throw new Error("Discord API rate limit did not clear after six retries.");
}

async function forumThreads(forum, token) {
  const active = await discordRequest("GET", `/guilds/${forum.guild_id}/threads/active`, token);
  const threads = (active.threads ?? []).filter((thread) => thread.parent_id === forum.id);
  let before = null;
  while (true) {
    const query = new URLSearchParams({ limit: "100" });
    if (before) query.set("before", before);
    const page = await discordRequest("GET", `/channels/${forum.id}/threads/archived/public?${query}`, token);
    threads.push(...(page.threads ?? []));
    if (!page.has_more || !page.threads?.length) break;
    before = page.threads[page.threads.length - 1].thread_metadata?.archive_timestamp;
    if (!before) break;
  }
  return [...new Map(threads.map((thread) => [thread.id, thread])).values()];
}

async function loadRoadmap(repository, token) {
  const milestones = (await githubPages(`/repos/${repository}/milestones?state=all&per_page=100`, token)).filter((m) => milestoneCode(m.title));
  const entries = [];
  for (const milestone of milestones) {
    const issues = await githubPages(`/repos/${repository}/issues?milestone=${milestone.number}&state=all&per_page=100`, token);
    for (const issue of issues) if (!issue.pull_request) entries.push({ milestone, issue });
  }
  return entries.sort((a, b) => a.milestone.number - b.milestone.number || a.issue.number - b.issue.number);
}

const message = (embed) => ({ embeds: [embed], allowed_mentions: { parse: [] } });
const sameTags = (a, b) => a.length === b.length && a.every((id) => b.includes(id));

async function sync({ dryRun }) {
  const githubToken = process.env.GITHUB_TOKEN;
  const repository = process.env.GITHUB_REPOSITORY;
  const discordToken = process.env.DISCORD_BOT_TOKEN;
  const forumId = process.env.DISCORD_ROADMAP_FORUM_ID;
  if (!repository || !/^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$/.test(repository)) throw new Error("Set GITHUB_REPOSITORY to owner/repo.");

  const entries = await loadRoadmap(repository, githubToken);
  const byNumber = new Map(entries.map((entry) => [entry.issue.number, entry.issue]));
  const syncedAt = new Date().toISOString();
  const cards = new Map(entries.map(({ issue, milestone }) => [issue.number, renderIssueCard(issue, milestone, byNumber, syncedAt)]));

  if (dryRun) {
    for (const card of cards.values()) if (card.state !== "done") console.log(`\n=== ${card.name} [${card.pillar}]\n${card.embed.description}`);
    console.log(`\nDry run: ${[...cards.values()].filter((c) => c.state !== "done").length} open threads, ${cards.size} roadmap issues.`);
    return;
  }
  // Not set up yet: say so and pass, so every issue event does not fail a workflow.
  if (!githubToken || !discordToken || !forumId) {
    console.log("::notice::Discord roadmap sync skipped: set the DISCORD_BOT_TOKEN secret and DISCORD_ROADMAP_FORUM_ID variable to enable it.");
    return;
  }
  if (!/^\d{17,20}$/.test(forumId)) throw new Error("DISCORD_ROADMAP_FORUM_ID must be a Discord channel ID.");

  let forum = await discordRequest("GET", `/channels/${forumId}`, discordToken);
  if (forum.type !== FORUM_CHANNEL_TYPE) throw new Error(`Discord channel ${forumId} is not a Forum channel (type 15).`);
  const tags = wantedTags(forum.available_tags);
  const tagsChanged = tags.length !== (forum.available_tags ?? []).length || tags.some((tag) => !tag.id);
  if (tagsChanged) forum = await discordRequest("PATCH", `/channels/${forum.id}`, discordToken, { available_tags: tags });
  const tagId = new Map(forum.available_tags.map((tag) => [tag.name, tag.id]));
  const tagsFor = (card) => [tagId.get(card.pillar), tagId.get(STATUS[card.state].tag)].filter(Boolean);

  const me = await discordRequest("GET", "/users/@me", discordToken);
  const threads = new Map();
  for (const thread of await forumThreads(forum, discordToken)) {
    const number = threadIssueNumber(thread.name);
    if (thread.owner_id !== me.id || number === null) continue;
    if (threads.has(number)) throw new Error(`Two Discord threads are for issue #${number}. Remove one and rerun.`);
    threads.set(number, thread);
  }

  const counts = { created: 0, updated: 0, closed: 0, retired: 0 };
  for (const [number, card] of cards) {
    const thread = threads.get(number);
    if (!thread) {
      if (card.state === "done") continue; // finished work gets no new thread
      await discordRequest("POST", `/channels/${forum.id}/threads`, discordToken, {
        name: card.name,
        auto_archive_duration: 10080,
        applied_tags: tagsFor(card),
        message: message(card.embed),
      });
      counts.created++;
      continue;
    }
    const meta = thread.thread_metadata ?? {};
    // A finished issue's thread was settled when it closed; leave it be.
    if (card.state === "done" && meta.archived) continue;
    // Editing needs the thread open; an open issue's thread stays open.
    if (meta.archived) await discordRequest("PATCH", `/channels/${thread.id}`, discordToken, { archived: false, locked: false });
    // A forum post's starter message has the thread's ID.
    await discordRequest("PATCH", `/channels/${thread.id}/messages/${thread.id}`, discordToken, message(card.embed));
    const patch = {};
    if (thread.name !== card.name) patch.name = card.name;
    if (!sameTags(thread.applied_tags ?? [], tagsFor(card))) patch.applied_tags = tagsFor(card);
    if (card.state === "done") patch.archived = true;
    if (Object.keys(patch).length) await discordRequest("PATCH", `/channels/${thread.id}`, discordToken, patch);
    if (card.state === "done") counts.closed++;
    else counts.updated++;
  }

  for (const [number, thread] of threads) {
    if (cards.has(number)) continue;
    if (thread.thread_metadata?.archived && thread.thread_metadata?.locked) continue;
    if (thread.thread_metadata?.archived) await discordRequest("PATCH", `/channels/${thread.id}`, discordToken, { archived: false });
    await discordRequest("PATCH", `/channels/${thread.id}`, discordToken, { archived: true, locked: true });
    counts.retired++;
  }

  console.log(
    `Discord roadmap sync: ${counts.created} created, ${counts.updated} updated, ${counts.closed} done, ${counts.retired} retired; ${cards.size} roadmap issues.`,
  );
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  sync({ dryRun: process.argv.includes("--dry-run") }).catch((error) => {
    console.error(`Discord roadmap sync failed: ${error.message}`);
    process.exitCode = 1;
  });
}
