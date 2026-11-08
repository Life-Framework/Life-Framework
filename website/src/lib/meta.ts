// Title, description, and share image for every route. The prerender writes these into each
// page's <head>; the client updates the title on navigation.
import { docs } from "@/lib/content";

// Where the site is served. Change to https://life-framework.org at the DNS cutover (README).
export const SITE_URL = "https://life-framework.org";
export const SITE_NAME = "Life Framework";

export type PageMeta = { title: string; description: string; og: string; path: string; noindex?: boolean };

const pages: Record<string, Omit<PageMeta, "path">> = {
  "/": {
    title: "Life Framework: open-source life and roleplay for Arma Reforger",
    description:
      "Cash you can lose, keys you can steal, a fine written on paper. An open-source life/RP framework for Arma Reforger, proven on a 1989 Everon server. In development: M1 playtest town.",
    og: "home",
  },
  "/roadmap": {
    title: "Roadmap",
    description: "Milestones M0 to M4 and every M1 scope, read from GitHub when the site is built.",
    og: "roadmap",
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
  const path = pathname.replace(/\/+$/, "") || "/";
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
