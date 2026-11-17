# Sites

## Intent

Contested ground a gang can win and hold. This first piece is the scoring maths only: who gains, who is frozen, how capture progress decays. The world side (the site object, presence sweep, UI) comes in later gang war slices (#1022).

## Interaction pattern

None yet: no player-facing object in this slice. `LF_SiteHold.Step` is a pure function of a site's state, the counted presence per organisation and the elapsed seconds; the server's site system will call it.

## Site hold (#2678)

- **Player sees:** nothing yet; the maths behind a site's holder, points and capture bar.
- **Owner and side:** `LF_SiteHold` decides on the server. Rules are a plain `LF_SiteRules` value (defaults until the site config lands; the points cap is `LF_OrgConfig.HeldGroundMaxPoints()`).
- **Rules:** one organisation present with at least `minMembersPresent` gains (the holder banks points and hold seconds, a challenger builds capture progress at counted members per second; inner ring counts double, capped by `maxCountedPerOrg`). Two or more organisations present freeze everything. An organisation under the minimum gains nothing but still contests. Nobody present: capture progress decays at `decayPerHour`; banked points stay. Progress at `captureSeconds` flips the site (new holder starts at zero) and starts `flipCooldownSeconds`, in which a full capture waits at the threshold.
- **Data:** none persisted; `LF_SiteHoldState` is the value a later slice saves.
- **Proof:** `sites/site-hold` (LOGIC).
