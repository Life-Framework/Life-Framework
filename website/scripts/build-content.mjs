// Builds the site's content from the repo, so the site and the mod never drift.
//
//   docs/direction.md, docs/decisions.md, docs/ecosystem.md (if present),
//   docs/roadmap/**/*.md, website/content/docs/*.md  -> src/generated/docs.json
//   parts of direction.md and decisions.md            -> src/generated/site.json
//   GitHub milestone M1 + its issues, good first issues -> src/generated/github.json
//
// GitHub is read through the public REST API (GITHUB_TOKEN is used when set, to
// lift the rate limit). If the API cannot be reached, the build falls back to
// content/github-snapshot.json and says so. `npm run snapshot` refreshes that file.

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import GithubSlugger from "github-slugger";

const here = path.dirname(fileURLToPath(import.meta.url));
const site = path.resolve(here, "..");
const repo = path.resolve(site, "..");
const outDir = path.join(site, "src", "generated");
const snapshotPath = path.join(site, "content", "github-snapshot.json");
const updateSnapshot = process.argv.includes("--update-snapshot");

export const GITHUB_REPO = "Life-Framework/Life-Framework";
const BLOB = `https://github.com/${GITHUB_REPO}/blob/main/`;

const toPosix = (p) => p.split(path.sep).join("/");
const read = (p) => fs.readFileSync(p, "utf8").replace(/\r\n/g, "\n");

// ---------------------------------------------------------------- frontmatter

function parseFrontmatter(text) {
  const m = /^---\n([\s\S]*?)\n---\n?/.exec(text);
  if (!m) return { data: {}, body: text };
  const data = {};
  for (const line of m[1].split("\n")) {
    const kv = /^([A-Za-z_][\w-]*):\s*(.*)$/.exec(line);
    if (!kv) continue;
    let v = kv[2].trim();
    if (v.startsWith('"')) {
      try { v = JSON.parse(v); } catch { /* keep raw */ }
    }
    data[kv[1]] = v;
  }
  return { data, body: text.slice(m[0].length) };
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

function firstHeading(md) {
  const m = /^#\s+(.+)$/m.exec(md);
  return m ? m[1].trim() : null;
}

function stripFirstHeading(md) {
  return md.replace(/^\s*#\s+.+\n+/, "");
}

function describe(md) {
  for (const block of md.split(/\n{2,}/)) {
    const t = block.trim();
    if (!t || /^(#|\||```|- |\* |\d+\.|>|<)/.test(t)) continue;
    const plain = t
      .replace(/\[([^\]]+)\]\([^)]*\)/g, "$1")
      .replace(/[`*]/g, "")
      .replace(/\s+/g, " ");
    return plain.length > 220 ? plain.slice(0, 217).trimEnd() + "..." : plain;
  }
  return "";
}

function headings(md) {
  const slugger = new GithubSlugger();
  const out = [];
  let fence = false;
  for (const line of md.split("\n")) {
    if (/^\s*```/.test(line)) fence = !fence;
    if (fence) continue;
    const m = /^(#{1,3})\s+(.+?)\s*#*\s*$/.exec(line);
    if (!m) continue;
    const text = m[2].replace(/\[([^\]]+)\]\([^)]*\)/g, "$1").replace(/[`*]/g, "");
    out.push({ level: m[1].length, text, id: slugger.slug(text) });
  }
  return out;
}

const docs = [];

function addRepoDoc(rel, { slug, group, order, title }) {
  const abs = path.join(repo, rel);
  if (!fs.existsSync(abs)) return;
  const raw = read(abs);
  docs.push({
    slug,
    group,
    order,
    title: title || firstHeading(raw) || slug,
    repoPath: rel,
    source: "repo",
    body: stripFirstHeading(raw),
  });
}

addRepoDoc("docs/direction.md", { slug: "direction", group: "Project", order: 0 });
addRepoDoc("docs/decisions.md", { slug: "decisions", group: "Project", order: 1 });
addRepoDoc("docs/ecosystem.md", { slug: "ecosystem", group: "Project", order: 2 });

const roadmapRoot = path.join(repo, "docs", "roadmap");
const roadmapFiles = walk(roadmapRoot)
  .map((p) => toPosix(path.relative(repo, p)))
  .sort((a, b) => {
    const rank = (p) => (p === "docs/roadmap/README.md" ? 0 : p === "docs/roadmap/m1-playtest-town.md" ? 1 : 2);
    // A folder's README sorts before the files in it.
    const key = (p) => p.replace(/README\.md$/, "");
    return rank(a) - rank(b) || key(a).localeCompare(key(b));
  });
roadmapFiles.forEach((rel, i) => {
  const inner = rel.slice("docs/roadmap/".length).replace(/\.md$/, "");
  const parts = inner.split("/");
  if (parts[parts.length - 1] === "README") parts.pop();
  const slug = ["roadmap", ...parts].join("-").toLowerCase();
  addRepoDoc(rel, { slug, group: "Roadmap", order: i });
});

for (const abs of walk(path.join(site, "content", "docs"))) {
  const { data, body } = parseFrontmatter(read(abs));
  const slug = data.slug || path.basename(abs, ".md");
  docs.push({
    slug,
    group: "Community guides",
    order: Number(data.order || 0),
    title: data.title || firstHeading(body) || slug,
    repoPath: toPosix(path.relative(repo, abs)),
    source: data.source || "site",
    updated: data.updated || null,
    body: stripFirstHeading(body),
  });
}

// Rewrite relative links: to /docs/<slug> when the target is on the site,
// otherwise to the file on GitHub.
const bySlug = new Map();
const byPath = new Map();
for (const d of docs) {
  if (bySlug.has(d.slug)) throw new Error(`duplicate doc slug ${d.slug} (${d.repoPath}, ${bySlug.get(d.slug).repoPath})`);
  bySlug.set(d.slug, d);
  byPath.set(d.repoPath, d);
}

function rewriteTarget(fromRel, target) {
  if (/^([a-z][a-z0-9+.-]*:|#|\/)/i.test(target)) return target;
  const [p, hash] = target.split("#");
  if (!p) return target;
  const resolved = path.posix.normalize(path.posix.join(path.posix.dirname(fromRel), p));
  if (resolved.startsWith("..")) return target;
  const doc = byPath.get(resolved) || byPath.get(resolved.replace(/\/$/, "") + "/README.md");
  const anchor = hash ? `#${hash}` : "";
  if (doc) return `/docs/${doc.slug}${anchor}`;
  return `${BLOB}${resolved}${anchor}`;
}

function rewriteLinks(d) {
  let fence = false;
  return d.body
    .split("\n")
    .map((line) => {
      if (/^\s*```/.test(line)) fence = !fence;
      if (fence) return line;
      return line
        .replace(/(\]\()([^)\s]+)((?:\s+"[^"]*")?\))/g, (_, a, t, b) => a + rewriteTarget(d.repoPath, t) + b)
        .replace(/^(\[[^\]]+\]:\s*)(\S+)/, (_, a, t) => a + rewriteTarget(d.repoPath, t));
    })
    .join("\n");
}

const docsOut = docs.map((d) => {
  const markdown = rewriteLinks(d);
  return {
    slug: d.slug,
    group: d.group,
    order: d.order,
    title: d.title,
    repoPath: d.repoPath,
    editUrl: `https://github.com/${GITHUB_REPO}/edit/main/${d.repoPath}`,
    source: d.source,
    updated: d.updated || null,
    description: describe(markdown),
    headings: headings(markdown),
    markdown,
  };
});

// ---------------------------------------------------------------- site.json

function section(md, title) {
  const lines = md.split("\n");
  const start = lines.findIndex((l) => new RegExp(`^##\\s+${title}\\s*$`, "i").test(l));
  if (start < 0) return "";
  let end = lines.findIndex((l, i) => i > start && /^##\s/.test(l));
  if (end < 0) end = lines.length;
  return lines.slice(start + 1, end).join("\n").trim();
}

function decisionEntry(md, needle) {
  const blocks = md.split(/\n{2,}/);
  const i = blocks.findIndex((b) => b.includes(needle));
  return i < 0 ? "" : blocks[i].trim();
}

function milestones(md) {
  const table = section(md, "Milestones");
  return table
    .split("\n")
    .filter((l) => /^\|\s*M\d/.test(l))
    .map((l) => {
      const cells = l.split("|").slice(1, -1).map((c) => c.trim());
      return { id: cells[0], name: cells[1].replace(/\*\*/g, ""), doneWhen: cells[2] };
    });
}

const direction = docs.find((d) => d.slug === "direction")?.body || "";
const decisions = docs.find((d) => d.slug === "decisions")?.body || "";
const siteJson = {
  building: section(direction, "What we are building"),
  playable: section(direction, "What \"playable\" means"),
  aiFirst: section(direction, "How we build: AI-first"),
  proof: section(direction, "Proof in the 3D world"),
  notNow: section(direction, "Not now"),
  inWorldFirst: decisionEntry(decisions, "In-world first.").replace(/^\*\*[^*]+\*\*\s*/, ""),
  milestones: milestones(direction),
  archive: JSON.parse(read(path.join(site, "content", "archive", "roadmap-2025.json"))),
  team: JSON.parse(read(path.join(site, "content", "team.json"))),
};
for (const k of ["building", "playable", "inWorldFirst"]) {
  if (!siteJson[k]) throw new Error(`site.json: could not extract "${k}" from docs/direction.md or docs/decisions.md; update build-content.mjs`);
}
if (siteJson.milestones.length === 0) throw new Error("site.json: no milestone rows in docs/direction.md");

// ---------------------------------------------------------------- GitHub

async function gh(pathAndQuery) {
  const headers = { Accept: "application/vnd.github+json", "User-Agent": "life-framework-website" };
  if (process.env.GITHUB_TOKEN) headers.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
  const out = [];
  let url = `https://api.github.com${pathAndQuery}`;
  while (url) {
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

function excerpt(body) {
  if (!body) return "";
  // Scope issues open with a "Part of ..." line; the "## Why" section says what it is.
  const why = /^##\s+Why\s*\n([\s\S]*?)(?=\n##\s|$(?![\s\S]))/m.exec(body.replace(/\r\n/g, "\n"));
  const text = (why ? why[1] : body)
    .replace(/```[\s\S]*?```/g, "")
    .replace(/<!--[\s\S]*?-->/g, "")
    .split("\n")
    .filter((l) => l.trim() && !/^\s*(#|\||- \[|>)/.test(l))
    .join(" ")
    .replace(/\[([^\]]+)\]\([^)]*\)/g, "$1")
    .replace(/[`*]/g, "")
    .replace(/\s+/g, " ")
    .trim();
  return text.length > 240 ? text.slice(0, 237).trimEnd() + "..." : text;
}

const issueOut = (i) => ({
  number: i.number,
  title: i.title,
  state: i.state,
  url: i.html_url,
  labels: i.labels.map((l) => l.name),
  assignees: i.assignees.map((a) => a.login),
  updatedAt: i.updated_at,
  closedAt: i.closed_at,
  excerpt: excerpt(i.body),
  wave: Number((/\bWave (\d+)\b/.exec(i.body || "") || [])[1]) || null,
});

async function fetchGithub() {
  const ms = await gh(`/repos/${GITHUB_REPO}/milestones?state=all&per_page=100`);
  const m1 = ms.find((m) => /^M1\b/.test(m.title));
  let milestone = null;
  let issues = [];
  if (m1) {
    milestone = {
      number: m1.number,
      title: m1.title,
      description: m1.description || "",
      url: m1.html_url,
      state: m1.state,
      open: m1.open_issues,
      closed: m1.closed_issues,
      dueOn: m1.due_on,
    };
    issues = (await gh(`/repos/${GITHUB_REPO}/issues?milestone=${m1.number}&state=all&per_page=100`))
      .filter((i) => !i.pull_request)
      .map(issueOut);
  }
  const goodFirst = (await gh(`/repos/${GITHUB_REPO}/issues?labels=${encodeURIComponent("good first issue")}&state=open&per_page=100`))
    .filter((i) => !i.pull_request)
    .map(issueOut);
  return { fetchedAt: new Date().toISOString(), milestone, issues, goodFirst };
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

// ---------------------------------------------------------------- write

fs.mkdirSync(outDir, { recursive: true });
fs.writeFileSync(path.join(outDir, "docs.json"), JSON.stringify(docsOut));
fs.writeFileSync(path.join(outDir, "site.json"), JSON.stringify(siteJson));
fs.writeFileSync(path.join(outDir, "github.json"), JSON.stringify(github));
console.log(
  `[content] ${docsOut.length} docs, ${siteJson.milestones.length} milestones, ` +
    `${github.issues.length} M1 issues, ${github.goodFirst.length} good first issues (${github.source})`,
);
