// Checks that every route answers with its own prerendered page.
//
//   node scripts/check-routes.mjs --dist                         # the local build, served like Pages
//   node scripts/check-routes.mjs https://life-framework.org
//
// Each route must answer 200 with its own <title>, a description, a canonical link, and an
// og:image that exists; old addresses must redirect; an unknown path must answer 404 with
// the not-found page. Markers prove the page body was rendered, not an empty shell.

import { startServer } from "./serve-dist.mjs";

const useDist = process.argv.includes("--dist");
let server = null;
let base = (process.argv.slice(2).find((a) => !a.startsWith("--")) || "https://life-framework.org").replace(/\/$/, "");
if (useDist) {
  server = await startServer(4199);
  base = "http://127.0.0.1:4199";
}

// route -> text that only that page renders
const pages = {
  "/": "Real cash.",
  "/roadmap": "Where we are on the road",
  "/devlog": "What changed, day by day",
  "/servers": "Host it, tune it, build on it",
  "/contribute": "Pick a scope. Prove it in the game.",
  "/decisions": "The calls that set the taste",
  "/support": "Where the money goes",
  "/docs": "The paperwork",
  "/docs/direction": "What we are building",
  "/docs/roadmap-m1-playtest-town": "The player journey",
  "/docs/welcome": "Founded 30th of November 2025",
};
const redirects = {
  "/philosophy": "/",
  "/team": "/contribute",
  "/documentation": "/docs",
  "/docs/decisions": "/decisions",
};

let failed = 0;
const fail = (msg) => {
  failed++;
  console.log(`FAIL ${msg}`);
};
const ok = (msg) => console.log(`ok   ${msg}`);

const titles = new Set();
for (const [route, marker] of Object.entries(pages)) {
  const res = await fetch(base + route, { redirect: "manual" });
  const html = await res.text();
  if (res.status !== 200) {
    fail(`${route} -> ${res.status}`);
    continue;
  }
  const title = /<title>([^<]*)<\/title>/.exec(html)?.[1];
  const problems = [];
  if (!title) problems.push("no <title>");
  else if (titles.has(title)) problems.push(`title "${title}" is not unique`);
  else titles.add(title);
  if (!/<meta name="description" content="[^"]{20,}"/.test(html)) problems.push("no description");
  if (!/<link rel="canonical" href="https?:\/\/[^"]+"/.test(html)) problems.push("no canonical");
  if (!html.includes(marker)) problems.push(`body is missing "${marker}"`);
  const og = /<meta property="og:image" content="https?:\/\/[^/]+(\/[^"]+)"/.exec(html)?.[1];
  if (!og) problems.push("no og:image");
  else {
    const img = await fetch(base + og);
    if (img.status !== 200 || !(img.headers.get("content-type") || "").startsWith("image/")) problems.push(`og:image ${og} -> ${img.status}`);
  }
  if (problems.length) fail(`${route}: ${problems.join("; ")}`);
  else ok(`${route} -> 200 "${title}"`);
}

for (const [from, to] of Object.entries(redirects)) {
  const res = await fetch(base + from, { redirect: "manual" });
  const loc = res.headers.get("location") || "";
  if (res.status >= 300 && res.status < 400 && (loc === to || loc === base + to)) ok(`${from} -> ${res.status} ${to}`);
  else fail(`${from} -> ${res.status} ${loc} (want a redirect to ${to})`);
}

{
  const res = await fetch(`${base}/no-such-page`, { redirect: "manual" });
  const html = await res.text();
  if (res.status === 404 && html.includes("No such address")) ok("/no-such-page -> 404 with the not-found page");
  else fail(`/no-such-page -> ${res.status}`);
}

for (const asset of ["/sitemap.xml", "/robots.txt", "/favicon.svg", "/favicon.ico", "/docs-images/d732dd0c-d2eb-4432-9753-80884c0ef132-1764498228939.png"]) {
  const res = await fetch(base + asset);
  if (res.status === 200) ok(`${asset} -> 200`);
  else fail(`${asset} -> ${res.status}`);
}

server?.close();
console.log(failed ? `${failed} route check(s) failed` : "all route checks passed");
process.exit(failed ? 1 : 0);
