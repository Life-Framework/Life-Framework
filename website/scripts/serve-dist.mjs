// Serves dist/ the way Cloudflare Pages does: /roadmap -> roadmap.html, the _redirects
// rules, 404.html with status 404 for anything else, and compressed text. Used by `npm run preview`,
// check-routes.mjs --dist, and Lighthouse runs.
//
//   node scripts/serve-dist.mjs [port]

import fs from "node:fs";
import http from "node:http";
import path from "node:path";
import zlib from "node:zlib";
import { fileURLToPath } from "node:url";

const dist = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "dist");
const types = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript",
  ".css": "text/css",
  ".json": "application/json",
  ".svg": "image/svg+xml",
  ".png": "image/png",
  ".jpg": "image/jpeg",
  ".ico": "image/x-icon",
  ".webp": "image/webp",
  ".woff2": "font/woff2",
  ".mp4": "video/mp4",
  ".xml": "application/xml",
  ".txt": "text/plain; charset=utf-8",
};

function redirects() {
  const f = path.join(dist, "_redirects");
  if (!fs.existsSync(f)) return new Map();
  return new Map(
    fs
      .readFileSync(f, "utf8")
      .split("\n")
      .map((l) => l.trim().split(/\s+/))
      .filter((p) => p.length >= 2 && p[0].startsWith("/"))
      .map(([from, to, code]) => [from, { to, code: Number(code || 302) }]),
  );
}

function resolve(urlPath) {
  const clean = path.posix.normalize(decodeURIComponent(urlPath));
  if (clean.includes("..")) return null;
  const candidates = clean.endsWith("/") ? [clean + "index.html"] : [clean, clean + ".html"];
  for (const c of candidates) {
    const p = path.join(dist, c);
    if (fs.existsSync(p) && fs.statSync(p).isFile()) return p;
  }
  return null;
}

export function startServer(port = 4173) {
  const rules = redirects();
  const server = http.createServer((req, res) => {
    const url = new URL(req.url, "http://x");
    const rule = rules.get(url.pathname);
    if (rule) {
      res.writeHead(rule.code, { Location: rule.to });
      return res.end();
    }
    if (url.pathname.endsWith(".html") && url.pathname !== "/404.html") {
      // Pages drops the extension with a redirect.
      res.writeHead(308, { Location: url.pathname.replace(/(index)?\.html$/, "") || "/" });
      return res.end();
    }
    let file = resolve(url.pathname === "/" ? "/index.html" : url.pathname);
    let status = 200;
    if (!file) {
      file = path.join(dist, "404.html");
      status = 404;
    }
    const type = types[path.extname(file)] || "application/octet-stream";
    const cache = /\/assets\//.test(file.split(path.sep).join("/")) ? "public, max-age=31536000, immutable" : "no-cache";
    const text = /^(text\/|application\/(json|xml)|image\/svg)/.test(type);
    if (text && /\bbr\b/.test(req.headers["accept-encoding"] || "")) {
      res.writeHead(status, { "Content-Type": type, "Cache-Control": cache, "Content-Encoding": "br", Vary: "Accept-Encoding" });
      return fs.createReadStream(file).pipe(zlib.createBrotliCompress()).pipe(res);
    }
    res.writeHead(status, { "Content-Type": type, "Cache-Control": cache });
    fs.createReadStream(file).pipe(res);
  });
  return new Promise((ok) => server.listen(port, "127.0.0.1", () => ok(server)));
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const port = Number(process.argv[2] || 4173);
  await startServer(port);
  console.log(`serving dist/ at http://127.0.0.1:${port}`);
}
