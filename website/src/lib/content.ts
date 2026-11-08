// Typed access to what scripts/build-content.mjs writes at build time.
import site from "@/generated/site.json";
import github from "@/generated/github.json";
import docsIndex from "@/generated/docs-index.json";
import supportJson from "@/generated/support.json";

export const REPO = "Life-Framework/Life-Framework";
export const REPO_URL = `https://github.com/${REPO}`;
export const DISCORD_URL = "https://discord.gg/MjMexJteqz";
export const blob = (p: string) => `${REPO_URL}/blob/main/${p}`;
export const tree = (p: string) => `${REPO_URL}/tree/main/${p}`;
export const issuesUrl = (q: string) => `${REPO_URL}/issues?q=${encodeURIComponent(q)}`;

export type Heading = { level: number; text: string; id: string };

export type DocMeta = {
  slug: string;
  href: string;
  group: string;
  order: number;
  title: string;
  repoPath: string;
  sourceUrl: string;
  editUrl: string;
  description: string;
};
export type Doc = DocMeta & { headings: Heading[]; html: string };

export type Issue = {
  number: number;
  title: string;
  state: "open" | "closed";
  url: string;
  labels: string[];
  assignees: string[];
  closedAt: string | null;
  excerpt: string;
  wave: number | null;
  parent: string | null;
  // "Layer: Core" or "Layer: Flagship" from the body, when the issue says.
  layer?: Layer | null;
  // From the body's sections (build-content.mjs issueBrief); empty when a body has none.
  why?: string;
  work?: string[];
  proof?: string[];
  dependsOn?: number[];
  comments?: number;
  createdAt?: string;
  updatedAt?: string;
  // An epic's sub-issues in GitHub's order (build-content.mjs subIssues); empty for other issues.
  subIssues?: SubIssue[];
};

export type SubIssue = { number: number; title: string; state: "open" | "closed"; url: string; assignees: string[]; labels?: string[]; layer?: Layer | null; milestone: string | null };

export type Milestone = {
  id: string;
  number: number;
  title: string;
  description: string;
  url: string;
  state: string;
  issues: Issue[];
};

export type Pull = { number: number; title: string; url: string; mergedAt: string; author: string | null; summary: string };

export type GithubData = {
  source: "github" | "snapshot";
  fetchedAt: string;
  milestones: Milestone[];
  goodFirst: Issue[];
  latestPulls: Pull[];
  contributors: { login: string; url: string; contributions: number }[];
};

export type Still = {
  id: string;
  alt: string;
  caption: string;
  where: string;
  date: string;
  source: string;
  sourceUrl: string;
  wip: boolean;
  widths: number[];
  width: number;
  height: number;
};
export type Clip = {
  id: string;
  title: string;
  caption: string;
  release: string;
  poster: string;
  date: string;
  source: string;
  sourceUrl: string;
  wip: boolean;
  src: string | null;
};

export type SiteData = {
  ambition: string;
  loop: string[];
  inWorldFirst: string;
  features: { name: string; what: string }[];
  decisionCount: number;
  latestDecisions: { date: string; title: string; id: string; summary: string }[];
  media: { stills: Still[]; clips: Clip[] };
};

export type DevlogEntry = { kind: "pr" | "decision"; date: string; title: string; summary: string; href: string; number: number | null };
export type Devlog = { source: "github" | "snapshot"; fetchedAt: string; entries: DevlogEntry[] };

export type Decision = {
  date: string;
  title: string;
  id: string;
  superseded: { date: string; by: string } | null;
  amended: string | null;
  summary: string;
  html: string;
};
export type Decisions = { introHtml: string; sections: { title: string; id: string; entries: Decision[] }[] };

export type Wallet = {
  asset: string;
  network: string;
  standard: string;
  address: string;
  explorer: string;
  explorerName: string;
  qr: { size: number; path: string };
};
export type Cost = { what: string; why: string; amount: number | null; period: "month" | "year"; status: "paying" | "planned" };
export type SupportData = {
  enabled: boolean;
  card: { name: string; url: string } | null;
  crypto: Wallet[];
  costs: { updated: string; items: Cost[] };
};

export const siteData = site as SiteData;
export const support = supportJson as SupportData;
// True once a way to give is live; the footer link and the page's methods follow it.
export const supportOpen = support.enabled && (support.card !== null || support.crypto.length > 0);
export const githubData = github as GithubData;
export const docs = docsIndex as DocMeta[];

export const milestone = (id: string) => githubData.milestones.find((m) => m.id === id) ?? null;
export function issueTopic(issue: Issue): string {
  const topics = issue.labels.filter((label) => label.startsWith("topic:")).map((label) => label.slice(6).trim()).filter(Boolean);
  if (topics.length === 0) return "Unsorted";
  if (topics.length > 1) return "Multiple topics";
  return topics[0];
}

export function milestoneName(m: Milestone): string {
  const fromTitle = m.title.replace(new RegExp(`^${m.id}\\b\\s*(?:[:—–-]\\s*)?`), "").trim();
  if (fromTitle) return fromTitle;
  const fromDescription = m.description.split(/[:.]\s/, 1)[0].trim();
  return fromDescription || m.id;
}

// The one-line goal of each milestone and where its plan lives. The milestones, their titles, and
// their issues come from GitHub; docs/direction.md and docs/roadmap/ hold the full "done when".
export const MILESTONE_PLAN: Record<string, { goal: string; plan: { label: string; to: string }; status?: string }> = {
  M1: {
    goal: "A signposted test town where a stranger plays every loop on a dedicated server without asking anyone.",
    plan: { label: "M1 plan", to: "/docs/roadmap-m1-playtest-town" },
    status: "Final playtest",
  },
  M2: {
    goal: "Moderation, medics and revive, comms, and investigation, proven in the playtest village, then placed in one Everon town.",
    plan: { label: "M2 outline", to: "/docs/roadmap-m2-outline" },
  },
  M3: {
    goal: "The mod splits into the LifeFramework core and the LF_Everon1989 pack, and an owner configures it from one config.json. No new gameplay.",
    plan: { label: "M3 outline", to: "/docs/roadmap-m3-outline" },
  },
  M4: {
    goal: "Our 1989 Everon server opens to the public on the released core and pack, for about 40 players.",
    plan: { label: "M4 outline", to: "/docs/roadmap-m4-outline" },
  },
  M5: {
    goal: "The flagship's own story. Players are relocated from Chernobyl and start life again on Everon in 1989.",
    plan: { label: "Ideas epic #214", to: "https://github.com/Life-Framework/Life-Framework/issues/214" },
  },
};

const currentMilestone =githubData.milestones.find((m) => m.state === "open") ?? githubData.milestones[0];
export const CURRENT_MILESTONE = currentMilestone?.id ?? "M1";
export const CURRENT_MILESTONE_NAME = currentMilestone ? milestoneName(currentMilestone) : "Current milestone";
export function milestoneStatus(m: Milestone): { label: string; tone: "green" | "red" | "ink" } {
  if (m.state === "closed") return { label: "Done", tone: "green" };
  if (m.id === CURRENT_MILESTONE) return { label: MILESTONE_PLAN[m.id]?.status ?? "Current", tone: "red" };
  return { label: "Planned", tone: "ink" };
}

// Core or Flagship. The core decides what the game can do; the flagship pack decides what our server is
// (docs/architecture/core-vs-pack.md). A `layer:core` or `layer:flagship` label wins, then a
// "Layer: Core/Flagship" line in the body, then the milestone: M1 to M3 build core systems
// (M2 with flagship content on top), M4 and M5 are flagship.
export type Layer = "Core" | "Flagship";
// The site page once the doc is on main, the file on GitHub until then.
export const CORE_VS_PACK_URL = (docsIndex as { slug: string }[]).some((d) => d.slug === "core-vs-pack") ? "/docs/core-vs-pack" : blob("docs/architecture/core-vs-pack.md");
const FLAGSHIP_MILESTONES = new Set(["M4", "M5"]);
export const MILESTONE_LAYER: Record<string, string> = {
  M1: "Core systems, placed in the test village",
  M2: "Core systems, with flagship content",
  M3: "Core",
  M4: "Flagship",
  M5: "Flagship",
};
export function issueLayer(issue: { labels?: string[]; layer?: Layer | null }, milestoneId: string | null): Layer {
  const l = (issue.labels ?? []).map((x) => x.toLowerCase());
  if (l.includes("layer:flagship")) return "Flagship";
  if (l.includes("layer:core")) return "Core";
  if (issue.layer) return issue.layer;
  if (l.includes("topic:flagship story")) return "Flagship";
  return milestoneId && FLAGSHIP_MILESTONES.has(milestoneId) ? "Flagship" : "Core";
}

// Milestones in number order (M2 before M10), whatever order GitHub returned them in.
export const milestonesInOrder = () => [...githubData.milestones].sort((a, b) => Number(a.id.slice(1)) - Number(b.id.slice(1)));

export const still = (id: string) => siteData.media.stills.find((s) => s.id === id)!;
export const stillSrc = (s: Still) => `/media/${s.id}-${Math.max(...s.widths)}.webp`;
export const stillSrcSet = (s: Still) => s.widths.map((w) => `/media/${s.id}-${w}.webp ${w}w`).join(", ");

export function milestoneProgress(id: string) {
  const issues = milestone(id)?.issues ?? [];
  const done = issues.filter((i) => i.state === "closed").length;
  return { done, total: issues.length, open: issues.length - done };
}

// "M1: M1-08 Registration form" -> { code: "M1-08", title: "Registration form" }
export function splitTitle(title: string): { code: string | null; title: string } {
  const m = /^(?:M\d+:\s*)?(M\d+-\d+):?\s+(.*)$/.exec(title);
  if (m) return { code: m[1], title: m[2] };
  return { code: null, title: title.replace(/^M\d+:\s*/, "") };
}

export const formatDate = (iso: string, opts: Intl.DateTimeFormatOptions = { day: "numeric", month: "short", year: "numeric" }) =>
  new Date(iso.length === 10 ? `${iso}T12:00:00Z` : iso).toLocaleDateString("en-GB", { timeZone: "UTC", ...opts });

export const DOC_GROUPS = ["Start here", "Direction & decisions", "Current plan", "Engineering reference", "Research & community"];
export function groupDocs(list: DocMeta[]): [string, DocMeta[]][] {
  const groups = new Map<string, DocMeta[]>(DOC_GROUPS.map((g) => [g, []]));
  for (const d of list) {
    if (!groups.has(d.group)) groups.set(d.group, []);
    groups.get(d.group)!.push(d);
  }
  for (const l of groups.values()) l.sort((a, b) => a.order - b.order || a.title.localeCompare(b.title));
  return [...groups.entries()].filter(([, l]) => l.length > 0);
}

export function issueStatus(i: Issue): { label: string; tone: "green" | "red" | "ink" } {
  if (i.state === "closed") return { label: "Done", tone: "green" };
  if (i.assignees.length === 0) return { label: "Needs a hand", tone: "red" };
  return { label: "In progress", tone: "ink" };
}
