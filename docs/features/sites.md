# Sites

### `LF_SiteHold` — `Feature/Sites/LF_SiteHold.c` (#2678, 2026-11-17)
- Scoring maths for a contested site: a lone organisation gains, two or more freeze it, an empty site decays capture progress, a full capture flips it behind a cooldown. No world yet. Tests: `sites/site-hold` (LOGIC).

### `LF_SitesConfig` — `Feature/Sites/LF_SitesConfig.c` (#2679, 2026-11-18)
- Per-site rules from `Configs/Sites/LF_Sites.conf` (no sites in the core), pack overlays, then the owner's `sites.rules` keyed by hideout id: radius, inner radius, member minimum, capture seconds, decay, flip cooldown, rent per hour, banked rent cap, active window. Bad values log and fall back. Tests: `sites/config` (LOGIC).
