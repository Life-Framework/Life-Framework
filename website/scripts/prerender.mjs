// Writes a static HTML file for every route after `vite build`, so each page is readable
// without JavaScript, carries its own <title> and share tags, and loads fast. Then the
// sitemap, robots.txt, and the Cloudflare Pages redirects.
//
//   /           -> dist/index.html        /docs/<slug> -> dist/docs/<slug>.html
//   /roadmap    -> dist/roadmap.html      (unknown)    -> dist/404.html (Pages serves it with 404)

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const site = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const dist = path.join(site, "dist");
const ssrDir = path.join(site, "dist-ssr");
const entry = fs.readdirSync(ssrDir).find((f) => /^entry-server\.m?js$/.test(f));
const { render, allRoutes, prime, SITE_URL } = await import(pathToFileURL(path.join(ssrDir, entry)).href);

// Every page-data file, so each route renders complete.
const dataDir = path.join(site, "src", "generated", "data");
prime(Object.fromEntries(fs.readdirSync(dataDir).map((f) => [f, { default: JSON.parse(fs.readFileSync(path.join(dataDir, f), "utf8")) }])));

const template = fs.readFileSync(path.join(dist, "index.html"), "utf8");
if (!template.includes("<!--app-head-->") || !template.includes("<!--app-html-->")) throw new Error("dist/index.html lost its <!--app-head--> or <!--app-html--> placeholder");

const fileFor = (route) => (route === "/" ? "index.html" : `${route.slice(1)}.html`);
const write = (rel, text) => {
  const p = path.join(dist, rel);
  fs.mkdirSync(path.dirname(p), { recursive: true });
  fs.writeFileSync(p, text);
};
// Cloudflare Web Analytics (privacy-friendly: no cookies, no personal data). Set VITE_CF_WEB_ANALYTICS_TOKEN
// when building and every page gets the beacon; without it nothing is added. The join page's privacy line
// reads the same variable. Alternatively switch Web Analytics on in the Pages project and leave this unset.
const beaconToken = process.env.VITE_CF_WEB_ANALYTICS_TOKEN || "";
const beacon = beaconToken
  ? `<script defer src="https://static.cloudflareinsights.com/beacon.min.js" data-cf-beacon='${JSON.stringify({ token: beaconToken })}'></script>
  `
  : "";
const page = (url) => {
  const { html, head } = render(url);
  return template.replace("<!--app-head-->", head).replace("<!--app-html-->", html).replace("</body>", `${beacon}</body>`);
};

const routes = allRoutes();
for (const r of routes) write(fileFor(r), page(r));
write("404.html", page("/404"));

const today = new Date().toISOString().slice(0, 10);
write(
  "sitemap.xml",
  `<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n` +
    routes.map((r) => `  <url><loc>${SITE_URL}${r}</loc><lastmod>${today}</lastmod></url>`).join("\n") +
    `\n</urlset>\n`,
);
write("robots.txt", `User-agent: *\nAllow: /\n\nSitemap: ${SITE_URL}/sitemap.xml\n`);

// Old addresses from the 2025 site and the first in-repo site.
write(
  "_redirects",
  ["/philosophy / 301", "/team /contribute 301", "/documentation /docs 301", "/docs/decisions /decisions 301", "/docs/ /docs 301", "/join/* /join 200"].join("\n") + "\n",
  // "/join/* /join 200" serves the join page at /join/<community> and keeps the address, so analytics sees the tag.
);

fs.rmSync(ssrDir, { recursive: true, force: true });
console.log(`[prerender] ${routes.length} pages + 404, sitemap.xml, robots.txt, _redirects (${SITE_URL})`);
