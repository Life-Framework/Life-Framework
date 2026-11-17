# Saint-Philippe: state-0 readiness for the cutover

This page lists what must be true before the flagship cutover (#2212) boots the Everon island. #2212 stays
`hold` until every row below reads **ready**. Epic: #2201. Card: #2333.

**As of 2026-11-17, `origin/main` 584f772fd: not ready.** No slot is fitted yet, `town ready` does not
exist yet, and the 0grind RAM check has not been run.

## How to refresh this page

Re-run these and rewrite the tables. Refresh whenever a draft merges and before anyone unholds #2212.

```
tools\cli town check saint-philippe                  # the "town check" column and the summary line
gh issue list --label town-draft --state all         # the draft cards and their state
gh pr list --state all --limit 300 --json number,state,isDraft,headRefName   # draft PRs: head ws/town-draft-saint-philippe-<slot>
tools\cli town ready state0                           # once #2207 lands
tools\cli perf building <slug> --size <size>         # per-building lights and shadow casters
node agent-box/session.mjs perf-stress everon        # place counts within 60 m (docs/architecture/performance.md)
tools\cli host remote 0grind status                  # the flagship is up and empty before the RAM check
```

A slot is **ready** when its draft PR has been reviewed by the maintainer and merged, and
`town check saint-philippe` reports no error for it. Warnings of the form "kind X is in no system's
m_aPlaces" do not block a slot; they block `town ready` (#2207), which checks that the systems are
present.

## Slots (14, from `mapping.json`)

`town check saint-philippe` today: `SUMMARY town-check fail saint-philippe: 14 mapped slot(s), 28 error(s), 20 warning(s)`.
Every slot has the same two errors: there is no claim (`tools/town/claims/saint-philippe-<slot>.json`) and
no layer (`MainWorld_Layers/Place_saint-philippe_<slot>.layer`). Both arrive with the slot's draft PR.

| Slot | Building | Draft card | Building card | Draft PR | `town check` today | Ready |
|---|---|---|---|---|---|---|
| arrival | b45 town hall (MunicipalOffice_E_01) | #2312 (agent:working) | town hall kit #2259 merged; A pass #2309, #2311 open | none yet | no claim, no layer; Spawn, Clerk, LicenceDesk unused | no |
| hostel | b14 town house | #2321 | #2289 shell, #2290 build, #2291 and #2292 polish | none yet | no claim, no layer; HostelBed unused | no |
| cafe | b03 pub | #2322 | #2293 shell, #2294 build, #2295 and #2296 polish | none yet | no claim, no layer; CafeTable unused | no |
| bank | b79 Office_E_01 | #2313 (agent:working) | bank building #2279 merged; polish #2278 open | none yet | no claim, no layer; Bank, BankCounter unused | no |
| police | b32 police villa | #2319 | police station kit #2285 (in flight) | none yet | no claim, no layer; PoliceStation, PoliceDesk, Armoury unused | no |
| clinic | b39 town house | #2320 | clinic building kit #2286 (in flight) | none yet | no claim, no layer; MedicDesk unused | no |
| general-store | b12 household-wares shop-house | #2314 | playtest shop kit `general-store` (#2216 merged) | none yet | no claim, no layer; StoreCounter unused | no |
| clothing | b38 clothes shop-house | #2315 | playtest shop kit `clothing-store` (#2216 merged) | none yet | no claim, no layer; ClothingCounter unused | no |
| gun-counter | b48 east drugstore shop-house | #2316 | playtest shop kit `gun-store` (#2216 merged) | none yet | no claim, no layer; GunCounter unused | no |
| fuel | b107 fuel station | #2323 | #2297 shell, #2298 build, #2299 and #2300 polish | none yet | no claim, no layer; FuelTill unused | no |
| fence | b33 workshop | #2324 | #2301 shell, #2302 build, #2303 and #2304 polish | none yet | no claim, no layer; FenceYard unused | no |
| board | kerb of b38 (shared) | #2317 | none (street furniture) | none yet | no claim, no layer; TownBoard, MapBoard unused | no |
| taxi | kerb of b39 (shared) | #2318 | none (street furniture) | none yet | no claim, no layer; TaxiRank unused | no |
| parcel | b21 newspaper kiosk | #2325 | #2305 shell, #2306 build, #2307 and #2308 polish | none yet | no claim, no layer; ParcelDepot unused | no |

"Unused" is the `town check` warning that a kind is in no system's `m_aPlaces` (`LF_SystemCatalog.conf`).

## Place rows

| Check | Limit or condition | Source | Today | Ready |
|---|---|---|---|---|
| `town check saint-philippe` | 0 errors over all 14 slots | `tools/town/check.mjs` | 28 errors, 20 warnings | no |
| `town ready state0` | passes: every place of the state passes, systems present, the arrival route walks | #2207 (open) | the command does not exist yet (`unknown town step 'ready'`) | no |
| Lights on at night, within 60 m of the square | at most 24 (vanilla has 12) | performance.md section 3, "Per place" | not measured: nothing fitted | no |
| Shadow casters, within 60 m | at most 12 (vanilla density) | performance.md section 3 | not measured | no |
| Entities added over the vanilla map, within 60 m | at most 1,500 (vanilla square: 3,720) | performance.md section 3 | not measured | no |
| Per-building budget | every fitted building within its size row (lights, casters, no point-light shadows, no unset `CastShadow`) | `tools\cli perf building` | per building card; the bank (#2279) and town hall (#2259) shipped perf-budgeted | no |
| 0grind RAM (D17) | the plan has room for the island with players on it | see below | not run | no |

### The 0grind RAM check (D17)

The island costs much more memory than the playtest town. Figures from `docs/architecture/performance.md`
section 2, measured with one client on the pool laptop:

| Scene | Client private | Server private (`-logStats`) |
|---|---:|---:|
| Saint-Philippe square, vanilla Everon, night | 5,332 MB | 2,501 MB (961 MB) |
| Playtest gun store | 2,898 MB | well under 1 GB (56 MB) |

- **Server.** The flagship's plan has 16 GB of RAM (service 298627). One empty island session took
  2.5 GB on the laptop; 16 players, AI walkers and the fitted buildings add to that. The check passes
  when the server's memory with 16 players and the AI walkers on the island, read on the 0grind panel,
  stays clear of the 16 GB plan. `tools\cli host remote 0grind status` does not report
  memory today, so the reading comes from the panel.
- **Clients.** Every player's client holds about 5.3 GB on the island, against 2.9 GB in the playtest town.
  Announce the higher minimum spec to players before the cutover; the fitted buildings add little
  memory (lights and props were not measurable, performance.md section 2).
- **Order.** The playtest-town header stays the flagship until this row and every slot row are ready
  (`docs/architecture/everon-world.md`, D17).

## What this page does not decide

It neither unholds #2212 nor edits it. The lead unholds it when every row reads ready, after a
playtest on `main`.
