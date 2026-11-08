// Typed access to the JSON that scripts/build-content.mjs writes at build time.
import site from "@/generated/site.json";
import github from "@/generated/github.json";

export const REPO_URL = "https://github.com/Life-Framework/Life-Framework";
export const DISCORD_URL = "https://discord.gg/MjMexJteqz";

export type Heading = { level: number; text: string; id: string };

export type Doc = {
  slug: string;
  group: string;
  order: number;
  title: string;
  repoPath: string;
  editUrl: string;
  source: string;
  updated: string | null;
  description: string;
  headings: Heading[];
  markdown: string;
};

export type Issue = {
  number: number;
  title: string;
  state: "open" | "closed";
  url: string;
  labels: string[];
  assignees: string[];
  updatedAt: string;
  closedAt: string | null;
  excerpt: string;
  wave: number | null;
};

export type GithubData = {
  source: "github" | "snapshot";
  fetchedAt: string;
  milestone: {
    number: number;
    title: string;
    description: string;
    url: string;
    state: string;
    open: number;
    closed: number;
    dueOn: string | null;
  } | null;
  issues: Issue[];
  goodFirst: Issue[];
};

export type ArchiveItem = {
  title: string;
  description: string | null;
  status: "planned" | "in-progress" | "completed";
  progress: number;
  assignees: string[];
  lookingForContributors: boolean;
};

export type SiteData = {
  building: string;
  playable: string;
  aiFirst: string;
  proof: string;
  notNow: string;
  inWorldFirst: string;
  milestones: { id: string; name: string; doneWhen: string }[];
  archive: {
    source: string;
    exported: string;
    phases: { number: number; name: string; description: string; items: ArchiveItem[] }[];
  };
  team: {
    members: { name: string; role: string; github?: string; bio?: string }[];
    roles: { name: string; role: string; bio: string; skills: string[] }[];
  };
};

export const siteData = site as SiteData;
export const githubData = github as GithubData;

// The docs bundle is large, so it loads as its own chunk on the docs pages.
let docsPromise: Promise<Doc[]> | null = null;
export function loadDocs(): Promise<Doc[]> {
  if (!docsPromise) {
    docsPromise = import("@/generated/docs.json").then((m) => m.default as Doc[]);
  }
  return docsPromise;
}

export const DOC_GROUPS = ["Project", "Roadmap", "Community guides"];
