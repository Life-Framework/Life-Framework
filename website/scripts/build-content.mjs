// Builds the site's content from the repo and GitHub, so the site and the mod never drift.
//
//   curated docs/*.md and docs/roadmap/**              -> src/generated/docs-index.json
//                                                         src/generated/data/doc-<slug>.json (HTML)
//   docs/decisions.md                                   -> src/generated/data/decisions.json
//   parts of docs/direction.md, content/                -> src/generated/site.json
//   GitHub milestones, issues and their sub-issues,     -> src/generated/github.json
//   merged PRs, contributors
//                                                         src/generated/data/devlog.json
//   public/docs-images/**, content/gallery.json         -> src/generated/data/gallery.json, public/gallery/*.webp (git-ignored)
//   content/support.json, content/costs.json            ->src/generated/support.json (scripts/support.mjs)
//   content/media.json (film-evidence release assets)   -> public/media/clips/ (downloaded, git-ignored)
//
// GitHub is read through the public REST API (GITHUB_TOKEN lifts the rate limit).
// If the API cannot be reached, the build falls back to content/github-snapshot.json
// and the pages say so. `npm run snapshot` refreshes that file.

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import GithubSlugger from "github-slugger";
import { unified } from "unified";
import remarkParse from "remark-parse";
import remarkGfm from "remark-gfm";
import remarkRehype from "remark-rehype";
import rehypeSlug from "rehype-slug";
import rehypeStringify from "rehype-stringify";
import { buildSupport } from "./support.mjs";
import { buildGallery } from "./gallery.mjs";

const here = path.dirname(fileURLToPath(import.meta.url));
const site = path.resolve(here, "..");
const repo = path.resolve(site, "..");
const outDir = path.join(site, "src", "generated");
const dataDir = path.join(outDir, "data");
const snapshotPath = path.join(site, "content", "github-snapshot.json");
const updateSnapshot = process.argv.includes("--update-snapshot");

export const GITHUB_REPO = "Life-Framework/Life-Framework";
const BLOB = `https://github.com/${GITHUB_REPO}/blob/main/`;

const toPosix = (p) => p.split(path.sep).join("/");
const read = (p) => fs.readFileSync(p, "utf8").replace(/\r\n/g, "\n");

const md = unified().use(remarkParse).use(remarkGfm).use(remarkRehype).use(rehypeSlug).use(rehypeStringify);
const toHtml = (text) => String(md.processSync(text));

// One line of plain text from markdown: links keep their text, code and emphasis lose their marks.
const plain = (s) =>
  s
    // Some issue bodies carry a UTF-8 arrow that was read as Windows-1252.
    .replace(/ÔåÆ/g, "→")
    .replace(/!\[[^\]]*\]\([^)]*\)/g, "")
    .replace(/\[([^\]]+)\]\([^)]*\)/g, "$1")
    .replace(/<[^>]+>/g, "")
    .replace(/[`*]/g, "")
    .replace(/(^|\s)_([^_]+)_(?=[\s.,;:)]|$)/g, "$1$2")
    .replace(/\s+/g, " ")
    .trim();

// Cut at a sentence end before `max` characters when there is one, else at a word.
// A last sentence that ends in ":" introduced a list that is not shown; drop it.
function clip(text, max) {
  let t = text.trim();
  if (t.length > max) {
    const cut = t.slice(0, max);
    const end = cut.lastIndexOf(". ");
    t = end > max * 0.4 ? cut.slice(0, end + 1) : cut.slice(0, cut.lastIndexOf(" ")).replace(/[,;:]$/, "") + "...";
  }
  if (t.endsWith(":")) {
    const end = t.lastIndexOf(". ");
    t = end > 0 ? t.slice(0, end + 1) : t.slice(0, -1) + ".";
  }
  return t;
}

// ---------------------------------------------------------------- docs

function walk(dir) {
  if (!fs.existsSync(dir)) return [];
  return fs.readdirSync(dir, { withFileTypes: true }).flatMap((e) => {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) return walk(p);
    return e.name.endsWith(".md") ? [p] : [];
  });
}

const firstHeading = (text) => (/^#\s+(.+)$/m.exec(text) || [])[1]?.trim() || null;
const stripFirstHeading = (text) => text.replace(/^\s*#\s+.+\n+/, "");

function describe(text) {
  for (const block of text.split(/\n{2,}/)) {
    const t = block.trim();
    if (!t || /^(#|\||```|- |\* |\d+\.|>|<|---)/.test(t)) continue;
    return clip(plain(t), 200);
  }
  return "";
}

function headings(text) {
  const slugger = new GithubSlugger();
  const out = [];
  let fence = false;
  for (const line of text.split("\n")) {
    if (/^\s*```/.test(line)) fence = !fence;
    if (fence) continue;
    const m = /^(#{1,3})\s+(.+?)\s*#*\s*$/.exec(line);
    if (!m) continue;
    const t = plain(m[2]);
    out.push({ level: m[1].length, text: t, id: slugger.slug(t) });
  }
  return out;
}

const docs = [];

function addRepoDoc(rel, { slug, group, order, title }) {
  const abs = path.join(repo, rel);
  if (!fs.existsSync(abs)) return;
  const raw = read(abs);
  docs.push({ slug, group, order, title: title || firstHeading(raw) || slug, repoPath: rel, body: stripFirstHeading(raw) });
}

addRepoDoc("docs/setup.md", { slug: "setup", group: "Start here", order: 0, title: "Set up your tools" });
addRepoDoc("docs/playtest.md", { slug: "playtest", group: "Start here", order: 1, title: "Open the playtest" });
addRepoDoc("CONTRIBUTING.md", { slug: "contributing", group: "Start here", order: 2 });

addRepoDoc("docs/direction.md", { slug: "direction", group: "Direction & decisions", order: 0 });
// Lands in its own PR; addRepoDoc skips a file that is not there yet.
addRepoDoc("docs/architecture/core-vs-pack.md", { slug: "core-vs-pack", group: "Direction & decisions", order: 1 });
addRepoDoc("docs/design.md", { slug: "design", group: "Direction & decisions", order: 2 });
addRepoDoc("docs/decisions.md", { slug: "decisions", group: "Direction & decisions", order: 3 });

addRepoDoc("docs/features.md", { slug: "features", group: "Engineering reference", order: 0 });
addRepoDoc("docs/persistence.md", { slug: "persistence", group: "Engineering reference", order: 1 });
addRepoDoc("docs/foundation-design.md", { slug: "foundation-design", group: "Engineering reference", order: 2 });
addRepoDoc("docs/worktrees.md", { slug: "worktrees", group: "Engineering reference", order: 3 });
addRepoDoc("docs/dedicated-session.md", { slug: "dedicated-session", group: "Engineering reference", order: 4 });
addRepoDoc("docs/debug-log-triage.md", { slug: "debug-log-triage", group: "Engineering reference", order: 5 });
addRepoDoc("docs/street-manifest.md", { slug: "street-manifest", group: "Engineering reference", order: 6 });

addRepoDoc("docs/ecosystem.md", { slug: "ecosystem", group: "Research & community", order: 0 });
addRepoDoc("docs/research/life-rp-benchmark.md", { slug: "life-rp-benchmark", group: "Research & community", order: 1 });
addRepoDoc("docs/overthrow-survey.md", { slug: "overthrow-survey", group: "Research & community", order: 2 });
addRepoDoc("docs/everonlife-forks.md", { slug: "everonlife-forks", group: "Research & community", order: 3 });
addRepoDoc("docs/community/discord.md", { slug: "discord", group: "Research & community", order: 4 });
addRepoDoc("docs/community/support.md", { slug: "support", group: "Research & community", order: 5 });

const roadmapFiles = walk(path.join(repo, "docs", "roadmap"))
  .map((p) => toPosix(path.relative(repo, p)))
  .sort((a, b) => {
    const order = ["docs/roadmap/README.md", "docs/roadmap/m1-playtest-town.md", "docs/roadmap/m2-outline.md", "docs/roadmap/m3-outline.md", "docs/roadmap/m4-outline.md"];
    const rank = (p) => (order.includes(p) ? order.indexOf(p) : order.length);
    const key = (p) => p.replace(/README\.md$/, ""); // a folder's README sorts before its files
    return rank(a) - rank(b) || key(a).localeCompare(key(b));
  });
roadmapFiles.forEach((rel, i) => {
  const parts = rel.slice("docs/roadmap/".length).replace(/\.md$/, "").split("/");
  if (parts[parts.length - 1] === "README") parts.pop();
  const title = rel === "docs/roadmap/README.md" ? "Current gameplay plan" : undefined;
  addRepoDoc(rel, { slug: ["roadmap", ...parts].join("-").toLowerCase(), group: "Current plan", order: i, title });
});

// Pages with their own route instead of /docs/<slug>.
const ownRoute = { decisions: "/decisions" };
const docHref = (d) => ownRoute[d.slug] || `/docs/${d.slug}`;

const bySlug = new Map();
const byPath = new Map();
for (const d of docs) {
  if (bySlug.has(d.slug)) throw new Error(`duplicate doc slug ${d.slug} (${d.repoPath}, ${bySlug.get(d.slug).repoPath})`);
  bySlug.set(d.slug, d);
  byPath.set(d.repoPath, d);
}

// Relative links: to the doc's page when the target is on the site, otherwise to the file on GitHub.
function rewriteTarget(fromRel, target) {
  if (/^([a-z][a-z0-9+.-]*:|#|\/)/i.test(target)) return target;
  const [p, hash] = target.split("#");
  if (!p) return target;
  const resolved = path.posix.normalize(path.posix.join(path.posix.dirname(fromRel), p));
  if (resolved.startsWith("..")) return target;
  const doc = byPath.get(resolved) || byPath.get(resolved.replace(/\/$/, "") + "/README.md");
  const anchor = hash ? `#${hash}` : "";
  if (doc) return docHref(doc) + anchor;
  return `${BLOB}${resolved}${anchor}`;
}

function rewriteLinks(text, fromRel) {
  let fence = false;
  return text
    .split("\n")
    .map((line) => {
      if (/^\s*```/.test(line)) fence = !fence;
      if (fence) return line;
      return line
        .replace(/(\]\()([^)\s]+)((?:\s+"[^"]*")?\))/g, (_, a, t, b) => a + rewriteTarget(fromRel, t) + b)
        .replace(/^(\[[^\]]+\]:\s*)(\S+)/, (_, a, t) => a + rewriteTarget(fromRel, t));
    })
    .join("\n");
}

// Wrap tables so a wide one scrolls inside its box instead of widening the page.
const wrapTables = (html) => html.replace(/<table>/g, '<div class="table-wrap"><table>').replace(/<\/table>/g, "</table></div>");

const docsIndex = [];
fs.rmSync(dataDir, { recursive: true, force: true });
fs.mkdirSync(dataDir, { recursive: true });
for (const d of docs) {
  const markdown = rewriteLinks(d.body, d.repoPath);
  const meta = {
    slug: d.slug,
    href: docHref(d),
    group: d.group,
    order: d.order,
    title: d.title,
    repoPath: d.repoPath,
    sourceUrl: `${BLOB}${d.repoPath}`,
    editUrl: `https://github.com/${GITHUB_REPO}/edit/main/${d.repoPath}`,
    description: describe(markdown),
  };
  docsIndex.push(meta);
  fs.writeFileSync(
    path.join(dataDir, `doc-${d.slug}.json`),
    JSON.stringify({ ...meta, headings: headings(markdown).filter((h) => h.level === 2), html: wrapTables(toHtml(markdown)) }),
  );
}

// ---------------------------------------------------------------- decisions

// "## Section" blocks hold entries that open with "**YYYY-MM-DD — Title.**".
function parseDecisions(text) {
  const [intro, ...rest] = text.split(/^---\s*$/m);
  const body = rest.join("---");
  const slugger = new GithubSlugger();
  const sections = [];
  let section = null;
  let entry = null;
  for (const block of body.split(/\n{2,}/)) {
    const b = block.trim();
    if (!b) continue;
    const h = /^##\s+(.+)$/.exec(b);
    if (h) {
      section = { title: h[1].trim(), id: slugger.slug(h[1].trim()), entries: [] };
      sections.push(section);
      entry = null;
      continue;
    }
    const e = /^\*\*(\d{4}-\d{2}-\d{2})\s+[—–-]+\s+([\s\S]+?)\*\*\s*([\s\S]*)$/.exec(b);
    if (e && section) {
      const title = e[2].replace(/\s+/g, " ").replace(/\.$/, "");
      entry = { date: e[1], title, id: slugger.slug(title), parts: [e[3]] };
      section.entries.push(entry);
    } else if (entry) {
      entry.parts.push(b);
    }
  }
  const out = sections.map((s) => ({
    title: s.title,
    id: s.id,
    entries: s.entries.map((en) => {
      const raw = en.parts.join("\n\n");
      const sup = /Superseded (\d{4}-\d{2}-\d{2}) by "([^"]+)"/.exec(raw);
      const amended = /Amended (\d{4}-\d{2}-\d{2})/.exec(raw);
      return {
        date: en.date,
        title: en.title,
        id: en.id,
        superseded: sup ? { date: sup[1], by: sup[2] } : null,
        amended: amended ? amended[1] : null,
        summary: clip(plain(en.parts[0]).split(/\s+Why:/)[0], 220),
        html: toHtml(rewriteLinks(raw, "docs/decisions.md")),
      };
    }),
  }));
  // The intro opens with the paragraph the page header already says; keep the rest.
  const introRest = stripFirstHeading(intro).trim().split(/\n{2,}/).slice(1).join("\n\n");
  return { introHtml: toHtml(rewriteLinks(introRest, "docs/decisions.md")), sections: out };
}

const decisionsDoc = bySlug.get("decisions");
const decisions = parseDecisions(decisionsDoc.body);
const decisionCount = decisions.sections.reduce((n, s) => n + s.entries.length, 0);
if (decisionCount < 5) throw new Error(`decisions.json: only ${decisionCount} entries parsed from docs/decisions.md; check the entry format`);
fs.writeFileSync(path.join(dataDir, "decisions.json"), JSON.stringify(decisions));

// ---------------------------------------------------------------- site.json

function section(text, title) {
  const lines = text.split("\n");
  const start = lines.findIndex((l) => new RegExp(`^##\\s+${title}\\s*$`, "i").test(l));
  if (start < 0) return "";
  let end = lines.findIndex((l, i) => i > start && /^##\s/.test(l));
  if (end < 0) end = lines.length;
  return lines.slice(start + 1, end).join("\n").trim();
}

// The numbered loop under "What playable means".
function loop(text) {
  return text
    .split("\n")
    .filter((l) => /^\d+\.\s/.test(l))
    .map((l) => plain(l.replace(/^\d+\.\s+/, "")));
}

// "- **Name** — what it does" bullets under "## Features" in the README.
function readmeFeatures() {
  const readme = read(path.join(repo, "README.md"));
  return section(readme, "Features")
    .replace(/\n[ \t]+/g, " ")
    .split("\n")
    .map((l) => /^- \*\*(.+?)\*\*\s*[—–-]+\s*(.+)$/.exec(l))
    .filter(Boolean)
    .map((m) => ({ name: m[1], what: plain(m[2]) }));
}

const direction = bySlug.get("direction").body;
const playable = section(direction, 'What "playable" means');
const siteJson = {
  ambition: plain(/The ambition is[\s\S]*?in that order\./.exec(direction)?.[0] || "").replace(/\s*—\s*/g, ", "),
  loop: loop(playable),
  inWorldFirst: plain(
    decisions.sections.flatMap((s) => s.entries).find((e) => e.title === "In-world first")?.html.replace(/<[^>]+>/g, " ") || "",
  ),
  features: readmeFeatures(),
  decisionCount,
  latestDecisions: decisions.sections
    .flatMap((s) => s.entries.map((e) => ({ ...e, section: s.title })))
    .filter((e) => !e.superseded)
    .sort((a, b) => b.date.localeCompare(a.date))
    .slice(0, 4)
    .map(({ date, title, id, summary }) => ({ date, title, id, summary })),
};
if (!siteJson.ambition) throw new Error('site.json: "The ambition is ... in that order." is gone from docs/direction.md; update build-content.mjs');
if (siteJson.loop.length < 5) throw new Error('site.json: the numbered loop under "What playable means" in docs/direction.md is missing');
if (!siteJson.inWorldFirst) throw new Error('site.json: the "In-world first" entry is missing from docs/decisions.md');
if (siteJson.features.length < 5) throw new Error('site.json: the "- **Name** — what" bullets under "## Features" in README.md are missing');
// ---------------------------------------------------------------- GitHub

async function gh(pathAndQuery, { maxPages = 10, allowTruncate = false } = {}) {
  const headers = { Accept: "application/vnd.github+json", "User-Agent": "life-framework-website" };
  if (process.env.GITHUB_TOKEN) headers.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
  const out = [];
  let url = `https://api.github.com${pathAndQuery}`;
  for (let page = 0; url; page++) {
    if (page >= maxPages) {
      if (allowTruncate) {
        console.warn(`[content] stopped after ${maxPages} pages for ${new URL(url).pathname}`);
        break;
      }
      throw new Error(`GitHub pagination exceeded ${maxPages} pages for ${new URL(url).pathname}`);
    }
    const res = await fetch(url, { headers, signal: AbortSignal.timeout(15000) });
    if (!res.ok) throw new Error(`GitHub ${res.status} for ${url}`);
    const body = await res.json();
    if (!Array.isArray(body)) return body;
    out.push(...body);
    const next = /<([^>]+)>;\s*rel="next"/.exec(res.headers.get("link") || "");
    url = next ? next[1] : null;
  }
  return out;
}

// What an issue is for: the "## Why" section of a scope issue, else the first prose.
function issueExcerpt(body) {
  if (!body) return "";
  const text = body.replace(/\r\n/g, "\n");
  const why = /^##\s+Why\s*\n([\s\S]*?)(?=\n##\s|$(?![\s\S]))/m.exec(text);
  return clip(
    plain(
      (why ? why[1] : text)
        .replace(/```[\s\S]*?```/g, "")
        .replace(/<!--[\s\S]*?-->/g, "")
        .split("\n")
        .filter((l) => l.trim() && !/^\s*(#|\||- \[|>|Part of\b|Parent:)/.test(l))
        .join(" "),
    ),
    240,
  );
}

// One line for the devlog: the first prose paragraph of the PR body, skipping
// headings, "Closes #n", images, tables, and code.
function prSummary(body) {
  if (!body) return "";
  const blocks = body
    .replace(/\r\n/g, "\n")
    .replace(/```[\s\S]*?```/g, "")
    .replace(/<!--[\s\S]*?-->/g, "")
    .split(/\n{2,}/)
    .map((b) => b.trim())
    .filter(Boolean);
  for (const b of blocks) {
    if (/^(#|\||!\[|>|(closes|fixes|resolves|part of|refs?)\b)/i.test(b) || /Generated with/.test(b)) continue;
    // Prose up to the first list item; a block that opens with a list gives its first item.
    const lines = b.split("\n").filter((l) => !/^#/.test(l));
    const isItem = (l) => /^\s*([-*]|\d+\.)\s+/.test(l);
    const firstItem = lines.findIndex(isItem);
    const prose = firstItem === 0 ? [lines[0]] : firstItem > 0 ? lines.slice(0, firstItem) : lines;
    const t = plain(prose.join(" ").replace(/^\s*([-*]|\d+\.)\s+/, ""));
    if (t.length < 25) continue;
    return clip(t, 200);
  }
  return "";
}

// A system parent named in the issue body: "Parent: #148" or "Parent: Police & Response".
function issueParent(body) {
  const match = /^\s*Parent:\s*(.+)$/im.exec(body || "");
  return match ? plain(match[1]).trim() || null : null;
}

// "Layer: Core" or "Layer: Flagship" in the body, when the issue names its side (docs/architecture/core-vs-pack.md).
function issueLayerLine(body) {
  const m = /\bLayer:\s*(Core|Flagship)\b/i.exec(body || "");
  return m ? m[1][0].toUpperCase() + m[1].slice(1).toLowerCase() : null;
}

// The body's "## Heading" sections, keyed by lowercased heading.
function issueSections(body) {
  const out = {};
  const text = (body || "").replace(/\r\n/g, "\n").replace(/```[\s\S]*?```/g, "").replace(/<!--[\s\S]*?-->/g, "");
  const parts = text.split(/^##\s+(.+)$/m);
  for (let k = 1; k < parts.length; k += 2) out[parts[k].trim().toLowerCase()] = parts[k + 1] || "";
  return out;
}

// Top-level list items of a section as short plain lines; nested items are skipped.
const bullets = (section, max, len) =>
  (section || "")
    .split("\n")
    .filter((l) => /^[-*]\s+/.test(l))
    .map((l) => clip(plain(l.replace(/^[-*]\s+/, "")), len))
    .filter(Boolean)
    .slice(0, max);

// What a contributor needs on the roadmap card: the work, the proof, what it waits on.
function issueBrief(body) {
  const s = issueSections(body);
  const deps = [...(s["depends on"] || "").matchAll(/#(\d+)/g)].map((m) => Number(m[1]));
  return {
    why: s.why ? clip(plain(s.why.split(/\n\s*[-*]\s/)[0]), 420) : "",
    work: bullets(s.work ?? s["candidate scopes (not cards yet)"], 5, 160),
    proof: bullets(s["acceptance proof"], 4, 140),
    dependsOn: [...new Set(deps)],
  };
}

const issueOut = (i) => ({
  number: i.number,
  title: i.title,
  state: i.state,
  url: i.html_url,
  labels: i.labels.map((l) => l.name),
  assignees: i.assignees.map((a) => a.login),
  closedAt: i.closed_at,
  excerpt: issueExcerpt(i.body),
  wave: Number((/\bWave (\d+)\b/.exec(i.body || "") || [])[1]) || null,
  parent: issueParent(i.body),
  layer: issueLayerLine(i.body),
  ...issueBrief(i.body),
  comments: i.comments ?? 0,
  createdAt: i.created_at,
  updatedAt: i.updated_at,
});

// An epic's sub-issues in GitHub's order. Asked for when GitHub counts any, or when the
// title says "epic" (the count can lag). A failure here leaves that epic without children
// instead of sending the whole GitHub read to the snapshot.
async function subIssues(i) {
  if (!(i.sub_issues_summary?.total > 0) && !/\bepic\b/i.test(i.title)) return [];
  try {
    return (await gh(`/repos/${GITHUB_REPO}/issues/${i.number}/sub_issues?per_page=100`)).map((s) => ({
      number: s.number,
      title: s.title,
      state: s.state,
      url: s.html_url,
      assignees: (s.assignees || []).map((a) => a.login),
      labels: (s.labels || []).map((l) => l.name),
      layer: issueLayerLine(s.body),
      milestone: (/^M\d+/.exec(s.milestone?.title || "") || [])[0] || null,
    }));
  } catch (err) {
    console.warn(`[content] sub-issues of #${i.number} not read (${err.message})`);
    return [];
  }
}

// The #numbers of an epic body's "## Sub-issues" checklist, in order. Used only when GitHub lists no
// linked sub-issues, so an epic filed with a checklist still shows its branch on the roadmap.
function checklistRefs(body) {
  const sections = issueSections(body);
  const key = Object.keys(sections).find((k) => /^sub[- ]?issues\b/.test(k));
  if (!key) return [];
  const refs = sections[key].split(/\r?\n/).filter((l) => /^\s*[-*]\s+\[[ xX]\]/.test(l)).map((l) => Number((/#(\d+)/.exec(l) || [])[1]));
  return [...new Set(refs.filter(Boolean))];
}

// Fill epics that have only a body checklist from the issues already read (milestones and tracks).
function resolveBodyRefs(groups) {
  const byNumber = new Map();
  for (const g of groups) for (const i of g.issues) byNumber.set(i.number, { i, milestone: g.id });
  for (const g of groups) {
    for (const e of g.issues) {
      const refs = e.bodyRefs || [];
      delete e.bodyRefs;
      if (e.subIssues.length > 0 || refs.length === 0) continue;
      e.subIssues = refs
        .filter((n) => n !== e.number && byNumber.has(n))
        .map((n) => {
          const { i, milestone } = byNumber.get(n);
          return { number: n, title: i.title, state: i.state, url: i.url, assignees: i.assignees, labels: i.labels, layer: i.layer, milestone };
        });
    }
  }
}

async function fetchGithub() {
  // LF_SITE_OFFLINE=1 builds from the snapshot, to test the fallback.
  if (process.env.LF_SITE_OFFLINE) throw new Error("LF_SITE_OFFLINE is set");
  const all = await gh(`/repos/${GITHUB_REPO}/milestones?state=all&per_page=100`);
  const readMilestone = async (m, id) => {
    const raw = (await gh(`/repos/${GITHUB_REPO}/issues?milestone=${m.number}&state=all&per_page=100`)).filter((i) => !i.pull_request);
    const issues = [];
    for (const i of raw) {
      const out = { ...issueOut(i), subIssues: await subIssues(i) };
      // GitHub has no linked children, but the body lists them: "## Sub-issues" with "- [ ] #411 ...".
      if (out.subIssues.length === 0) out.bodyRefs = checklistRefs(i.body);
      issues.push(out);
    }
    return { id, number: m.number, title: m.title, description: m.description || "", url: m.html_url, state: m.state, issues };
  };
  const milestonesOut = [];
  const ms = all.filter((m) => /^M\d+\b/.test(m.title)).sort((a, b) => a.title.localeCompare(b.title, undefined, { numeric: true }));
  for (const m of ms) milestonesOut.push(await readMilestone(m, /^M\d+/.exec(m.title)[0]));
  // A track runs beside the milestone path and is never the current milestone. "Development" is one.
  const tracks = [];
  for (const m of all.filter((x) => /^development$/i.test(x.title.trim()))) tracks.push(await readMilestone(m, "Dev"));
  resolveBodyRefs([...milestonesOut, ...tracks]);
  const goodFirst = (await gh(`/repos/${GITHUB_REPO}/issues?labels=${encodeURIComponent("good first issue")}&state=open&per_page=100`))
    .filter((i) => !i.pull_request)
    .map(issueOut);
  const pulls = (await gh(`/repos/${GITHUB_REPO}/pulls?state=closed&sort=updated&direction=desc&per_page=100`, { maxPages: 3, allowTruncate: true }))
    .filter((p) => p.merged_at && p.base.ref === "main")
    .sort((a, b) => b.merged_at.localeCompare(a.merged_at))
    .slice(0, 150)
    .map((p) => ({ number: p.number, title: p.title, url: p.html_url, mergedAt: p.merged_at, author: p.user?.login || null, summary: prSummary(p.body) }));
  const contributors = (await gh(`/repos/${GITHUB_REPO}/contributors?per_page=100`, { maxPages: 2, allowTruncate: true }))
    .filter((c) => c.type === "User" && !/\[bot\]$/.test(c.login))
    .map((c) => ({ login: c.login, url: c.html_url, contributions: c.contributions }));
  return { fetchedAt: new Date().toISOString(), milestones: milestonesOut, tracks, goodFirst, pulls, contributors };
}

let github;
try {
  github = { ...(await fetchGithub()), source: "github" };
  if (updateSnapshot) {
    const { source: _s, ...snap } = github;
    fs.writeFileSync(snapshotPath, JSON.stringify(snap, null, 2) + "\n");
    console.log(`[content] wrote ${toPosix(path.relative(repo, snapshotPath))}`);
  }
} catch (err) {
  if (updateSnapshot) throw err;
  if (!fs.existsSync(snapshotPath)) throw new Error(`GitHub unreachable (${err.message}) and no snapshot at ${snapshotPath}`);
  console.warn(`[content] GitHub unreachable (${err.message}); using the committed snapshot`);
  github = { ...JSON.parse(read(snapshotPath)), source: "snapshot" };
}

// ---------------------------------------------------------------- devlog

// Merged PRs and recorded decisions, newest first.
const devlog = [
  ...github.pulls.map((p) => ({ kind: "pr", date: p.mergedAt, title: p.title, summary: p.summary, href: p.url, number: p.number })),
  ...decisions.sections.flatMap((s) =>
    s.entries.map((e) => ({ kind: "decision", date: e.date, title: e.title, summary: e.summary, href: `/decisions#${e.id}`, number: null })),
  ),
].sort((a, b) => b.date.slice(0, 10).localeCompare(a.date.slice(0, 10)) || (a.kind === b.kind ? b.date.localeCompare(a.date) : a.kind === "decision" ? -1 : 1));
fs.writeFileSync(path.join(dataDir, "devlog.json"), JSON.stringify({ source: github.source, fetchedAt: github.fetchedAt, entries: devlog }));

// ---------------------------------------------------------------- media

// Clips are release assets (film-evidence); the build downloads them so the page serves them itself.
// The API asset endpoint is used because the browser download URL answers 504 to scripts at times.
async function releaseAsset(downloadUrl) {
  const m = /\/releases\/download\/([^/]+)\/([^/]+)$/.exec(downloadUrl);
  if (!m) throw new Error("not a release download URL");
  const rel = await gh(`/repos/${GITHUB_REPO}/releases/tags/${m[1]}`);
  const asset = rel.assets.find((a) => a.name === decodeURIComponent(m[2]));
  if (!asset) throw new Error(`no asset ${m[2]} on release ${m[1]}`);
  const headers = { Accept: "application/octet-stream", "User-Agent": "life-framework-website" };
  if (process.env.GITHUB_TOKEN) headers.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
  return fetch(asset.url, { headers, redirect: "follow", signal: AbortSignal.timeout(60000) });
}

const media = JSON.parse(read(path.join(site, "content", "media.json")));
const clipDir = path.join(site, "public", "media", "clips");
fs.mkdirSync(clipDir, { recursive: true });
for (const c of media.clips) {
  const file = path.join(clipDir, path.basename(c.release));
  c.local = fs.existsSync(file);
  if (!c.local) {
    try {
      const res = await releaseAsset(c.release);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      fs.writeFileSync(file, Buffer.from(await res.arrayBuffer()));
      c.local = true;
    } catch (err) {
      console.warn(`[content] clip ${c.release} not downloaded (${err.message}); the page links to the release instead`);
    }
  }
  c.src = c.local ? `/media/clips/${path.basename(c.release)}` : null;
}

// ---------------------------------------------------------------- write

// The playtest download (tools/release/pack-playtest.mjs, .github/workflows/playtest-release.yml): the
// "latest" release carries LifeFramework-Playtest.zip. The page also refreshes this in the browser
// (src/lib/release.ts), so a release published after this build still shows its own version and size.
async function fetchPlaytestRelease() {
  try {
    const rel = await gh(`/repos/${GITHUB_REPO}/releases/latest`);
    const asset = (rel.assets || []).find((a) => a.name === "LifeFramework-Playtest.zip");
    if (!asset) return null;
    return { tag: rel.tag_name, name: rel.name || rel.tag_name, publishedAt: rel.published_at, size: asset.size };
  } catch (err) {
    console.warn(`[content] no playtest release (${err.message}); the download button shows no version`);
    return null;
  }
}

const { pulls, ...githubRest } = github;
const githubOut = { ...githubRest, latestPulls: pulls.slice(0, 5), playtestRelease: await fetchPlaytestRelease() };
// ---------------------------------------------------------------- gallery

const gallery = await buildGallery({ site, github, repoUrl: `https://github.com/${GITHUB_REPO}` });
fs.writeFileSync(path.join(dataDir, "gallery.json"), JSON.stringify(gallery));
console.log(`[content] gallery: ${gallery.count} pictures`);

fs.writeFileSync(path.join(outDir, "docs-index.json"), JSON.stringify(docsIndex));
fs.writeFileSync(path.join(outDir, "site.json"), JSON.stringify({ ...siteJson, media }));
fs.writeFileSync(path.join(outDir, "github.json"), JSON.stringify(githubOut));
fs.writeFileSync(path.join(outDir, "support.json"), JSON.stringify(buildSupport({ site, repo })));
console.log(
  `[content] ${docsIndex.length} docs, ${decisionCount} decisions, ${github.milestones.length} milestones, ` +
    `${github.milestones.map((m) => `${m.id} ${m.issues.length} issues, ${m.issues.reduce((n, i) => n + (i.subIssues?.length ?? 0), 0)} sub-issues`).join("; ")}, ${pulls.length} merged PRs, ` +
    `${(github.tracks ?? []).map((t) => `${t.title} track ${t.issues.length} issues`).join(", ") || "no tracks"}, ` +
    `${github.contributors.length} contributors (${github.source})`,
);
