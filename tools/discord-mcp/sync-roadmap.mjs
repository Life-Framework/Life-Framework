#!/usr/bin/env node
// Mirror the website's Tree of Life into a read-only Discord Forum: one post per pillar.
// GitHub is the source of truth. A pillar is an issue's `topic:` label; a system is the
// `Parent:` line at the top of its body; only issues in an M milestone count (as on the site).
// Posts are edited in place. A post whose pillar no longer exists is renamed [ARCHIVED],
// archived, and locked, never deleted. That also retires the older one-post-per-milestone cards.
//
//   node tools/discord-mcp/sync-roadmap.mjs            sync (needs the env below)
//   node tools/discord-mcp/sync-roadmap.mjs --dry-run  print the posts; needs only GITHUB_REPOSITORY (+ GITHUB_TOKEN for rate limits)

import { setTimeout as delay } from "node:timers/promises";
import { pathToFileURL } from "node:url";

const GITHUB_API = "https://api.github.com";
const DISCORD_API = "https://discord.com/api/v10";
const FORUM_CHANNEL_TYPE = 15;
const MAX_DESCRIPTION = 3800;
const SITE_ROADMAP = "https://life-framework.org/roadmap";
const ARCHIVED = "[ARCHIVED] ";

// Same order as the website (website/src/components/RoadmapTree.tsx).
export const PILLAR_ORDER = [
  "Law & Order",
  "Economy & Work",
  "Life & Identity",
  "Community & Organizations",
  "World & Travel",
  "Framework & Playtest",
  "Flagship Story",
  "Unsorted",
  "Multiple topics",
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

function escapeMarkdown(text) {
  return String(text).replace(/([\\`*_{}\[\]()<>#+\-.!|~])/g, "\\$1");
}

const logins = (issue) => (issue.assignees ?? []).map((user) => (typeof user === "string" ? user : user.login));

export function issuePillar(issue) {
  const topics = (issue.labels ?? [])
    .map((label) => (typeof label === "string" ? label : label.name))
    .filter((name) => name?.startsWith("topic:"))
    .map((name) => name.slice(6).trim())
    .filter(Boolean);
  if (topics.length === 0) return "Unsorted";
  if (topics.length > 1) return "Multiple topics";
  return topics[0];
}

export function issueSystem(issue) {
  const match = /^\s*Parent:\s*(.+)$/im.exec(issue.body ?? "");
  return match?.[1].replace(/[*_`]/g, "").trim() || "General";
}

function issueState(issue) {
  if (issue.state === "closed") return "done";
  return logins(issue).length ? "in-progress" : "open";
}

function displayTitle(title) {
  return title
    .replace(/^(?:M\d+:\s*)?(?:M\d+-\d+):?\s+/, "")
    .replace(/^M\d+:\s*/, "")
    .replace(/^M\d+\s+(?:epic|flagship):\s*/i, "")
    .replace(/\s*\(M\d+\)$/, "");
}

/** Group milestone issues into pillars and systems, ordered like the website. */
export function buildPillars(entries) {
  const pillars = new Map();
  for (const { milestone, issue } of entries) {
    const name = issuePillar(issue);
    if (!pillars.has(name)) pillars.set(name, []);
    pillars.get(name).push({ milestone, issue });
  }
  const rank = (name) => {
    const i = PILLAR_ORDER.indexOf(name);
    return i < 0 ? PILLAR_ORDER.length : i;
  };
  const order = (a, b) => a.milestone.number - b.milestone.number || a.issue.number - b.issue.number;
  return [...pillars]
    .map(([name, items]) => {
      const systems = new Map();
      for (const item of items) {
        const system = issueSystem(item.issue);
        if (!systems.has(system)) systems.set(system, []);
        systems.get(system).push(item);
      }
      return {
        name,
        items,
        systems: [...systems].map(([system, list]) => ({ name: system, items: list.sort(order) })).sort((a, b) => order(a.items[0], b.items[0])),
      };
    })
    .sort((a, b) => rank(a.name) - rank(b.name) || a.name.localeCompare(b.name));
}

export function milestoneCode(title) {
  return /^(M\d+)\b/.exec(title)?.[1] ?? null;
}

function issueLine(item) {
  const state = issueState(item.issue);
  const mark = state === "done" ? "✅" : state === "in-progress" ? "🔄" : "🙋";
  const code = milestoneCode(item.milestone.title) ?? "";
  const title = escapeMarkdown(displayTitle(item.issue.title));
  const people = logins(item.issue);
  const owners =
    state === "in-progress" ? ` · ${people.map((login) => `[${escapeMarkdown(login)}](https://github.com/${encodeURIComponent(login)})`).join(", ")}` : "";
  return `${mark} \`${code}\` [${title}](${item.issue.html_url})${owners}`;
}

/** The thread name is the pillar name, so it is also the key that finds the post again. */
export function threadName(pillar) {
  return pillar.slice(0, 100);
}

export function archivedThreadName(name) {
  return (name.startsWith(ARCHIVED) ? name : `${ARCHIVED}${name}`).slice(0, 100);
}

export function renderPillarPost(pillar, syncedAt = new Date().toISOString()) {
  const done = pillar.items.filter((i) => issueState(i.issue) === "done").length;
  const open = pillar.items.filter((i) => issueState(i.issue) === "open");
  const total = pillar.items.length;
  const percent = total ? Math.round((done / total) * 100) : 0;

  const head = [
    `**${done}/${total} done (${percent}%)** · ${pillar.systems.length} system${pillar.systems.length === 1 ? "" : "s"} · ${
      open.length ? `**${open.length} need${open.length === 1 ? "s" : ""} a hand**` : "nothing open right now"
    }`,
    `🙋 needs a hand · 🔄 in progress · ✅ done. To join, open an issue and comment \`/claim\` on GitHub; \`/unclaim\` steps back. Several people can claim one issue.`,
  ].join("\n");

  let description = head;
  let omitted = 0;
  for (const system of pillar.systems) {
    const sysDone = system.items.filter((i) => issueState(i.issue) === "done").length;
    // Open work first so the post leads with what a contributor can pick up.
    const rank = { open: 0, "in-progress": 1, done: 2 };
    const items = [...system.items].sort((a, b) => rank[issueState(a.issue)] - rank[issueState(b.issue)]);
    const heading = `\n\n**${escapeMarkdown(system.name)}** · ${sysDone}/${system.items.length}`;
    if (description.length + heading.length > MAX_DESCRIPTION) {
      omitted += items.length;
      continue;
    }
    description += heading;
    for (let i = 0; i < items.length; i++) {
      const line = `\n${issueLine(items[i])}`;
      if (description.length + line.length > MAX_DESCRIPTION) {
        omitted += items.length - i;
        break;
      }
      description += line;
    }
  }
  const tail = `\n\n${omitted ? `_${omitted} more issue(s) not shown._ ` : ""}[See the whole tree](${SITE_ROADMAP})`;
  description += tail;
  if (description.length > 4096) description = `${description.slice(0, 4093)}…`;

  const emoji = PILLAR_EMOJI[pillar.name];
  return {
    name: threadName(pillar.name),
    embed: {
      title: `${emoji ? `${emoji} ` : ""}${pillar.name}`.slice(0, 256),
      url: SITE_ROADMAP,
      description,
      color: 0xe3b23c,
      footer: { text: `Synced from GitHub · ${syncedAt.slice(0, 10)}` },
      timestamp: syncedAt,
    },
  };
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
  for (let attempt = 0; attempt < 5; attempt++) {
    const headers = { Authorization: `Bot ${token}`, "User-Agent": "DiscordBot (life-framework-roadmap-sync, 2.0)" };
    if (body !== undefined) headers["Content-Type"] = "application/json";
    const response = await fetch(`${DISCORD_API}${path}`, { method, headers, body: body === undefined ? undefined : JSON.stringify(body) });
    if (response.status === 429) {
      const data = await response.json().catch(() => ({}));
      await delay(Math.ceil((data.retry_after ?? 1) * 1000) + 100);
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
  throw new Error("Discord API rate limit did not clear after five retries.");
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

async function loadPillars(repository, token) {
  const milestones = (await githubPages(`/repos/${repository}/milestones?state=all&per_page=100`, token)).filter((m) => milestoneCode(m.title));
  const entries = [];
  for (const milestone of milestones) {
    const issues = await githubPages(`/repos/${repository}/issues?milestone=${milestone.number}&state=all&per_page=100`, token);
    for (const issue of issues) if (!issue.pull_request) entries.push({ milestone, issue });
  }
  return buildPillars(entries);
}

const message = (embed) => ({ embeds: [embed], allowed_mentions: { parse: [] } });

async function sync({ dryRun }) {
  const githubToken = process.env.GITHUB_TOKEN;
  const repository = process.env.GITHUB_REPOSITORY;
  const discordToken = process.env.DISCORD_BOT_TOKEN;
  const forumId = process.env.DISCORD_ROADMAP_FORUM_ID;
  if (!repository || !/^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$/.test(repository)) throw new Error("Set GITHUB_REPOSITORY to owner/repo.");

  const pillars = await loadPillars(repository, githubToken);
  const syncedAt = new Date().toISOString();
  const posts = pillars.map((pillar) => renderPillarPost(pillar, syncedAt));

  if (dryRun) {
    for (const post of posts) console.log(`\n=== ${post.name} (${post.embed.description.length} chars)\n${post.embed.description}`);
    console.log(`\nDry run: ${posts.length} pillar posts.`);
    return;
  }
  // Not set up yet: say so and pass, so every issue event does not fail a workflow.
  if (!githubToken || !discordToken || !forumId) {
    console.log("::notice::Discord roadmap sync skipped: set the DISCORD_BOT_TOKEN secret and DISCORD_ROADMAP_FORUM_ID variable to enable it.");
    return;
  }
  if (!/^\d{17,20}$/.test(forumId)) throw new Error("DISCORD_ROADMAP_FORUM_ID must be a Discord channel ID.");

  const forum = await discordRequest("GET", `/channels/${forumId}`, discordToken);
  if (forum.type !== FORUM_CHANNEL_TYPE) throw new Error(`Discord channel ${forumId} is not a Forum channel (type 15).`);
  const me = await discordRequest("GET", "/users/@me", discordToken);

  // Only the bot's own posts are managed; anything a person posted is left alone.
  const threads = (await forumThreads(forum, discordToken)).filter((thread) => thread.owner_id === me.id);
  const byName = new Map();
  for (const thread of threads) {
    if (thread.name.startsWith(ARCHIVED)) continue;
    if (byName.has(thread.name)) throw new Error(`Two Discord posts are named "${thread.name}". Remove one and rerun.`);
    byName.set(thread.name, thread);
  }

  const refresh = process.env.GITHUB_EVENT_NAME === "schedule";
  let created = 0;
  let updated = 0;
  let retired = 0;
  for (const post of posts) {
    const thread = byName.get(post.name);
    if (!thread) {
      await discordRequest("POST", `/channels/${forum.id}/threads`, discordToken, { name: post.name, auto_archive_duration: 10080, message: message(post.embed) });
      created++;
      continue;
    }
    const wasArchived = thread.thread_metadata?.archived ?? false;
    if (wasArchived) await discordRequest("PATCH", `/channels/${thread.id}`, discordToken, { archived: false });
    // A forum post's starter message has the thread's ID.
    await discordRequest("PATCH", `/channels/${thread.id}/messages/${thread.id}`, discordToken, message(post.embed));
    // Forum posts auto-archive after a week idle; a scheduled reply keeps them in the list.
    if (refresh || wasArchived) {
      await discordRequest("POST", `/channels/${thread.id}/messages`, discordToken, {
        content: `Refreshed from GitHub · ${syncedAt.slice(0, 10)}`,
        allowed_mentions: { parse: [] },
      });
    }
    updated++;
  }

  const live = new Set(posts.map((post) => post.name));
  for (const thread of threads) {
    if (live.has(thread.name)) continue;
    if (thread.name.startsWith(ARCHIVED) && thread.thread_metadata?.archived && thread.thread_metadata?.locked) continue;
    if (thread.thread_metadata?.archived) await discordRequest("PATCH", `/channels/${thread.id}`, discordToken, { archived: false });
    await discordRequest("PATCH", `/channels/${thread.id}`, discordToken, { name: archivedThreadName(thread.name), archived: true, locked: true });
    retired++;
  }

  console.log(`Discord roadmap sync: ${created} created, ${updated} updated, ${retired} retired; ${posts.length} pillars.`);
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  sync({ dryRun: process.argv.includes("--dry-run") }).catch((error) => {
    console.error(`Discord roadmap sync failed: ${error.message}`);
    process.exitCode = 1;
  });
}
