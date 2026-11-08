// Checks every link and asset reference in the built site (dist/).
//
//   node scripts/check-links.mjs              # internal links, #anchors, assets, and repo file links
//   node scripts/check-links.mjs --external   # also request every non-GitHub external URL once
//
// Internal: the target page must exist in dist/ and a #fragment must be an id on it.
// Links to github.com/<this repo>/blob|tree/main/<path> must name a file in this checkout.

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const site = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const dist = path.join(site, "dist");
const repo = path.resolve(site, "..");
const REPO_PREFIX = /^https:\/\/github\.com\/Life-Framework\/Life-Framework\/(?:blob|tree|edit)\/main\/([^#?]+)/;
const external = process.argv.includes("--external");

const walk = (d) => fs.readdirSync(d, { withFileTypes: true }).flatMap((e) => (e.isDirectory() ? walk(path.join(d, e.name)) : [path.join(d, e.name)]));
const pages = walk(dist).filter((f) => f.endsWith(".html"));
const routeOf = (file) => {
  const rel = path.relative(dist, file).split(path.sep).join("/").replace(/\.html$/, "");
  return rel === "index" ? "/" : `/${rel}`;
};
const htmlByRoute = new Map(pages.map((f) => [routeOf(f), fs.readFileSync(f, "utf8")]));
const redirects = new Map(
  fs
    .readFileSync(path.join(dist, "_redirects"), "utf8")
    .split("\n")
    .map((l) => l.trim().split(/\s+/))
    .filter((p) => p.length >= 2)
    .map(([a, b]) => [a, b]),
);
const ids = new Map();
const idsOf = (route) => {
  if (!ids.has(route)) ids.set(route, new Set([...htmlByRoute.get(route).matchAll(/\sid="([^"]+)"/g)].map((m) => m[1])));
  return ids.get(route);
};

let failed = 0;
let checked = 0;
const externals = new Map();
const report = (from, msg) => {
  failed++;
  console.log(`FAIL ${from}: ${msg}`);
};

for (const [route, html] of htmlByRoute) {
  if (route === "/404") continue;
  const refs = [...html.matchAll(/\s(href|src|srcset|poster)="([^"]+)"/g)].flatMap(([, attr, v]) =>
    attr === "srcset" ? v.split(",").map((s) => s.trim().split(/\s+/)[0]) : [v],
  );
  for (const raw of refs) {
    const ref = raw.replace(/&amp;/g, "&");
    checked++;
    if (ref.startsWith("mailto:")) continue;
    if (/^https?:\/\//.test(ref)) {
      const m = REPO_PREFIX.exec(ref);
      if (m) {
        const target = path.join(repo, decodeURIComponent(m[1]));
        if (!fs.existsSync(target)) report(route, `${ref} names ${m[1]}, which is not in the repo`);
      } else if (!/^https?:\/\/(life-framework-arn\.pages\.dev|life-framework\.org|github\.com\/)/.test(ref)) {
        // github.com answers scripted requests with 429 and 504 too often to be a signal;
        // issue and PR links come from the API, and repo file links are checked above.
        if (!externals.has(ref)) externals.set(ref, route);
      }
      continue;
    }
    if (ref.startsWith("#")) {
      if (!idsOf(route).has(decodeURIComponent(ref.slice(1)))) report(route, `anchor ${ref} has no matching id`);
      continue;
    }
    if (!ref.startsWith("/")) {
      report(route, `relative link ${ref}`);
      continue;
    }
    const [p, frag] = ref.split("#");
    const target = redirects.get(p) || p.replace(/\/$/, "") || "/";
    if (htmlByRoute.has(target)) {
      if (frag && !idsOf(target).has(decodeURIComponent(frag))) report(route, `${ref}: no id "${frag}" on ${target}`);
    } else if (!fs.existsSync(path.join(dist, decodeURIComponent(p)))) {
      // Clips are downloaded at build time and may be absent offline; the page then links to the release.
      if (!p.startsWith("/media/clips/")) report(route, `${ref} does not exist in dist/`);
    }
  }
}

if (external) {
  for (const [url, from] of externals) {
    try {
      let res = await fetch(url, { method: "HEAD", redirect: "follow", signal: AbortSignal.timeout(15000) });
      if (res.status === 405 || res.status === 403) res = await fetch(url, { redirect: "follow", signal: AbortSignal.timeout(15000) });
      if (res.status >= 400 && res.status !== 429) report(from, `${url} -> ${res.status}`);
    } catch (err) {
      report(from, `${url} -> ${err.message}`);
    }
  }
}

console.log(
  `${pages.length} pages, ${checked} references, ${externals.size} external URLs ${external ? "requested" : "not requested (--external)"}`,
);
console.log(failed ? `${failed} link problem(s)` : "all links resolve");
process.exit(failed ? 1 : 0);
