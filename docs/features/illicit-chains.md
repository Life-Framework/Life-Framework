# Illicit chains

### `LF_ChainCatalog` — `Feature/Chains/` (#416, M2 Underworld)
- ✅ A chain is config: `m_aChains` in `Configs/Resources/ResourceChains.conf` (the M1
  catalog, extended). A chain has a tier (1 to 5), steps in order, equipment, and heat settings.
  A step names the station type it needs, its inputs and outputs (the `LF_ProcessAction`
  recipe rows), the hands-on work units one batch takes (`m_iWork`), a licence
  (`LF_ELicenseType`, `UNEMPLOYED` = no gate), a minimum level in the job it names
  (`m_sLevelJob`, CIVILIAN by default; #749), and an equipment id.
  A pack adds a chain by overriding the `.conf`; there is no script.
- ✅ **A bad chain disables only itself.** `LF_ChainCatalog.Build` validates each row (no id,
  tier out of range, no steps, no station, no work, a malformed recipe, unlisted equipment,
  duplicate ids, negative heat, a prefab that does not load); a failure logs
  `[LFDebug:Chains] chain <id> disabled: <reason>` and the other chains load. Lookup is
  `FindChain` / `FindStep`; a disabled chain returns null.
- ✅ **Gates.** `LF_ChainGate.Evaluate` (pure) and `ForPlayer` (server) refuse a worker without
  the licence, below the level, or not carrying the equipment, in that order, with the keys
  `#LF-Chain_NeedLicense`, `#LF-Chain_NeedLevel`, `#LF-Chain_NeedEquipment`. Equipment is an item
  the worker carries (priced for a shop), so owning it persists with the inventory.
- ✅ **Heat settings** per chain: `m_fPerWork`, `m_fDecayPerSecond`, `m_fMax`, `m_fRangeM`.
  `LF_ChainHeat` does the arithmetic (`AfterWork`, `AfterDecay`, `RangeAt`); the per-site number
  and the trace that reads it come with the stations (#417) and the smell (#418).
- ✅ The core ships one neutral example, `cider` (tier 1, one `barrel` step, apples to juice).
- Test: `chains/config` (LOGIC).

### Hands-on stations — `Feature/Chains/` (#417, M2 Underworld)
- ✅ **A station is a component, not a timer.** `LF_ChainStationComponent` is one shape for a barrel,
  a still, a press or a cooker; the step it runs comes from config (`m_sChainId`, `m_sStepId`).
  A worker `Load`s a batch (the step's inputs leave their inventory, all or none), `Work`s it
  with a held action that adds one unit of work, and `Collect`s the product. No menu and no
  progress bar: the offer changes with the station state, and the work prompt says in words how
  far along it is (`#LF-Chain_Work_Fresh`, `_Mid`, `_Late`).
- ✅ **Progress belongs to the station.** Every accepted unit adds to the same number, whoever does
  it, so two workers on one barrel finish it in half the time, and three workers on three barrels
  clear three batches in the time one worker clears one. The server rate-limits each worker per
  station (`m_iMinTickMs`, 1200 ms against a 2 s hold), checks reach (`m_fReach`, 3.5 m), the
  hideout access when the barrel carries `LF_HideoutMachineComponent`, and the licence, level and
  equipment gates. Refusals toast `#LF-Chain_*` keys and log `[LFDebug:Chains]`.
- ✅ **Site heat** (`LF_ChainSites`): stations with the same `m_sSiteId` share a number that rises
  with accepted work (`m_fPerWork`), fades with time (`m_fDecayPerSecond`, computed when read) and
  caps at `m_fMax`. `GetHeat`, `GetRange` and `GetWarmSites` are what the smell and smoke trace
  (#418) reads. Heat is not saved; a restart starts every site cold.
- ✅ **Bus events** `LF_OnChainStep` (load, ready, collect) and `LF_OnChainWork` (each accepted
  unit, with the site heat and smell range). See `docs/architecture/events.md`.
- ✅ **A half-finished batch survives a restart.** Payload `chainstations`, save version 20,
  matched by the station's `m_sStationKey` (unique and stable, not the entity id). A saved batch
  that arrives before its station waits and is applied when the station registers; a batch for a
  chain or step the station no longer runs is dropped and logged. Loading twice converges.
- ✅ The core ships `Prefabs/Chains/LF_ChainBarrel.et` (the vanilla open metal barrel with the
  station component, the sound data and the three actions) and three of them on DebugWorld at
  `300 0 322`, `304 0 322` and `308 0 322` (`Street_Chains.layer`, site `Chains_Site`, chain
  `cider`: 4 apples to 1 apple juice, 8 units of work). Work plays the vanilla pick-up gesture and
  `SOUND_PICK_UP`; better animation and sound follow the animation pipeline (#340).
- ✅ `LF_ProcessAction.TryConsumeInputs` / `RestoreInputs` are the all-or-nothing input removal the
  instant processors already did, extracted so the stations share it.
- Tests: `stations/progress` (LOGIC), `stations/world` (WORLD), `persist/chain-stations-roundtrip`
  (PERSISTENCE). The suite's version checks now read 20.
- ✅ **Moonshine (#420, tier 1, config only).** `moonshine` in `ResourceChains.conf`: `mash` at a
  barrel (4 plums and 2 cabbages to 2 water bottles of wash, 12 work), then `distill` at a still
  (2 bottles of wash to 1 `Moonshine` bottle, #2001, 16 work). No gate; heat 3 per work, range 60 m.
  A third step, `pack` (#2441, station type `bench`, 6 bottles to 1 `LF_ContrabandCrate`, 10 work), makes the crate the
  convoy (barn to pub) carries; a bench is a `LF_ChainBarrel` placed with `m_sChainId "moonshine"` and `m_sStepId "pack"`
  (grid cell `slice-2441`; the town bench is #1935). Steps carry `m_bGoods`: `distill` sets it so the bottle stays
  chain goods for an officer search now that the crate is the last step. `Prefabs/Chains/LF_ChainStill.et`
  is the barrel prefab with the boiler mesh and `distill`. The hidden DebugWorld site
  (`Street_Moonshine.layer`, site `Moonshine_Site`) has three barrels at `380/384/388 0 300`
  and a still at `384 0 305`, about 150 m from the police station. Test: `chains/moonshine`
  (WORLD); `chains/moonshine-crate-pack` (WORLD) drives the bench. No save change.
- ✅ **Smoke and smell (#418).** `LF_ChainTrace` (server): the first accepted work wakes one 5 s
  timer that runs only while a site is warm or a station still smokes; nothing is checked per frame.
  Each sweep sets every station's smoke level from its site heat (`LF_ChainTrace.SmokeLevel`:
  none, then thirds of `m_fMax`; the level is a replicated int on `LF_ChainStationComponent`, so it
  changes only at a threshold and a late joiner sees it) and lets every player inside the
  heat-scaled range (`LF_ChainSites.GetRange`) notice the smell: on-duty officers get
  `#LF-Chain_Smell_Officer`, everyone else `#LF-Chain_Smell_Witness`, once per listener and site per
  60 s. Smoke is the vanilla `Smoke_Smokestack_Medium` (levels 1 and 2, scaled 0.5 and 1) or
  `_Big` (level 3), a local particle on every non-dedicated machine. Each notice is an
  `LF_OnChainSmell` on the bus; `LF_ChainSmellLead` turns it into a police service call
  (`#LF-Chain_SmellLead`, one per site per 5 min) so dispatch logs and radios it and the service
  board pins it. NPC witnesses do not smell yet. Test: `chains/trace` (LOGIC). Heat is still not
  saved; no save change.
- ⚠️ Not built: nothing in this section. Manual: three
  players on three barrels, see the PR.

