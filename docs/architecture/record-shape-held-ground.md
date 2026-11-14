# Record shape: held ground on the hideout (#751)

Design spec for the M3 reshape of the hideout record (epic #746): how long and
how strongly an organisation has held a site, and the member minimum before a
site scores. It fixes the saved shape that later behaviour fills: the scoring
tick and the flag at the site, the org desk read-out, pay-outs into the org
fund, and organisation conflicts (#858, M5). No behaviour ships with it.
Status: design, not built.

Rules this follows: `docs/persistence.md` (positional payloads, append only),
D5 and D6 in `docs/roadmap/m3-plan.md` section 6 (one PR per reshape, its own
version bump), and #761 (append-only versions, every older save loads).
Decided 2026-11-11: a minimum number of a group's members present before a site
scores, owner-configurable, checked per scoring tick.

## Today

- `LF_HideoutRecord` (`Feature/Hideouts/LF_Hideout.c`), `VERSION = 1` with a
  per-instance `m_iVersion`: hideout id, organisation id, locked.
- Payload `hideout`, save version 3, written with `context.Write(array)` by
  reflection. `LF_Hideout.ExportAll` writes only leased hideouts (organisation
  id not empty); `ApplyAll` skips a record with no id or `m_iVersion < 1`.
- Hideout ids are place ids and go through `LF_SaveIds.ResolveHideouts` on load
  (#965 stable ids).
- The save version on `main` is 32 (`buildstamp` last).

## Shape: a sidecar payload, not new fields on the record

Adding fields to `LF_HideoutRecord` changes the positional layout of the
`hideout` payload and breaks every older save, `m_iVersion` or not
(`docs/persistence.md`, "Rules"). The precedent on `main` is #750a
(`orgranks`) and #749 (`joblevels`): the record keeps its layout and the new
data is a row payload appended at the end.

- Live state goes on `LF_HideoutState` (the session map entry), so the scoring
  tick, the desk and the export read one object.
- A new payload `heldground` is appended to `RegisterPersistencePayloads`, one
  `LF_HeldGroundRow` per hideout with non-default state.

```
class LF_HeldGroundRow : Managed
{
	static const int VERSION = 1;

	int m_iVersion;            // VERSION at write
	string m_sHideoutId;       // key: LF_HideoutRecord.m_sHideoutId (a place id)
	string m_sHolderOrgId;     // organisation the hold and points belong to
	int m_iHoldSeconds;        // current uninterrupted hold, online seconds
	int m_iHoldPoints;         // banked points, 0..config maximum
	int m_iMemberMinimum;      // members present before the site scores; 0 = config default
}
```

## Fields

| Field | Type | Default | Owner | Written by | Read by |
|---|---|---|---|---|---|
| `m_sHolderOrgId` | string, organisation id | `""` (nobody holds it) | `LF_Hideout` | the scoring tick when a hold starts; cleared when the hold is lost or the organisation is removed | the tick (whose members count), `LF_OrgView` (the org desk), #858 conflicts (who holds what), the site flag colour |
| `m_iHoldSeconds` | int, seconds counted only while the holder's members are online and the site is uncontested | 0 | `LF_Hideout` | the scoring tick (adds, resets to 0 when the hold is lost) | the tick (`m_fHoldSecondsToScore` before points accrue), `LF_OrgView` |
| `m_iHoldPoints` | int, banked points, clamped to `m_iMaxPoints` | 0 | `LF_Hideout` | the scoring tick (adds `m_fPointsPerHour` pro rata); a pay-out into the org fund through `LF_OrgLedger` (subtracts) | `LF_OrgView`, the pay-out, #858 supremacy |
| `m_iMemberMinimum` | int, members | 0 = use the config default | `LF_Hideout` | the holding organisation's leader at the org desk, if per-site setting is chosen (question 1); otherwise never written | the scoring tick, every tick |

Config, not saved (the `zones` section, D8): `m_fPointsPerHour`,
`m_fHoldSecondsToScore`, `m_iMaxPoints`, `m_iMemberMinimumDefault` (and its
floor and ceiling if organisations may set their own), `m_bContestable` per
site, `m_fContestRadius`. Pack: which sites are contestable and the pay-out.

Not saved, on purpose:

- **Contested-by.** The issue named `m_sContestedByOrgId`. A contest is
  presence: nobody is in the radius after a restart, so the first tick
  recomputes it. Saving it would load a stale contest. It is a session field on
  `LF_HideoutState` only (see question 3).
- **Hold time as a float.** Whole seconds in an int; the tick keeps the
  fraction in a session accumulator. A float loses whole seconds after about
  194 days of hold.

## Save-version plan

- Append `heldground` at the end of `RegisterPersistencePayloads`. The save
  version becomes that list's length at merge time: 33 if this lands first, one
  more per persistence PR before it (#689 and #752a's `vehiclestate` may land
  first; read `RegisterPersistencePayloads` when the PR opens).
- One row in the version table in `docs/persistence.md` and one header line in
  `LF_PersistenceComponentSerializer.c`: "A version N-1 save includes ... and has
  no held-ground rows (#751): every site loads with no holder, no hold time and
  no points."
- The `hideout` payload, `LF_HideoutRecord.VERSION` and its layout do not
  change.

## Load, migration and old saves

- **Order.** `heldground` loads after `hideout` and `organizations` (both are
  earlier in the list).
- **Ids.** The row's hideout id goes through the same
  `LF_SaveIds.Resolve(KIND_PLACE, ...)` as `ResolveHideouts`, so a renamed site
  keeps its points. A row whose site is not in this world is kept in a held map
  and written back unchanged (the #965 "hold missing content" rule), so a pack
  that drops a site for one boot does not wipe its points (see question 4).
- **Holder checks.** A row whose `m_sHolderOrgId` is not a known organisation
  is cleared with `[LFDebug:Hideout] held ground at <site> dropped: organisation <id> is gone`.
  Points above `m_iMaxPoints` are clamped with an `LF_Debug.Warn` (a lowered
  cap is an owner choice, not an error).
- **Old saves.** A save below version N has no rows: every site loads
  unheld with zero time and points. Nobody held ground before #751, so that is
  exact, and there is nothing to convert.
- **Idempotent.** `ApplyHeldGround(rows)` replaces the held-ground fields of
  every state; a second run gives the same state.

## Replication and JIP

Nothing per tick replicates (#751: "No per-tick replication").

- The org desk reads hold and points on the server and sends them in the
  existing `LF_OrgView` publish to the owning client; that view already
  re-publishes on join, which covers a late joiner.
- The holder's members get a notification (`Feature/Notifications`) when a
  hold starts, is contested or is lost: events, not state.
- The flag at the site is world state every player sees: the behaviour issue
  owns it as one replicated value on the flag entity (the holder's colour index,
  `BumpMe` only on a change of holder) and must prove a late joiner sees the
  raised flag. The record stores the holder, so the flag is restored after a
  restart from `m_sHolderOrgId`.

## Test plan

Each test is one file, `// tier:` and `// red-proof:` as `AGENTS.md` asks.

| Test | Tier | Proves | Red-proof |
|---|---|---|---|
| `LF_Test_HeldGroundRoundTrip` | PERSISTENCE | rows through `SerializeStore` / `DeserializeStore`; holder, seconds, points and member minimum restored; an unheld site writes no row | drop `m_iHoldPoints` from the row copy |
| `LF_Test_HeldGroundLegacy` | PERSISTENCE | a save at version N-1 (and the e2e legacy save at 25) loads every hideout lease and lock unchanged and every site unheld | apply a holder when no row exists |
| `LF_Test_HeldGroundBadRow` | LOGIC | an unknown organisation clears the row with the log line; points over the cap clamp; a site missing from the world is held and written back | let an unknown holder through |

The scoring behaviour (`LF_Test_HeldGround`: accrual, contest pause, points
cap, offline time not counted, member minimum) belongs to the behaviour PR,
not to this reshape.

Restart proof: `tools\cli wt e2e <slug>` with a pass-1 site given a holder,
3600 s and 12 points; pass 2 finds them, and the hideout lease and lock are
unchanged.

Manual testing for the PR: none player-visible (no behaviour). Host, lease a
hideout, lock it, save, restart: the lease and the lock are as before.

## Open questions for the maintainer

1. **Who sets the member minimum?** "Owner-configurable" can mean the server
   owner (one `zones` config value, nothing saved) or the organisation that
   holds the site (a per-site value saved here). The row carries
   `m_iMemberMinimum` with 0 = config default so both work; if it is the server
   owner only, drop the field before the PR (adding it later costs a new
   payload).
2. **Is the holder always the leasing organisation?** If only the organisation
   that leases a hideout can hold it, `m_sHolderOrgId` duplicates the lease and
   can go. If any organisation can take an unleased or rival site (territory,
   #858), it must stay. Recommend keeping it: #858's conflicts read it.
3. **Persist the contest?** The spec does not save `m_sContestedByOrgId`
   (presence is recomputed on the first tick). Confirm, or say why a restart
   should remember a contest.
4. **What happens to points when a lease lapses or the organisation is
   disbanded?** The spec clears a row whose organisation is gone. A lapsed lease
   with the organisation still alive keeps the row. Confirm, or choose a decay.
5. **The camping exploit.** #751 suggests two online members or a cap on
   points per real hour. A cap per real hour needs a window start and a window
   count in the row (two more ints) now, or a new payload later. The member
   minimum may cover it; say whether the per-hour cap is wanted.
