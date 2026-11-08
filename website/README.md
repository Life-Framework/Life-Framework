# Life Framework website

The public site (life-framework.org). Vite + React + TypeScript + Tailwind,
with the look carried over from the 2025 Lovable site. It has no database,
login, or editor: it is built from this repository, so the site and the mod
always match. To change the site, open a pull request.

## Develop

```sh
cd website
npm ci
npm run dev          # http://localhost:8080
npm run build        # content + typecheck + vite build -> dist/
npm run lint
npm run preview      # serve dist/
```

Node 20 or newer. `website/` has its own `package.json` and `package-lock.json`;
the repo's `tools\cli` does not touch it.

## Where content comes from

`scripts/build-content.mjs` runs before every `dev`, `build`, and `lint` and
writes `src/generated/` (git-ignored):

| Page | Source |
| --- | --- |
| Docs (`/documentation`, `/docs/:slug`) | `docs/direction.md`, `docs/decisions.md`, `docs/ecosystem.md` (when present), every `docs/roadmap/**/*.md`, and `website/content/docs/*.md` |
| Vision (`/philosophy`) | Sections of `docs/direction.md` ("What we are building", "What playable means", "How we build: AI-first") and the "In-world first" entry of `docs/decisions.md` |
| Roadmap (`/roadmap`) | The milestone table in `docs/direction.md`, the `M1` milestone and its issues from the GitHub REST API, and `content/archive/roadmap-2025.json` |
| Contribute (`/contribute`) | Open `good first issue` issues, else unassigned M1 issues, from the GitHub API |
| Team (`/team`) | `content/team.json` |
| Home (`/`) | M1 progress from the GitHub API, the M1 row of `docs/direction.md` |

Relative links in the docs are rewritten at build time: a link to another
rendered doc becomes `/docs/<slug>`, anything else points at the file on GitHub.
The build fails if a section it extracts from `direction.md` or `decisions.md`
disappears; rename the heading in `build-content.mjs` when you rename it in the doc.

GitHub is read without a token (the repo is public). CI passes `GITHUB_TOKEN`
for a higher rate limit. When the API cannot be reached, the build uses the
committed `content/github-snapshot.json` and the roadmap page says so. Refresh
the snapshot with `npm run snapshot` and commit it now and then.

`content/docs/` holds the three guides exported from the old site's Supabase
database (HTML converted to markdown, images in `public/docs-images/`).
`content/archive/roadmap-2025.json` holds its roadmap phases and items.

## Deploy

`.github/workflows/website.yml`:

- On a PR that touches `website/**` or `docs/**`: `npm ci`, lint, build.
- On a push to `main` (same paths), daily at 05:17 UTC, or by hand
  (`workflow_dispatch`): the same, then `wrangler pages deploy` to the
  Cloudflare Pages project `life-framework`. The daily run keeps the roadmap in
  step with issue changes that touch no file.

Repo secrets: `CLOUDFLARE_API_TOKEN` (an account token with Pages edit) and
`CLOUDFLARE_ACCOUNT_ID`.

Deploy by hand from a worktree:

```powershell
cd website; npm run build
$env:CLOUDFLARE_API_TOKEN = [Environment]::GetEnvironmentVariable('CLOUDFLARE_API_TOKEN','User')
$env:CLOUDFLARE_ACCOUNT_ID = [Environment]::GetEnvironmentVariable('CLOUDFLARE_ACCOUNT_ID','User')
npx wrangler@4 pages deploy dist --project-name life-framework --branch main
node scripts/check-routes.mjs https://life-framework-arn.pages.dev
```

`public/_redirects` (`/* /index.html 200`) is the SPA fallback: every route
serves the app shell and React Router renders the page, including the 404.
`scripts/check-routes.mjs` requests every route, expects 200 and the shell,
and checks that each page's text is in the JS bundle.

Production URL until the DNS cutover: https://life-framework-arn.pages.dev

## Domain cutover (not done yet)

`life-framework.org` and `www.life-framework.org` are attached to the Pages
project as custom domains. They stay pending until DNS points at Pages. The
zone (`c29e37322367503662ab07c7490890e0`) still has the Lovable records and is
waiting for the nameserver change:

| Type | Name | Content | Record id |
| --- | --- | --- | --- |
| A | life-framework.org | 185.158.133.1 | `ad1b923e779fae8d47e0b1fd4514257b` |
| A | www.life-framework.org | 185.158.133.1 | `6c70ea7940f0110bb0a1ae4f22bd73ca` |

The MX, SPF, and `_dmarc` records (IONOS mail) are not touched.

After the maintainer has reviewed the pages.dev site and the zone is active on
Cloudflare nameservers, run (PowerShell):

```powershell
$t = [Environment]::GetEnvironmentVariable('CLOUDFLARE_API_TOKEN','User')
$a = [Environment]::GetEnvironmentVariable('CLOUDFLARE_ACCOUNT_ID','User')
$z = 'c29e37322367503662ab07c7490890e0'
$h = @{ Authorization = "Bearer $t"; 'Content-Type' = 'application/json' }
$api = 'https://api.cloudflare.com/client/v4'

# 1. Confirm the zone is active (not "pending").
(Invoke-RestMethod "$api/zones/$z" -Headers $h).result.status

# 2. Replace the two Lovable A records with proxied CNAMEs to Pages.
#    (Cloudflare flattens the apex CNAME.)
Invoke-RestMethod -Method Delete "$api/zones/$z/dns_records/ad1b923e779fae8d47e0b1fd4514257b" -Headers $h
Invoke-RestMethod -Method Delete "$api/zones/$z/dns_records/6c70ea7940f0110bb0a1ae4f22bd73ca" -Headers $h
Invoke-RestMethod -Method Post "$api/zones/$z/dns_records" -Headers $h -Body (@{
  type = 'CNAME'; name = 'life-framework.org'; content = 'life-framework-arn.pages.dev'; proxied = $true; ttl = 1
} | ConvertTo-Json)
Invoke-RestMethod -Method Post "$api/zones/$z/dns_records" -Headers $h -Body (@{
  type = 'CNAME'; name = 'www'; content = 'life-framework-arn.pages.dev'; proxied = $true; ttl = 1
} | ConvertTo-Json)

# 3. Ask Pages to re-check both domains, then poll until status is "active".
foreach ($d in 'life-framework.org', 'www.life-framework.org') {
  Invoke-RestMethod -Method Patch "$api/accounts/$a/pages/projects/life-framework/domains/$d" -Headers $h | Out-Null
}
(Invoke-RestMethod "$api/accounts/$a/pages/projects/life-framework/domains" -Headers $h).result |
  Select-Object name, status

# 4. Prove it.
node scripts/check-routes.mjs https://life-framework.org
node scripts/check-routes.mjs https://www.life-framework.org
```

Rollback: delete the two CNAMEs and recreate the A records to `185.158.133.1`
(`proxied = $false`).

Optional after cutover: redirect `www` to the apex with a Cloudflare redirect
rule, and turn off the Lovable project.
