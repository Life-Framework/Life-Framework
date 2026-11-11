# Integration spec

**Ideas from a live life server's player wiki, cross-referenced with Overthrow and our code**

Prepared 2026-11-11 for the maintainer; decisions taken the same day. Sources:
the public player wiki of a live Reforger life server (67 pages, ideas only),
Overthrow for Arma Reforger (`ArmaOverthrow/Overthrow.Arma4`, commit
`9dbb8fad`, 2026-09-18, 705 script files, APL-SA with the author's permission
to port under attribution), and the Life Framework tree (`main`, 2026-11-11).
Every item below is filed as a GitHub issue, listed in the epics: M3 #746,
M5 Framework #632, M5 Flagship #633, M4 #231 and M6 #747.

## 1. Summary

The reference wiki shows what a live Reforger life server players stay on looks
like. Its strength is the economy and the social structures: a market that
reacts to selling, skills that grow by doing, gangs that declare wars and hold
ground for rent, police careers with qualifications, and crimes that announce
themselves so they become someone else's story.

We read every page, sorted each idea into framework (core, era-neutral) or
flagship (1989 Everon pack) work, and then went looking for real code in
Overthrow and in our own tree. Three conclusions:

1. **Record shapes are settled in M3**, because they shape saved data or the
   owner config schema. Most need only the record shape now; behaviour follows
   in M5. Doing it after the persistent launch (#556) means migrations.
2. **Overthrow is war code, so its value is small exact pieces, not features.**
   The best ports are its persistence versioning pattern, the bulk-sale
   marginal pricing seam, the trade-XP cooldown, the fractional fuel-charge
   ledger and pricing, the storage ledger rules, the objective phase machine,
   and the server-picks-the-building purchase request. It has no jail, licence,
   rank, war, crop, fishing or reputation code.
3. **We already own more than the plan assumed.** Quest chains with auto-accept
   (`LF_QuestDefinition`) make onboarding content, not code. Hideout stash and
   persistent storage are the locker pattern. The garage, impound and vehicle
   record exist. `PricedSign` already paints live prices on a chalkboard. The
   zone service and the incident log carry the event and war rules. The base
   game ships a full firing-range system for a shooting range.

## 2. Principles

- **Fixed rules, player decisions.** Tables and formulas, never improvisation.
- **Every crime is an announcement.** Smoke, alarms, dispatch, the gazette.
- **Scarce, public, decaying rewards.** Rent that decays, prices that fall
  under farming, buyers that rotate.
- **Hauling is the progression.** Goods carried and worked at stations.
- **Declared free-fire contexts** (wars, contested sites, events) instead of
  PvP bans.
- **Gates name what is missing.** Every refusal says which level or licence.
- **In 1989, every app becomes a place.** A board, a form, a radio, a person.

## 3. Build these shared pieces first (M3)

Several items reuse each of these, so they come before the features:

| Piece | Used by | Based on |
| --- | --- | --- |
| Board answers per read | Price board, stall board, record board, gazette, wanted board | `LF_WantedBoard`, `LF_SignComponent.PricedSign` |
| Cash escrow (pay before delete, claw back on failure) | Stall market, race pot, locker rent | `LF_MoneyUtils`, the ledger |
| Event shell (named phases, typed key bag, conditions AND, aborts OR) | Prison break, evidence convoy, airdrop later | Adapted from Overthrow `OVT_ObjectivePhase`, `OVT_ObjectiveConfig` |
| Save-version discipline | Every new record | Overthrow `OVT_PlayerManagerSerializer` append-only versions; our `docs/persistence.md` |
| Nearest named place | Crime reports, gazette, pack content | Adapted from Overthrow `OVT_TownManagerComponent.ProcessTown` |

The course runner (checkpoints, timer, refund on restart) is no longer shared:
licences stay bought, so it is built inside the race runner.

## 4. Order of work

| Phase | Milestone | What |
| --- | --- | --- |
| A | M3 | Record shapes in one save-format pass: market, skills on job levels, org ranks and wars, site hold time, vehicle fields, lockers, police ladder, zone rule table, property reservation |
| B | M3 | The shared pieces above; playtime (#692) and the request audit (#693) |
| C | M5 Framework | Market curve and board, skill gates, wars, contested sites, staged break-in, AI convoy driving, garage slots and towing, lockers, inspection and compartments, crew robbery, hands-up prompt, fuel stock, onboarding |
| D | M5 Flagship | Goods and price board, police career, evidence convoy, prison break, contested hideout, fishing and honey, fuel depot, village texture |
| E | M4 | Police application gates, moderation and appeals, clip stamp, Discord verification, FAQ |
| F | M6 | Housing built by hauling, hidden crops (sidecar), house spawn |
| Later | M5 or after | Recycler, stall market, race runner, gazette, hidden reputation (sidecar) |

## 5. Decisions (maintainer, 2026-11-11)

| # | Question | Decision |
| --- | --- | --- |
| 1 | Item wear on weapons and tools | **No wear.** No condition field in the M3 schema |
| 2 | Skills versus job levels | **A job level is the skill level.** One XP track per job, with unlock gates per level |
| 3 | Licences by practical exam | **No exams.** Licences stay bought with skill points; the licence-exam system, the driving test and the firearms test are dropped |
| 4 | Locker rent lapses | **Lost-and-found box** at the site, reclaimed for a fee; the locker is freed |
| 5 | Minimum members to score a contested site | **Yes, owner-configurable**, checked per scoring tick |
| 6 | Evidence convoy, first version | **AI driven from the start**, with stuck detection and a fallback |
| 7 | Hands-up | **The target chooses** through a short prompt; never a forced pose |
| 8 | Hidden reputation | **Later, as a sidecar payload**; not in the M3 pass |
| 9 | Planted crops and race records | **Neither in M3**; both become sidecar payloads when built |
| 10 | Asset pipelines #340 to #343 | **Move to M5**, beside the 3D harness (#339) |

## 6. Licence and attribution

- Overthrow code ported into our tree keeps the header from
  `THIRD_PARTY.md`: derived from Overthrow, used with the author's permission,
  relicensed MIT.
- Reference wiki content is described in our own words; no text or numbers are
  reused as ours. The Altis Life framework (CC BY-NC-ND) is ideas only.

## 7. Reading the detailed sections

Sections A, B and C follow, one block per item: goal, Overthrow reference,
our code to extend, data model, config, authority and network, world
interface, proof, size and risks. Paths marked "check" were not confirmed.
Proposed test names are proposals, not existing tests. Where a decision above
changes an item, a **Decided** line sits under its heading.

## A. Decide in M3: saved data and owner schema


Conventions used below. Overthrow paths are relative to `Scripts/Game/` in the Overthrow repo (commit `9dbb8fad`). LF paths are relative to `addons/LifeFramework/Scripts/Game/`. Line ranges are from the files as read on 2026-11-11; "check" marks a guess. A copied Overthrow file takes the header from `THIRD_PARTY.md` and gets a row there. Four files are already ported (`Feature/Money/LF_MoneyDeltaTracker.c`, `LF_MoneyFormat.c`, `Feature/Resources/LF_ResourceDefs.c`, `Feature/Shop/LF_ShopRules.c`).

**Save-format ground rules every item below obeys** (from `docs/persistence.md` line 285 and the `LF_PersistenceComponentSerializer.c` header):
- Payload order is `RegisterPersistencePayloads` (line 94); the written version is the list length, currently 25 (`storeunpaid` last). A new system appends one `order.Insert` and bumps the version. Never insert or reorder.
- Records are written by reflection and are positional. A field added to an existing record class breaks every older save of that payload. Two sanctioned routes: (a) before the persistent launch (#556) M1 was never published, so a record may be reshaped once with the version bumped and the pre-release reset note in `docs/persistence.md` extended; (b) after launch, a new field travels in its own sidecar payload and is `[NonSerialized()]` on the base record (the `m_iAmmo` / `itemstate` precedent, `Feature/Character/Persistence/LF_StorageSnapshot.c` lines 15-30). The M3 decision is: reshape now (route a), sidecar after #556.
- Overthrow's version of the same rule: `Persistence/Serializers/Components/OVT_PlayerManagerSerializer.c` lines 22-75 (append only; each version clears what the older payload never wrote) and `Deserialize` lines 235-330 (per-version back-fill after load). Its `OVT_PersistedWarehouse` / `OVT_PersistedWarehouseV2` pair (`OVT_RealEstateManagerSerializer.c` lines 28-65) records the second lesson: the container writes the concrete class name as `$type`, so a persisted class name is frozen for the life of every save. Name new record classes for what they are and never rename them.
- Config surfaces are the seven in `docs/roadmap/m3-outline.md` lines 88-100 (`jobs`, `shops`, `crime`, `licences`, `vehicles`, `zones`, `persistence`). Items without a surface below add a section; the section list is itself an M3 schema decision (check with #283).
- Tests: one file per test `Tests/LF_Test_*.c`, tier comment, `// red-proof:` line, names written `area/name` (for example `persist/progression-roundtrip`). The test names below are proposals.

---

### 1. Dynamic market price state

- **Goal:** each good has a per-server price that moves with trade between a floor and a ceiling and recovers over time, tuned by owners.
- **Overthrow reference:**
  - `GameMode/Managers/OVT_EconomyManagerComponent.c` `GetSellPriceAtOffset` lines 708-731: price is derived, not stored: `price + (1 - stock/maxStock) * price * 0.1 + distanceToPort * price * 0.0001`. The `stockOffset` seam (comment lines 696-707) prices a bulk sale unit by unit so a truckload rides the curve down. **Adapt the pattern** (marginal pricing per unit), not the formula: ours must be a stored price with bounds.
  - Same file `ReplenishStock` lines 443-520 (refill to half of max each morning), `UpdateShops` lines 522-568 (simulated NPC buying drains stock, scaled by town population and stability), `CheckUpdate` lines 162-238 (hour latches so a restart does not pay twice) and `HandleTimeSkip` lines 240-304. **Adapt**: the hour-latch idea for the recovery tick; ignore the NPC-buying simulation until M5.
  - `Configuration/OVT_PricesConfig.c` lines 9-30 (`cost`, `demand` per type/mode/name filter). Ours already has the same rule shape in `LF_ShopPricesConfig`.
  - Persisted state: only resistance money and tax (`Persistence/Serializers/Components/OVT_EconomyManagerSerializer.c` lines 16-60). **No close match** for a per-good persisted price with floor, ceiling and recovery.
- **Our code to extend:** `Feature/Shop/LF_ShopPriceResolver.c` (`ResolvePrices`, one code path for display and charge), `Feature/Shop/LF_ShopPricesConfig.c` and `Configs/Shop/ShopPrices.conf` (rule list), `Feature/Shop/LF_StoreComponent.c` (`Export`/`ImportRow`, line ~603-630) and `Feature/Shop/LF_Stores.c`, `Feature/Shop/LF_StoreRules.c` (`LF_StoreStockRow`, `storestock` payload version 21), `Feature/Money/LF_Ledger.c` (reasons for trades). The `LF_StoreComponent` stock restock interval is the natural place for the recovery tick.
- **Data model:** new payload `marketprices` (append, next version after `storeunpaid`; version 26 unless something lands first). One `LF_MarketPriceRow` per good: `m_sGoodKey` (item prefab ResourceName string, same key as `LF_StoreStockRow.m_sPrefab`), `m_iCurrent` (price in whole cash units), `m_iLastTradeMinute` (in-game minutes, for recovery maths on load). Floor, ceiling and recovery rate are config, not saved, so a retune reaches old saves (Overthrow does the same for storage capacity, `OVT_StorageComponentSerializer.c` header lines 36-45). Version first, rows keyed by good only (per-server). Per-town keys are not needed in M3; reserve with a `m_sScopeKey` string that is empty today (check: costs one string per row, saves a reshape if town markets come).
- **Config:** `shops` section. Core defaults: a curve in the `LF_ShopPricesConfig` shape, per rule `m_fFloorMult`, `m_fCeilingMult` (relative to the rule's base price), `m_fStepPerUnit` (price move per unit sold or bought), `m_fRecoveryPerHour` (pull back toward base), `m_bDynamic` (off by default so the flagship's fixed prices stay as they are). Pack values: which goods are dynamic and the numbers. Cross-check with #686 (no profit loop): floor times buy margin must stay at or above ceiling times sell margin, and the validator must read these fields.
- **Authority and network:** server only. Current prices are read by the shop UI through the existing shop open path; if the menu shows a live price, push the rows of the opened store in the existing open RPC reply (no new `RplProp`, no per-trade broadcast). Price at charge time is computed on the server from the same resolver the display uses.
- **World interface:** the shop shelf and till already in `Feature/Shop`; prices show on shelf tags and the receipt. A "market board" prop by the trader showing the three biggest movers is optional M5 content, not M3.
- **Proof:** `LF_Test_MarketPrice` (LOGIC): sells push current down to the floor and no lower; buys push to the ceiling; recovery moves toward base by the configured rate; a bulk sale prices each unit marginally. `LF_Test_MarketPricePersist` (PERSISTENCE): save, restore, old save (version 25) loads with every good at base. Manual: sell 20 of one good at a store, read the shelf tag, restart, read again.
- **Size and risks:** M. Risk: a profit loop between two shops (buy low at one, sell high at another) and a price that drifts on every hour tick after a long offline gap; clamp recovery by elapsed in-game time and run the #686 check.

---

### 2. Crime catalog depth (#685)

- **Goal:** one typed table of crimes giving wanted value, fine, sentence, bail rule, repeat-offence escalation and licence consequence, replacing offence strings and scattered constants.
- **Overthrow reference:** **No close match.** Overthrow has no per-crime catalog or jail; its police surface is the occupying faction's response (`Components/Controller/OVT_IllegalActionComponent.c`) and the survey leaves wanted level behind (`docs/overthrow-survey.md`, "Leave behind"). Do not port.
- **Our code to extend:** `Feature/Police/LF_WantedReports.c` (offence key constants lines 45-58, `Report` line 84; today the offence is a localisation key string), `Feature/Crime/LF_CrimeRecord.c` (hidden per-account, per-offence counts, `crimerecord` payload version 15, already the repeat-offence source), `Feature/Police/LF_PoliceConfig.c` (ticket offences `LF_TicketOffence` line 343, wanted-add attributes lines 88-96), `Feature/Police/LF_Jail.c` (`SentenceSeconds(wantedLevel)` line 268, `Imprison` line 349), `Feature/License/LF_LicenseManagerComponent.c` (`LF_ELicenseType`, `m_iMinLevelRequired`; no revoke call found by grep: check), `Feature/Crime/LF_CrimeEvents.c` (`LF_OnCrimeCommitted`).
- **Data model:** the catalog is config, not saved. What is saved: `LF_CrimeRecordRow` stays `{uid, offence, count}`; the offence key becomes the stable catalog `m_sCrimeId` (same string field, so no reshape: keep today's `#LF-Offence_*` keys as the ids, or map them once at load, check). New saved state is per-account `m_iSeizedLicenses` bits or a list of `{licence, untilMinute}` rows (check) as sidecar payload `licencebans`. Repeat offence is derived: `LF_CrimeRecord.Count(uid, crimeId)` against catalog thresholds. No-bail is a catalog flag read when a sentence starts; a bail row needs no storage if bail is only a fine paid at the desk.
- **Config:** `crime` section. Per entry: `m_sCrimeId`, `m_iWantedAdd`, `m_iFine`, `m_iSentenceSeconds` (or wanted-scaled), `m_bNoBail`, `m_iBailFine`, `m_iRepeatThreshold`, `m_fRepeatMultiplier`, `m_iRepeatWantedAdd`, `m_eLicenceSeized` (none or a `LF_ELicenseType` by name, tolerate unknown names like `LF_Progression` does), `m_iSeizureHours`. Core ships the structure and neutral defaults; the pack ships the 1989 Everon penal code.
- **Authority and network:** server only. `LF_WantedReports.Report` resolves the catalog row; the client sees wanted level, the ticket and the jail slip as today. No new RPC. Licence seizure removes the licence from the character through the existing licence component and the `progression` write-through, then the sidecar timer restores it.
- **World interface:** the penal code is a poster or desk sheet at the police station (a read-only `LF_Sign` row, check `Feature/Signs`), the ticket form already lists offences, the jail slip prints the code. No menu.
- **Proof:** `LF_Test_CrimeCatalog` (LOGIC): lookup, repeat escalation at threshold, no-bail flag, unknown id logs and falls back. `LF_Test_CrimeLicenceSeizure` (WORLD): a seized licence blocks its action and returns after the timer. `LF_Test_CrimeCatalogPersist` (PERSISTENCE): `licencebans` round trip, version 25 save loads with none seized. Manual: commit a repeat offence twice, check wanted, jail slip, licence gate.
- **Size and risks:** M to L (many call sites pass offence strings). Risk: two sources of truth during the move; do it as one PR that deletes the `OFFENCE_*` constants and the scattered `m_iMurderWanted` style attributes in `LF_PoliceConfig`.

---

### 3. Skills by doing (XP map, unlock gates)

- **Decided (2026-11-11):** a job level is the skill level. One XP track per job, unlock gates per level; no second system.
- **Goal:** each character earns XP per skill by doing the work, and config gates jobs, licences and actions on job-plus-level.
- **Overthrow reference:**
  - `GameMode/Managers/OVT_SkillManagerComponent.c`: `GiveXP` line 347 / `TakeXP` 332 (server only, then `StreamPlayerXP` RPC), per-action hooks `OnPlayerBuy`/`OnPlayerSell`/`OnAIKilled` lines 271-330 with `TryStartTradeXPCooldown` (per-player cooldown so trade cannot be churned for XP, lines 271-290), `AddSkillLevel` line 111 (server re-derives spendable points and level cap), `OnPlayerDataLoaded` line 77 (replays skill effects, skips an unknown key with a warning). **Port the cooldown and the server-side re-derivation; adapt the rest.** Overthrow has ONE xp and skills bought with points, so its XP map does not exist.
  - `Configuration/OVT_SkillsConfig.c` lines 1-40: `OVT_SkillConfig { m_sKey, levels[ effects[] ] }`, `OVT_SkillEffect` with `OnPlayerData`/`OnPlayerSpawn`. **Adapt** as the shape of config-defined effects.
  - `Data/OVT_PlayerData.c`: `xp`, `skills` map<string,int>, `GetLevel()` line 153 (`1 + 0.1*sqrt(xp)`), `CountSkills` line 190. **Ignore** the formula; ours is a configured threshold table.
  - Persistence: `OVT_PersistedPlayer` (`OVT_PlayerManagerSerializer.c` lines 25-75) stores `skillKeys[]` and `skillLevels[]` as parallel arrays, unknown keys skipped on load. **Adapt**: parallel arrays are fragile, use one row per skill.
- **Our code to extend:** `Feature/Level/LF_PlayerLevelComponent.c` (level/XP/skill points, `AddExperience` line 51, `GetExperienceForNextLevel = level * 100`; `Feature/Level/Feature.md` lists its defects: unguarded setters, save-per-gain), `Feature/Account/LF_Progression.c` and `Feature/Account/Persistence/LF_ProgressionRecord.c` (the `progression` payload, save version 7; fields `m_fPlayerExperience`, `m_iCareerLevel`, `m_fCareerExperience`), `Feature/Jobs/LF_CivilianJob.c` (career level that gates jobs), `Feature/Jobs/LF_JobConfig.c`, `Feature/Gathering/`, `Feature/Mining/`, `Feature/Processing/` (the "doing" sources), `Feature/License/LF_LicenseManagerComponent.c` (`m_iMinLevelRequired`, `m_iSkillPointCost`: existing gate shape).
- **Data model:** new payload `skills` (append): `LF_SkillXpRow { m_sPlayerUid /*character record key, LF_Utils.GetPlayerUID*/, m_sSkillId, m_iXp }`, one row per (character, skill); level is derived from config thresholds, never saved, so rebalancing reaches old saves. Keep `m_fPlayerExperience` in `LF_ProgressionRecord` as is (route b: do not touch the shipped record). Migration: a version 25 save has no `skills` payload; each skill starts at 0 (the old career XP is not split, state that in the PR).
- **Config:** `licences` section (the surface already named "licences, skill points, progression rules"). Core: skill ids and the engine of per-action XP award (`m_iXpPerAction`, `m_fCooldownSeconds`, daily cap `m_iXpDailyCap` to defeat farming), threshold table `m_aLevelThresholds`. Gate rows: `{ m_eJob, m_sSkillId, m_iMinLevel }` read by job sign-up (`LF_CivilianJob` sign-up already checks career level) and by licence purchase. Pack: skill list, thresholds, which jobs need which skill.
- **Authority and network:** XP is added only by server callers behind an `LF_Progression.AddSkillXp(uid, skillId, amount, reason)` mutator (public mutator rule, AGENTS.md). Replicate the character's own skill levels to its owner through the existing player-level replication path (check how `LF_PlayerLevelComponent` replicates; Overthrow uses a broadcast `RpcDo_SetPlayerXP`, ours should be owner-only because skill levels of others are not shown). A client never sends XP; "spend" no longer exists.
- **World interface:** progress is background state; the player reads it at a trainer or the job board desk (matches `Feature/Level/Feature.md` rung 3). Gated action refusals toast `#LF-Skill_NeedLevel` with the job and level; no screen.
- **Proof:** `LF_Test_SkillXp` (LOGIC): XP to level thresholds, cooldown and daily cap, unknown skill id skipped. `LF_Test_SkillGate` (WORLD): job sign-up refused below level and accepted at level. `LF_Test_SkillXpPersist` (PERSISTENCE): round trip and old-save. Manual: mine 10 times, read the trainer, restart.
- **Size and risks:** M. Risk: XP farming loops (cooldown and cap above) and a second progression system beside `LF_PlayerLevelComponent`; decide in the PR whether the per-skill map replaces the single level (preferred) or sits under it.

---

### 4. Organisation ranks and rights, declared wars

- **Goal:** a rank table with a rights bitmask per organisation, and a persisted conflict record (declared, accepted, ends) between two organisations.
- **Overthrow reference:** **No close match.** Overthrow has one resistance faction and an occupier; there are no player organisations, ranks or inter-player wars, and the survey says "Leave the war in Overthrow" (`docs/overthrow-survey.md`). The only transferable pattern is the owner map keyed by persistent id (`GameMode/Managers/OVT_OwnerManagerComponent.c` lines 13-14, `SetOwner` line 78), which `LF_Organization` already satisfies.
- **Our code to extend:** `Feature/Organizations/LF_Organization.c`: `LF_OrganizationRankRecord` line 34 (four bool fields: `m_bCanWithdraw`, `m_bCanInvite`, `m_bCanKick`, `m_bCanBuyUpgrade`), `LF_OrganizationRecord` line 75 (`VERSION = 1`, ranks and members arrays), `LF_EOrganizationPermission` (WITHDRAW, INVITE, KICK, BUY_UPGRADE), `LF_OrganizationManager` line 502 (`ExportAll` 572, `ApplyAll` 590, `Invite`/`Kick`/`Deposit`/`Withdraw`/`BuyUpgrade`), `Feature/Organizations/LF_OrgRules.c` and `LF_OrgInvites.c`, `LF_OrgReplication.c` (view publish to the owning client via `modded class SCR_ChimeraCharacter`, line 121), `LF_OrganizationDeskAction.c`. Payload `organizations` (version 2).
- **Data model:** ranks: replace the four bools with `int m_iRights` (bitmask of an enum that starts with the current four and reserves bits for: manage-ranks, declare-war, use-stash, manage-territory, spend-fund). Order matters: keep the four old bits at positions 0-3 so a version 24 save back-fills `m_iRights` from the bools. This is a reshape of `LF_OrganizationRankRecord`, so use route (a), bump the `organizations` record `VERSION` to 2 and add a load-time conversion (check: `LF_OrganizationManager.ApplyAll` line 590 already tests `m_iVersion < 1`; add a `< 2` branch). Rank order: add `int m_iOrder` so a rank table has a ladder. Wars: new payload `orgwars` (append): `LF_OrgWarRecord { m_sWarId, m_sDeclarerOrgId, m_sTargetOrgId, m_iState /*DECLARED, ACCEPTED, ENDED*/, m_iDeclaredAtMinute, m_iAcceptedAtMinute, m_iEndsAtMinute }`, keyed by org ids (stable PersistenceId strings), time in in-game minutes. A war record with a missing org is dropped on load with a warning.
- **Config:** new `organizations` section (check against the seven surfaces: add or fold under `crime`). Core: rights enum and default rank table (founder, member as `LF_OrganizationConfig.RANK_FOUNDER/RANK_MEMBER` today), `m_iMaxRanksPerOrg`, war rules: `m_bWarsEnabled` (off by default in core), `m_iDeclareCost`, `m_iAcceptWindowHours`, `m_iWarLengthHours`, `m_iCooldownHours`. Pack: names and the ladder; what a war lifts (kills in a war are not murder: ties to the crime catalog and `LF_Murder`).
- **Authority and network:** server only. Every rank edit and war transition re-checks the actor's rights bit on the server and publishes the changed `LF_OrgView` to online members through `LF_OrgReplication.Publish`. The war state needs a view line (opponent name, state, time left) in `LF_OrgView` (`LF_OrgRules.c` line 20); the view is already replicated per owner. If a client-sent request is added it is `RpcAsk_` on the player controller, never on an org entity.
- **World interface:** the organisation desk (`LF_OrganizationDeskAction`): rank table is read and set there; a war declaration is a signed paper handed over at the target's hideout door desk (check) rather than a menu entry.
- **Proof:** `LF_Test_OrgRights` (LOGIC): bit check per action, a lower rank refused, back-fill from old four bools. `LF_Test_OrgWar` (LOGIC): declare, accept, expire, a war with a missing org dropped. `LF_Test_OrgWarPersist` (PERSISTENCE). Manual: two characters, two orgs, declare and accept, restart mid-war.
- **Size and risks:** M for ranks and rights, M for the war record, L if war consequences (kill rules, territory) ship together; schema only in M3. Risk: the bitmask conversion on a shipped `organizations` payload; do it before #556.

---

### 5. Held ground (hold time and points per site)

- **Decided (2026-11-11):** a minimum number of a group's members present before a site scores, owner-configurable, checked per scoring tick.
- **Goal:** a site records how long and how strongly an organisation has held it, for supremacy and pay-outs.
- **Overthrow reference:** closest is the occupation and capture model (`GameMode/Managers/OVT_TownManagerComponent.c`: `OVT_TownData` line 19 with `support`, `stability`, `faction`, plus `Persistence/Serializers/Components/OVT_OccupyingFlagComponentSerializer.c`). The survey lists "capture" under "Leave behind". **Ignore** for code; **adapt only the idea** that a site holds a faction and a score the server ticks (`OVT_TownManagerComponent.c` modifier tick near line 284) with a stable key.
- **Our code to extend:** `Feature/Hideouts/LF_Hideout.c`: `LF_HideoutRecord` line 10 (`VERSION = 1`: `m_sHideoutId`, `m_sOrganizationId`, `m_bLocked`), `LF_HideoutState` line 32, `LF_Hideout.ExportAll`/apply; `Feature/Hideouts/LF_HideoutDoorComponent.c`; `Feature/Organizations/LF_OrgInvites.c` (`LF_OrgHideoutLease`, line 144). Payload `hideout` (version 3).
- **Data model:** on `LF_HideoutRecord` append `float m_fHoldSeconds` (current uninterrupted hold) and `int m_iHoldPoints` (banked points) plus `string m_sContestedByOrgId` (empty when uncontested). Route (a): bump record `VERSION` to 2 and `hideout` payload gets a conversion for old saves (check how `LF_Hideout.ApplyAll` treats the version today). Time is in-game seconds counted only while players are online (use the `TickOnline` approach in `LF_Jail.TickOnline` line 498 to avoid offline farming).
- **Config:** new `zones` section keys: `m_fPointsPerHour`, `m_fHoldSecondsToScore`, `m_iMaxPoints`, `m_bContestable` per site, `m_fContestRadius`. Pack: which hideouts are contestable and the point payout (links to a pay-out into the org fund through `LF_OrgLedger`).
- **Authority and network:** server only. A site is contested while a member of another organisation is inside its radius and none of the holder's is; result goes to the holder's members as a notification (`Feature/Notifications`), points to the org view (`LF_OrgView`) only. No per-tick replication.
- **World interface:** the hideout door and a flag prop at the site that raises when held (visible, owner-coloured); the org desk reads the points.
- **Proof:** `LF_Test_HeldGround` (LOGIC): hold accrual, contest pause, points cap, offline time not counted. `LF_Test_HeldGroundPersist` (PERSISTENCE). Manual: two orgs, stand in the radius, restart.
- **Size and risks:** M. Risk: a camping exploit (a one-member org sitting at a quiet site all night); require two online members or cap points per real hour (check).

---

### 6. Vehicle record fields (slots, tow, inspection, hidden compartment, cargo tuning)

- **Goal:** the owned-vehicle record carries a garage slot cap, tow state, inspection date, hidden-compartment contents and a cargo-capacity tuning value.
- **Overthrow reference:**
  - `Persistence/Serializers/Components/OVT_VehicleManagerSerializer.c` lines 12-30, `OVT_PersistedPlayerVehicle { persistentId, ownerUid, prefab, position, angles, locked }` is both the live and the saved shape (header lines 3-11); `GameMode/Managers/OVT_VehicleManagerComponent.c` `CaptureVehicleRecord` lines 1262-1300 keeps a transform fresh at every save. **Port** the idea (one record the manager owns, append-only); the survey already says the same ("Garage store and retrieve").
  - `Data/OVT_StorageRules.c` `ResolveAutoCapacity` lines 28-47: cargo capacity per vehicle by registered, legal and parking type, with an ERROR log and 0 for an unregistered prefab instead of a silent default. **Port** for cargo-capacity tuning; capacity is deliberately NOT persisted (`Persistence/Serializers/Components/OVT_StorageComponentSerializer.c` header lines 36-45), so a retune reaches old saves.
  - `Data/OVT_StorageLedger.c` (class line 27, `Add` line 52): a pure map<prefab,count> ledger with a capacity passed in. **Port** for the hidden compartment's contents; far lighter than snapshotting a whole entity tree.
  - Tow, inspection, hidden compartment: **no close match**. (`OVT_VehicleStorageComponent` is not in the tree, per the survey.)
- **Our code to extend:** `Feature/Vehicles/LF_VehicleOwnership.c` (`LF_VehicleRecord` lines 6-58, `VERSION = 1`: id, owner, key id, prefab, spawn prefab, shop id, plate, name key, locked, stored; `ExportAll`/`ApplyAll`; `IsImpounded` line 301, `ImpoundStreetVehicles` line 275), `Feature/Vehicles/LF_Garage.c` (`LF_GarageConfig.m_iImpoundFee`, `Configs/Vehicles/LF_Garage.conf`; `LF_ImpoundDeskComponent` line 342), `Feature/Vehicles/LF_VehicleGarageComponent.c`, `Feature/Vehicles/LF_VehicleCatalog.c`, `Feature/Police/LF_VehicleSearchAction.c`, `Feature/Crime/LF_StolenVehicles.c`. Payload `vehicles` (version 4).
- **Data model:** #688 is the vehicle price issue; the fields below extend it. Route (a) for the base record, then a sidecar for anything that grows. In `LF_VehicleRecord` append, with `VERSION = 2`: `m_iSlotRank` (index in the owner's garage), `m_iTowState` (0 none, 1 towed, 2 impounded by police), `m_iTowedAtMinute`, `m_iInspectionMinute` (in-game minute of the last inspection, 0 never), `float m_fCargoTuneMult`. The hidden compartment is a sidecar payload `vehiclecache`: `LF_VehicleCacheRow { m_sVehicleId, ref array<LF_StorageLineRow> lines }` with a `{prefab, count}` row (the `OVT_PersistedStorageLine` shape, `OVT_StorageComponentSerializer.c` line 14). Slot cap is owner or pack config, not saved per vehicle: count of the owner's `m_bStored` records against `LF_GarageConfig.m_iSlotCap`.
- **Config:** `vehicles` section. Core: `LF_GarageConfig.m_iSlotCap` (default high enough not to bind), `m_iTowFee`, `m_iInspectionIntervalHours`, `m_iCompartmentCapacity`, per-prefab `m_fCargoMult` bounds. Pack: numbers per vehicle class and where the tow yard is. The vehicle prices of #688 live in the same section.
- **Authority and network:** server only. Stow and take-out of the compartment is a server action on a hidden `ScriptedUserAction` (not shown to a non-owner without the key and a search); police search (`LF_VehicleSearchAction`) reads the cache server-side. Nothing in the record replicates except the plate and locked flag as today. RPCs go through the existing action path (no new `RpcAsk_`).
- **World interface:** the garage door pad for slots, the tow yard or impound desk (`LF_ImpoundDeskComponent`) for towed cars, a mechanic bay for inspection and cargo tuning, the car's own boot or a concealed spot (second action visible only after a search skill check) for the compartment.
- **Proof:** `LF_Test_VehicleRecordV2` (PERSISTENCE): old save (version 25, record `VERSION = 1`) back-fills defaults. `LF_Test_VehicleSlotCap` (LOGIC). `LF_Test_VehicleTow` (WORLD): tow, fee, release. `LF_Test_VehicleCache` (LOGIC plus PERSISTENCE). Manual: store a car with items in the compartment, restart, retrieve, police search.
- **Size and risks:** M for the record, L for all behaviours. Risk: item duplication through the cache and the main trunk (the compartment must be removed from the capture and spawn paths); and the impound path already derived state (`IsImpounded`), so tow state must be explicit rather than another derivation.

---

### 7. Personal lockers

- **Decided (2026-11-11):** when rent lapses, contents move to a lost-and-found box at the site, reclaimed for a fee, and the locker is freed.
- **Goal:** storage keyed by character and site that survives death and restart.
- **Overthrow reference:**
  - `Data/OVT_StorageLedger.c` (pure map ledger, line 27) and `Data/OVT_StorageRules.c` `TransferLedgerLine` line 140 (move a line between ledgers within destination capacity and report shortfall), `HolderIsInRange` line 181. **Port** both: they are pure and testable at LOGIC tier.
  - `Persistence/Serializers/Components/OVT_StorageComponentSerializer.c` lines 14-172: one persisted line type `{prefab, count}`, capacity not saved, `ApplyPersisted` clears and rebuilds (idempotent). **Adapt** as the lockers payload row shape.
  - Ownership by persistent id: `GameMode/Managers/OVT_OwnerManagerComponent.c` lines 13-14, keyed by a position string; the survey's "Housing ownership" says copy that. A locker is a smaller case of the same key.
- **Our code to extend:** `Persistence/LF_PersistentStorageComponent.c` (placed storage saved under a stable key via `LF_StorageSnapshot` slot `placed`; it is one container per key, so it is a shared stash, not a per-character locker), `Feature/Character/Persistence/LF_StorageSnapshot.c` (`Capture` line 332, `Apply` line 405, `Set`/`Get`/`Clear`; `OnCharacterDied` line 181 clears only slot `carried`, so a `locker:` slot survives death by construction), `Feature/Hideouts/LF_HideoutStashStorageComponent.c`, `LF_HideoutStashStorageManagerComponent.c`, `LF_HideoutStashPlayerController.c` (the member-only open RPC, `RpcAsk_LF_OpenStash` line 31 / `RpcDo_LF_OpenStash` line 82), `Feature/Police/LF_DutyLocker.c` (swap pattern: clock-in stores the civilian tree as a named snapshot slot), `Feature/Investigation/LF_EvidenceLocker.c`.
- **Data model:** no new payload. A locker is an `LF_StorageSnapshotRecord` with `m_sOwnerKey = characterId` and `m_sSlot = "locker:<siteId>"`, riding the existing `carried` payload (as hideout stash contents do: `LF_PersistentStorageComponent` header lines 7-9), so no version bump. `LF_StorageItemRecord` per-item state (ammo, serial) already has sidecars. The site id is the physical locker bank's stable key. Per-site capacity is config.
- **Config:** `persistence` section for what persists; locker capacity and rental fee in a new `lockers` key under `shops` (check). Core: `m_iSlotsPerLocker`, `m_iRentPerDay` (0 default). Pack: where banks of lockers are and who may use them.
- **Authority and network:** server only. Open: the player's controller sends `RpcAsk_OpenLocker(rplId)`; the server resolves the caller's character, builds a transient container by `LF_StorageSnapshot.Apply(lockerEntity, record)`, and on close or disconnect calls `Capture` and `Set` back. Two characters never see each other's contents because the entity is a per-open instance. The server must capture on server shutdown and disconnect (as `CaptureAllPlaced` does for placed storage) to prevent loss. Do not trust a client-supplied character id.
- **World interface:** a row of lockers (gym, station, hideout) opened with the hold-use action; the key is the character, no key item. The existing hideout stash chest is the org-shared sibling.
- **Proof:** `LF_Test_LockerRoundtrip` (WORLD): put, close, reopen, same items. `LF_Test_LockerSurvivesDeath` (WORLD): death clears `carried` and not `locker:*`. `LF_Test_LockerPersist` (PERSISTENCE): restart, old save. `LF_Test_LockerIsolation` (WORLD): a second character at the same locker sees an empty one. Manual: store a weapon, die, respawn, open.
- **Size and risks:** M. Risk: duplication (item both in the transient container and the carried tree on a crash between apply and capture); mark the snapshot "open" on apply and treat an open flag at load as "contents are lost to the container, restore from the last closed snapshot" (check).

---

### 8. Item condition (wear and repair count): decision

- **Decided (2026-11-11):** no item wear. No condition or repair-count field in the M3 schema. If wanted later, it comes as a sidecar payload.

### 9. Hidden reputation

- **Decided (2026-11-11):** not in the M3 pass. Built later as a sidecar payload; the shape below is the starting point.
- **Goal:** a per-character record of how they treat people, never shown as a number and not a price modifier.
- **Overthrow reference:** closest is town `support` and `stability` (`GameMode/Managers/OVT_TownManagerComponent.c` `OVT_TownData` line 19), which are town-level, shown on the map, and do modify prices (`OVT_EconomyManagerComponent.UpdateShops` line 530 uses stability). Opposite of what we want. **No close match; do not port.**
- **Our code to extend:** `Feature/Crime/LF_CrimeRecord.c` is the precedent and the host: a hidden server record, never replicated and never shown, read by the fence and serial checks (`LF_CrimeRecord.Count/Total`, `crimerecord` payload version 15), fed by a bus subscriber `LF_CrimeRecordSubscriber : LF_EventSubscriber` on `LF_OnCrimeCommitted` (`Feature/Crime/LF_CrimeEvents.c`). `Feature/Account/LF_Introductions` (who knows whose name) is the other hidden per-character table. No `reputation` string exists in `Scripts/Game` (grep, empty).
- **Data model:** new payload `standing` (append): `LF_StandingRow { m_sPlayerUid, m_sKind /*"fair-trade","robbed","helped-medic","killed-civilian"... catalog key*/, m_iCount }`. Same shape as `LF_CrimeRecordRow` so it can reuse the load and export helpers; counts, not a score, so no number can leak and owners derive meaning in config. Tolerate unknown kinds like unknown licence names (`LF_Progression.GetLicenses`, warn and skip).
- **Config:** new `standing` keys under `crime`: per kind a `m_sEventId` mapping from bus events, `m_iDecayDays` (0 = none), and read-side thresholds used by behaviours (NPC refusal, a fence tip-off, police patience). No price modifier ships; guard it in the validator so a pack cannot wire one without an explicit flag (the plan says it is not a price modifier).
- **Authority and network:** server only; never replicated, never in the character sheet or a toast. Readers call `LF_Standing.Count(uid, kind)`; nothing exposes a total.
- **World interface:** none directly. Effects show through NPC dialogue lines, a closed door at a shop, police patience; those are M5 behaviours.
- **Proof:** `LF_Test_Standing` (LOGIC): increment through the bus, decay, unknown kind skipped. `LF_Test_StandingPersist` (PERSISTENCE). A test asserting no replicated surface (the `LF_CrimeRecord` test `LF_Test_CrimeRecord.c` shows the form).
- **Size and risks:** S for the record, M with the first behaviour. Risk: someone adds a price modifier later; document the rule in `Feature.md` and validate the config.

---

### 10. Police career schema

- **Decided (2026-11-11):** licences stay bought with skill points (no exams). Police qualifications are granted by a senior officer at the station desk, recorded on the career record.
- **Goal:** ranks with pay and qualifications (driving, firearms, detective) as config rather than two hard-coded constants.
- **Overthrow reference:** `Data/OVT_PlayerData.c` `isOfficer` (persisted in `OVT_PersistedPlayer.isOfficer`, `OVT_PlayerManagerSerializer.c` line 37) is a boolean role flag only. **No close match.**
- **Our code to extend:** `Feature/Police/LF_PoliceRanks.c` (`PATROL = 0`, `SENIOR = 1`, `FineCap` line 95, `DutySecondsForNext` line 104 (hard-coded 3600), `IsRankUsable` line 86, `TitleKey` line 77, `RankMeets`, `Promote` line 273, `CollectKit` line 261 for kit per rank), `Feature/Police/LF_PoliceConfig.c` (`m_iArrestMinRank`, `m_iEscortMinRank`, patrol fine cap), `Feature/Police/LF_DutyPay.c` (pay per second), `Feature/Police/LF_PoliceApplicationRules.c` / `LF_PoliceApplications.c` (the application form), `Feature/Account/Persistence/LF_ProgressionRecord.c` (`m_iPoliceRank`, `m_iOnDutySeconds`, `m_iUnpaidDutySeconds`). The persisted rank is an int index, which is what keeps it config-friendly.
- **Data model:** rank stays an int index into a config ladder; the shipped `progression` record is not touched. Qualifications are new per-character state: sidecar payload `qualifications` (append): `LF_QualificationRow { m_sPlayerUid, m_sQualificationId, m_iGrantedAtMinute }`, one row per qualification held. Keep indexes stable (an index is never reused; add at the end), because a saved rank index must mean the same rank forever.
- **Config:** `crime` or a new `police` section. Per rank: `m_sId`, `m_sTitleKey`, `m_iFineCap`, `m_fPayPerSecond`, `m_iDutySecondsForNext`, `m_iArrestMinRank` kept as ranks, `m_aKit`, `m_aRequiredQualifications`, and for each qualification id, name key, prerequisites and a `m_sGrantAtStation` (the training desk). Core ships the two ranks as today's numbers; pack ships the real ladder.
- **Authority and network:** server only, already: `LF_PoliceRanks.AuthorizeArrest/AuthorizeFine` (lines 184-224) and `Promote` run on the server; `RequestSelfPromotion` always refuses. Qualification grant is a server action at a training desk; the client view of rank and quals goes in the existing account replication (check how `LF_PlayerAccount` publishes police rank).
- **World interface:** the duty desk and armoury (`LF_DutyAction`, `LF_ArmoryAction`) already exist; add a training instructor NPC or a range lane that grants firearms and a driving course that grants driving. Application form stays at the station clerk (`LF_PoliceApplyAction`).
- **Proof:** `LF_Test_PoliceRankTable` (LOGIC): config ladder replaces constants, fine cap and promotion time per rank, unknown rank clamps (the existing `LF_Test_PoliceRankPermissions.c` is the base). `LF_Test_PoliceQualifications` (LOGIC) and `LF_Test_PoliceQualificationsPersist` (PERSISTENCE). Manual: apply, train, gain rank, restart.
- **Size and risks:** M. Risk: `LF_PoliceRanks` constants are referenced across `Feature/Police`; replace them in one PR and keep `PATROL`/`SENIOR` as the first two ladder ids so existing saves read the same.

---

### 11. Zone rule sets

- **Goal:** rule overrides scoped to a zone (lawless, event, safe) beyond today's three.
- **Overthrow reference:** **No close match.** Overthrow's "zone" hits are QRF and deployment geometry (`Controllers/OccupyingFaction/OVT_QRF*.c`), not player rules.
- **Our code to extend:** `Feature/Moderation/LF_Zones.c`: `enum LF_ZoneRule` (line 21: SAFE, LAWLESS, NORMAL), `enum LF_ZoneShape` (line 28), `LF_ZoneConfig` (line 37; flags `m_bSafeBlocksPlayerDamage`, `m_bLawlessSuppressesWanted`, `Configs/Moderation/LF_Zones.conf`), `LF_ZoneArea` (line 103), `LF_Zones.RuleAt` line 284 (overlap order safe, lawless, normal), `Register`/`Unregister` 197-260, `LF_ZoneComponent` line 401 (a world entity, so a zone persists through the layer, not through a save). `Feature/Crime/LF_Murder.c` (reads the lawless flag), `Feature/Moderation/LF_IncidentLog.c` (stamps `m_sZone`), `HUD/LF_LawlessHud.c`.
- **Data model:** zones are placed entities, so no payload. Add `EVENT` to `LF_ZoneRule` at the END of the enum (existing values must keep their numbers, they are stable ids in prefab data). Replace the two booleans in `LF_ZoneConfig` with a per-rule table `array<LF_ZoneRuleSet>`: `{ rule, blocksPlayerDamage, suppressesWanted, allowsRobbery, allowsVehicleTheft, allowsWeaponDraw, fineMultiplier, copsEnter }`. A runtime event zone (staff-created) is a record only if it must survive restart; if so, one `LF_ZoneRecord { name, rule, shape, center, size, endsAtMinute }` in a `zones` payload (append, decide: "check"; default no).
- **Config:** `zones` section, already a named surface. Core: the table with SAFE, LAWLESS, NORMAL entries as today's booleans, EVENT entry off. Pack: the rule values and which zones exist (which stay in layers).
- **Authority and network:** server only. `LF_Zones.RuleAt(pos)` is the one answer for every caller; the client HUD already shows lawless (`LF_LawlessHud`); add a one-word event banner only. Staff create or end an event zone through the moderation path (`Feature/Moderation`), never from a client.
- **World interface:** the zone sign (`lawless_zone` catalog row, `Feature/Signs`), the border fence or lane marker; no menus.
- **Proof:** `LF_Test_ZoneRules` already exists (`Tests/LF_Test_ZoneRules.c`); extend it for the per-rule table and add `LF_Test_ZoneEvent` (WORLD). `LF_Test_ZoneRuleSetBackcompat` (LOGIC): a config with only the two old booleans behaves as before. Manual: walk into an event zone and rob.
- **Size and risks:** S for the table, M for each new rule's behaviour. Risk: a rule checked at only some call sites; make every consumer call `RuleAt` and the table, not the zone enum.

---

### 12. Planted crops and race records: decisions

- **Decided (2026-11-11):** neither is reserved in M3. Planted crops (M6) and race records (race runner) each become a sidecar payload when built.

### 13. Property record

- **Goal:** reserve a record for plot, owner, build stage and contents so M6 homes do not reshape saves.
- **Overthrow reference:**
  - `GameMode/Managers/OVT_OwnerManagerComponent.c` lines 13-14 and `SetOwner` line 78: owner and renter maps keyed by the building's position string (`GetOrigin().ToString(false)`), owner by persistent id. **Port the key idea**; the survey ("Housing ownership") says the same. `Persistence/Serializers/Components/OVT_RealEstateManagerSerializer.c` stores the key strings verbatim (header lines 1-12): no float round trip, no world query on load. **Adapt** this for stable keys.
  - `OVT_PersistedOwnership { persistentId, array<string> positions }` line 13, `OVT_PersistedWarehouseV2 { location, owner, isPrivate }` line 59, with stock moved out onto the building's storage ledger. **Adapt** the split: ownership in one record, contents in the storage payload.
  - `Components/Controller/OVT_RealEstateRequestComponent.c` `RpcAsk_BuyBuilding` lines 64-66 and `GetNearestBuilding(player.GetOrigin())` line 158: the server chooses the building from the caller's feet. **Port** for our purchase RPC. Money is taken on the server before `SetOwner`.
  - `Configuration/OVT_RealEstateConfig.c` lines 13-16: base rent per type, price multiplier by population and stability. **Ignore** the town-stability maths; use flat price per plot in pack config.
- **Our code to extend:** `Feature/Houses/LF_HouseManagerComponent.c` (houses by `m_sHouseIdentifier`), `LF_HouseSaveData.c` (`LF_HouseLockRecord`; its header says lock state is still in-session, so the house is NOT persisted yet: check), `LF_LockComponent.c`, `LF_KeyComponent.c`, `LF_DoorUserAction.c`, `Feature/Hideouts/LF_Hideout.c` (org-owned building record, the closest existing "property"), `Persistence/LF_PersistentStorageComponent.c` (contents).
- **Data model:** reserve `LF_PropertyRecord { m_iVersion, m_sPropertyId /*stable: editor-set house id or plot key*/, m_sOwnerCharacterId, m_sOrgId /*org-owned, empty for personal*/, m_iBuildStage, m_iPurchasedAtMinute, m_iRentPaidUntilMinute, m_sLayoutId /*which plot design or house variant*/ }`. Contents are not in the record: they ride `LF_PersistentStorageComponent` under key `property:<propertyId>` (existing route, `carried` payload). Reserve payload `property` (append, not written in M3 unless houses are bought in M3: check). Fold `LF_HouseLockRecord` into this record (lock flag) when M6 lands.
- **Config:** a `property` section (new): plot catalogue (price, rent, stages, build cost), core ships the structure, pack ships the plots. Rent and eviction rules: `m_iRentIntervalHours`, `m_iGraceHours`.
- **Authority and network:** server only. Buy: `RpcAsk_BuyProperty` sends no building id, the server resolves the nearest plot to the caller. A client reads its own property list in the existing account view; others see only the door.
- **World interface:** a for-sale sign on the plot and the door key; the agent's desk for rent and sale. Build stages are placed by materials delivered to the site, not a menu.
- **Proof:** `LF_Test_PropertyRecord` (LOGIC) for the reserved shape and round trip once built, `LF_Test_PropertyPersist` (PERSISTENCE). M3 proof is only that the reserved payload name is registered and an old save loads untouched.
- **Size and risks:** S to reserve, L to build (M6). Risk: saved keys that depend on world positions; keep stable ids, never raw coordinates, so a map edit does not orphan a property.

---

## Best Overthrow finds, as ranked for the port

1. `Persistence/Serializers/Components/OVT_PlayerManagerSerializer.c` (append-only versions, per-version back-fill, tolerant load) and the frozen `$type` lesson in `OVT_RealEstateManagerSerializer.c`: the rule behind every record change above.
2. `Data/OVT_StorageLedger.c`, `Data/OVT_StorageRules.c` (`ResolveAutoCapacity`, `TransferLedgerLine`) and `OVT_StorageComponentSerializer.c` (capacity never persisted): lockers, vehicle compartment and cargo tuning.
3. `GameMode/Managers/OVT_SkillManagerComponent.c` (`TryStartTradeXPCooldown`, server re-derived level cap, unknown-skill skip) plus `Configuration/OVT_SkillsConfig.c`: XP award hygiene for skills by doing.
4. `GameMode/Managers/OVT_OwnerManagerComponent.c` and `OVT_RealEstateRequestComponent.c` (`GetNearestBuilding` from caller's feet, owner by position string): property record and purchase RPC.
5. `OVT_EconomyManagerComponent.GetSellPriceAtOffset` (marginal bulk pricing) and `CheckUpdate` hour latches: patterns only; Overthrow has no stored market price, ranks, wars, reputation, wear or crops.

## B. Framework systems (M5 Framework track)


Conventions used below. Overthrow paths are relative to `Scripts/Game/` in the Overthrow repo (commit `9dbb8fad`) (written `OVT:`). Our paths are relative to `addons/LifeFramework/Scripts/Game/` (written `LF:`). "Port" means copy with the Overthrow attribution header (`docs/ecosystem.md` section 5, `THIRD_PARTY.md`) and rename to `LF_`. "Adapt" means we reuse the shape and write our own code. Line ranges were read, not guessed; where I only skimmed a file I say "check". Save payloads go at the end of `RegisterPersistencePayloads` (`LF: Persistence/Serializers/LF_PersistenceComponentSerializer.c:94`, currently 25 payloads ending `storeunpaid`), and each new payload is one version bump.

Two facts shape the whole section.

1. Overthrow's economy is abstract. Stock is a count per good per shop, and a player's "storage" is a ledger (`OVT_StorageLedger`), not entities. Ours is physical (real stacks, real containers, cash as `MoneyStack`). So Overthrow helps with formulas, config shape and lifecycle, not with data models for goods or lockers.
2. Overthrow is a campaign war mod. Capture, objectives, deployments and occupation do not map to gangs or the player-vs-player drama we want. Only their state-machine and config patterns are worth reading.

---

### Market price curve service and price board

- **Goal:** Each tradable good has a per-server price that falls as players sell, is clamped to floor and ceiling, and recovers only through a visible world buyer or a timed market event.
- **Overthrow reference:**
  - `OVT: GameMode/Managers/OVT_EconomyManagerComponent.c:708-735`, `GetSellPriceAtOffset`. Price = base + (1 - stock/max) * 10% of base, with a bulk-sale seam: unit i is priced as if i units had already entered stock, so dumping a truckload rides the curve down. Adapt: the "offset" idea is the right way to price a stack sale; our curve is driven by units sold, not town stock.
  - `OVT: ...EconomyManagerComponent.c:~948-960`, `TOWN_STOCK_BUY_CAP_MULTIPLIER` and `CanTownAbsorbStock`: a hard cap beyond which shops refuse to buy. Adapt as the floor (we stop paying less rather than refuse).
  - `OVT: ...EconomyManagerComponent.c:443-520` (`ReplenishStock`) and `522-588` (`UpdateShops`): a daily top-up and an NPC-demand drain on the clock. Adapt only if we want NPC demand to move prices; the plan says no self-recovery, so ignore the replenish half.
  - `OVT: GameMode/Systems/OVT_TownModifierSystem.c:1-268` (`OnTick`): modifiers with ids and expiry, ticked and rebuilt. Adapt for the single market-event slot (multiplier plus duration, expires on tick).
  - `OVT: Configuration/OVT_PricesConfig.c:1-78`: a rule list matched by type, mode and name substring. We already have the same shape (`LF_ShopPriceRule`, `LF_ShopPriceResolver`), so ignore.
  - No close match for "family links" (one good lifts related goods) or a price board.
- **Our code to extend:** `LF: Components/InventorySystem/LF_TraderManager.c` (`LF_TraderManagerComponent.GetUnitValue`, the trader's quote seam, tested in `LF_Test_Trader.c`); `LF: Feature/Shop/LF_ShopPriceResolver.c`, `LF_ShopPricesConfig.c`, `LF_ShopManagerComponent.c` (`GetSellPrice` at line 124); `LF: Feature/Crime/LF_FenceRules.c` (fence prices should also read the curve, check); `LF: Feature/Signs/LF_SignComponent.c` and `LF_SignCard.c` for a world board whose text is drawn on the prop; `LF: Feature/Shop/LF_ShopEvents.c` for the bus event; `LF: Feature/Money/LF_Ledger.c`. New: `Feature/Market/LF_PriceCurve.c` (pure rules), `LF_MarketBook.c` (state), `LF_MarketEvents.c`, `LF_PriceBoardComponent.c`.
- **Data model:** New payload `market`, one record per good id: `version`, `currentPrice` (int), `unitsSoldSinceReset` (int), `lastWorldBuyTime` (int, unix seconds), plus one event record `eventGoodIds`, `eventMultiplierPct`, `eventEndsAt`. Floor, ceiling and curve are config, never saved, so tuning needs no migration. Persist the price, not a derived number, so a restart reads the same value players saw.
- **Config:** `Configs/Market/LF_Market.conf` (core default: three neutral goods, a gentle curve, no family links, event off). Fields per good: `base`, `floor`, `ceiling`, `dropPerUnit` (pct of base), `recoverPerHour` (0 means none), `familyId`, `familyLiftPerUnit`, `worldBuyer` (bool). Global: event interval, event multiplier range, event duration. The pack (`LF_Everon1989`) lists which goods exist and their numbers. Setting `dropPerUnit` to 0 gives today's fixed prices.
- **Authority and network:** The server computes every price and applies every sale; the client never sends a price. A quote and a sale go through the existing trader path. The board does not stream live: reading it asks the server (`RpcAsk_ReadPriceBoard`) and gets one `RpcDo_ShowPriceBoard` with the rows, the same way `LF_WantedBoard` answers per read (`LF: Feature/Police/LF_WantedBoard.c:1-14`). No `RplProp`, so no JIP state.
- **World interface:** A chalkboard prop at each trader, using `LF_SignComponent`'s printed-face technique, plus a "price of the week" line when an event is running. Boosted goods carry a marker on the board. No app, no menu.
- **Proof:** `LF_Test_PriceCurve` (LOGIC; floor and ceiling clamp, bulk sale of N units prices each marginally, family lift, zero drop equals fixed price; red-proof: break the clamp). `LF_Test_MarketBookPersist` (PERSISTENCE; save, reload, same price and the same event with its remaining time). `LF_Test_MarketTrader` (WORLD; a sale through the trader moves the price and the cash paid matches the quoted per-unit sum). Manual: sell 10 stacks, read the board, restart, read again; see the profit-loop check noted against #686 (a buy-low sell-high loop must not exist).
- **Size and risks:** M. Main risk is an exploit loop between buy price and sell curve (shop stock buys are fixed while the curve only moves on selling, so confirm a good can never be bought cheaper than it sells).

---

### Skill XP and unlock gates

- **Decided (2026-11-11):** a job level is the skill level; no second XP track.
- **Goal:** Each character earns XP per skill by doing the work; each skill level unlocks one thing (a job tier, a station, a chain step), and a refusal names the missing skill and level.
- **Overthrow reference:**
  - `OVT: GameMode/Managers/OVT_SkillManagerComponent.c:111-135` (`AddSkillLevel`): the server re-derives spendable points and the level cap, then stores `player.skills[key]`. Adapt the server re-check; we do not spend points.
  - `OVT: ...OVT_SkillManagerComponent.c:271-306` (`TryStartTradeXPCooldown`, `OnPlayerBuy/Sell`): a per-player cooldown on trade XP so a buy-then-sell loop at one counter cannot farm XP. Port the idea (our `LF_PlayerLevelComponent.AddExperience` has no such guard).
  - `OVT: ...OVT_SkillManagerComponent.c:332-372` (`GiveXP`, `TakeXP`): server writes then streams. Adapt.
  - `OVT: Configuration/OVT_SkillsConfig.c:1-41` and `GameMode/Systems/SkillEffects/OVT_GivePermissionSkillEffect.c:1-17`: a skill is a list of levels, each level a list of effect objects, one effect being "grant a permission string". Adapt: our gate is data (a string `skillId:level`), not effect objects.
  - Ignore the point-buy model; the plan says no perk tree and no XP popups.
- **Our code to extend:** `LF: Feature/Level/LF_PlayerLevelComponent.c` (one global level and XP, `AddExperience` at line 51); `LF: Feature/Jobs/LF_PlayerJobComponent.c` (per-job `m_mJobLevels` and `m_mJobExperience`, lines 20-21, `AddExperience` at 400); `LF: Feature/Jobs/LF_CivilianJob.c` (the only persisted career; `RestoreCareer` at line 95); `LF: Feature/Account/LF_Progression.c` and `Persistence/LF_ProgressionRecord.c` (record keyed by uid, own `VERSION = 1`); `LF: Feature/Chains/LF_ChainGate.c` (`LF_EChainGate` with `LEVEL`, the template for a named refusal). New: `Feature/Skills/LF_Skills.c`, `LF_SkillsConfig.c`, `LF_SkillsEvents.c`.
- **Data model:** In `LF_ProgressionRecord`, append `ref map<string,int> skillXp` as two parallel arrays (`skillIds`, `skillXp`), and bump `LF_ProgressionRecord.VERSION` to 2; older records read as empty. Level is derived from XP by the config curve, never stored. This is an append to an existing payload, so it needs no new payload name, but check whether `validate-persistence-version` wants a save-version bump anyway.
- **Config:** `Configs/Skills/LF_Skills.conf`: per skill, `id`, `nameKey`, an XP-per-level table (level thresholds), a per-source XP award table (`source -> xp`), a trade/farm cooldown seconds, and `unlocks` rows (`level`, `gateKind`, `gateId`). The core ships one neutral skill; the pack names the real ones.
- **Authority and network:** Server only. XP comes from bus events already published (`LF_OnJobCompleted`, chain step done, gather, `LF_OnCrimeCommitted`); a subscriber awards it. A skill level is replicated to its owner only as a toast and in the character's own screen; no one else needs it. Gate checks run on the server inside the action that is being gated.
- **World interface:** No menu. The refusal toast names "needs Farming 3". A trainer NPC or an instructor sign can state the table (a sign, `LF_SignCard`).
- **Proof:** `LF_Test_SkillCurve` (LOGIC; thresholds, level from XP, cooldown blocks a repeat award; red-proof: remove the cooldown). `LF_Test_SkillGate` (LOGIC; gate refusal names skill and level, wired through `LF_ChainGate`). `LF_Test_SkillPersist` (PERSISTENCE). Manual: do runs until the unlock, see the refusal clear, restart, still unlocked.
- **Size and risks:** M. Main risk is double-counting with the job level that already exists (`LF_CivilianJob` career XP). Decide whether a job level IS a skill level (migrate it into the skill record) or whether they stay separate; the plan wants one unlock per job level, so fold them.

---

### Practical licence exams (course runner)

- **Decided (2026-11-11):** dropped. Licences stay bought with skill points. The course runner is built inside the race runner instead.

### Event shell (preconditions, prep, hot phase, cooldown, reward)

- **Goal:** One reusable state machine runs world events: it waits for world-state conditions, opens a prep phase, then a hot phase in which a zone's rules lift, then a cooldown, with a reward only for those who hunted.
- **Overthrow reference:**
  - `OVT: GameMode/Objectives/OVT_ObjectivePhase.c:1-93` and `OVT_ObjectiveConfig.c:23-149`: a plan is an ordered list of phases, each with a stable name, a cadence, an idle timeout, and a module bag. Port the shape (see "Adapt" below).
  - `OVT: GameMode/Objectives/Modules/OVT_BaseObjectiveModule.c`, `OVT_BaseObjectiveConditionModule.c:30` (`Evaluate`), `OVT_BaseObjectiveOperationModule.c:40` (`TryAct`), `OVT_BaseObjectiveAbortModule.c:33` (`ShouldAbort`): three module roles. Conditions are AND-ed to advance, operations run in authored order with the first that acts using up the cadence, aborts are OR-ed and end the event. This is a good, proven decomposition for "preconditions, hot phase, abort".
  - `OVT: GameMode/Objectives/OVT_ObjectiveInstance.c:30-46` and `:126-283`: the persisted state is a typed key bag (int and vector bags plus an asset registry), written by one serializer, so a module never needs its own save format. The phase NAME is the save key, not an index, so phases can be added without breaking saves.
  - `OVT: GameMode/Objectives/OVT_ObjectivePhaseRules.c:1-422`: pure static rules (gates, clocks, `TickDown`) so the Logic tier can test the rules without a world. Port that habit.
  - Adapt, do not port wholesale: the code is 7,259 lines, tied to occupation and factions, and several concepts (CloneModule by hand, deployment hooks) exist only for AI armies.
- **Our code to extend:** `LF: Feature/Moderation/LF_Zones.c` (`LF_Zones.RegisterCircle` at 238 and `Unregister` at 250: the hot phase lifts rules by registering a LAWLESS circle and removing it after; publishes `LF_OnZoneChanged`); `LF: Core/Events/LF_EventBus.c` (publish `LF_OnEventPhase`); `LF: Feature/Comms/LF_ServiceBoard.c` and `LF_ServiceCalls.c` (announce on the dispatch board); `LF: Feature/Police/LF_Dispatch.c` (`Report`); `LF: Feature/Investigation/LF_EvidenceLocker.c` and `LF: Feature/Crime/LF_HeistVault.c` (reward containers); `LF: Feature/Chains/LF_ChainSites.c` (site ids). New: `Feature/Events/LF_EventShell.c`, `LF_EventDef.c`, `LF_EventPhase.c`, `LF_EventPrecondition.c`, `LF_EventsEvents.c`.
- **Data model:** New payload `worldevents`: per event id `version`, `phaseName` (string, the key), `phaseStartedAt`, `cooldownUntil`, an int bag for module state (counts, ids). Phase names are the persistence key as in Overthrow. A restart in a hot phase resumes it if its time remains, or moves to cooldown; never re-opens the event.
- **Config:** `Configs/Events/LF_Events.conf`: per event `id`, `preconditions` (min players, min police on duty, hour range, a world flag), `prepSeconds`, `hotSeconds`, `cooldownSeconds`, `zoneName` and `zoneRule`, `rewardTable`, `announceKeys`. Core ships the shell and one neutral example event; the pack defines the prison break, transport and airdrop equivalents.
- **Authority and network:** Server only; a 1 s or 5 s timer tick like `LF_ChainTrace`. Clients learn of an event through the announcement (a toast and a board entry) and through the zone's own replicated entity, not through a stream of event state. No client RPC.
- **World interface:** Each event is a place: a vault, a convoy route, a drop zone. The announcement is a siren, a radio line and a board entry; the reward is a physical crate or bag. No event menu.
- **Proof:** `LF_Test_EventShell` (LOGIC; phase order, preconditions block, cooldown blocks a restart of the cycle, abort path; red-proof: let a precondition pass when it should not). `LF_Test_EventZone` (WORLD; hot phase registers a LAWLESS zone and cooldown removes it). `LF_Test_EventPersist` (PERSISTENCE; resume in the right phase). Manual: run the example event with two players, with and without police on duty.
- **Size and risks:** L. Main risk is scope creep: the shell must stay data-driven and small. Also check `validate-registries` for the bus events and keep them in `LF_EventsEvents.c`.

---

### Declared organisation wars

- **Goal:** One organisation declares war on another at an NPC, the other has a window to answer, both are announced, and kills between them are tagged in the incident log as war kills.
- **Overthrow reference:** No close match. Overthrow's war is the resistance against the occupying faction, with no player-to-player declaration. Ignore. The only portable idea is that a faction relationship is a state read by systems that decide whether killing is free, which `LF_Zones` already models for places.
- **Our code to extend:** `LF: Feature/Organizations/LF_Organization.c` (`LF_OrganizationRecord` at line 75, `LF_OrganizationManager` at 502); `LF: Feature/Organizations/LF_OrgRules.c` (`RIGHT_INVITE/KICK/WITHDRAW/BUY` bits at 66-69; add `RIGHT_DECLARE_WAR`); `LF: Feature/Organizations/LF_OrganizationDeskAction.c` (the NPC/desk entry); `LF: Feature/Organizations/LF_OrgInvites.c` (a session offer with a 2 min lapse is the model for the answer window, `OFFER_MS` at `LF_OrgRules.c:63`); `LF: Feature/Moderation/LF_IncidentLog.c` (`Append`, subscribers at lines 355-461) and `LF_IncidentFlags.c`; `LF: Feature/Moderation/LF_Zones.c`; `LF: Feature/Crime/LF_Murder.c` (a war kill must not count as murder). New: `Feature/Organizations/LF_OrgWars.c`, `LF_OrgWarEvents.c`.
- **Data model:** New payload `orgwars`: `version`, rows of `warId`, `declarerOrgId`, `targetOrgId`, `state` (declared, accepted, ended), `declaredAt`, `acceptedAt`, `endsAt`, `cooldownUntil`. Key by organisation id, not name (a name can be reused). Also a rights bit; the existing rank records carry named booleans (`m_bCanWithdraw`...), so adding `m_bCanDeclareWar` is an append to `LF_OrganizationRankRecord` with the record `VERSION` bumped (check how `LF_OrganizationRecord.m_iVersion` is read).
- **Config:** `Configs/Organizations/LF_OrgWars.conf`: declare fee, answer window seconds, war length, post-war cooldown, min members per side, whether war kills count as murder (default off), whether the incident log tags them.
- **Authority and network:** Server only. Declare is `RpcAsk_DeclareWar(targetOrgId)` from the desk, checked against the declarer's rights, fee in cash, cooldown and members. The answer is `RpcAsk_AnswerWar`. Announcement through `LF_NotificationManagerComponent`, the service board, and a bus event. A kill between two members of opposed organisations during an active war is tagged in the incident log by a bus subscriber.
- **World interface:** Declare and answer at the registry desk (the same NPC as `LF_OrganizationDeskAction`), plus a war notice pinned on a public board and in the gazette. Nothing in a menu on a phone.
- **Proof:** `LF_Test_OrgWarState` (LOGIC; legal transitions, window lapse means no war, cooldown). `LF_Test_OrgWarKillTag` (WORLD or LOGIC; a kill between warring members is tagged, between others is not and still counts as murder). `LF_Test_OrgWarPersist` (PERSISTENCE). Manual: two clients in two organisations, declare, answer, kill, read the incident log, restart mid-war.
- **Size and risks:** M. Main risk is rule interaction: war kills must not trip `LF_Murder`, `LF_Zones` SAFE refusal still applies inside a safe zone (a war does not lift a safe zone), and wanted must not rise. Needs the crime-catalog decisions from #685.

---

### Contested sites with rent

- **Decided (2026-11-11):** minimum members present to score, owner-configurable.
- **Goal:** An organisation holds a site by being the only one standing in it; held time earns points per hour, points decay if the site is left, and the holder receives rent.
- **Overthrow reference:**
  - `OVT: GameMode/Deployments/OVT_ResistancePresence.c:1-389`: the one place that answers "is a friendly force holding this ground", by asking the world for living characters of a faction in a radius, not by listing kinds. Adapt: ask for living player characters grouped by organisation, instead of by faction. Its header comment (don't use it for visibility) is worth keeping.
  - `OVT: GameMode/Deployments/Modules/OVT_RadioTowerCaptureBehaviorDeploymentModule.c` and `Controllers/OccupyingFaction/OVT_BaseControllerComponent.c:165-190` (`SetControllingFaction`, which swaps the affiliated faction and raises an event): the handover is a faction swap on the entity. Adapt; do not use faction affiliation for organisations.
  - `OVT: GameMode/Systems/OVT_TownModifierSystem.c:1-268`: ticked, expiring state, the model for decay.
  - No close match for rent per hour or points that decay; written from scratch.
- **Our code to extend:** `LF: Feature/Hideouts/LF_Hideout.c` (`LF_HideoutRecord` line 10: `m_sHideoutId`, `m_sOrganizationId`, `m_bLocked`; the lease `LF_OrgHideoutLease`); `LF: Feature/Organizations/LF_Organization.c` (`m_iFund`); `LF: Feature/Organizations/LF_OrgLedger.c`; `LF: Feature/Moderation/LF_Zones.c` (the site is also a LAWLESS zone while held, optionally); `LF: Core/Events/LF_EventBus.c`. New: `Feature/Sites/LF_ContestedSite.c`, `LF_SiteHold.c`, `LF_ContestedSiteComponent.c`, `LF_SitesEvents.c`.
- **Data model:** Extend the hideout or add payload `sites`: per site `version`, `siteId`, `holderOrgId`, `points` (int), `lastTickAt`, `lastRentAt`. The plan says hold time and points belong on the hideout record in M3; I recommend a separate payload keyed by `siteId` so a non-hideout site works, with a field-level link to the hideout id. Rent is paid from a pool (config), not minted from nothing; decide pool versus mint (check against the "no printing money" rule).
- **Config:** `Configs/Sites/LF_Sites.conf` (core default: none). Per site: `radius`, `pointsPerHour`, `decayPerHour`, `captureThreshold`, `rentPerHour`, `maxRentBanked`. The pack supplies the real sites.
- **Authority and network:** Server only, on a 5-10 s sweep like `LF_ChainTrace`. The marker prop shows the holder's name from a replicated string on the site entity (a single short `RplProp`; `Replication.BumpMe()` only on change of holder). Rent is withdrawn at the site's strongbox in cash, not paid into an account.
- **World interface:** A flag or sign at the site showing the holder; a strongbox at the site holds banked rent that a member of the holder takes by hand.
- **Proof:** `LF_Test_SiteHold` (LOGIC; one org alone gains points, two orgs present means no gain, decay when empty; red-proof: let two orgs gain). `LF_Test_SitePersist` (PERSISTENCE; holder and points survive). Manual: two groups at one site with a dedicated or listen server.
- **Size and risks:** L. Main risk is balance and AFK holding (an organisation could stand a member there forever); add a presence check that needs an alive, active character and a cap on banked rent.

---

### Staged break-in with defenders

- **Goal:** A heist needs two workers at two points at once while a defender garrison resets if the break-in stops, and a scout can read how far along the work is without opening it.
- **Overthrow reference:**
  - `OVT: GameMode/Deployments/Modules/OVT_NoPlayersNearbyConditionDeploymentModule.c:1-121`: a creation gate versus a runtime gate, with a header explaining that a distance test at runtime would delete a garrison the moment a player arrives. The principle (a defender reset must not fire on approach) transfers. Adapt, do not port.
  - `OVT: GameMode/Deployments/Modules/OVT_PlacedInfantrySpawningDeploymentModule.c` (670 lines) and `OVT_DeploymentComponent.c` (656 lines): spawn defenders from a config. Heavy and AI-army oriented; check whether we have NPC defenders at all (`LF: Core/Npcs/` has only a base entity and damage).
  - No close match for two simultaneous workers or a fill-level hint.
- **Our code to extend:** `LF: Feature/Crime/LF_Breach.c` (`LF_BreachRecord`, `Arm`, `Advance`; a configured charge opens an `LF_LockComponent`); `LF: Feature/Crime/LF_BreachAction.c`, `LF_BreachTargetComponent.c`; `LF: Feature/Crime/LF_BankVaultComponent.c`, `LF_HeistVault.c`; `LF: Feature/Chains/LF_StationProgress.c` (progress belongs to the station, more hands finish sooner: reuse for the fill level); `LF: Feature/Police/LF_SiteAlarmComponent.c` and `LF_Dispatch.c`; `LF: Feature/Houses/LF_LockComponent.c`; `LF: Feature/Investigation/LF_EvidenceLocker.c` (the evidence-locker target). The multi-stage heist is tracked as #662 (idea) in `docs/roadmap/m5-outline.md:116`.
- **Data model:** Extend `LF_BreachRecord` (version 1, fields `m_sTargetId`, `m_bOpen`) by appending per-stage progress ints; keep it minimal (stage index, per-point progress). Defender state is NOT saved; defenders respawn from the site on a restart.
- **Config:** On the target component: `stagePoints` (entity names of the two or more work points), `workSecondsPerPoint`, `defenderCount`, `defenderResetSeconds`, `hintLevels`. Core default: a single stage, no defenders (so `LF_Breach` behaviour is unchanged).
- **Authority and network:** Server owns progress and the reset. Progress drops (or resets) when only one point is staffed for longer than a grace period. The fill-level hint is a prop state (a gauge or lamp on the target) driven by a single replicated int on the target, bumped only on level change, which also serves a late joiner.
- **World interface:** Two work points on the target, one hands-on action each (a held action, like `LF_ChainStationAction`'s work mode). The hint is a visible gauge, not a menu.
- **Proof:** `LF_Test_BreachStaged` (LOGIC; both points needed, one point stalls, reset after grace). `LF_Test_BreachHint` (WORLD; the level int changes the gauge state). Manual: two players on a heist with a defender present.
- **Size and risks:** L. Main risk is defender AI: we have no garrison system, and a garrison from scratch is big. Ship stage logic first and keep defenders as an optional second step, or use player or scripted guards.

---

### Garage slots and towing

- **Goal:** Each character has a capped number of garage slots (the next slot costs more), and a vehicle abandoned in the world is towed to the impound lot after a delay.
- **Overthrow reference:**
  - `OVT: GameMode/Managers/OVT_VehicleManagerComponent.c:749-870` (`HasLockedVehicles`, `ProcessOfflineTimers` at 780, `DespawnPlayerLockedVehicles` at 823): an offline-timer that takes an owner's locked vehicles out of play after a delay, hidden not destroyed. Adapt the timer-then-remove-but-keep-the-record shape for tow-after-abandonment.
  - `OVT: ...OVT_VehicleManagerComponent.c:1268-1303` (`CaptureVehicleRecord`) and the survey's `OVT_PersistedPlayerVehicle`: one append-only vehicle record. We already have the equivalent (`LF_VehicleRecord`).
  - `OVT: ...OVT_VehicleManagerComponent.c:1698+` (`InitialVehicleCleanup`): re-index vehicles after a world load. Compare with `LF_VehicleOwnership.ImpoundStreetVehicles`.
  - `OVT: Components/OVT_ParkingComponent.c:1-232`: parking spots as child data on a building, with an obstruction check. Adapt for impound pads (we already have `LF_ImpoundDeskComponent` with a pad id).
  - Ignore `ReserveVehicle` (line 875), per the survey. No close match for slot caps, slot price or towing; `docs/overthrow-survey.md` already says the same.
- **Our code to extend:** `LF: Feature/Vehicles/LF_Garage.c` (`LF_GarageConfig.m_iImpoundFee` at 16-60, `LF_Garage.CanPark` at 83, `LF_ImpoundDeskComponent` at 347, `LF_ImpoundCollectAction` at 370); `LF: Feature/Vehicles/LF_VehicleOwnership.c` (`LF_VehicleRecord` at 6, `ImpoundStreetVehicles` at 275, `IsImpounded` at 301, `CollectFromImpound` at 386); `LF: Feature/Vehicles/LF_VehicleGarageComponent.c` (`SharesGarage`); `LF: Feature/Crime/LF_StolenVehicles.c`; `LF: Configs/Vehicles/LF_Garage.conf`. The #688 vehicle-price fields and the #690 cleanup service are M3 work this should pair with.
- **Data model:** Add to `LF_VehicleRecord` (version 1, fields at lines 8-18) a `m_iTowedAt` (unix seconds, 0 for none) and `m_iInspectedAt` (shared with the inspection item below); a per-owner slot count is derived by counting stored vehicles, but a bought "extra slots" count is a new persisted int per uid on the progression record or a small payload `garageslots`. Today impound is derived and "no save field"; towing needs a persisted timestamp so the delay survives a restart.
- **Config:** Extend `LF_GarageConfig`: `baseSlots`, `slotPriceStart`, `slotPriceStep`, `maxSlots`, `towAfterMinutes`, `towFee` (can differ from `m_iImpoundFee`), `towExemptNearGarage` (radius).
- **Authority and network:** Server only. A tow is a server sweep that finds an owned, unattended, unlocked or abandoned vehicle in the world, records `towedAt`, deletes the entity, and leaves the record at the impound lot exactly as an impound does. Buying a slot is an action at the garage with carried cash (`RpcAsk_BuySlot`). The ledger card (`LF_GarageLedger`) shows slots used and the next slot price.
- **World interface:** The existing garage use point and impound desk. A "slots" line on the garage ledger card; a sign at the garage; a tow notice (toast and an entry on the ledger) when a car is taken.
- **Proof:** `LF_Test_GarageSlots` (LOGIC; cap, price step, refusal names the cap). `LF_Test_VehicleTow` (WORLD; abandoned car is towed after the delay, recoverable at the desk for the fee; red-proof: tow a car with its owner seated). Extend `LF_Test_VehicleImpound` and `restart/vehicle-street-impounded`. Manual: leave a car, wait the tow delay, collect it.
- **Size and risks:** M. Main risk is towing a car someone is using or a police or job vehicle; the sweep must skip occupied, keyed-recently, job and stolen-hot vehicles (the last belongs to the chop shop path).

---

### Personal locker

- **Decided (2026-11-11):** rent lapse moves contents to a lost-and-found box.
- **Goal:** A character rents a locker once at a site; it holds real items privately, survives death and restart, and only its renter can open it.
- **Overthrow reference:**
  - `OVT: Components/OVT_StorageComponent.c:1-562` and `Data/OVT_StorageRules.c:1-185`, `Data/OVT_StorageLedger.c`: a holder with a capacity mode (auto, unlimited, fixed, none), the contents server-only and the resolved capacity replicated. Adapt only the capacity-mode enum and the "contents never leave the server" rule. Ignore the ledger model itself: our items are entities.
  - `OVT: GameMode/Managers/OVT_OwnerManagerComponent.c` (survey: `SetOwner` keyed by persistent id): ownership keyed by a stable id. Adapt as the renter key.
- **Our code to extend (the pattern already exists):** `LF: Feature/Hideouts/LF_HideoutStashStorageManagerComponent.c` (lines 14-56: `AllowsUser`, `AllowsAccessFrom`, `HandleAccessValidation` refuse a non-member in and out); `LF: Feature/Hideouts/LF_HideoutStashStorageComponent.c` (`CanStoreItem`); `LF: Feature/Hideouts/LF_HideoutStashOpenAction.c` and `LF_HideoutStashPlayerController.c` (open storage asks the server); `LF: Persistence/LF_PersistentStorageComponent.c` (contents saved under a stable key, `SLOT_PLACED`, rides the existing "carried" payload, no version bump); `LF: Feature/Houses/LF_KeyComponent.c`. New: `Feature/Lockers/LF_LockerBank.c` (rental records), `LF_LockerStorageManagerComponent.c` (a copy of the stash manager keyed by renter), `LF_LockerRentAction.c`.
- **Data model:** New payload `lockers`: per locker `version`, `lockerKey` (stable, e.g. `site:12`), `renterUid` (character record key from `LF_Utils.GetPlayerUID`), `paidUntil` (unix seconds, 0 for a one-time rent). Contents are not here; they ride `LF_PersistentStorageComponent` under the same `lockerKey`. A death keeps the locker because the storage is a placed container, not carried.
- **Config:** `Configs/Lockers/LF_Lockers.conf`: `rentPrice`, `rentPeriodDays` (0 means one-time), `gracePeriodDays` before contents are cleared or moved to a lost-and-found, `weightCapacity` (set on the prefab as in the stash's 200 kg). The pack places the lockers and sets the price.
- **Authority and network:** Server only. Rent by `RpcAsk_RentLocker(lockerRplId)` (cash from carried money via `LF_MoneyUtils`); open through the existing stash request path. Reject any move by a non-renter in `HandleAccessValidation` as the stash does. Nothing replicates except the vanilla inventory.
- **World interface:** A bank of locker props at a station, with a name plate and a rent action on each; the locker's open action is the stash-style "Open storage".
- **Proof:** `LF_Test_LockerRent` (LOGIC or WORLD; rent, double rent refused, non-renter refused in and out, short cash refused). `LF_Test_LockerPersist` (PERSISTENCE through `e2e/pass2-verify-load`, like `hideout/stash-save`). Manual: rent, store, die, respawn, restart, open.
- **Size and risks:** S to M. Main risk is a lapsed rent (what happens to the contents); decide "contents move to a lost-and-found box" versus "frozen until paid" before the schema ships.

---

### Vehicle inspection and hidden compartments

- **Goal:** Police can check a sticker that shows a vehicle was inspected, and a vehicle can hide goods in a compartment that a quick search misses.
- **Overthrow reference:** No close match. Overthrow's vehicle storage is an abstract ledger with a capacity (`OVT: Components/OVT_StorageComponent.c`), with no inspection, sticker or hidden space. Ignore.
- **Our code to extend:** `LF: Feature/Police/LF_VehicleSearchAction.c` and `LF: Feature/Chains/LF_ChainSearch.c` (`SearchVehicle` at 236, `Collect` at 137, `FindChance` at 20: goods are found by a roll that grows with load; a hidden compartment lowers the roll for goods inside it, so give `Collect` a "visible vs hidden" split); `LF: Feature/VehicleLock/LF_VehicleLockComponent.c`, `LF_OpenVehicleStorageAction.c`, `LF_VehicleStorageAccessControl.c`; `LF: Feature/LicensePlate/` (the sticker could sit beside the plate, check its files); `LF: Feature/Vehicles/LF_VehicleOwnership.c` (`LF_VehicleRecord`). New: `Feature/Vehicles/LF_VehicleInspection.c`, `LF_InspectionAction.c`, a hidden storage component on the vehicle prefab.
- **Data model:** On `LF_VehicleRecord`: `m_iInspectedAt` (unix seconds). Hidden-compartment contents ride `LF_PersistentStorageComponent` under a key derived from the vehicle id (check: that component expects a placed entity; a vehicle moves, but "keep the key when the entity moves" is already its contract, so this should work). The compartment upgrade (owned or not) is one bool on the record.
- **Config:** `Configs/Vehicles/LF_Inspection.conf`: inspection validity days, who may inspect (on-duty police, or a mechanic job), fee, `hiddenFindChanceMultiplier` (e.g. 0.25), compartment capacity.
- **Authority and network:** Server only; an inspection is a police action that sets `m_iInspectedAt` and replicates the sticker state as one int on the vehicle (or a prop swap). The search roll runs on the server as now.
- **World interface:** A sticker on the windscreen (prop) that a police officer reads by looking; a compartment opened by a held action at a specific spot on the vehicle (boot panel). No menu.
- **Proof:** `LF_Test_InspectionSticker` (LOGIC; validity window, expiry). `LF_Test_HiddenSearch` (LOGIC; goods in the compartment are found less often with the same roll; red-proof: remove the multiplier). Manual: stash goods, get searched, compare.
- **Size and risks:** M. Main risk is that goods hidden in a compartment need an inventory component on every vehicle prefab we use (a prefab edit across all vehicles; check `enfusion-blast-radius`). The plan also says this belongs with the tuning page, which we do not ship (no tuning), so reduce scope to sticker plus compartment.

---

### Crew robbery roles

- **Goal:** A till robbery uses a lookout and a holder with a police response window, instead of one player clicking one action.
- **Overthrow reference:** No close match. `OVT: Components/Controller/OVT_IllegalActionComponent.c` exists (illegal action detection, check) but it feeds the occupation's wanted level; ignore.
- **Our code to extend:** `LF: Feature/Crime/LF_RobAction.c` (a single till robbery: `m_iMinPoliceOnDuty`, `m_fCooldown`, payout range, `ResolvePayout`, `LF_RobCooldown`); `LF: Feature/Crime/LF_RobCooldown.c`; `LF: Feature/Moderation/LF_HoldUp.c` (the holder aims at the clerk: `LF_HoldUp.Attempt`); `LF: Feature/Police/LF_Dispatch.c` and `LF_SiteAlarmComponent.c` (alarm and dispatch), `LF: Feature/Comms/LF_ServiceCalls.c`; `LF: Feature/Crime/LF_BankVaultComponent.c` (gating and bags, same shape). New: `Feature/Crime/LF_CrewRob.c` (roles, state, window), `LF_CrewRobEvents.c`.
- **Data model:** None persisted (a robbery is session state). Cooldowns that must survive a restart ride the existing till cooldown, check whether that persists today (`LF_RobCooldown` is runtime only).
- **Config:** On the till: `lookoutRequired` (bool), `holderRadius`, `lookoutRadius`, `responseWindowSeconds` (how long police have before the payout), `payoutScaleWithCrew`, `minCrew`. Core default: crew of 1 (today's behaviour).
- **Authority and network:** The server starts a robbery only when a holder has the clerk held up and a second player stands in the lookout radius; the lookout role is simply "in radius and alive for the window", detected on the server. Dispatch fires at start; payout is delayed by the response window, so the window is real.
- **World interface:** The till and the clerk; the lookout marker is a floor decal or a named doorway. The clerk reacts (hands up) through the hands-up state below.
- **Proof:** `LF_Test_CrewRobGates` (LOGIC; a solo holder is refused when the till demands a lookout, the lookout leaving cancels, the payout waits for the window). Extend `LF_Test_*` for rob cooldown (check existing names). Manual: two players, one police on duty arriving inside the window.
- **Size and risks:** M. Main risk is the minimum-police gate (`m_iMinPoliceOnDuty` is 2 in config; a small playtest never meets it, so this needs the same debug override the existing rob uses).

---

### Hands-up as a state

- **Decided (2026-11-11):** the target chooses through a short prompt; never a forced pose.
- **Goal:** A character can be in a "hands up" state set by a hold-up (or by choice) that search and robbery read, with a timeout and a refusal if they move or are armed.
- **Overthrow reference:** No close match (a grep for surrender, hands up and similar found nothing). Ignore.
- **Our code to extend:** `LF: Feature/Moderation/LF_HoldUp.c` (the actor plays vanilla gesture 2 via `LF_PlayerInteraction.PlayGesture`, `GESTURE_ID` at line 112 of the role; the target only gets a toast; no state exists), `LF_OnHoldUp` in `LF: Feature/Moderation/LF_ModerationEvents.c:47`; `LF: Feature/Interaction/LF_PlayerInteraction.c` and `LF_PlayerInteractionActions.c` (the one interaction menu and its role handlers); `LF: Feature/Police/LF_ConfiscateAction.c` (a player search); `LF: Feature/Crime/LF_RobWeaponAction.c`, `LF_RobVehicleAction.c`; `LF: Feature/Chains/LF_ChainSearch.c` (`SearchPerson` at 190). New: `Feature/Interaction/LF_HandsUp.c`.
- **Data model:** None. A session map `uid -> expiresAt`. A restart or a death clears it.
- **Config:** Add to `Configs/Moderation/LF_Incident.conf` (see `LF_IncidentConfig`): `handsUpSeconds`, `handsUpBreakOnMoveMetres`. Core default 8 s.
- **Authority and network:** Server holds the state and clears it on move, weapon draw, damage or expiry. The target's pose is a gesture played through the vanilla animation (gesture 2 is already confirmed in `LF_HoldUpRole`); the plan says new animations only go through the animation pipeline, so a held pose on the target is the open question. Replicate as one bool on the character (a single `RplProp` bumped on change) only if other players need to see it; the pose itself already replicates through the animation graph (check).
- **World interface:** A "Hands up" entry the target chooses in the interaction menu (a gamepad-bound action, never WASD), or a forced prompt on being held up. Search and robbery refuse to run unless the state is on (or the target is downed).
- **Proof:** `LF_Test_HandsUpState` (LOGIC; set, expiry, breaks on move, read by the search gate; red-proof: skip the expiry). Manual: hold up an NPC-free player, search them, see the refusal when they are not hands-up.
- **Size and risks:** S. Main risk is forcing a pose on another player: it is a griefing vector, so the target must be given the choice (a short prompt) rather than being locked, and a downed target counts automatically.

---

### Recycler

- **Goal:** A station breaks an item into part of its inputs, always at a loss.
- **Overthrow reference:** No close match. A grep for recycle/dismantle/scrap matched only unrelated prose. Ignore.
- **Our code to extend:** `LF: Feature/Processing/LF_ProcessAction.c` (input and output lists, drop offset, chain machine id); `LF: Feature/Chains/LF_ChainDefs.c`, `LF_ChainCatalog.c`, `LF_ChainStationComponent.c`, `LF_ChainStationAction.c` (load, work, collect; the plan names the M5 crafting bench as the prerequisite, which is not in the tree, check); `LF: Feature/Crime/LF_ChopShop.c` (LOAD, WORK, COLLECT hands-on bench, payout fraction, server-side removal: the closest working analogue); `LF: Configs/Resources/ResourceChains.conf`. New: `Feature/Processing/LF_RecycleRules.c` and a recycle recipe type in config.
- **Data model:** None beyond the station's batch (it rides the existing `chainstations` payload if built on the chain station).
- **Config:** `Configs/Resources/LF_Recycle.conf` or rows in `ResourceChains.conf`: per input prefab, outputs, `returnPercent` (must be under 100 by validation), `workSeconds`. Add a validator check that no recycle row returns at or above full value (the "always a loss" rule, so no loop with the crafting bench).
- **Authority and network:** Server only; reuse the station action path. The client never names outputs.
- **World interface:** A recycler bench prop at a yard: load an item, hold work, collect parts.
- **Proof:** `LF_Test_RecycleRules` (LOGIC; return is below input value; a looping chain craft then recycle is a net loss; red-proof: set `returnPercent` to 100 and see the validator fail). `LF_Test_RecycleStation` (WORLD). Manual: craft, recycle, count.
- **Size and risks:** S. Main risk is an infinite loop with crafting if any recipe yields more than its inputs cost; the validator above is the guard.

---

### Stall board market (cash only)

- **Goal:** A player lists a good at a fixed price on a public board; a buyer pays cash to a clerk, who hands the item to the buyer and the cash to the seller, never bank to bank.
- **Overthrow reference:** No close match. (Matches for "auction" and "stall" in the grep were unrelated words.) Overthrow's closest idea is the shop inventory with stock and a price (`OVT: Components/Economy/OVT_ShopComponent.c:1-163`), but that is NPC-owned stock. Ignore.
- **Our code to extend:** `LF: Feature/Shop/LF_StoreComponent.c` and `LF_Stores.c` (physical store with shelf state and saved stock rows, `ExportAll` at 627, `ApplyAll` at 646; the escrow of a listed item is a shelf item here); `LF: Feature/Shop/LF_StoreRules.c` (`LF_StoreStockRow` at line 26); `LF: Components/InventorySystem/LF_TraderManager.c`; `LF: Feature/Money/LF_MoneyUtils.c` and `LF_Ledger.c`; `LF: Feature/Comms/LF_ServiceBoard.c` (a board listing model); `LF: Feature/Interaction/LF_GiveCashSheet.c`; `LF: Feature/Quantity/LF_InventoryUtils.c`. New: `Feature/Stalls/LF_StallBoard.c`, `LF_StallListing.c`, `LF_StallClerkAction.c`, `LF_StallEvents.c`.
- **Data model:** New payload `stalls`: per listing `version`, `listingId`, `sellerUid`, `prefab`, `quantity`, `price`, `listedAt`, `expiresAt`, and the escrowed item. Escrow: the item sits in a placed container (`LF_PersistentStorageComponent`) at the clerk, so it persists as real entities and cannot be duplicated; a payout owed to a seller while offline is a record `owedCash` per uid, paid at the clerk on return. The listing fee, if any, is cash.
- **Config:** `Configs/Stalls/LF_Stalls.conf`: listing fee, commission percent, max listings per player, listing days, min and max price per good (the market floor and ceiling from the price curve, so a stall cannot launder value).
- **Authority and network:** Server only. `RpcAsk_ListItem`, `RpcAsk_BuyListing`, `RpcAsk_CollectProceeds`, each re-validated on the server, with items moved in a pay-before-delete order and clawed back on failure (the pattern the trader already uses). The board answers per read like the wanted board; no stream.
- **World interface:** A clerk stall with a physical board of listings; the seller hands the item to the clerk, the buyer points at the listing and pays cash at the counter. No remote listing.
- **Proof:** `LF_Test_StallRules` (LOGIC; price bounds, listing cap, commission math, expiry). `LF_Test_StallEscrow` (WORLD; item leaves the seller, appears at the clerk, is delivered once, no duplicate on a race). `LF_Test_StallPersist` (PERSISTENCE). Manual: two clients, one lists, one buys, restart between list and buy.
- **Size and risks:** L. Main risk is duplication of items and cash on partial failure; each step must be atomic on the server, and the tests must hit a failure between steps.

---

### Fuel station stock

- **Goal:** A fuel station holds real stock that the fuel-delivery job refills; buying fuel is refused when the stock is empty or the station is full for deliveries.
- **Overthrow reference:**
  - `OVT: Components/Controller/OVT_FuelRequestComponent.c:1-319` (`FillFuel` at 95, `RpcAsk_FillFuel` at 122-215, `AddFuelToManager` at 216, `DrainFuelFromManager` at 256, `CostForDelivered` at 292): server-authoritative fill. The client names only an RplId and a tank id, never litres or a price; the server moves fuel through `SCR_FuelManagerComponent` and charges for what actually moved. Port the request shape and the "charge for what arrived" rule.
  - `OVT: Data/OVT_FuelChargeLedger.c:1-96` and `Data/OVT_FuelPricing.c:1-167`: a fractional-cost accumulator keyed by persistent id, deliberately unsettled and unsaved (the header says not to "fix" it), and pure price arithmetic with a kill switch at price 0. Port both; they solve the sub-dollar tick problem.
  - `OVT: GameMode/Placeables/OVT_FuelDepotHandler.c:1-63`: placement gate only; ignore.
  - No close match for station stock that a delivery refills; Overthrow stations dispense fuel from the vanilla tank without a stock model beyond the vanilla tank's own level.
- **Our code to extend:** We have no pump or refuel code (a grep for refuel, pump and `SCR_FuelManagerComponent` in our scripts found nothing). `LF: Feature/Jobs/LF_CivilianJob.c` (`PAY_FUEL`, `FUEL_PREFAB` at 35: a fuel run carries a jerrycan), `LF: Feature/Jobs/LF_JobSiteComponent.c` (the fuel can site), `LF: Feature/Jobs/LF_JobAction.c`; the station's store pattern in `LF: Feature/Shop/LF_StoreComponent.c` and `LF_Stores.c` (stock, max, restock, saved as `storestock`); `LF: Feature/Crime/LF_RobAction.c` (the DebugWorld till is `Roadmap_Fuel_Till`). New: `Feature/Fuel/LF_FuelStation.c` (stock rules), `LF_FuelStationComponent.c`, `LF_FuelPump*Action.c`.
- **Data model:** New payload `fuelstock`: per station `version`, `stationKey`, `litres` (int). Same append-a-row pattern as `storestock`. A delivery run adds litres; a sale removes them; the station's stock never exceeds capacity.
- **Config:** `Configs/Fuel/LF_Fuel.conf`: per station `capacity`, `startLitres`, `pricePerLitre`, `litresPerDelivery`; global `minStockToSell`. The fuel-delivery job's pay can scale with how empty the station was (an owner choice).
- **Authority and network:** Server only. Pumping asks the server (`RpcAsk_FillFuel(vehicleRplId, tankId)` in the Overthrow shape); the server prices with the fractional ledger and subtracts from stock. A delivery drop-off adds litres at a station action. Show the stock on the pump as a text prop (like the price board), read per request, not streamed.
- **World interface:** The pump itself is the interaction; a sign shows "out of fuel" when empty.
- **Proof:** `LF_Test_FuelStationRules` (LOGIC; sale blocked at zero, delivery blocked when full, fractional cost carried; red-proof: allow a sale at zero). `LF_Test_FuelStationPersist` (PERSISTENCE). Manual: empty a station, run a delivery, buy.
- **Size and risks:** M. Main risk is the vanilla fuel stack: the engine fuel flow is not testable on a dedicated server (the Overthrow header says so), so the pure rules must carry the tests and one manual refuel proves the integration (check how vanilla pumps expose their fuel level).

---

### Race runner

- **Decided (2026-11-11):** the course runner (checkpoints, timer, refund on restart) is built here. Race records are a sidecar payload.
- **Goal:** Players pay an entry fee into a cash pot, drive a checkpoint course, and the winner takes the pot; a record board at the track keeps best times.
- **Overthrow reference:** No close match. Ignore. (The job stage range checks, `OVT: GameMode/Systems/Jobs/Stages/OVT_WaitTillPlayerInRangeJobStage.c:1-19`, are the only checkpoint-like code.)
- **Our code to extend:** Share the checkpoint and timer machinery of the licence course runner (`Feature/Courses/`, above) so the two do not diverge. `LF: Feature/Money/LF_MoneyUtils.c`; `LF: Feature/Vehicles/LF_VehicleSpawnPointComponent.c`; `LF: Feature/Signs/LF_SignComponent.c` (record board). New: `Feature/Racing/LF_RaceRunner.c`, `LF_RaceBoard.c`, `LF_RaceEvents.c`.
- **Data model:** Optional payload `racerecords` (best time per track and per character: `version`, `trackId`, `uid`, `bestMs`), and the plan says "decide, build later". The pot is session state; a restart mid-race refunds entries (same refund row as the course runner, or skip refunds by taking fees only at the start of a lobby that closes in under a minute).
- **Config:** `Configs/Racing/LF_Races.conf`: per track checkpoints, lap count, entry fee, minimum entrants, lobby seconds, payout split, whether a vehicle spawn is provided.
- **Authority and network:** Server only: lobby, start, per-checkpoint detection, finish order. Clients get `RpcDo_RaceState` (next checkpoint, position). Entry by an action at the track sign with carried cash.
- **World interface:** A starter sign and the track props; a record board on the same sign technique.
- **Proof:** `LF_Test_RaceRunner` (LOGIC; entry fee into pot, finish order, payout, no payout with fewer than min entrants). Manual: two players race, one leaves.
- **Size and risks:** M. Main risk is cheating by skipping checkpoints or shortcuts; check checkpoint order and count laps server-side. Low priority: build after the licence course runner so it reuses it.

---

### Gazette from the event bus

- **Goal:** A printed town paper is built automatically from bus events and incidents, readable at a newsstand, with no volunteer staff.
- **Overthrow reference:** No close match. `OVT: GameMode/Managers/OVT_NotificationManagerComponent.c:77-130` (`SendTextNotification`, `SendExternalNotifications` which posts to an external webhook via `RestCallback`) is a toast and an outbound feed, not an in-world record of what happened. Ignore, except that the webhook idea could feed a community Discord post later (out of scope here).
- **Our code to extend:** `LF: Core/Events/LF_EventBus.c` (subscribe by exact event class, isolated handlers); the event files `LF: Feature/Crime/LF_CrimeEvents.c` (`LF_OnCrimeCommitted`, `LF_OnPlayerKilled`), `LF: Feature/Jobs/LF_JobsEvents.c` (`LF_OnJobCompleted`), `LF: Feature/Police/LF_PoliceEvents.c`, `LF: Feature/Moderation/LF_ModerationEvents.c`, `LF: Feature/Money/LF_MoneyEvents.c`, `LF: Feature/Shop/LF_ShopEvents.c`, `LF: Feature/Vehicles/LF_VehiclesEvents.c`, plus the new market, event-shell, war and site events above; `LF: Feature/Moderation/LF_IncidentLog.c` (a saved, trimmed record store with `Query` by place and time; the gazette can read it instead of keeping its own); `LF: Feature/Police/LF_WantedBoard.c` and `LF: Feature/Comms/LF_ServiceBoard.c` (a board that answers per read); `LF: Feature/Signs/LF_SignComponent.c`, `LF_SignCard.c`. New: `Feature/Gazette/LF_Gazette.c` (subscribers and edition builder), `LF_GazetteConfig.c`, `LF_NewsstandReadAction.c`.
- **Data model:** Payload `gazette`: the last N editions (`version`, `editionNumber`, `issuedAt`, a list of short headline rows: `kind`, `textKey`, `args`). Store localisation keys plus arguments, not formatted text, so a language change re-renders old editions. Privacy: never print a name that has not been identified (reuse the wanted-board rule that a name is withheld until the report is marked identified, `LF_WantedBoard` header).
- **Config:** `Configs/Gazette/LF_Gazette.conf`: edition interval (in-game days), max headlines, which event classes count and how they are worded (a table `eventClass -> template key, priority`), identified-names-only flag, and a per-kind cap so one griefer cannot fill the paper. Pack supplies the paper's name and tone.
- **Authority and network:** Server builds an edition on a timer from events collected since the last one. Reading the newsstand is `RpcAsk_ReadGazette` answered with `RpcDo_ShowGazette(rows)`, the same shape as the wanted board.
- **World interface:** A newsstand prop in the village and a paper item (an `LF_SignCard`-style sheet) a player can pick up. No phone, no app.
- **Proof:** `LF_Test_GazetteBuild` (LOGIC; events become headlines, a cap per kind, names withheld until identified; red-proof: print an unidentified name). `LF_Test_GazettePersist` (PERSISTENCE). Manual: commit a crime and a job, wait for the edition, read the stand.
- **Size and risks:** M. Main risk is leaking information a crime should hide (names, locations of an unsolved crime) and griefing the paper with spam; the identified-only and per-kind cap rules address both.

---

### Onboarding chain

- **Goal:** A new character is walked through the core loop by a short chain of tasks, with a non-cash welcome gift and a grace period against being robbed or arrested in the first minutes.
- **Overthrow reference:**
  - `OVT: Configuration/OVT_TutorialEntryConfig.c:1-102`: a tutorial entry is one `.conf` with a stable id, title key, pages, priority and trigger, so a server owner adds or retires entries with no script. Adapt the config shape and the "id is immutable, retire with enabled=0" rule.
  - `OVT: Configuration/OVT_TutorialTrigger.c:1-126` (`OVT_TutorialEvent` enum of 13 triggers, 11 raised on the server from existing manager events): a catalogue of the events a hint can bind to. Adapt; ours are bus events and quest signals, so a trigger is already a string key.
  - `OVT: Data/OVT_TutorialQueue.c:1-158`, `OVT_TutorialGate.c:1-49`, `OVT_TutorialSeenStore.c:1-164`, `OVT_TutorialMatcher.c:1-112`: a priority queue, a gate that avoids interrupting play, and a per-machine "seen" store. Ignore the per-machine store (ours is per character on the server); adapt the queue and gate idea only if we want hint popups, which the plan avoids.
  - Overthrow's tutorial is hint-driven, not a task chain. We already have the chain machinery (below).
- **Our code to extend:** `LF: Feature/Quests/LF_QuestDefinition.c` (lines 31-64: `m_sRequiredPreviousQuestId`, `m_bAutoAccept`, `m_bAutoTurnIn`, `m_iMinLevel`, `m_iRewardMoney`, `m_aRewardItems`, `m_bRewardIdCard`: a quest chain with automatic acceptance and turn-in is already supported); `LF: Feature/Quests/LF_QuestEnums.c` (`LF_EQuestEventType.SIGNAL`); `LF: Feature/Quests/LF_QuestHooks.c` (`LF_QuestSignals`: `sign.read:<id>`, `job.run_complete:<job>`, `atm.deposit`; new signals for first purchase, first fine, first garage use are added here); `LF: Feature/Quests/LF_QuestManager.c`; `LF: Feature/Quests/LF_QuestGiverComponent.c`; `LF: Feature/Quests/LF_QuestTrackerComponent.c`; `LF: Feature/Character/Spawning/LF_SpawnLogic.c` (the arrival, and the existing town-clerk ID-card reward `m_bRewardIdCard`); `LF: Feature/Moderation/LF_Zones.c` (the spawn safe zone, #125). New: content only in the pack (`LF_Everon1989`), plus a "protected" flag (`LF: Feature/Account/LF_PlayerAccount.c`, check) for the grace period.
- **Data model:** Quest progress already persists through `LF_Progression` (turned-in quest ids). The grace period is a session timer from first spawn or a persisted `graceUntil` on the progression record (append a field, bump `LF_ProgressionRecord.VERSION`), so a restart does not restart the grace period.
- **Config:** Quests are config on the giver prefab. Add `Configs/Spawning/LF_Onboarding.conf` (check `Configs/Spawning` for what exists): `gracePeriodMinutes`, `gracePolicy` (cannot be robbed, cannot be arrested), `welcomeGift` (a non-cash item list, such as a bus ticket, a map, a sandwich), `chainId`.
- **Authority and network:** The server accepts, advances and turns in quests today; grace enforcement is a server rule read by `LF_RobAction`, `LF_RobWeaponAction`, the jail arrest action and the hold-up. Nothing new on the wire.
- **World interface:** An NPC at the arrival point gives the first task; the chain leads through signs, the shop, the ATM, a job site, the garage. Each step is "go there and do the thing", no popups. Tied to a `docs/playtest.md` line.
- **Proof:** `LF_Test_OnboardingChain` (WORLD; the chain's quests link by `m_sRequiredPreviousQuestId` with no gaps and no cycle, every reward item resolves, the gift is not cash; red-proof: break one link). `LF_Test_GraceProtection` (LOGIC; robbery and arrest refuse inside the grace, resume after). Manual: new character from a clean save, finish the chain, restart mid-chain.
- **Size and risks:** S to M (mostly content on existing systems). Main risk is the grace period being abused to farm protection; cap it to one per character and cancel it on the first crime the character commits.

---

## Cross-cutting notes

- **Shared infrastructure to build once.** (1) A course runner (licence exams and races share checkpoints, timer, refund). (2) A "board answers per read" helper (price board, stall board, record board, gazette, wanted board) following `LF_WantedBoard`. (3) A cash-escrow pattern (stall, race pot, locker rent) built on `LF_MoneyUtils` with pay-before-delete and claw-back. Building these first removes duplicate work across six items.
- **Save versions.** New payloads in this section: `market`, `worldevents`, `orgwars`, `sites`, `lockers`, `stalls`, `fuelstock`, `gazette`, optionally `garageslots` and `racerecords`. That is up to ten bumps; group them into as few releases as possible, and decide the record shapes in M3 as the plan says.
- **Core versus pack.** Every config above has a neutral core default and the pack supplies names, prices and places (`docs/architecture/core-vs-pack.md`). Until the pack loads in M3, pack resources land in the core with a row in `docs/architecture/core-pack-inventory.md`.
- **Best Overthrow reuse overall (port with header):** the fractional fuel charge ledger and pure fuel pricing; the bulk-sale marginal pricing seam (`GetSellPriceAtOffset`); the trade-XP cooldown; the name-keyed phase machine with a typed key bag from the objective framework. Everything else is a pattern to adapt or no match.

## C. Flagship content, live ops (M4), homes and land (M6)


Scope: plan section 3 (flagship content), section 4 (M4 live ops), section 5 (M6). Read-only cross-reference of the integration plan against our tree and the Overthrow tree.

Conventions. `OVT:` paths are under the Overthrow repo root (commit `9dbb8fad`). Our paths are under `addons/LifeFramework/` unless they start `addons/` or `docs/`. Base-game paths were confirmed with `tools\cli call asset_search` (they exist in the game paks). "check" marks anything not verified in code or in the editor. Any Overthrow code we copy gets the header from `THIRD_PARTY.md` and a row there; "adapt the pattern" means we write our own `LF_` code and copy no lines.

### Findings that cut across items

1. **The Overthrow tree is war code.** Almost everything useful is a small pure rule or a data pattern, not a feature. There is no jail, licence, rank, convoy, fishing, bees, chess or crop code in it (grep of Scripts, Prefabs, Configs, Language: no hits for prison, jail, licen, hideout, honey, chess, crop; `fishing` only appears in `OVT:Scripts/Game/GameMode/Civilians/OVT_TownVehicleSourceConfig.c` as a vehicle label).
2. **The base game already ships a firing range system** (`SCR_FiringRangeController`, `SCR_FiringRangeTarget` with `Event_TargetChangeState` and score constants, prefabs `Prefabs/Props/Military/Training/FiringRangeBasePrefab.et`, `FiringRangeManager.et`, `FiringRangeNetworkEntity.et`, `Editor_FiringRangeLineController.et`, `TargetInfantry_01.et`, and a vanilla placed range `Prefabs/World/Locations/Eden/Urban/MTA_ShootingRange_01.et`). The firearms test builds on that, not on custom targets.
3. **The licence replication bitmask has exactly 3 free bits.** `LF_LicenseBits.Order()` (`Scripts/Game/Feature/License/LF_LicenseManagerComponent.c:91`) lists 28 types and `BitOf` rejects an index above 30. Car, truck and weapon licences fill bits 28, 29, 30 and nothing else can be added without widening the replicated value. Licences persist by enum name, so only replication is bound. There is no driving or weapon licence in `LF_ELicenseType` today (the enum has farmer, miner, lumberjack, medic, police and criminal types only).
4. **We already own a price-board mechanism.** `Feature/Signs/LF_SignComponent.c:158` `PricedSign` fills a sign title from a vehicle lot's live price (#540) and paints it on the face of `Prefabs/Props/Signs/LF_Sign_Chalkboard.et` (parent: vanilla `Prefabs/Props/Services/ChalkBoard_01/ChalkBoard_01.et`). A market chalk board extends that.
5. **Our own decision rules out AI-driven convoys as a first cut.** `docs/features.md` (cash van, #274): "base-game AI driving is not reliable enough to carry a robbery", so `Feature/Crime/LF_CashVanRoute.c` parks and moves a van between stops. Overthrow does drive AI crews (waypoint pattern below) and is the only reference if we want a driven convoy.
6. **Overthrow's persistence pattern for runtime-spawned objects** (`OVT:Configs/Systems/Persistence/Overthrow.conf:170-225`: `EntityPersistenceConfig` with a `ComponentClassPersistenceConfigRule`, `SelfSpawn 1`, `GenericEntitySerializer`, per-component serializers incl. vanilla `SCR_FuelManagerComponentSerializer`) is the model for placed depots, crop plants and house sites. Our `Configs/Systems/Persistence/LifeFramework.conf:49-61` has one `SelfSpawn 1` entry (`LF_QuantityComponent`) and the rest `SelfSpawn 0`. Every new persisted record also needs a payload appended in `RegisterPersistencePayloads` (`Scripts/Game/Persistence/Serializers/LF_PersistenceComponentSerializer.c:94`) with a version bump (AGENTS.md; `validate-persistence-version`).
7. **Named places without hard-coded coordinates.** `OVT:Scripts/Game/GameMode/Managers/OVT_TownManagerComponent.c:1141-1175` (`ProcessTown`) discovers towns from `MapDescriptorComponent` (`MDT_NAME_VILLAGE/TOWN/CITY`) and `:872` `GetNearestTownMarker`. Adapt as a tiny `LF_` helper so crime reports and the pack's content can name the nearest Everon place. The only Everon coordinate we have in the repo is St Philippe apple farm around x 4450, z 10570 (`addons/LifeFramework/Worlds/MainWorld/MainWorld_Layers/Area_StPhillipe_AppleFarm.layer`, trader at 4442.5, 19.4, 10568.7). Every other Everon place name below is from the vanilla map, coordinates unknown: "check in Workbench".
8. **Pack placement rule.** Until M3 loads `addons/LF_Everon1989/` (skeleton: `Assets/`, `Configs/`, `Language/`, `Missions/`, `Prefabs/`, `UI/`, `Worlds/` all `.gitkeep`), pack content still lands in the core and gets a row in `docs/architecture/core-pack-inventory.md` (golden rule 9). A "Pack" row below means that.

---

### Section 3: flagship content

#### Market goods and the chalk price board
- **Goal:** Village-scale goods with prices that fall as players sell and recover over time, shown on a chalk board at the trader. Layers: Framework (curve service, plan section 2) plus Flagship (the goods list, the board).
- **Overthrow reference:**
  - `OVT:Scripts/Game/Data/OVT_ResourceRules.c:29-48` `DriftStep(base, current, roll, pressure, volatility, stepFraction, bandMin, bandMax)`: a pure clamp-to-band price step. `:77-85` `ShouldDrift(hour, lastDriftedHour)` is a latch on four fixed drift hours (0, 6, 12, 18). `:91` `ApplyLevelMultiplier`, `:101` `SellPrice`. Adapt: take the band-clamp and the hour latch; replace the random walk with "drop per unit sold, recover per hour". Port-able as a pure static class if we want the math, with header.
  - `OVT:Scripts/Game/Configuration/OVT_PricesConfig.c:1-78` (`cost`, `demand`, `m_sFind` string match). Ignore: it keys prices on arsenal item types, not on a good id.
  - Nothing in Overthrow reacts to the seller's volume.
- **Our code to extend:** `Scripts/Game/Components/InventorySystem/LF_TraderManager.c:30` `LF_TraderManagerComponent.GetUnitValue` (the single sell-value lookup) and `:47` `CanAccessBlackMarket`; `Scripts/Game/Feature/Shop/LF_ShopPriceResolver.c`; `Configs/Shop/ShopPrices.conf`; `Scripts/Game/Feature/Resources/LF_ResourceChainCatalog.c` with `Configs/Resources/ResourceChains.conf` (the goods the chains already make: apple, plum, cabbage, iron bar, gravel, sand, cement, planks; `Prefabs/Items/Resources/{CementBag,GravelBag,Sandbag,Brick}.et`, `Prefabs/Resources/WoodCutting/WoodPlank.et`). Board: `Feature/Signs/LF_SignComponent.c:158` `PricedSign`, `LF_SignCatalog.c:155` `PricedTitle`, `Prefabs/Props/Signs/LF_Sign_Chalkboard.et`, `UI/Layouts/Signs/LF_SignBoardFace.layout`. DebugWorld trader: `Worlds/DebugWorld/DebugWorld_Layers/TraderChain.layer`.
- **Content spec:**
  - Goods (proposal): apple, plum, cabbage, potato if a prefab exists (check), iron bar, plank, cement bag, gravel bag, sandbag, later fish and honey. Illicit chain goods stay off the board and go to the fence.
  - Per good: base (the current `ShopPrices.conf` sell price), floor 0.5x, ceiling 1.5x, drop 2 % of base per unit sold, recovery 10 % of the gap per game hour. These are starting numbers to define in tuning, not measured.
  - Fixed-price exceptions: apples at a village stall (the reference server keeps apples and fish flat). Decide with #686.
  - Board copy: one line per good "Apples ... 12", header "Prices today", period wording; all strings as keys in `Language/everonlife_localization.st` then `tools\cli regen-localization`.
  - Board placement: on the trader's wall in the Everon pack town; chalk board prefab as above. The board needs several lines, `FitBoardTitle` (`LF_SignComponent.c:447-475`) already wraps lines.
- **Dependencies:** Section 1 decision "dynamic market" (no issue found; state: price, floor, ceiling, recovery, event), #686 profit-loop check, #688. Persist current price per good as a new payload.
- **Proof:** LOGIC test of the curve (drop, floor, recovery, restart round trip of the state), WORLD test that selling 20 apples lowers the next quote and the board string changes, PERSISTENCE round trip. Shot of the board before and after a sale.
- **Size and risks:** M. The board text is painted on each client from config today (`PricedSign` reads the lot's config); a live price must be replicated (`RplProp` plus `BumpMe`, batched per sale burst, not per frame) and re-painted on change, or the board lies on a client. Late joiners must receive current prices.

#### Driving test at the station

- **Decided (2026-11-11):** dropped with the licence exams. Licences stay bought with skill points.

#### Firearms test at a range

- **Decided (2026-11-11):** dropped with the licence exams. The shooting range stays as village texture (below).

#### Police career: ranks, pay, qualifications, checkpoint kit

- **Decided (2026-11-11):** qualifications are granted by a senior officer, not by exam.
- **Goal:** More than two ranks, pay scaled by rank, optional qualification tags, and a road checkpoint kit. Layer: Framework (rank ladder and qualification config) plus Flagship (1989 militia titles, pay table, kit).
- **Overthrow reference:** No close match for ranks or pay (wanted level only: `OVT:Scripts/Game/Components/Player/OVT_PlayerWantedComponent.c`; ignore). `OVT:Scripts/Game/GameMode/Deployments/Modules/OVT_MobileCheckpointBehaviorDeploymentModule.c:1-72` and `OVT:Scripts/Game/Data/OVT_CheckpointApproachRules.c` are about AI vehicles moving to road approaches; ignore for a player kit, though `OVT_CheckpointApproachRules` bearing arithmetic is a pure helper if we ever auto-place a checkpoint on a road.
- **Our code to extend:** `Feature/Police/LF_PoliceRanks.c` (two fixed ranks `PATROL=0`, `SENIOR=1`; `DUTY_SECONDS=3600`; `TitleKey`, `IsRankUsable`, `FineCap`, `DutySecondsForNext`, `CollectKit`, `Promote`, `Draw`); `LF_PoliceConfig.c` (`ArrestMinRank`, `EscortMinRank`, `PatrolFineCap`, ticket offences); `LF_DutyPay.c:36` `RateFor(role)` reads one flat `m_fPoliceDutyPayPerSecond` from `Configs/Jobs/LF_Jobs.conf` (`LF_JobsConfig`); `LF_ArmoryAction.c` and `Prefabs/Police/LF_ArmoryStore.et`; `Feature/Toys/LF_SpikeStrip.c` (exists; #646 adds cones and barriers); persisted via `LF_CharacterStanding` (rank and duty totals belong to the character, saved in the account payload; a rank above the ladder clamps to senior on load, `docs/features.md` around line 456). `docs/roadmap/scopes/police/ranks.md` is the existing scope card.
- **Content spec:**
  - Ladder (proposal, period-plain, localization keys `EL-Rank_*` pattern at `LF_PoliceRanks.TitleKey`): Cadet, Patrol, Senior patrol, Sergeant, Lieutenant, Captain. Keep it to 5 or 6, not the reference server's 9. Titles are copy to define with the maintainer.
  - Config: per rank `{title key, duty seconds to next, fine cap, pay per second, kit prefabs, can arrest, can escort, can promote}`. The current constants (`PATROL_FINE_CAP 1000`, `DUTY_SECONDS 3600`, `SIDEARM` M9, `LONG_GUN` M16A2) become config.
  - Qualifications (tags): traffic, firearms instructor, detective. Gate on the named station kit and on driving and firearms test roles; stored as a small bitset on the character record (a new `version` and append, never insert).
  - Pay: duty pay already settles at clock-out (`LF_DutyPay.TakeShiftPay`); scale `RateFor` by rank only. No timer paycheck (decision).
  - Checkpoint kit (draws from the armoury or a station crate, #646): `Prefabs/Props/Infrastructure/ConeTraffic/ConeTraffic_01_red.et`, `Prefabs/Props/Infrastructure/Roadblocks/RoadBlock_01.et`, `Prefabs/Structures/Infrastructure/Barriers/CrashBarrier_E_01/CrashBarrier_E_01_3m.et`, `Prefabs/Props/Roadblocks/RallySafteyTape_01/RallySafetyTape_01_4m.et`, plus our spike strip. Placing goes through `Components/BuildingSystem/LF_BuildingManagerComponent.c` (`PlaceHeldBuilding`, 5 m range, consumes the held item).
- **Dependencies:** Section 1 "police career schema" (M3, extends `LF_PoliceRanks`), #685, #646/#647 (kit), #314 and #128 (entry), #692 (playtime gate).
- **Proof:** LOGIC tests for ladder math and pay by rank, PERSISTENCE round trip of rank and qualification, WORLD test of the kit placement range. Existing tests to extend: grep `LF_Test_*` for `PoliceRanks` (check names). Manual: two players, one promotes the other, pay on clock-out.
- **Size and risks:** M for the ladder and pay (most of it is config), S for kit placement. Risk: the persisted rank value is read in several places as `PATROL` or `SENIOR` (grep `LF_PoliceRanks.SENIOR` before widening); the account state packs police rank into a bitfield (`LF_AccountStateBits`, `docs/features.md` around line 560) whose width must be checked.

#### The evidence-locker convoy

- **Decided (2026-11-11):** AI driven from the start, with stuck detection and a fallback (teleport to the next stop and log it).
- **Goal:** When the evidence locker fills, police move the stock to a central store by truck; anyone can hit the truck. Layer: Flagship (route, timing) on a Framework event shell (plan section 2 "Event shell", "Staged break-in").
- **Overthrow reference:**
  - `OVT:Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c:780-835` `IssueCrewMove`/`ClearOwnedWaypoints`: spawn a move waypoint, `crew.AddWaypoint(waypoint)`, delete owned waypoints by hand (`AIGroup.AddWaypoint` takes no ownership). Waypoint spawn helper `OVT:Scripts/Game/GameMode/Managers/OVT_OverthrowConfigComponent.c:610-627` `SpawnWaypoint`/`SpawnMoveWaypoint`, with the prefab `{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et` set at `OVT:Prefabs/GameMode/OVT_OverthrowGameMode.et:75`. Adapt the pattern (we have no `AIWaypoint` code anywhere in `Scripts/`).
  - `OVT:Scripts/Game/GameMode/Deployments/Modules/OVT_VehicleSpawningDeploymentModule.c:659-693` `SeatAgent` / `:696` `FillCompartment`: seat an AI into pilot, then turret, then cargo. Port-able in about 40 lines, with header; also listed in `docs/overthrow-survey.md`.
  - The `OVT_MobileCheckpoint...` header (lines 1-72) records the key hazards: never move an occupied hull by transform, AI crews need a null plan, and virtualisation drops waypoints when a group goes dormant. Read before trusting AI to drive.
- **Our code to extend:** `Feature/Crime/LF_CashVanRoute.c` (a parked vehicle moved between stop markers, held while a player is within range: the safe first version) and its `LF_CashVanRouteComponent`; `Feature/Investigation/LF_EvidenceLocker.c` (`Rows()`, `Count()`, `LogBag`, `Raid`, `LF_RaidLockerAction`; rows are records only, items do not survive, see the file header); `Feature/Crime/LF_Breach.c` (`Arm`, `Repair`, `IsOpen`), `LF_BankVaultComponent.c`, `LF_HeistVault.c` and `LF_BankLoot.c` (the cash van box as a lootable); `Feature/Crime/LF_Contraband.c` and `LF_ConvoyActions.c` (a different feature: the player convoy run that carries crates, do not conflate); `Feature/Police/LF_Dispatch.c` for the alert; `Prefabs/Vehicles/Wheeled/M923A1/EL_M923A1_transport.et`, `.../S1203/EL_S1203_transport_beige.et`.
- **Content spec:**
  - Trigger: `LF_EvidenceLocker.Count()` passes a config threshold and at least N officers on duty (config).
  - Version 1 (recommended): a parked evidence truck at the station that the server moves between 3 stop markers on a timer, loaded with a crate whose contents are rolled from the locker's logged rows. Same safety as the cash van.
  - Version 2: a driven convoy, an escort car and the truck with AI drivers on a road route, using the waypoint and seating helpers above. Do not ship until a real dedicated-server drive of the full route is proven; record why in the PR.
  - Places (Everon, check each): police station in the pack town to a county store in another town, 2 to 4 km. Candidate towns from the vanilla map: Montignac, Meaux, Morton, Levie, Saint Pierre; verify road connectivity in Workbench.
  - Loot: seized goods rows are re-materialised as items on the truck (`LF_InventoryUtils.AddAmount`), cash never rides (the reference server rule; ours: cash is not logged today either).
  - Announcement: a dispatch line (police only) and a market-board style note, no public broadcast.
- **Dependencies:** Event shell and staged break-in (section 2), #662/#663 multi-stage heist, #269 (locker, built), #685, Section 1 persistence if the truck state must survive restart (recommend not: reset at restart).
- **Proof:** LOGIC (threshold, route stepping via `NextStop`-style tests), WORLD (truck reaches a stop, hold when a player is near, loot appears), dedicated-server run for version 2, manual two-player heist. Existing harness verb to model: `session.mjs village-proof` drives the cash van, vault and locker chain.
- **Size and risks:** M for version 1, L for version 2. Risk: AI driving on Everon roads and virtualisation (Overthrow's own notes); items-vs-rows mismatch in the locker (rows exist, items do not).

#### A prison break
- **Goal:** A rare, gang-started event: a free member opens the way, prisoners may walk out, police respond, escapees run to an extraction point. Layer: Flagship on the Framework event shell.
- **Overthrow reference:** No close match (no jail). Pattern only: stage handler shape `OVT:Scripts/Game/GameMode/Systems/Jobs/OVT_JobStage.c` for prep, hot and cooldown phases. Seating a prisoner in a getaway car: `OVT_VehicleSpawningDeploymentModule.SeatAgent` (above, and `docs/overthrow-survey.md` "Vehicle escort") for the server-side seat call.
- **Our code to extend:** `Feature/Police/LF_Jail.c` (`NoteEscape` at line 1028: an escapee outside the cell volume while the jail door is open is reported once and the sentence is left alone), `LF_JailCellComponent.c`, `LF_JailEscort.c`, `LF_JailSlip.c`, `LF_JailWork.c`; `Feature/Crime/LF_Breach.c` (the jail door `Roadmap_Jail_Door` is a breach target, #235); `Feature/Moderation/LF_Zones.c:238` `RegisterCircle(name, rule, centre, radius)` and `:250` `Unregister(name)` (open a LAWLESS circle around the prison for the hot phase and remove it); `Feature/Police/LF_Dispatch.c`; `Prefabs/Police/EL_JailCell.et`. The first slice (door breach, walk out, police repair) is in the tree: `docs/features.md` around line 1334, harness `session.mjs jailbreak-proof`, tests `roadmap/jailbreak`, `e2e/roadmap-jailbreak`.
- **Content spec:**
  - Contact: an inmate NPC in the yard who starts the event once preconditions hold (a jailed member with 25 or more minutes left, N police on duty, cooldown over; the reference server uses 1.5 h server-wide, tune).
  - Phases: 2.5 minute prep announced to police and prisoners, 15 minute hot phase, then cooldown. Hot phase: jail volume does not pull prisoners back, the zone rule at the prison becomes LAWLESS for the window.
  - Extraction: 3 to 5 fixed markers in the countryside, one chosen at random; escapees keep their sentence and wanted level (`LF_Jail.NoteEscape` already leaves them untouched); recapture by police clears escape status.
  - Where: the pack town's jail (DebugWorld jail is inside the station yard, `Street_Jail.layer`). Everon site: check; a real station would sit in a town such as Montignac.
  - Copy: period notices ("Prisoner escape reported"), dispatch strings, localization keys.
- **Dependencies:** Event shell (section 2), #235 (built), #242 (zones, built), #656/#657 prison work if prisoners are to be busy, #685.
- **Proof:** LOGIC for preconditions and phase clock, WORLD for the zone register and unregister and the door/volume rule, manual two-client run (gang member and police) and a restart during the hot phase (should end the event; document).
- **Size and risks:** M. Risk: the rare-event shell has to be the shared one; a one-off script here would be rewritten. Disconnect while jailed or escaped is #691.

#### The hideout as contested ground in the lawless area
- **Goal:** One lawless area whose hideout can be taken by force: holders accrue hold time and unlock benefits. Layer: Framework (contested site with rent, section 2, held-ground record from section 1) plus Flagship (the place and the rewards).
- **Overthrow reference:**
  - `OVT:Scripts/Game/UserActions/OVT_CaptureBaseAction.c:6-51`: a long hold action that reports the illegal act at `OnActionStart` (not on completion) and cancels the report in `OnActionCanceled`. Adapt the idiom for the flag-pole capture so being seen starting a capture matters.
  - `OVT:Scripts/Game/GameMode/Deployments/OVT_ResistancePresence.c:1-40`: "who holds this ground" asked of the world by faction rather than from lists. We have no such faction; adapt only the lesson (one function answers "who is present", used by every test).
  - Everything else (QRF, base capture, faction control) is war logic: ignore.
- **Our code to extend:** `Feature/Hideouts/LF_Hideout.c` (`HideoutId`, `Assign`, `GetOwner`, `IsLocked`, `TryUnlock`, `AllowsStash`, `ExportAll`/`ApplyAll`), `LF_HideoutDoorComponent.c`, `LF_HideoutStashStorageComponent.c`; `Feature/Organizations/LF_Organization*.c` (member lists, `LF_OrgHideoutLease` lease from the registry); `Feature/Moderation/LF_Zones.c` (`LF_ZoneComponent`, `LF_ZoneRule.LAWLESS`, `RuleAt`; `Scripts/Game/HUD/LF_LawlessHud.c` and `UI/Layouts/HUD/LF_LawlessTag.layout` already tell a player they have entered a lawless area); `Feature/Moderation/LF_IncidentLog.c`; `Feature/Crime/LF_Murder.c` (zone waives murder rules); `docs/roadmap/scopes/ownership/hideouts.md`. DebugWorld: `Street_Hideout.layer`.
- **Content spec:**
  - One lawless area on Everon (maintainer decision 2026-11-08, `docs/decisions.md:357`). Site: an isolated compound away from the pack town. Candidate Everon locations (check): a quarry or industrial site, the vanilla military training area `Prefabs/World/Locations/Eden/Military_Training_Area_MilitaryBase_01.et` or `...Natural/Military_Training_Area_Valley_01.et` (both exist as location prefabs; fit unverified). Flag pole: `Prefabs/Props/Fabric/Flags/Flag_1_2_Everon_01.et` and `Prefabs/Props/Core/Flag_Base.et`.
  - Rules (from the plan, the reference server's numbers are the starting point): capture needs an organisation, a civilian character and a drawn weapon; cooldown after a flip and after a defence; defenders alerted wherever they are; presence scored for alive players only, inner ring double, a hard cap; no timer, it ends when the bar fills or the zone clears.
  - Benefits unlocked by hold hours: processing upgrade (`LF_Hideout.CanProcess` exists), a stash size, a cheaper lease. Config table.
  - HUD: the existing lawless tag plus an in-zone capture bar (new layout).
- **Dependencies:** Section 1 "held ground" (hold time and points on the hideout record), declared organisation wars if used, #242, #685 for the crime codes the zone suppresses, #555 (not before a live server).
- **Proof:** LOGIC (score maths, caps, cooldown), PERSISTENCE (hold seconds, owner), WORLD (capture flip with two scripted characters), manual with two orgs on a dedicated server. Shot: the lawless tag and capture bar.
- **Size and risks:** L. Risks: orgs are the owner (not characters), so org disband and member leave must release the claim; the save record is new (`LF_Hideout` records version bump); abuse by a single player standing in the zone (presence counts need org member checks).

#### Cook failures and a rotating buyer in the illicit chains
- **Goal:** Hands-on cooking can fail with a delayed blast, and finished goods sell to one buyer at a time that rotates. Layer: Framework (chain config) plus Flagship (chain values, buyer spots).
- **Overthrow reference:** No close match. `OVT:Scripts/Game/UserActions/OVT_SellDrugsAction.c:1-40` and `OVT_EconomyRequestComponent.RpcAsk_SellDrugs` sell weed to a street dealer with a 25 % chance to consume the action; it is far simpler than a rotating buyer. Ignore.
- **Our code to extend:** `Feature/Chains/LF_ChainDefs.c` (`LF_ChainDef`, `LF_ChainStepDef`, `LF_ChainHeatDef`), `LF_ChainStationComponent.c` (hands-on work ticks, line 393 `AddWork`, outputs at 432), `LF_StationProgress.c`, `LF_ChainHeat.c`, `LF_ChainTrace.c` (smoke, smell, police lead per site, #418), `LF_ChainSearch.c` (#419), `LF_ChainCatalog.c`; `Configs/Resources/ResourceChains.conf` (cider and moonshine chains, station types `barrel`, `still`); fence: `Feature/Crime/LF_FenceRules.c`, `LF_FenceSerials.c`, and the fence NPC `FenceNpc` (Vince, `Crimes.layer`); explosions: `Feature/Crime/LF_Breach.c` charge visuals (check for a reusable blast effect).
- **Content spec:**
  - Failure: a chain step flag `m_iMaxMistakes` and `m_fMistakeChance`; a mistake while a station is worked raises a hidden strike counter; at the limit a delayed blast hurts and burns the station's loaded batch and raises site heat to the cap. The player gets only a faint cue (smoke colour); the explosion uses vanilla explosion effect prefabs (check; `asset_search` for "explosion" or "Explosion" before choosing).
  - Rotating buyer: a list of N buyer spots in config, one open at a time, rotating every K game hours; the open one is announced by a note at the fence or on a board; the price paid at the open buyer is a config multiple of the fence price. Informant list written on sale (a row per sale, readable by police): ties to #419 and the witness feature.
  - Heat and search find chances are the existing levers; do not add a second heat system.
- **Dependencies:** #416, #417, #418, #419 (built), the market curve (the buyer price can use the same service), #685 (new offence codes).
- **Proof:** LOGIC (strike counter, rotation clock, informant row), WORLD (a station run that blows up with a forced seed), manual two-player run (cook, fail, buyer rotates). 
- **Size and risks:** M. Risk: the plan says hard drugs are not a fit (`docs/roadmap/m5-outline.md`); the failure and rotating buyer must be applied to the already approved chains (moonshine, tobacco, counterfeit papers) and not introduce a drug.

#### Fishing
- **Goal:** Quiet civilian income from fishing, with optional smoking at a station. Layer: Framework (gather action with a timed bite, config) plus Flagship (species, places).
- **Overthrow reference:** No close match.
- **Our code to extend:** `Feature/Gathering/LF_GatherAction.c` (held gather action, used for seeds and ore), `Feature/Mining/` for tool-gated gathering (pickaxe) as the model for a rod gate (#650/#651), `Feature/Processing/LF_ProcessAction.c` (recipe rows: smoking), `Feature/Chains/` stations if smoking is a chain, `LF_TraderManager` for the sale, `Feature/Survival/` (food items that feed a survival stat), `Feature/Jobs/LF_HarvestJobType.c` (a job could wrap fishing).
- **Content spec:**
  - Props found in the base game: `Prefabs/Props/Naval/FishingGear_01.et`, `FishingBuoy_01.et`, `FishingNet_01.et`, `FishingNet_02.et`, `FishingNet_03.et`, `FishingRope_01.et`; sheds `Prefabs/Structures/Infrastructure/Naval/Sheds/FishermanShed_01/FishermanShed_01_base.et`; recreation `Prefabs/Props/Recreation/Beach/PedalBoat_01/PedalBoat_01_base.et` and `Paddle_01/Paddle_01.et`. No fishing rod prefab, no fish item, no fish meshes found (searches "Fish", "FishingRod", "Rod" returned only the gear above): **needs 3D harness** for the rod and 2 to 3 fish items (#670). Interim: the rod is a re-skinned existing tool, fish are food prefabs from `Prefabs/Items/Food` (placeholder).
  - Mechanic: cast at water (a `LF_FishingSpot` marker with a radius so we do not trace water surface; check whether a water test is available in the API), wait for a bite, strike inside the window, short reel-in as a held action with a timer; three misses scare the spot for a time. Species by player job level (`LF_JobsConfig` level step, #692), sold on the market curve or fixed (the reference server: fixed).
  - Places (Everon, check): a river mouth and the east coast. No coordinates known; verify in Workbench. Fisherman NPC sells the rod (cash).
- **Dependencies:** Skill XP and unlock gates (section 2), market service, #650/#651 (tool gate), 3D harness (#339, #670).
- **Proof:** LOGIC (bite and reel rules), WORLD (a scripted catch yields an item), manual (cast, wait, strike on a real client). Shot: the pier and rod.
- **Size and risks:** M (L with the new meshes). Risk: the reference server's minigame depends on a custom UI; ours must stay a held action plus a cue, since AGENTS.md input rules forbid new keys that clash and the UI kit is in flux.

#### Beehives and honey
- **Goal:** Hives on smallholdings yield honeycomb, processed into jars and sold. Layer: Flagship (placement, copy) on the existing gather and process systems.
- **Overthrow reference:** No close match.
- **Our code to extend:** `Feature/Farming/LF_FarmPlotComponent.c` and `LF_FarmPlots.c` (a growth clock per keyed plot, saved as the `farmplots` payload; reuse the keyed-plot persistence for a hive that refills), `Feature/Gathering/LF_GatherAction.c`, `Feature/Processing/LF_ProcessAction.c`, `Feature/Resources/LF_ResourceChainCatalog.c`.
- **Content spec:**
  - Base game: `Prefabs/Props/Agriculture/Beehive_01/Beehive_01_base.et`, `Beehive_01_blue.et`, `Beehive_01_green.et`, `Beehive_01_red.et`, `Beehive_01_small_base.et`, small colours. These exist as props; whether they carry an interactable component: check (likely none).
  - Needs: honeycomb and honey jar items: **needs 3D harness** or reuse a jar-like food prefab (check `asset_search` for "Jar" found nothing). Placeholder: a bucket (`Prefabs/Props/Civilian/Bucket_01.et`) is a prop only.
  - Mechanic: each hive has a refill clock (keyed like `FarmPlot`), up to 5 combs, one comb to a few jars at a processing bench or the market. Colours from the reference server are dropped; one honey good.
  - Places (Everon, check): near farms, for example the St Philippe orchard area (known: x 4450, z 10570) with a small row of hives; not marked on the map.
- **Dependencies:** Market service, #614 (farm plots, built), 3D harness (jars).
- **Proof:** LOGIC (refill clock), PERSISTENCE (hive state across restart), WORLD (gather yields combs). Shot: hive row.
- **Size and risks:** S to M. Risk: unattended hives invite abuse (anyone can empty them, the reference server accepts it); keep income low and bounded by the refill clock.

#### A collective-farm fuel depot feeding the fuel stations
- **Goal:** A depot holds real fuel stock that the fuel-delivery job moves to stations; a station sells only while it has stock. Layer: Framework (fuel station stock, plan section 2) plus Flagship (the depot site).
- **Overthrow reference (best match in this batch):**
  - `OVT:Prefabs/Structures/Military/FOB/OVT_FuelDepot.et`: a vanilla `FuelTank_02_green` with `SCR_FuelManagerComponent` (a `SCR_FuelNode`, `MaxFuel 10000`, `m_fInitialFuelTankState 0`, `m_MaxFlowCapacityIn FUEL_CARGO`, `m_eFuelNodeType 11`) and a `SCR_FuelSupportStationComponent` (priority HIGH, range 7) derived from `Prefabs/Components/SupportStations/Fuel/BaseFuelSupportStation.ct`. Port the prefab pattern (author a new `LF_` prefab over the same vanilla parent).
  - `OVT:Scripts/Game/Utilities/OVT_FuelUtils.c:473-492` `GetRefuelableCapacity(manager)` and `:501-520` `GetProvidableFuel(manager)` (sum over `SCR_FuelNode` by flag `CAN_RECEIVE_FUEL`/`CAN_PROVIDE_FUEL`), `:144-184` `FindFuelSourcesCovering/Near`, `:253` `FindBestFillSource`. Port-able with header; these are exactly the "how much does this station hold" and "how much room" reads.
  - `OVT:Scripts/Game/Data/OVT_FuelPricing.c:113` `ComputeFillPlan(litresNeeded, sourceAvailable, balance, pricePerLitre)` (litres clamped by stock and by money, cost derived from litres) and `OVT:Scripts/Game/Components/Controller/OVT_FuelRequestComponent.c:123` `RpcAsk_FillFuel`/`:256` `DrainFuelFromManager` (server drains the station on a fill). Adapt for the paid fill.
  - `OVT:Scripts/Game/Components/OVT_FuelSourceComponent.c` (free versus paid source marker) and `OVT:Scripts/Game/GameMode/Placeables/OVT_FuelDepotHandler.c` (server placement gate; deleting before charging): adapt the marker, ignore the base rule.
  - Persistence trap, recorded in `OVT:Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c:7612-7632`: vanilla `SCR_FuelManagerComponentSerializer` only walks `SCR_FuelNode`-typed nodes and skips a node whose fuel equals its initial state; an initial state authored as a fraction makes persistence a silent no-op. Test that the saved level survives.
- **Our code to extend:** `Feature/Jobs/LF_CivilianJob.c` (fuel delivery: `PAY_FUEL = 60`, `FUEL_PREFAB` jerrycan `{12D5AD21E383B768}Prefabs/Items/Fuel/Jerrycan_01/Jerrycan_01_item.et`, level 2 gate), `LF_JobSiteComponent.c`, `LF_JobAction.c`; `Worlds/DebugWorld/DebugWorld_Layers/Street_Fuel.layer` (fuel station building and the till `Roadmap_Fuel_Till`, `EL_FuelTill`), `Street_Jobs.layer` (`Roadmap_Job_FuelCan`); vehicles' fuel is vanilla. No station-stock or pump code exists today. The fuel till robbery is `Feature/Crime/LF_RobAction.c`.
- **Content spec:**
  - Depot prefab: new `LF_FuelDepot.et` over `Prefabs/Structures/Industrial/Containers/FuelTanks/FuelTank_02/FuelTank_02_green.et`, with pump `Prefabs/Structures/BuildingParts/Industrial/FuelPump_01/FuelPump_01.et` and sheds `Prefabs/Structures/Agriculture/Barn/Barn_E_01/Barn_E_01.et` or silo `Prefabs/Structures/Industrial/Containers/Silos/Silo_01/Silo_01.et` for the collective-farm look.
  - Stations: the town fuel station (`Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/FuelStation_E_01_V1.et`) needs a scripted fuel node with a finite initial level.
  - Loop: the fuel job fills jerrycans at the depot and pours into the station (litres move from depot to station), pay per litre moved; the station sells only while stock remains; a delivery truck variant carries more (a vanilla fuel truck prefab: not found by search; check, Ural4320 folder, `Prefabs/Vehicles/Wheeled/Ural4320/VehParts/CIV/Ural4320_fuel_tank_CIV_fuel_*.et` are tank parts).
  - Numbers (to define): depot 10 000 L (as Overthrow), station 2 000 L, a jerrycan 20 L, price per litre via config (the reference server keeps petrol as the one self-recovering good, so the station's buy price recovers).
  - Where (Everon, check): a depot at a farm cluster on the vanilla map (St Philippe farm area is known), stations in towns; 1989 reading: the kolkhoz-style supply point.
- **Dependencies:** Fuel station stock (section 2), market service (#686 for the no-profit loop), section 1 persistence note (vehicle fuel is vanilla; station stock is new), #627/#729 (till).
- **Proof:** LOGIC (clamp and cost math, port `ComputeFillPlan` tests), PERSISTENCE (stock survives restart, mind the trap above), WORLD (pour jerrycan into station raises level, a fill drains it), manual (a real fuel run: depot to station, then fill a car). Shot: the depot.
- **Size and risks:** M. Risk: the persistence trap; vehicles refuel through the vanilla support station system, so "refuel only while stock" must hook the support station, not our own action (api check `SCR_FuelSupportStationComponent`).

#### Village texture: chess, a dirt race loop, a shooting range, a community hall
- **Goal:** Free social places that make the town feel lived in. Layer: Flagship only, except the race runner (Framework, section 2).
- **Overthrow reference:** No close match for any of the four (no games, no races in the tree).
- **Our code to extend:** `Worlds/DebugWorld/DebugWorld_Layers/Village_Buildings.layer`, `Village_Dressing.layer`, `Village_Roads.layer` (the village, see `docs/roadmap/m1-25-village.md`); `Feature/Signs/` for notices; `Feature/Toys/LF_JumpPad.c` (a precedent for small playful props); `Feature/Interaction/` (check the sit and interact pieces); `Feature/Quests/` for a social errand.
- **Content spec:**
  - **Chess:** no chess prefab or mesh in the base game (search "Chess", "Chessboard", "Checker" found only `PortableCabin_E_01_checkers.et`, a building). **Needs 3D harness** for a board and pieces. Mechanic: a two-seat table, a server-held game state, moves by interaction. Large UI; defer to after the 3D pipeline and the UI kit. Seats: `Prefabs/Props/Civilian/PicnicTable/PicnicTable_01.et`, `Prefabs/Props/Furniture/Stool_01.et`.
  - **Dirt race loop:** needs the Race runner (entry fee into a pot, checkpoints, record board, plan section 2) and a loop of existing dirt track around the village. Props: `Prefabs/Props/Roadblocks/RallySafteyTape_01/RallySafetyTape_01_4m.et`, `ConeTraffic_01_red.et`, `Prefabs/Compositions/Misc/SubCompositions/VehicleGarbage_TireStack_02.et` and `TireStack_M939A1_02.et` as tyre walls, a start flag `Prefabs/Props/Fabric/Flags/Flag_1_2_Everon_01.et`. Record board: a chalk board sign.
  - **Shooting range:** the firearms test site (above), free practice mode.
  - **Community hall:** a social hub. Use a vanilla cultural building (`Prefabs/Structures/Cultural/Chapels/Chapel_01/Chapel_01_base.et` exists; a hall building: not found, check `asset_search` "Community" returned nothing) with the notice board `Prefabs/Props/Civilian/NoticeBoardCork_01/Filled/NoticeBoardCork_USSR_01_filled_V1.et`, a radio, and the town gazette board once Press exists (section 2). Function: where jobs, rules and the gazette are posted, where the police application desk could sit.
  - Places (Everon, check): the hall at the pack town's square; the race loop on a dirt road near a farm (St Philippe area known).
- **Dependencies:** Race runner (section 2), Gazette (section 2), 3D harness (#339, #670) for chess, #642/#643 in-game handbook for the hall's posters.
- **Proof:** Layer placement checks with `tools\cli validate` and `agent-box` area shots; the race runner brings its own tests. Manual: play a lap, read the board.
- **Size and risks:** S for hall and range dressing, M for the race, L for chess. Risk: chess is the only one that needs a new mesh and a new UI; keep it last.

---

### Section 4: live ops and community (M4)

#### Police application gates
- **Goal:** Gate police applications on playtime, a cooldown after rejection, and an expiring trial. Layer: live ops (server rules) on a Framework check.
- **Overthrow reference:** No close match (no applications). Admin gate only: `OVT:Scripts/Game/Components/Controller/OVT_AdminCommandsComponent.c:119` `RegisterChatCommands` (idempotent chat command registration with a remove-then-insert guard and an admin-only check); adapt for `!papprove`-style staff commands if more are added.
- **Our code to extend:** `Feature/Police/LF_PoliceApplicationRules.c` (`RefuseSubmit(onRoster, hasOpenApplication)`, `CanApprove(isAdmin)`), `LF_PoliceApplications.c` (`Submit`, `Approve`, `ApproveInstant`, a text file store with `|`-separated rows and an `LF_PoliceApplication` with status constants; `Load`/`Save` at 167 and 202), `LF_PoliceApplyAction.c`, `LF_PoliceApplicationForm.c`, `Feature/Whitelist/LF_Whitelist*.c` (roster), `LF_MissionHeader.m_bLF_InstantPoliceApproval` (playtest skip); `docs/features.md` row for #128. Admin check: `LF_PlayerWhitelistComponent` chat `!wadd` uses `EPlayerRole.ADMINISTRATOR` (`docs/features.md` around line 1087).
- **Content spec:**
  - Playtime gate: needs #692 (per account and per job playtime, `priority:next` because it cannot be backfilled). Config `m_iMinPlaytimeHours` (the reference server: 50 civilian and police combined); a refusal key `#LF-PoliceApp_Error_Playtime`.
  - Cooldown after a rejection: add a `rejectedAt` field and `m_iCooldownHours`; the file format is positional pipe-separated (`parts.Count() != 6` at `LF_PoliceApplications.c:186`), so a new field means a version bump of the file and a tolerant reader.
  - Trial period that expires (#128, #314): a trial flag with `trialEndsAt`; an expired trial removes the roster entry through the whitelist API. Server clock source: check (world time versus wall time; persistence uses wall-clock seconds in `LF_PoliceRanks` duty tracking? check).
  - Staff review (#314): the chat approve path stays admin-only; the Game Master panel can list open applications (`UI/GM/LF_IncidentGmPanel.c` is the model).
  - Rules copy: where the rejection reason is shown to the applicant (a slip at the desk, `LF_JailSlip` pattern).
- **Dependencies:** #692, #128, #314, #312.
- **Proof:** LOGIC (`RefuseSubmit` extended: playtime, cooldown, trial expiry), PERSISTENCE (applications file survives a restart), manual on a live server (apply, reject, wait, re-apply).
- **Size and risks:** S for cooldown and trial, M with playtime. Risk: the playtime counter does not exist yet and cannot be backfilled, so #692 ships first.

#### Moderation process and appeals
- **Goal:** Tickets-not-chat reports, a three-level staff ladder, an appeal path and a ban record. Layer: live ops (process and docs), with light Framework (an appeals record) later.
- **Overthrow reference:** `OVT:Scripts/Game/GameMode/Managers/OVT_NotificationManagerComponent.c:108-129` `SendExternalNotifications` posts a localised message to a configured Discord webhook through `GetGame().GetRestApi().GetContext(url)` and `context.POST`, with success and error callbacks (`:131-146`). Adapt for a staff-only webhook (URL from server config, never hard-coded): relay a flagged report or an admin action to the staff channel. Do not copy its `content=` body without escaping.
- **Our code to extend:** the incident stack is already built: `Feature/Moderation/LF_IncidentLog.c` (#239; death, damage, cash, downed, holdup, aim, admin, `nlr`; saved `incidents` payload; 14-day retention), `LF_IncidentConfig.c`, `LF_IncidentGm.c` plus `UI/GM/LF_IncidentGmPanel.c` (#240), `LF_IncidentFlags.c` and `LF_IncidentReportCommand.c` (`/report`, flags until `!handled <number>`; #241), `LF_AdminAction.c` (#246), `LF_Zones.c`. Staff terminal: #654/#655 (M5). `docs/community/discord.md` and `docs/community/support.md` exist; `docs/community/bot-playbook.md` for the bot.
- **Content spec:**
  - Docs: server rules (#312), moderator guide (#313), appeals channel and ban record (#315). Reuse the reference server's good rules only: precise tickets, evidence stays a week, no @-mentions, three escalation levels (moderators, operators, administrators), appeals reviewed by a different staff member. Record bans by account uid, because whitelists and bans key on it (decision 2026-11-09).
  - Staff powers: only administrators have in-game powers (matches the reference server); moderators work from the incident view.
  - Webhook relay (optional): a `m_sStaffWebhookUrl` in the server config; relay `LF_OnAdminAction` and new report flags to the staff channel. Fail-safe: a missing or failing URL logs one `LF_Debug.Error` and does nothing else.
  - The ban record: a plain file keyed by uid plus a reason and the staff uid; enforced through the whitelist or kick path (check how `LF_WhitelistManager` removes access).
- **Dependencies:** #312, #313, #315, #316, #654/#655, #246.
- **Proof:** Docs reviewed in `docs/`; a manual staff dry run on a live server (a mock report, the flag, the incident view, the appeal). LOGIC test for any new record if code ships.
- **Size and risks:** S (docs), M with the webhook. Risk: privacy; incident rows carry uids, so the webhook must send only a case number and a place, never names.

#### Clip-recording guide with an in-game time and server stamp
- **Goal:** A small corner stamp (server name, account id, a clock with date) so a clip can be matched to the incident log, plus a short per-platform guide. Layer: live ops (guide) with a tiny Framework HUD option.
- **Overthrow reference:** HUD pattern only: `OVT:Scripts/Game/UI/HUD/OVT_EconomyInfo.c` (a `SCR_InfoDisplay` subclass with `OnInit`, ticked `UpdateValues`, widgets from `OVT:UI/Layouts/HUD/`). Our own `Scripts/Game/HUD/LF_LawlessHud.c` is the same pattern and closer to the tree: use that.
- **Our code to extend:** `Scripts/Game/HUD/LF_LawlessHud.c` (`SCR_InfoDisplay`, `OnStartDraw`, `UpdateValues`, widget `Tag` from `UI/Layouts/HUD/LF_LawlessTag.layout`; the HUD is declared on `Prefabs/Characters/Core/Character_Base.et`), `HUD/LF_BetaHud.c`; the world clock is read as `TimeAndWeatherManagerEntity` (`Feature/Film/LF_FilmScene.c:141` `GetClock`, `Feature/Character/Spawning/LF_HospitalBill.c:247`) , and the account uid from `LF_Utils.GetAccountUID`; the incident log keys rows by time (`LF_IncidentLog`), so the stamp must show the same clock the log uses (check which: world time or session seconds).
- **Content spec:**
  - Stamp text: "<server short name> | <account id suffix> | 1989-MM-DD HH:MM" in a bottom-right corner, 60 percent opacity, off by default, on through a config flag (`m_bClipStamp`) or a per-player option. Show only the last 6 characters of the account id so a clip does not leak the full uid (check with #241 report format).
  - Server name: from `LF_MissionHeader` (check field) or the server config.
  - Guide: a one-page doc and a website page (`website/src/pages/Support.tsx`, `website/src/pages/Docs.tsx` exist): whole screen, stamp visible, link stays up a week, report with the incident number from `/report`. No attachments rule is Discord-side. Note that our server-side incident log already covers most moderation, so clips are a backup (plan page notes).
  - Platform steps: PC, Xbox, PlayStation, short.
- **Dependencies:** #312/#316 (where the guide is linked), the incident log (#239, built). 
- **Proof:** LOGIC test of the stamp formatter; shot of the stamp in a `--profile full` session (look at it, commit to `website/public/docs-images/<issue>/`); manual: record a clip on a client and match the stamp to an incident row. Add a line to `docs/playtest.md`.
- **Size and risks:** S. Risks: console clock timezone (use the world clock, not the wall clock, and say so); extra HUD cost every frame (tick once a second, as `LF_LawlessHud` does at 0.25 s).

#### Discord verification
- **Goal:** Link an in-game account to a Discord account for support and whitelists. Layer: live ops (bot) with a small Framework token feature, and optional.
- **Overthrow reference:** `OVT:Scripts/Game/GameMode/Managers/OVT_NotificationManagerComponent.c:108-146` posts to a Discord webhook (outbound only, via `RestContext.POST`). Adapt the REST call and callbacks for sending a verification code outbound. It does no inbound linking and carries no identity. Ignore the rest.
- **Our code to extend:** `tools/discord-mcp/` (the Discord MCP server, check its contents), `docs/community/bot-playbook.md`, `docs/community/discord.md`; the account uid `LF_Utils.GetAccountUID`; `Feature/Whitelist/LF_WhitelistManager.c`; staff terminal #654/#655; #696 Discord welcome relay (not built).
- **Content spec:**
  - Flow: in game, "Get link code" at a desk (or chat `/link`) shows a short random code bound to the account uid for 10 minutes; in Discord the member pastes it to the bot, the bot stores `discordId <-> accountUid`.
  - Perks (ours, not the reference server's paycheck): faster support, a whitelist application requirement, a badge. No in-game bonus pay (decision against passive paycheck; the reference server pays outside the world, we do not).
  - Where the mapping lives: the bot's store, not the game save. The game only holds a hash of the code until it expires. Use the REST call pattern above for a server-to-bot notify; secrets in server config (golden rule: no generated or secret files committed).
- **Dependencies:** #696, #654/#655, M4 staff process (#313, #314).
- **Proof:** Manual: link on a test server and test Discord; LOGIC for code generation and expiry.
- **Size and risks:** M. Risk: it is a server-owner concern for a framework (plan: optional); do not put Discord assumptions in the core, and respect console policies on external links.

#### FAQ and "external mods" explanation
- **Goal:** One short FAQ page in the site and one in game; a plain explanation of why only our own mods are allowed. Layer: live ops (docs).
- **Overthrow reference:** `OVT:Configs/FieldManual/FieldManualConfigRoot.conf` plus `OVT:Configs/FieldManual/Categories/` and `OVT:Scripts/Game/Global/OVT_FieldManualHelper.c` (`Open(titleKey)` deep-links into the vanilla Field Manual UI with a safe fallback on an unknown key). The in-game FAQ and handbook (#642/#643) can be Field Manual entries authored as config. Adapt the pattern; the helper's `SCR_FieldManualUI` modded class sits in `OVT:Scripts/Game/UI/Modded/` and is war-independent, copy only if deep links are wanted.
- **Our code to extend:** `website/src/pages/` (`Support.tsx`, `Docs.tsx`, `Join.tsx`; no FAQ page exists, check `docs/community/outreach/faq.md`, which is for outreach, not players), `docs/community/onboarding.md`, `docs/community/support.md`; `Feature/Signs/` for a town board version; the plan's #642/#643 handbook.
- **Content spec:**
  - Questions (from our own rules, not the reference server's): no donations or purchasable perks; no wipes announced as policy (check M4 wipe decisions #556: wipes follow each milestone until a launch date); progress is per server; how to report (`/report` and a Discord ticket); lock and flip shortcuts (verify the actual keys in `Configs/` before printing); stuck under the map (the move is staff-only today; do not copy the reference server's "switch server").
  - External mods: base game plus our own addons only; the Workshop dependency list is the answer; why (collisions with `EL_`-prefixed classes: golden rule 5, and unverified behaviour).
  - One page, short answers, links to rules and the clip guide.
- **Dependencies:** #312 (rules), #642/#643, the clip guide, the Workshop page (#318).
- **Proof:** Site build (`website/`), reviewed copy; in-game: open the Field Manual entry on a client.
- **Size and risks:** S. Risk: facts that drift (wipes, keys); keep one source and link from the site and game.

---

### Section 5: homes and land (M6)

#### Housing built by hauling, with build stages
- **Goal:** Buy a plot, haul materials to a fenced site, and finish the house in stages together. Layer: Framework (staged site, property record) plus Flagship (plots, tiers, prices, materials).
- **Overthrow reference (strong):**
  - Construction sites: `OVT:Scripts/Game/Components/OVT_ConstructionSiteComponent.c:1-150` (a site stores which buildable and prefab were ordered, replicates the buildable index and name, persists through the buildable's serializer), `OVT:Scripts/Game/UserActions/OVT_BuildFromSiteAction.c:1-157` (an action whose label reads "Need N more Timber" until the piles cover it, then "Build X"; the server re-derives every number, label cached 1 s), `OVT:Prefabs/Structures/OVT_ConstructionSite.et` (vanilla `Prefabs/Props/Construction/ConcreteMixer_01.et` plus the two actions), `OVT:Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c:784` `BuildItem` (puts a site down and charges on finish) and `:651-775` `PlaceItem` (server checks funds, a 50 m range from the builder, item limits; spawns, stamps owner, runs a handler, tracks for persistence), `OVT:Scripts/Game/Components/Controller/OVT_ResourceRequestComponent.c:815` `RpcAsk_BuildFromSite` (server consumes piles and finishes).
  - Requirements config: `OVT:Scripts/Game/Configuration/OVT_BuildablesConfig.c` (`m_aResourceRequirements`, `m_SitePrefab`) and `OVT:Scripts/Game/Configuration/OVT_ResourcesConfig.c` (resource id, volume, price), `OVT:Scripts/Game/Data/OVT_ResourceRules.c:183-229` (`ScaleRequirement`, `RepairRequirement`), `:234-265` (`IsSatisfied`, `AmountOf`).
  - Hauling objects: `OVT:Prefabs/Props/Resources/OVT_ResourcePile.et` with `OVT_ResourcePileComponent` (dropped crate pile merges nearby, `SelectMergeTarget` at `OVT_ResourceRules.c:132`), and `OVT:Scripts/Game/Components/OVT_ResourceCargoBedComponent.c` (a truck shows crate stacks on its bed from the replicated ledger, closes the bed seats while loaded).
  - Ownership: `OVT:Scripts/Game/GameMode/Managers/OVT_OwnerManagerComponent.c:78-185` (owner and renter keyed by persistent id and a position, `SetOwnerPersistentId`, `IsOwner`) and `OVT_RealEstateRequestComponent.RpcAsk_BuyBuilding` (`:145`: the server finds the building from the caller's feet, `re.GetNearestBuilding`; the client never names it; checks ownership, takes money, `SetOwnerPersistentId`). `docs/overthrow-survey.md` "Housing ownership" already says copy this.
  - **Gap in Overthrow that we must write:** a site finishes in ONE step (all requirements, then spawn). There are no phases. Our four stages (excavation, foundation, walls and roof, doors and windows) are our own design.
  - Overthrow's resources are abstract litre ledgers in a crate pile, not carried items; we want physical hauling, so adapt the model (a site that accepts deposits, a requirement list per stage, a visible state) and not the storage.
- **Our code to extend:**
  - Hauling and work: `Feature/Chains/LF_ChainStationComponent.c`, `LF_ChainStationAction.c` (modes load, work, collect) and `LF_StationProgress.c` (`AddWork`, `Rounds(needed, workers)`: more hands finish sooner, saved by station key, `chainstations` payload). A build stage is a chain step: inputs deposited, work units, shared among workers. The same primitive gives the reference server's "no timers, speed is how fast people haul". Carry items: `Prefabs/Items/Resources/{Sandbag,GravelBag,CementBag,Brick}.et`, `Prefabs/Resources/WoodCutting/WoodPlank.et`; the quantity feature (`Feature/Quantity`, `LF_QuantityComponent`; `LF_InventoryUtils.AddAmount`); carry meshes (test `items/carry-meshes`); production sources already in DebugWorld: `LoggingChain.layer`, `CementChain.layer`, `GravelChain.layer`, `SandChain.layer`.
  - Ownership: `Feature/Houses/LF_HouseManagerComponent.c` (house id, lock state, door sync), `LF_HouseSaveData.c` (`LF_HouseLockRecord`, `VERSION = 1`), `LF_KeyComponent.c`, `LF_LockComponent.c`, `Prefabs/Items/HouseKeyItem.et`, key model #613; `Feature/Organizations/LF_Org*` for the gang tier. Scope card: `docs/roadmap/scopes/later/housing.md` (buy at an office, keys, interior storage; furniture and raids out of scope there). Money: `LF_MoneyUtils.RemoveAmount`, bank: `Feature/Banking`.
  - Placement: `Components/BuildingSystem/LF_BuildingManagerComponent.c` (`PlaceHeldBuilding`, 5 m range) for the fence and the site marker.
- **Content spec:**
  - Plots (Everon, check): lots in the pack town and a few at the edge; plot markers are entities with a plot key (like `m_sPlotKey` on `LF_FarmPlotComponent`). Prices in config.
  - Stage definitions (proposal): 1 Excavation (shovel work units, no materials), 2 Foundation (gravel bags, cement bags), 3 Walls and roof (planks, bricks), 4 Doors and windows (planks, a door item). Counts and tier tables are config; the reference server's seven materials map to our five items plus shovel work.
  - Props per stage: a fenced marker with `Prefabs/Props/Construction/PlankPile/PlankPile_01_Large.et`, `Prefabs/Props/Construction/GravelPile_01/GravelPile_01_large.et`, `Prefabs/Props/Construction/ConcreteMixer_01.et`, `Prefabs/Props/Civilian/Wheelbarrow_01.et`, `Prefabs/Props/Military/Camps/ConstructionMaterialPacked_WoodPlanks_01_USSR.et`; the house itself is a vanilla village house prefab per tier (`Prefabs/Structures/Houses/...`, which prefab per tier: check in `asset_search`; DebugWorld uses `House_Town_E_2I03_Base`, `docs/features.md` line 403), shown as an incomplete shell by hiding parts per stage or by swapping prefabs (the swap is simpler: stage n entity replaced by stage n+1). Building shell stages need real assets: **needs 3D harness or prefab swaps of vanilla ruined and unfinished variants** (check vanilla "UnderConstruction" houses).
  - Tiers: Tier 1 hut to Tier 3 gang-only, price, tax and slots in config (the section A property record shape: plot, owner, build stage, contents). Tax: prepaid blocks; unpaid tax releases the plot (money sink, decisions 2026-11-08). Release must not delete player belongings silently: store them at a clerk or in the impound pattern (a decision).
  - Persistence: a new payload `properties`, rows `{plotKey, ownerUid, tier, stage, workDone, deposited[], taxPaidUntil}`; a house site is also an entity: either a `SelfSpawn 1` persistence config like Overthrow.conf:170-225 or a world-placed plot that is a record only (recommended: records only, plots are placed in the layer, the shell swaps at load).
- **Dependencies:** Section 1 "property record" (M3, reserve the shape), M5 crafting bench for planks, #613 key model, #656/#657, a tax and sink decision, M6 milestone itself; farming under it is the next item.
- **Proof:** LOGIC (stage rules, requirement arithmetic, tax), PERSISTENCE (mid-build site survives restart with deposited materials), WORLD (a scripted builder walks stages), manual two players hauling on a dedicated server and a restart mid-stage.
- **Size and risks:** L. Risks: shell visuals per stage (assets), deposited items cannot persist as entities (store counts, not items), grief (a stranger can finish someone else's site: the site is owned and only the owner and invited can deposit, a rule to decide), and plot tax purging (unowned items).

#### Growing hidden crops

- **Decided (2026-11-11):** planted crops are a sidecar payload, not part of the M3 pass.
- **Goal:** Plant seeds on open ground, water them, and risk theft; other players are the threat. Layer: Framework (crop catalogue, free planting) plus Flagship (which crops).
- **Overthrow reference:** No close match for crops. Placement and persistence of spawned objects: `OVT:Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c:651-775` `PlaceItem` (server validates range and limits, spawns via `OVT_WorldUtils.SpawnEntityPrefabMatrix`, stamps `OVT_PlaceableComponent` owner, calls `OVT_PersistenceTracking.Track(entity)`; `OVT:Scripts/Game/Persistence/OVT_PersistenceTracking.c:26`), `OVT:Scripts/Game/Components/OVT_PlaceableComponent.c` (replicated owner id), `OVT:Scripts/Game/Configuration/OVT_PlaceablesConfig.c` (illegal flag: being seen placing raises wanted: adapt for an illegal crop), `OVT:Configs/Systems/Persistence/Overthrow.conf:170-225`. Adapt; ignore everything faction-related.
- **Our code to extend:** `Feature/Farming/LF_FarmPlotComponent.c` (one fixed plot per key: attributes `m_sPlotKey`, `m_sCropId`, seed, yield, growth stages as `LF_CropStage`, stand distance 3 m), `LF_FarmPlots.c` (tick every second, `ExportAll`/`ApplyAll`, `farmplots` payload, save version 23, row `LF_FarmPlotRow` {plot key, crop id, whole seconds grown}), `LF_FarmPlotAction.c`, `LF_FarmPlotRules` (pure stage maths, test `farm/plot-rules`); `Prefabs/Vegetation/Crops/FarmPlot_Cabbage.et`, `CabbageCrop.et`, `BaseCrop.et`; `Prefabs/Items/Seeds/CabbageSeeds.et`; `Components/BuildingSystem/LF_BuildingManagerComponent.c` (free placement of a held item, 5 m range); `Feature/Crime/LF_Murder.c` and zones for who may harvest; `LF_Zones` (plants in lawless areas). Base game crops: `Prefabs/Vegetation/Vegetables/CabbageCrop_01.et`; meshes for capsicum, potato, pumpkin, tomato exist (`Assets/Vegetation/Vegetables/*Crop_01.xob`, prefabs: check).
- **Content spec:**
  - Crop catalogue in config: `{id, seed item, growth seconds, water window (min, max seconds between waterings), lifetime after ripe, yield, legal flag, stage meshes}`; the reference server runs 5 seeds with 10 min to 24 h growth; ours: 2 legal (cabbage, potato) and 1 illicit if the maintainer approves (the plan excludes hard drugs; weed not in M5 outline, "check decision" before adding). Tomato was removed on purpose (poor asset, `docs/features.md` around line 787).
  - Free planting: new row key = a generated id, not a plot key. A planted crop is a runtime entity: the server records `{cropId, position, plantedAt, wateredAt}`, spawns the meshes on every client from the record, and removes them on harvest. Position rows are the persisted truth (`plants` payload, version bump), not entities; spawn them again on load (idempotent), tolerating "not there".
  - Rules: plant within 4 m of the player, 1.5 m apart, not from a vehicle, not inside a safe zone, per-character cap (config). Water: an action with a bucket (`Prefabs/Props/Civilian/Bucket_01.et` is a prop; a water item is new) taking longer than a second; plants die if the window passes; an Inspect action shows stage and water state.
  - Unprotected: anyone can harvest or trample a plant; a police officer can destroy an illegal one (ties to `LF_ConfiscateAction`).
  - Sprinkler (later): a placeable that waters in 15 m; placement via the building manager.
  - Hidden: no map marker; mild visual camouflage (low crop height) is a copy decision.
- **Dependencies:** Section 1 "planted crops" (decide and shape in M3), the placeable item system (`LF_BuildingManagerComponent`), #614 (built), M6.
- **Proof:** LOGIC (growth, water window, death), PERSISTENCE (a planted plant grows after restart and a harvested one does not return), WORLD (plant, advance time, harvest, theft by a second character), manual two players. Shot: crops in three stages.
- **Size and risks:** L. Risks: entity count (cap per character and a global cap; `LF_Perf`/`LF_PerfBudget` in `Scripts/Game/Core/` for budgets), position persistence versus ground height changes, client-local crop meshes (the plot already does "local crop copies on every machine but a dedicated server"; keep that model).

#### House spawn point
- **Goal:** Once a player owns a house, they may spawn there; otherwise the existing rules hold. Layer: Framework (spawn resolution) plus Flagship (the pick).
- **Overthrow reference (strong):**
  - `OVT:Scripts/Game/Components/OVT_SpawnPointComponent.c:1-60` (authored `PointInfo` points on a house, `GetSpawnPoint()` tries them in random order and returns the first clear of geometry, falls back to the origin; `HasSpawnPoints()`). Port-able, with header. Clearance boxes `PERSON_MINS/MAXS`.
  - `OVT:Scripts/Game/Utilities/OVT_WorldUtils.c:94` `IsPositionClear`, `:144` `FindSpawnPositionOutside(building, out pos)`, `:196` `FindSafeSpawnPosition`, `:210` `TryFindSafeSpawnPosition`. Adapt the collision tests (the engine call used inside should be read before copying, check).
  - `OVT:Scripts/Game/GameMode/Managers/OVT_RealEstateManagerComponent.c:798-816` `SetHome(playerId, building)`: never write the building origin (it is inside the shell); use the authored point, else a position outside the building.
  - `OVT:Scripts/Game/Respawn/Logic/OVT_SpawnLogic.c:846-895` `GetCreationPosition`: a stored last-known position beats home; an empty home is the zero vector and must not be spawned at; an unsafe home falls back to a safe location; `EnsureClearSpawnPosition` heals a stored home that collides. `OVT:Scripts/Game/Data/OVT_PlayerData.c:63-66` `m_vLastKnownPosition`. Copy the ordering: rejoin in place, then home, then fallback, and clear the last position on death.
  - `OVT:Scripts/Game/Components/OVT_ReconnectComponent.c`: keeps a disconnected character alive and hidden (BUG-086); relevant to #640 but a bigger change; ignore for now.
- **Our code to extend:** `Feature/Character/Spawning/LF_SpawnLogic.c` (`GetCreationPosition` at 375, `ResolveSpawnPoint` at 400 picks an `SCR_SpawnPoint` by role key, shared with character creation), `LF_HospitalBedComponent.c` and `LF_HospitalRespawn.c` (the precedent for "a component on a placed object is a respawn point", wake offset on the bed, one-shot pending flag), `LF_RespawnConfig.c`; `Feature/Houses/LF_HouseManagerComponent.c` (house id); `Feature/Debug/LF_SpawnHub.c` and `Street_SpawnHub.layer` (spawn hub, #607). Jail: a prisoner with time left already goes back to the cell (`LF_SpawnLogic`, `docs/features.md` line 1323), and that override must keep priority. `#640`/`#641` (rejoin where you logged out) is not built; the plan says rejoin-in-place covers today.
- **Content spec:**
  - Order: jail time left, then rejoin position (if #641 shipped), then the player's own house if the house id is owned by them (new), then the role spawn.
  - `LF_HouseSpawnComponent` (name proposed) on the house prefab with authored points; resolved by the house id stored on the character's record; an owned house is picked by a "Make this my spawn" action at the door (owner only), mirroring `OVT_SetHomeAction.c` (server takes the position from the caller, no coordinates sent).
  - Save: one string field `spawnHouseId` on the character, appended to the account payload with a version bump; on load, a house that no longer exists falls back with an `LF_Debug.Warn`, never aborts the spawn.
  - A death still respawns at the hospital (existing rule, `LF_HospitalRespawn`); the house is the first-login and logout rule, not the death rule, unless the maintainer wants otherwise.
- **Dependencies:** #640/#641, the housing item above (ownership), #613.
- **Proof:** LOGIC (the priority order as a pure function: jail, rejoin, house, role), PERSISTENCE (the spawn house survives a restart), WORLD (owner spawns at the house, a stranger does not). Manual: log out inside the house, rejoin, die, check the hospital.
- **Size and risks:** S to M. Risk: the order of overrides; every case where two rules claim the spawn must have a named test. Also interior spawns inside vanilla shells need clearance checks (a point inside a wall is the failure Overthrow healed, `EnsureClearSpawnPosition`).
