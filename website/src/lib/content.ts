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
};

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

export type ArchiveItem = { title: string; description: string | null; status: "planned" | "in-progress" | "completed" };

export type SiteData = {
  ambition: string;
  loop: string[];
  inWorldFirst: string;
  features: { name: string; what: string }[];
  archive: { exported: string; phases: { number: number; name: string; description: string; items: ArchiveItem[] }[] };
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

const currentMilestone = githubData.milestones.find((m) => m.state === "open") ?? githubData.milestones[0];
export const CURRENT_MILESTONE = currentMilestone?.id ?? "M1";
export const CURRENT_MILESTONE_NAME = currentMilestone ? milestoneName(currentMilestone) : "Current milestone";
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

export const DOC_GROUPS = ["Project", "Roadmap", "Community guides (2025)"];
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
