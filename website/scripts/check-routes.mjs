// Checks that a deployed site serves every route and that the content is in the bundle.
//
//   node scripts/check-routes.mjs https://life-framework-arn.pages.dev
//
// Each route must answer 200 with the SPA shell (the _redirects fallback), and the
// JS bundle must contain a marker string from each page, so a blank or stale deploy fails.

const base = (process.argv[2] || "https://life-framework-arn.pages.dev").replace(/\/$/, "");

const routes = [
  "/",
  "/philosophy",
  "/roadmap",
  "/documentation",
  "/docs",
  "/docs/direction",
  "/docs/decisions",
  "/docs/roadmap-m1-playtest-town",
  "/docs/welcome",
  "/contribute",
  "/team",
  "/no-such-page",
];

// Strings each page renders; they must appear in the main bundle or the docs chunk.
const markers = [
  "Building the Future of",
  "Philosophy & ",
  "Development ",
  "Documentation",
  "Help Build ",
  "Join the ",
  "What we are building",
  "In-world first",
  "M1 Playtest Town",
  "Architecture & Principles",
];

let failed = 0;
const fail = (msg) => {
  failed++;
  console.log(`FAIL ${msg}`);
};

let shell = "";
for (const r of routes) {
  const res = await fetch(base + r, { redirect: "follow" });
  const html = await res.text();
  const ok = res.status === 200 && html.includes('<div id="root">');
  if (!ok) fail(`${r} -> ${res.status}`);
  else console.log(`ok   ${r} -> ${res.status}`);
  if (r === "/") shell = html;
}

const scripts = [...shell.matchAll(/src="(\/assets\/[^"]+\.js)"/g)].map((m) => m[1]);
let js = "";
for (const s of scripts) js += await (await fetch(base + s)).text();
// The docs chunk is imported lazily from the main bundle.
for (const m of js.matchAll(/"(?:\.\/|\/assets\/)?(docs-[\w-]+\.js)"/g)) {
  js += await (await fetch(`${base}/assets/${m[1]}`)).text();
}
for (const m of markers) {
  if (js.includes(m)) console.log(`ok   bundle has "${m}"`);
  else fail(`bundle is missing "${m}"`);
}

const img = await fetch(`${base}/docs-images/d732dd0c-d2eb-4432-9753-80884c0ef132-1764498228939.png`);
if (img.status === 200 && (img.headers.get("content-type") || "").startsWith("image/")) console.log("ok   exported doc image");
else fail(`exported doc image -> ${img.status} ${img.headers.get("content-type")}`);

console.log(failed ? `${failed} check(s) failed` : "all route checks passed");
process.exit(failed ? 1 : 0);
