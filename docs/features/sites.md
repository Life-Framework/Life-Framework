# Sites

### `LF_SiteHold` — `Feature/Sites/LF_SiteHold.c` (#2678, 2026-11-17)
- Scoring maths for a contested site: a lone organisation gains, two or more freeze it, an empty site decays capture progress, a full capture flips it behind a cooldown. No world yet. Tests: `sites/site-hold` (LOGIC).

### `LF_SitesConfig` — `Feature/Sites/LF_SitesConfig.c` (#2679, 2026-11-18)
- Per-site rules from `Configs/Sites/LF_Sites.conf` (no sites in the core), pack overlays, then the owner's `sites.rules` keyed by hideout id: radius, inner radius, member minimum, capture seconds, decay, flip cooldown, rent per hour, banked rent cap, active window. Bad values log and fall back. Tests: `sites/config` (LOGIC).

### `LF_ContestedSites` — `Feature/Sites/LF_ContestedSites.c`, `LF_ContestedSiteComponent.c` (#2680, 2026-11-18)
- A 5 s server sweep scores every site placed with `LF_ContestedSiteComponent` (LF_Places kind `contested-site`, id = the hideout it guards) with `LF_SiteHold.Step` under `LF_SitesConfig.For(id).ToHoldRules()`. Counts living player characters of an organisation inside the radius that moved 1 m or acted within `activeWindowSeconds`; the inner ring counts double. The holder is the hideout lessee and its held ground (no new payload). A capture releases the old lease (reason `captured`), leases the hideout to the capturer at zero held ground (`LF_Hideout.HandOver`) and publishes `LF_OnSiteCaptured`; a contest start publishes `LF_OnSiteContested`. A capturer that already leases another hideout is refused and the site stays with its lessee. Tests: `sites/site-sweep` (WORLD).

### `LF_SiteRent` — `Feature/Sites/LF_SiteRent.c` (#2683, 2026-11-18)
- A held site that is not contested banks `rentPerHour` pro rata on each sweep, capped at `maxRentBanked` (defaults 600/h, 3000). Rent is a capped faucet like a wage: no pool account pays it. The bank stays on the site, so a capture hands it to the new holder. Each full hour of banking logs the amount once as an `LF_OnMoneyMoved` with source `site_rent` (no account side; the incident log records it, the ledger does not). Saved as the `siterent` payload (hideout id, banked, last rent time). Tests: `sites/site-rent` (PERSISTENCE).
