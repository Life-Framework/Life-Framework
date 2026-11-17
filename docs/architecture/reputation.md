# Standing: per-faction reputation

Design, 2026-11-17, for the maintainer to review with the [M6 outline](../roadmap/m6-outline.md).
Nothing is built yet. Player-facing, the system is called **standing**: how well the town, the police
and the underworld know you. Read it with the world bible's per-player gates
([`world-bible.md`](../everon/world-bible.md) "The progression arc") and the world plan's D6 and D12
([`everon-world.md`](everon-world.md)).

**Decided (maintainer, 2026-11-16).**

- A small system in M6, per faction: town, police, underworld (D6).
- Jobs-first: in M6, standing mainly unlocks better jobs and better pay (D12). Shop terms and the
  landlord come later (M8).
- Grounded, light RPG, no visible levels (D2). Progression reads as licences, standing, jobs and story.

## 1. What a player sees

| Moment | What happens |
|---|---|
| A job they cannot take yet | The sign-up greys out with a reason in words: "The harbour master doesn't know you yet." Never "Standing 2 needed" |
| A tier is reached | One toast: "People in town have started to trust you." Town and police only |
| They want to check | The clerk's "My file" dialogue line, and one line each on the ID card sheet: "Town: known. Police: no trouble." No bar, no number |
| Underworld standing | **Never shown.** Vince's greeting changes, a contract appears in his back room, a place turns up on your map as "heard of". You work it out |
| A contract job | A better run that puts standing at stake: finish it and you gain more, walk away and you lose some |

Tier words (pack copy, core keys are neutral):

| Tier | Town | Police | Underworld (never shown) |
|---|---|---|---|
| -2 | Not welcome | Known to us | Not trusted |
| -1 | Talked about | Under watch | Suspect |
| 0 | A newcomer | No record | Nobody |
| 1 | Known | No trouble | Known face |
| 2 | Trusted | Helpful | Reliable |
| 3 | Respected | A friend of the station | One of us |

## 2. Data model (core)

New feature folder `Feature/Standing/`. All state is server-side.

**Config** `Configs/Standing/LF_Standing.conf`, class `LF_StandingConfig`, owner key `standing` (the
config service, as `jobs.tracks` is):

| Row | Fields |
|---|---|
| `LF_StandingFactionDef` | `m_sId` (`town`, `police`, `underworld`), `m_sLabel` (`#` key), `m_aTierThresholds` (points, ascending, may be negative: `-300 -100 0 100 400 1000`), `m_aTierLabels` (`#` keys, one per tier), `m_bShown` (false for the underworld), `m_iDailyCap` (points a character can gain per in-game day) |
| `LF_StandingAwardDef` | `m_sAction` (`job_completed:PARCEL`, `fence_sale`, `witness_statement`, ...), `m_sFaction`, `m_iPoints` (signed), `m_fCooldownSeconds` |
| `LF_StandingGateDef` | `m_sGate` (a job, outlet or contract id), `m_sFaction`, `m_iMinTier` |
| `LF_StandingPayDef` | `m_sFaction`, `m_aPayPercentByTier` (`0 0 0 5 10 15`): the bonus on that faction's jobs only |

The core ships the three factions with neutral labels and conservative awards. The pack overlays the
labels, the tier words, the award table and the gates (`LF_ConfigOverlay`), and the server owner can
retune any of it in `config.json`. Nothing in core names a place or a person.

**Store** `LF_Standing` (static, like `LF_CrimeRecord` and `LF_JobLevels`): a
`map<string, ref LF_StandingRecord>` keyed by the character record key. A record holds one int of
points per faction id (clamped to the first and last threshold) and the session's daily-cap counters.

```
LF_Standing.Award(characterKey, action, units = 1)   // config-driven, cooldown and cap applied
LF_Standing.Add(characterKey, faction, points, reason) // for quests and dialogue; logs the reason
LF_Standing.Tier(characterKey, faction) -> int
LF_Standing.Passes(characterKey, gateId) -> bool, reasonKey
```

Every change logs `[LFDebug:Standing] <key> <faction> <old> -> <new> (<reason>)` and publishes
`LF_OnStandingChanged` (in `Feature/Standing/LF_StandingEvents.c`) only when a tier changes.

## 3. How standing is earned and lost (jobs-first)

Standing listens to the event bus. No feature calls it to report work; it subscribes, as the ledger does.

| Faction | Earned by (event) | Lost by (event) |
|---|---|---|
| Town | Any paid civilian run (`LF_OnJobCompleted`: parcel, fuel, taxi, dock, fish delivery, garbage, tanker); a factory shift; a licensed fish sale batch; a market stall day; a town contract | An arrest (`LF_OnArrested`, which is also the conviction stamp in `LF_ConvictionRecord`), a crime the police pin on you (`LF_OnSuspectIdentified`), an abandoned town contract |
| Police | A witness statement (`LF_OnWitnessStatement`); a stolen car recovered and returned (`LF_OnVehicleRecovered`); a ticket paid on time; a police contract (vehicle recovery, escorting the cash van in M7) | An arrest (`LF_OnArrested`; it is the conviction), a flagged logout while cuffed or fleeing (`LF_OnFlaggedLogout`) |
| Underworld | A fence sale (`LF_OnFenceSale`); a car delivered to the chop shop (`LF_OnVehicleChopped`); an illicit chain step (`LF_OnChainStep`); heist loot (`LF_OnHeistLoot`); one of Vince's contracts; unlicensed fish sold to the fence | A statement that identifies a suspect (recommend yes, see question R4); an abandoned underworld contract |

Rules:

- **Gain needs work.** No passive gain, no gain for logging in or standing in town.
- **Loss needs proof.** The town and the police lose trust only for what is proven: an arrest (the conviction) or an
  identification, never a mere accusation or a wanted level that is later cleared.
- **The factions are independent.** One event may move two (an arrest costs town and police and
  gives the underworld a little), but no rule says "underworld up means police down". A player can be
  trusted in town and reliable to Vince.
- **Daily caps** (per faction, per in-game day; 2 real hours) stop a grind loop: about 60 points a day,
  so Trusted (400) takes about a week of evenings and Respected (1000) about three weeks.
- **Cooldowns per action** (the `jobs.xpAwards` pattern) stop one repeated act from filling a day.

### Spending standing: contracts

"Spent through jobs" means standing is a stake, not a currency:

- A **contract** is a job run with a minimum tier (`LF_StandingGateDef`), better pay, and a standing
  stake. It is an ordinary `LF_CivilianJob` run, or a chain or quest step, with two extra fields:
  `m_sStandingGate` and `m_iStakePoints`.
- Finish it: the job pays, plus the faction's tier bonus, plus the award.
- Abandon it, let it time out or lose the cargo: the stake is taken (`Add(-stake)`), and the job
  logs why.
- Examples (pack): the harbour master's early crates (town, Known), the inland fish delivery (town,
  Trusted), vehicle recovery for the station (police, Helpful), a car to order for the chop shop
  (underworld, Known face), a package run for Vince at night (underworld, Reliable).

Standing is never bought with money and never transferred between players.

## 4. Gates: where standing is read

One gate check, `LF_Standing.Passes(key, gateId)`, used by the existing gated actions:

| Gate | Hook | Today |
|---|---|---|
| Job sign-up and contract runs | `LF_JobSignUpAction`, `LF_JobStartRunAction` | They read job levels; add the standing gate beside it |
| Catalogue outlets | The outlet's job and licence check (`#988`) | Later (shop terms, M8); the hook is reserved |
| Chain steps and quests | `m_sLevelJob` style fields on the step and quest | Add `m_sStandingGate` |
| Dialogue lines | A line's condition in the dialogue flow | Vince's and the harbour master's lines |
| Map discovery | `LF_MapKnown.Hear` on a tier change | A tier can make a place "heard of" (the chop shop at Known face) |

A refusal always names the faction and the tier word in its reason key, never a number.

## 5. Server authority and replication

- **Server only.** Awards come only from server events; clients never send standing.
- **The client knows only its own tiers.** On spawn and on each tier change the server sends
  `RpcDo_StandingTiers(array<int>)` to the owner's controller (`RplRcver.Owner`). Never an `RplProp`:
  that would show every client everyone's underworld tier. The underworld tier is still sent to its
  owner, because offers grey out on the client, but no screen draws it.
- **Gated actions re-check on the server**, as the job offers already do (`LF_JobStartRunAction.ClientRefusal`
  is a hint; the server decides).
- **Police tools never read standing.** It is not a record and it does not appear on the case board,
  the wanted board or a search. The hidden crime record stays the police-relevant data.

## 6. Persistence

Payload `standing`, appended to `RegisterPersistencePayloads` with a version bump
(`validate-persistence-version`).

| Field | Type | Note |
|---|---|---|
| version | int | Written first |
| count | int | Rows that follow |
| characterKey | string | The character record key, never an entity or `EntityID` |
| faction | string | The faction id, so a removed faction is skipped with a warning on load |
| points | int | Clamped on load to the current thresholds |

- Rows with 0 points are not written.
- Daily-cap counters and cooldowns are session state (as `LF_JobLevels` caps are). A restart resets
  them, which is acceptable at a 60-point cap.
- Load is idempotent: it replaces the store.
- Keyed by character, so a character wipe or an alpha wipe resets standing. Progress kept from the
  public launch follows the M4 wipe decision.
- Size: about 40 bytes a row, three rows per active character. 2,000 characters is under 250 KB.
- Proof: `wt e2e` (award, restart, read the tier back), never an in-session round trip.

## 7. Map integration

- Standing never draws on the map.
- A tier change can call `LF_MapKnown.Hear(player, placeId)` from the pack's gate rows, so a place
  comes to the player through people: the harbour master's cove at town Trusted, the chop shop at
  underworld Known face. `HIDDEN` places stay personal pencil marks (D-M5).
- Contract destinations are `PLAYER` shared marks and the waypoint (#2223 card M11), owned by the job.

## 8. Economy

Measured sources and sinks today (core configs and `LF_CivilianJob`):

| Source | Pay | Per hour, rough |
|---|---|---|
| Parcel run | 40 | 500 to 600 |
| Fuel run (delivery level 2) | 60 | 650 to 750 |
| Taxi fare (delivery level 3) | 80 | 850 to 950 |
| Police duty | 0.5 a second | 1,800 |
| Medic duty | 0.4 a second | 1,440 |
| Benefit (bank under 500) | 100 per 20 min | 300, only while poor |
| Gathering and selling (juice 15, iron bar 45) | per item | 400 to 800 |

Sinks: vehicles 700 to 8,000, the firearm permit 250, an organisation 500, fines up to 1,500, ID
replacement 50, fuel, food and kit, catalogue weapons up to 1,500.

What standing adds:

- **Pay bonus at most +15 %, on that faction's jobs only**, and only from tier 3 (Respected). A
  respected taxi driver earns about 1,050 an hour instead of 920. Police duty pay is not touched.
- **Contracts pay 1.3 to 1.6 times the job's base run** and are rarer (one at a time, a cooldown), so
  a top contract hour stays under police duty pay.
- **Stakes destroy no money**, only standing.
- Net effect: about +5 % on the civilian money supply at full adoption. The M6 market curve (#748) and
  the new sinks in M6 (repair kit, fishing gear and licence, market stall fees) absorb it. The ledger
  (`LF_Ledger`) gets the reason `standing-bonus` on the bonus part, so the effect can be measured after
  the first week.

## 9. Performance at 128 players

| Cost | Size | Why it is bounded |
|---|---|---|
| Event handlers | A few map lookups per event | Same path as the ledger; events are a few per player a minute. The bus budget (under 200 microseconds an event, `performance.md`) holds |
| Ticks | None | No decay tick; caps reset by comparing the stored day key on the next award |
| Network | One owner RPC of 3 ints on spawn and on a tier change | A tier changes a few times a week per player |
| Gate checks | One map lookup | On action use, not per frame |
| Save | Under 250 KB at 2,000 characters | Positional payload, zero rows skipped |

## 10. Build order (for the slice planner, not cards yet)

1. Store, config, award rules, LOGIC tests (pure: thresholds, caps, cooldowns, clamps).
2. Event subscriptions for the existing events, one per faction.
3. Persistence payload and a `wt e2e` pass.
4. Gate in job sign-up and run start; the refusal reason keys.
5. Contracts: the gate and stake fields on a run; one town contract in the pack.
6. Owner RPC, the ID card line, the clerk's "My file", the tier toast.
7. Pack: tier words, awards, the harbour master's and Vince's contracts and lines.

## 11. Questions

These are in the M6 outline's open questions (R1 to R5) with a recommendation each.
