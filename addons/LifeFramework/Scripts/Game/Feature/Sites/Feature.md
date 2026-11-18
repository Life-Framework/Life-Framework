# Sites

## Intent

Contested ground a gang can win and hold. This first piece is the scoring maths only: who gains, who is frozen, how capture progress decays. The world side (the site object, presence sweep, UI) comes in later gang war slices (#1022).

## Interaction pattern

None yet: no player-facing object in this slice. `LF_SiteHold.Step` is a pure function of a site's state, the counted presence per organisation and the elapsed seconds; the server's site system will call it.

## Site rules from config (#2679)

- **Player sees:** nothing yet; the numbers behind each contested site.
- **Owner and side:** `LF_SitesConfig` (server), `Configs/Sites/LF_Sites.conf` (core: no sites), then each pack overlay of it in load order, then the owner's `sites.rules` keyed by the hideout id the site guards (override model: `add`, `replace`, `patch`, `remove`). `LF_SitesConfig.For(hideoutId)` hands out a copy (`LF_SiteSettings`); an id with no entry gets the default rules. `ToHoldRules()` gives `LF_SiteHold.Step` its `LF_SiteRules`.
- **Keys and defaults:** `radius` 60 m, `innerRadius` 20 m (at most the radius), `minMembersPresent` 2, `captureSeconds` 300, `decayPerHour` 600 s of progress, `flipCooldownSeconds` 1800, `rentPerHour` 600, `maxRentBanked` 3000, `activeWindowSeconds` 300. A value out of range, of the wrong type or an inner ring wider than the radius logs an error and keeps the value below it.
- **Data:** none persisted. **Proof:** `sites/config` (LOGIC).

## Site hold (#2678)

- **Player sees:** nothing yet; the maths behind a site's holder, points and capture bar.
- **Owner and side:** `LF_SiteHold` decides on the server. Rules are a plain `LF_SiteRules` value (defaults until the site config lands; the points cap is `LF_OrgConfig.HeldGroundMaxPoints()`).
- **Rules:** one organisation present with at least `minMembersPresent` gains (the holder banks points and hold seconds, a challenger builds capture progress at counted members per second; inner ring counts double, capped by `maxCountedPerOrg`). Two or more organisations present freeze everything. An organisation under the minimum gains nothing but still contests. Nobody present: capture progress decays at `decayPerHour`; banked points stay. Progress at `captureSeconds` flips the site (new holder starts at zero) and starts `flipCooldownSeconds`, in which a full capture waits at the threshold.
- **Data:** none persisted; `LF_SiteHoldState` is the value a later slice saves.
- **Proof:** `sites/site-hold` (LOGIC).

## Site sweep and capture (#2680)

- **Player sees:** nothing on screen yet; an organisation that holds a site alone long enough gets its hideout (the door now opens for them).
- **Owner and side:** `LF_ContestedSites` (server, every 5 s) over the sites `LF_ContestedSiteComponent` registers with `LF_Places` (kind `contested-site`, id = hideout id; no coordinates in script).
- **Rules:** living player characters of an organisation inside `radius` count when they moved at least 1 m or acted (`NoteAction`) within `activeWindowSeconds`; inside `innerRadius` they count double. The holder is the hideout's lessee; a lease changed elsewhere resets the hold to the new lessee and its held ground. A capture: `LF_Hideout.HandOver` releases the old lease (reason `captured`, its held ground goes with it), leases the capturer and sets its held ground to zero, then `LF_OnSiteCaptured`. Contest start: `LF_OnSiteContested`, once per contest. The site is held as CAPTURED ground (#2816): a capturer that already leases a base keeps it and holds the site beside it, any number of captures; a captured site never becomes the organisation's base place (`org_base`). `LF_Hideout.SitesOf(org[, capturedOnly])` and `IsCaptured(hideout)` list what an organisation holds (the data the ledger page and war board list, #2834).
- **Data:** the hideout lease, the "heldground" payload (#751) and the "tenancy" payload (#2816, save version 58: one row per captured site; a save from before it loads every lease as a base). Capture progress is runtime only and starts at zero after a restart.
- **Proof:** `sites/site-sweep` (WORLD, annex hideout, sightings through the `SweepSite` seam), `sites/capture-held`, `sites/capture-base-reload` (WORLD), `persist/site-capture` (PERSISTENCE), `wt e2e` (a real restart).

## Site rent (#2683)

- **Player sees:** nothing on screen yet; the bank a held site builds up for its holder.
- **Owner and side:** `LF_SiteRent` (server), called by `LF_ContestedSites.SweepSite` after each step with the holder before the step and the outcome.
- **Rules:** a held site banks `rentPerHour * elapsed / 3600` per sweep (fractions carried at runtime), capped at `maxRentBanked`. Nothing banks while unheld, contested, or on the step that captures it. A capture keeps the bank on the site. One `LF_OnMoneyMoved` (`site_rent`, the holder org id as the uid, the bank as the balance after) per hour of banking, or when the cap is reached.
- **Data:** the `siterent` save section (`LF_SiteRentSavePayload` in `LF_SiteRent.c`, its own version 1; lock `tools/validation/record-shapes/siterent.json`), rows `LF_SiteRentRow` {version, hideout id, banked, last rent time (unix s), unlogged amount and seconds}. Load replaces the banks (idempotent) and keeps a bank above the cap at the cap. A save without the section loads every bank empty. Under one unit of carried fraction is lost on a restart.
- **Proof:** `sites/site-rent` (PERSISTENCE), and `tools\cli wt e2e` across a restart.

## Site flag board (#2685)

- **Player sees:** a flag pole at the site with a small painted board that reads the holding organisation's name, or "Unclaimed". The back of the board is plain.
- **Owner and side:** `LF_SiteFlagComponent` on `Prefabs/Sites/LF_SiteFlag.et`. The server sets the name (`LF_OnSiteCaptured` for its hideout id, and a 5 s reconcile with `LF_Hideout.GetOwner`); one `RplProp` string replicates it; clients (and a listen host, locally) paint it on the plate child.
- **Data:** none persisted; the board rebuilds from the lessee after a restart.
- **Proof:** `sites/site-flag` (WORLD). A client's board is looked at in a session (`docs/features/sites.md`).
