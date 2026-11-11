// Title, description, and share image for every route. The prerender writes these into each
// page's <head>; the client updates the title on navigation.
import { docs } from "@/lib/content";

// Where the site is served.
export const SITE_URL = "https://life-framework.org";
export const SITE_NAME = "Life Framework";

export type PageMeta = { title: string; description: string; og: string; path: string; noindex?: boolean };

const pages: Record<string, Omit<PageMeta, "path">> = {
  "/": {
    title: "Life Framework: the modern life and roleplay framework for Arma Reforger",
    description:
      "We are building the modern Life Framework for Arma Reforger: inspired by Altis Life, built in the world instead of menus, open source. Join the Discord, test what we build, or pick an issue.",
    og: "home",
  },
  "/roadmap": {
    title: "Roadmap",
    description: "Milestones M1 to M6, each with its goal, its status, and its issues and epics, read from GitHub when the site is built.",
    og: "roadmap",
  },
  "/features": {
    title: "Features",
    description: "What the M2 systems do in the playtest town today: moderation, comms, medic, investigation and the underworld.",
    og: "roadmap",
  },
  "/gallery": {
    title: "Gallery",
    description: "A few pictures taken in the game in the playtest village.",
    og: "roadmap",
  },
  "/playtest": {
    title: "Join the playtest",
    description: "The M2 Living Town community playtest: what it is, how to install and join, what to test, and where to send feedback.",
    og: "home",
  },
  "/join": {
    title: "Join Life Framework",
    description:
      "An open-source life and roleplay framework for Arma Reforger, early test build. Join the Discord, then try the playtest.",
    og: "home",
  },
  "/devlog": {
    title: "Devlog",
    description: "Every merged pull request and recorded decision, newest first, with one line on each.",
    og: "devlog",
  },
  "/servers": {
    title: "For server owners",
    description: "What the framework gives a server owner, what the 1989 Everon flagship adds, how it is configured, and the licences.",
    og: "servers",
  },
  "/contribute": {
    title: "Contribute",
    description: "How to pick up a scope: read AGENTS.md, take an issue, prove it in the world, open a pull request. Agents and people welcome.",
    og: "contribute",
  },
  "/decisions": {
    title: "Decisions",
    description: "The design calls that set the taste of Life Framework, each one dated, with why and what it means for code.",
    og: "decisions",
  },
  "/support": {
    title: "Support the project",
    description: "What running Life Framework costs, where donations go, and how to give. A donation buys nothing in the game.",
    og: "home",
  },
  "/docs": {
    title: "Docs",
    description: "Direction, decisions, the roadmap scopes, and the community guides, rendered from the repository.",
    og: "docs",
  },
};

export function metaFor(pathname: string): PageMeta {
  let path = pathname.replace(/\/+$/, "") || "/";
  // /join/<source> is the /join page, tagged for one community (content/outreach-sources.json).
  if (/^\/join\/[^/]+$/.test(path)) path = "/join";
  const page = pages[path];
  if (page) return { ...page, path };
  const m = /^\/docs\/([^/]+)$/.exec(path);
  const doc = m && docs.find((d) => d.slug === m[1]);
  if (doc) return { title: doc.title, description: doc.description || pages["/docs"].description, og: "docs", path };
  return { title: "Page not found", description: "There is nothing at this address.", og: "home", path, noindex: true };
}

export const fullTitle = (m: PageMeta) => (m.path === "/" ? m.title : `${m.title} | ${SITE_NAME}`);

// Routes the prerender writes, besides the 404 page.
export function allRoutes(): string[] {
  return [...Object.keys(pages), ...docs.filter((d) => d.href.startsWith("/docs/")).map((d) => d.href)];
}
