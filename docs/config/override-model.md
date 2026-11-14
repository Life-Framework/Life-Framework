# The override model: add, replace, patch and remove by id

Issue #962. One set of rules for every list in a content config, so a pack can say "add one shop
item" or "drop this offence" without copying the whole list.

## Layers

A list section starts from its core `.conf`. Then, in this order:

1. each pack overlay of the same class, in the mission header's load order (`Core/Config/Feature.md`, "D4 seam");
2. the owner's `config.json` section;
3. on a client, the server's streamed values (the catalogue replaces its lists with them; shops replace theirs).

Every entry in a list has a stable id. A layer names entries by id and gives each one a verb.
The merge is `LF_ConfigOverlay.Apply` (`Core/Config/LF_ConfigOverlay.c`). A section describes its
list once (`LF_ConfigList`: how to find, make, patch and check a record) and has no merge code of its own.

## Verbs

| Verb | What it does | Id the list lacks | Id the list has |
|---|---|---|---|
| `add` | Adds the entry at the end | added | refused |
| `replace` | Rebuilds the entry from this layer alone, in the same place in the list. Fields the layer leaves out take the class defaults, not the lower layer's values | refused | replaced |
| `patch` | Changes only the fields the layer gives | refused | patched |
| `remove` | Takes the entry out of the list. A later layer may `add` it again | refused | removed |
| none | Patches an id that exists, adds one that does not (the rule before #962) | added | patched |

- **Refused** logs `LF_Debug.Error` naming the source file, the list and the id. Only that entry is
  skipped; the rest of the overlay still applies.
- An unknown verb is refused the same way.
- A new or replaced entry must still be complete: a catalogue item needs a prefab that loads.
- **Two packs on one id.** The later pack in load order wins, and a warning names both packs:
  `catalogue.items pistol_m9: pack B.conf changes an id pack A.conf already changed; B.conf wins`.
  The owner file overriding a pack is the owner's job and is not warned.

## Where the verb goes

**Owner file** (`config.json`): records are an object keyed by id, and the verb is `"op"`.

**Pack overlay** (`.conf`): every list record extends `LF_ConfigRecord`, which has `m_sOp`. A
patch writes only the fields it changes (`IsVariableSetDirectly`), the same as before.

## One example per verb

Owner file, crime catalog:

```json
{
  "crime": {
    "crimes": {
      "#LF-Offence_Robbery":     { "op": "patch", "fine": 400 },
      "#LF-Offence_Murder":      { "op": "replace", "wantedAdd": 5, "sentenceSeconds": 1800 },
      "#LF-Offence_BreakIn":     { "op": "remove" },
      "#LF-Offence_Poaching":    { "op": "add", "wantedAdd": 1, "fine": 150 }
    }
  }
}
```

The same in a pack overlay of the catalogue:

```
LF_ItemCatalogueConfig {
 m_aItems {
  LF_CatalogueItem "{...}" {
   m_sOp "patch"
   m_sId "pistol_m9"
   m_iPrice 650
  }
  LF_CatalogueItem "{...}" {
   m_sOp "add"
   m_sId "hunting_rifle"
   m_Prefab "{GUID}Prefabs/Weapons/....et"
   m_sCategory "weapon"
   m_iPrice 1200
   m_iSellPrice 400
   m_sLegality "licensed"
   m_sLicence "FIREARM_PERMIT"
  }
 }
 m_aOffers {
  LF_OutletOffer "{...}" {
   m_sOp "remove"
   m_sOutlet "clothing_rack"
   m_sItem "raincoat_green"
  }
 }
}
```

## Ids per section

| Section | List | Id |
|---|---|---|
| `catalogue` | `items` | item id (`pistol_m9`) |
| `catalogue` | `offers` | `outlet/item` (`gun_counter/pistol_m9`) |
| `crime` | `crimes` | crime id (`#LF-Offence_Robbery`) |
| `police` | `ranks` | rank id (`patrol`). A saved rank is its index: a remove is refused on a shipped rung and on any rung that is not the last |
| `organizations` | `rankDefaults` | `founder`, `officer` or `member`. A remove drops the entry, so the rank keeps its shipped rights; a new or replaced entry starts from them |
| `zones` | `ruleSets` | rule name `NORMAL`, `SAFE` or `LAWLESS`. A remove is refused (every zone names a rule); a new or replaced entry starts from the legacy flags |
| `licences` | `rules` | licence name (`POLICE_ACCESS`). The ids are the catalog's: add of an existing one, a name the catalog lacks and every remove are refused; replace turns both flags off unless the layer gives them |
| `systems` (catalog) | `LF_SystemDef` list | system id (`Pager`). Pack overlays only. A framework (`LOCKED`) system and one another system names as parent or requirement cannot be removed |
| `jobs` | `tracks` | job id (`MINER`). A new or replaced track needs `levelThresholds` |
| `jobs` | `xpAwards` | action id (`gather`). A new or replaced award needs a `job` |
| `jobs` | `licenceGates` | licence name (`MINER_GOLD`, an `LF_ELicenseType` name). A new or replaced gate needs a `job` and a `minLevel`; removing a gate gives the licence its shipped gate back |
| `shops` | `priceRules` | `prefab:<GUID>`, a prefab-path substring, or `type:<flags>,mode:<mode>` (`type:64` is `type:64,mode:2`). Two core rules can share a key; a layer addresses the last one, the one that wins the walk |

The catalogue's `removed: true` is a field, not a verb: it hides an entry and a later layer can set
it back to false. `"op": "remove"` takes the entry out of the list.

A list that must keep some entries says so in `LF_ConfigList.RemoveRefusal(index)`: the reason is
logged like any refusal and the entry stays.

## Moving a section onto the model

1. Make the record class extend `LF_ConfigRecord`.
2. Write an `LF_ConfigList` subclass beside the config class: `GetName`, `Count`, `Get`, `IdOf`,
   `IdOfContainer`, `NewRecord`, `Set`, `RemoveAt`, `PatchFromContainer`, `PatchFromReader` (its key
   list starts with `"op"`), and `CanonicalId`, `IdHint`, `Incomplete` where the defaults do not fit.
3. In `ApplyLayers`, load the packs with `LF_ConfigOverlay.LoadPackOverlays` and merge each with
   `ApplyContainerList(list, container, "m_aField", source, claims)`; in `ApplyOwner`, call
   `LF_ConfigOverlay.ApplyRecords(reader, "field", list, source)`.
4. `node tools/config/schema.mjs --write`: the schema gains `op` on the section's records.

Tests: `LF_Test_ConfigOverlayIds` (`config/overlay-ids`); police, organizations, zones, licences and
systems: `LF_Test_ConfigListsOverlay` (`config/lists-overlay`, remove and a refused patch per list); jobs: `LF_Test_JobsOverrideModel`
(`config/jobs-override-model`, owner verbs per list) and `LF_Test_JobsPackOverlay`
(`config/jobs-pack-overlay`, a pack `.conf` over the shipped `LF_Jobs.conf`).
