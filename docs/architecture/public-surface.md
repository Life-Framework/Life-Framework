# Public surface

What a content pack may rely on. Everything else in the core is internal and may change
in any release. The machine list is [`public-surface.json`](public-surface.json); the check is
`tools/validation/validate-pack-manifest.mjs` (#963). Rule: [`core-vs-pack.md`](core-vs-pack.md).

## Pack manifest

A pack ships a `.conf` of class `LF_PackManifest` (flagship:
`addons/LF_Everon1989/Configs/LF_PackManifest.conf`) and its mission header names it in
`m_LF_PackManifest`.

| Field | Meaning |
|---|---|
| `m_sPackId` | the pack's addon id, for log lines |
| `m_sCoreMin` | lowest core version supported, `x.y.z`, inclusive; empty is no lower bound |
| `m_sCoreBelow` | first core version not supported, `x.y.z`, exclusive; empty is no upper bound |

When the running core (`LF_PackManifest.CORE_VERSION`) is outside the range, the core logs one
line and loads none of that mission's config overlays:

`[LFDebug:Packs] pack 'LF_Everon1989' refused: it needs core >= 0.1.0, < 0.2.0, this core is 0.3.0; its config overlays are not loaded`

A header with no manifest loads as before (the core's own test missions). A manifest that is
named but does not load as an `LF_PackManifest` also refuses.

## What is public

| Surface | Public names | Status |
|---|---|---|
| Config classes | the root `.conf` classes in `public-surface.json` `configClasses`, with their field names, plus the pack manifest and mission header | listed |
| Bus events | `Feature/<Area>/LF_<Area>Events.c` declarations | to be listed |
| Dialogue hooks and params | the dialogue config and action params | to be listed |
| Prefab slots | `LF_PrefabSlotsConfig` (`Configs/PrefabSlots/LF_PrefabSlots.conf`): a pack patches `m_Prefab` by slot id; the slot ids are the core file's `m_sSlot` rows | class listed; slot ids to be listed (#964) |
| Sound slots | `LF_SoundSlotsConfig` (`Configs/SoundSlots/LF_SoundSlots.conf`): a pack patches `m_Project` by slot id | class listed; slot ids to be listed (#323) |
| Residents | `LF_ResidentsConfig` (`Configs/Residents/LF_Residents.conf`): a pack adds, replaces or removes a resident by `m_sId` (home and work place kinds, prefab, timetable); a `replace` swaps the whole record | class listed (#2906) |
| Gang war rules and sites | `LF_OrgWarConfig` (`Configs/Organizations/LF_OrgWars.conf`): a pack sets the fee, answer window, length, cooldown and member minimum field by field; `LF_SitesConfig` (`Configs/Sites/LF_Sites.conf`): a pack adds, patches or removes a contested site by hideout id (#2687) | class listed |
| Property plots | `LF_PropertyConfig` (`Configs/Property/LF_Property.conf`): a pack adds, patches or removes a plot by `m_sPlotId` (tier and price; #2976); the house tiers stay core | class listed |
| Medic ranks | `LF_MedicConfig` (`Configs/Medic/LF_Medic.conf`): a pack adds, patches, replaces or removes a rung of the medic ladder by rank id (title key, pay per second, duty seconds to the next rung, kit); the core ships none, so the pack's rungs are the ladder (#3047) | class listed |
| Taxi fares | `LF_TaxiFareConfig` (`Configs/Jobs/LF_TaxiFares.conf`): a pack sets the floor, rate per metre, cap and short hop field by field (#3164) | class listed |
| Market goods | `LF_MarketConfig` (`Configs/Market/LF_Market.conf`): a pack adds, patches or removes a good's price curve by the good's prefab GUID, and may switch the market on with `m_bEnabled` on the overlay root (#3018) | class listed |
| Locker rent | `LF_LockerConfig` (`Configs/Lockers/LF_Lockers.conf`): a pack sets the rent price, rent period, grace days, reclaim fee and locker weight field by field on the overlay root (#3192) | class listed |
| Parked cars | `LF_ParkedCarConfig` (`Configs/Vehicles/LF_ParkedCars.conf`): a pack adds, patches or removes a parked civilian car by its locked prefab GUID; each parked-car marker picks one at random (#3365, #3532) | class listed |
| Fishing tables | `LF_FishingConfig` (`Configs/Fishing/LF_Fishing.conf`): a pack adds, patches, replaces or removes a species or a table by id (`m_aSpecies`, `m_aTables`; a cove is a table, a species lists its bite hours); rods, baits and the global numbers stay core (#3451) | class listed |
| Crops | `LF_CropCatalogue` (`Configs/Farming/LF_Crops.conf`): a pack adds, patches or removes a crop by `m_sCropId` (seed, growth and water window, harvest item and yield, two or more stage prefabs; a crop with a blank stage is dropped; #3176) | class listed |
| Premium buyers | `LF_BuyerDemandConfig` (`Configs/Market/LF_BuyerDemand.conf`): a pack adds, patches or removes a buyer's demand row by buyer id plus good (price, cap, refill per hour, optional curve good; #3267, #3269) | class listed |
| Market stalls | `LF_StallConfig` (`Configs/Market/LF_Stalls.conf`): a pack sets the root fields on an overlay (`m_bEnabled`, market day (0 is every day), hours, fee, listing and price limits, cut); a field it leaves out stays core (#3182) | class listed |
| Dirty money | `LF_LaunderConfig` (`Configs/Crime/LF_Launder.conf`): a pack sets the dirty-money switch, buyer period, cut, batch size and time, daily cap and informant fee and tier field by field; `LF_BuyerPrices` (`Configs/Crime/LF_BuyerPrices.conf`): a pack sets the reach and repeat gap and adds or replaces a price row by the chain good's prefab (#2922) | class listed |
| Courses | `LF_QualificationConfig` (`Configs/Qualifications/LF_Qualifications.conf`): a pack adds, patches or removes a course by id (faction, minimum rank, fee, study minutes, unlocks) (#3436) | class listed |
| Licence seizure | `LF_LicenceSeizureRules` (`Configs/Crime/LF_LicenceSeizure.conf`): a pack adds, patches or removes the rule a crime code takes licences by, with its ban days (#3436) | class listed |
| Compartment cars | `LF_VehicleCompartmentConfig` (`Configs/Vehicles/LF_VehicleCompartments.conf`): a pack names the vehicle prefabs that take a hidden compartment, its size and the chop-shop fitting price (#3436) | class listed |
| Heist jobs | `LF_HeistConfig` (`Configs/Crime/LF_Heist.conf`): a pack adds a job by site id, with a clock, a police minimum and ordered stage rows (kind door, vault or getaway, plus the world id it binds to); `LF_HeistKinds` names the components and gates the kinds resolve to (#3414) | class listed |
| Quest giver quests | `LF_QuestGiverConfig` (`Configs/Quests/LF_QuestGiverTable.conf`): a pack adds quests to a giver already in the world by giver id (`m_sGiverId`); a quest id the giver already has is refused (#3124, first used by the Everon clerk's settling-in chain, #3125) | class listed |
| Terms list | `LF_TermsConfig` (`Configs/Terms/LF_Terms.conf`): a pack patches a term's forms by id, each form a key in its own table; the term ids are the core file's `m_sId` rows | class listed; term ids to be listed (#958) |
| Events | `LF_EventsConfig` (`Configs/Events/LF_Events.conf`): a pack adds, patches or removes an event by id (#1952); a pack may also ship `LF_EventScene` and `LF_EventPrecondition` confs for its events (#1957) | classes listed |
| Systems list | the system ids the owner switches on and off | to be listed (#960) |

A surface not yet listed is internal until it is. Adding a name is free; removing or renaming
one is covered below.

## Changing the surface

- A pack's `.conf` files may only use root classes that are listed. `validate` fails on a pack
  `.conf` whose root class is a core `LF_` class that is not.
- `public-surface-baseline.json` holds every name released so far. A name that is in the
  baseline and in neither `configClasses` nor `deprecated` is a removed public name, and
  `validate` fails on it.
- To retire a name, move it from `configClasses` to `deprecated` as
  `{ "name": "LF_OldConfig", "replacement": "LF_NewConfig", "since": "0.2.0", "removeIn": "0.3.0" }`
  and keep it working for one minor version, logged once at boot. Add a line to the release
  notes. Once `removeIn` has shipped, the entry and its baseline name go together.
- Every listed class must still be declared in core script, so a rename that forgets the list
  fails too.
