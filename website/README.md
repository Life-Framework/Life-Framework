# Life Framework website

The public site (life-framework.org). Vite + React + TypeScript + Tailwind,
prerendered to one static HTML file per route. It has no database, login, or
editor: it is built from this repository and GitHub, so the site and the mod
always match. To change the site, open a pull request.

The look, the palette, the type, and the voice are in [BRAND.md](BRAND.md).
Read it before you change how anything looks or reads.

## Develop

```sh
cd website
npm ci
npm run dev          # http://localhost:8080, client-rendered, no prerender
npm run build        # content, typecheck, client + SSR build, prerender -> dist/
npm run preview      # serve dist/ like Cloudflare Pages: http://127.0.0.1:4173
npm run lint
npm run check        # route check against dist/, then the link check
```

Node 20 or newer. `website/` has its own `package.json` and `package-lock.json`;
the repo's `tools\cli` does not touch it.

## How a build works

1. `scripts/build-content.mjs` reads the repo and GitHub and writes
   `src/generated/` (git-ignored). Big page data (each doc, the decisions, the
   devlog) goes to `src/generated/data/*.json`, one chunk each, so the main
   bundle stays small.
2. `vite build` builds the client; `vite build --ssr src/entry-server.tsx`
   builds the renderer.
3. `scripts/prerender.mjs` renders every route into `dist/<route>.html` with its
   own `<title>`, description, canonical link, and share image, then writes
   `404.html`, `sitemap.xml`, `robots.txt`, and `_redirects`.

In the browser, `src/entry-client.tsx` loads the current page's data chunk and
hydrates the static HTML. Moving between pages after that is client-side.

## Where content comes from

| Page | Source |
| --- | --- |
| Home (`/`) | Copy in `src/pages/Home.tsx` (written to turn visitors into contributors, including the framework vs flagship explainer), the milestones, current-milestone progress, and open issues from GitHub, and `content/media.json` |
| Roadmap (`/roadmap`) | Every `M<n>` milestone from GitHub with its issues and each epic's sub-issues (`/issues/{n}/sub_issues`), the one-line goals and Core/Flagship rule in `src/lib/content.ts` (`MILESTONE_PLAN`, `issueLayer`), the playable loop in `docs/direction.md`, and the village pictures from `content/media.json` |
| Devlog (`/devlog`) | Merged pull requests into `main` from GitHub (title and the first prose line of the body) and every entry in `docs/decisions.md`, grouped by day |
| Server owners (`/servers`) | The "Features" list in `README.md`, the M3 milestone from GitHub, and copy in `src/pages/Servers.tsx` (the core/pack split and config model from `docs/roadmap/m3-outline.md`, the M4 hosting plan) |
| Contribute (`/contribute`) | Open `good first issue` issues, else unassigned issues in the current milestone, and the contributor list, from GitHub |
| Decisions (`/decisions`) | `docs/decisions.md`, split into its sections and dated entries |
| Docs (`/docs`, `/docs/:slug`) | Setup and playtest guides, direction and decisions, engineering references, research and community docs, and every `docs/roadmap/**/*.md` |

Markdown is turned into HTML at build time (remark and rehype); the browser
never parses markdown. Relative links in the docs are rewritten: a link to a
rendered doc becomes its page, anything else points at the file on GitHub.

The build fails when something it extracts disappears: the ambition line or
the numbered loop in `direction.md`, the milestone table, the "In-world first"
decision, the README feature list, or the `**YYYY-MM-DD — Title.**` entry format
in `decisions.md`. Fix the extractor in `build-content.mjs` when you change the
doc.

GitHub is read without a token (the repo is public). CI passes `GITHUB_TOKEN`
for a higher rate limit. When the API cannot be reached, the build uses the
committed `content/github-snapshot.json` and the pages say so. Refresh the
snapshot with `npm run snapshot` and commit it now and then.

## Pictures, clips, and share images

Only captures from the game (BRAND.md, "Imagery"). `content/media.json` lists
each still and clip with its caption, place, date, source PR, and whether it is
work in progress.

- **Stills** are webp files in `public/media/<id>-<width>.webp`, at 640 and,
  when the source is big enough, 1280 wide. Add the file and the entry together.
- **Clips** stay assets on the `film-evidence` release. The build downloads
  each one into `public/media/clips/` (git-ignored). If the download fails, the
  page links to the release instead of embedding the video.
- **Share images** (`public/og/*.jpg`, 1200x630) are drawn by
  `scripts/make-og.mjs` from the brand fonts and a still. Run it after changing
  a page's title or the brand; it needs Playwright, which is not a dependency:
  `npm i --no-save playwright@1.49.1`, then `node scripts/make-og.mjs` (set
  `PLAYWRIGHT_CHANNEL=msedge` to use an installed Edge instead of downloading
  Chromium).

## Checks

- `npm run lint`: ESLint and TypeScript rules.
- `scripts/check-routes.mjs --dist` serves `dist/` with
  `scripts/serve-dist.mjs` (Pages rules: `/x` to `x.html`, `_redirects`, a 404
  status with `404.html`, brotli). Every page must answer 200 with a unique
  title, a description, a canonical link, an og:image that loads, and its own
  body text; old addresses must redirect; an unknown path must 404.
- `scripts/check-links.mjs` reads every page in `dist/`: internal links must
  reach a page, `#anchors` must exist on it, assets must exist, and links to
  `github.com/Life-Framework/Life-Framework/blob|tree/main/<path>` must name a
  file in the checkout. `--external` also requests every other URL once (not
  in CI, since third-party sites rate-limit).

Lighthouse, mobile, against `npm run preview` (record new scores in the PR when
a change could move them):

```sh
npx lighthouse http://127.0.0.1:4173/ --only-categories=performance,accessibility,best-practices,seo
```

## Deploy

`.github/workflows/website.yml`:

- On a PR that touches `website/**`, `docs/**`, or `README.md`: `npm ci`, lint,
  build, check.
- On a push to `main` (same paths), daily at 05:17 UTC, or by hand
  (`workflow_dispatch`): the same, then `wrangler pages deploy` to the
  Cloudflare Pages project `life-framework`, then the route check against the
  live site. The daily run keeps the roadmap and devlog in step with GitHub.

Repo secrets: `CLOUDFLARE_API_TOKEN` (an account token with Pages edit) and
`CLOUDFLARE_ACCOUNT_ID`.

Deploy by hand from a worktree:

```powershell
cd website; npm run build
$env:CLOUDFLARE_API_TOKEN = [Environment]::GetEnvironmentVariable('CLOUDFLARE_API_TOKEN','User')
$env:CLOUDFLARE_ACCOUNT_ID = [Environment]::GetEnvironmentVariable('CLOUDFLARE_ACCOUNT_ID','User')
npx wrangler@4 pages deploy dist --project-name life-framework --branch main
node scripts/check-routes.mjs https://life-framework.org
```

The site is served at https://life-framework.org (and `www.`), a custom domain
on the Pages project. Canonical links, the sitemap, and share images use
`SITE_URL` in `src/lib/meta.ts`.

## Domain

`life-framework.org` and `www.life-framework.org` are custom domains on the
Pages project. DNS is on Cloudflare nameservers, in the zone
`c29e37322367503662ab07c7490890e0`. The MX, SPF, and `_dmarc`
records (IONOS mail) live in the same zone; leave them alone. The cutover steps
are in git history (`git log -p -- website/README.md`).
