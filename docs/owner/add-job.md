# Add a delivery job to your map

This page places a courier route on your own world: a depot where a player signs up and picks up
the parcel, and a drop where the run pays. It is prefabs and config only, but it needs Workbench,
because a new map is a world you place.

The route reuses the courier job the core already has. A **new kind of job** (a fisherman, a miner)
is script and cannot be added from a pack; ask for it in the core.

What you can change without script today:

| Setting | Where |
|---|---|
| Where the depot, drop, taxi rank and fuel can are | prefabs on your map (this page) |
| How many runs, and which runs open fuel delivery and taxi | script constants (level 1, 2, 3) |
| XP per run, the level table, a daily XP cap | `jobs` in `config.json` |
| The unemployment benefit | `jobs` in `config.json` |
| The pay for one run (40 / 60 / 80) | script constants in a later build |

## 1. Make a pack

The server you build is a content pack: the core says what the game can do, the pack says what your
server is. In a checkout of the repository:

```
tools\cli pack new Town
```

That writes `addons/LF_Town/` with a `.gproj` that depends on the core, a mission header, a config
overlay and the folder tree. Open `addons/LF_Town/LF_Town.gproj` in Workbench once so it registers
the new resources. The pack's `README.md` lists what to fill in; this page fills in the world and
its jobs.

Do not add a `.c` file here. `validate-addon-boundary` fails a script in a pack, and a pack that
needs code asks the core for an extension point.

A job with a punch clock (police, medic) needs no hand assembly. Add `--job` and the scaffold also
writes a workplace for it:

```
tools\cli pack new Town --job MEDIC
```

That adds `Prefabs/Jobs/TownMedicWorkplace.et`, which inherits the core workplace
`{F2DA01057E99744B}Prefabs/Jobs/LF_Workplace.et`. See [A workplace with a punch clock](#a-workplace-with-a-punch-clock).

## 2. Make the world

In Workbench:

1. **File > New World**, choose the terrain your server runs on (Everon is the playtest's).
2. Save it as `addons/LF_Town/Worlds/Town.ent`.
3. Create two layers: `Gamemode` and `Jobs`.
4. On `Gamemode`, place the roleplay game mode
   `{03913608C59017A3}Prefabs/MP/Modes/Roleplay/GameMode_Roleplay.et`.
5. On `Gamemode`, place at least one spawn point
   `{260C1BEFBF9AA86B}Prefabs/MP/Spawn/SpawnPoint_LIFE.et` and set `m_sFaction` to `CIVILIAN`
   (add a second one with `POLICE` for the police spawn).

A world without the roleplay game mode and a spawn point starts no life server; keep those two
things even after you have placed the jobs.

## 3. Place the depot

On the `Jobs` layer, place the counter
`{28F9F4DC40089B51}Prefabs/Props/Services/Post/CounterPostOffice_01.et` where a player can walk up
to it. Rename the entity in the hierarchy, for example `Town_Depot`. The name must not be empty:
the job site component logs an error and skips a site without one.

Add these components to it:

| Component | Values |
|---|---|
| `ActionsManagerComponent` | one action context named `default`, position offset `0 1 0`, radius `3` |
| `LF_JobSignUpAction` (three) | `m_eJob` `PARCEL`, `FUEL_DELIVERY`, `TAXI`; each in the `default` context |
| `LF_JobStartRunAction` | `m_eJob` `PARCEL`; the row that hands over the parcel |
| `LF_JobAbandonAction` | the row that ends a run with no pay |
| `LF_CollectBenefitAction` | optional; the row a broke player uses for the benefit |
| `LF_JobSiteComponent` | `m_eSite` `DEPOT` |

One depot can carry all three sign-ups. The context's radius is how close the player must stand;
the offset lifts the point to the counter's top.

## 4. Place the drop

On the same layer, place a crate prop from the placeables browser (search `Crate`) or any static
prop, and name it, for example `Town_Drop`. Add:

| Component | Values |
|---|---|
| `ActionsManagerComponent` | one action context `default`, offset `0 0.5 0`, radius `3` |
| `LF_JobDropoffAction` | in the `default` context |
| `LF_JobSiteComponent` | `m_eSite` `DROP` |
| `RplComponent` | `Enabled 1` |

The courier run completes here when the player is carrying the parcel. Place the drop far enough
from the depot that the walk is the job.

The taxi rank and the fuel can are the same pattern. A taxi rank needs an
`LF_JobStartRunAction` with `m_eJob TAXI` and `LF_JobSiteComponent` `m_eSite TAXI`; a fuel can needs
`m_eJob FUEL_DELIVERY` and `m_eSite FUEL`, and usually holds the jerrycan prefab as a child. Both
carry `RplComponent Enabled 1`, as the playtest town's do. Level 2 opens fuel delivery and level 3
opens taxi.

## A workplace with a punch clock

The core ships one workplace prefab, `Prefabs/Jobs/LF_Workplace.et`: a desk with a punch clock that
carries the clock-in action, plus `LF_WorkplaceComponent`. A pack does not copy it; it inherits
it and sets values, which is what `pack new <Name> --job <ID>` writes. In Workbench, place the
prefab on your layer and set:

| Field (`LF_WorkplaceComponent`) | Meaning |
|---|---|
| `m_sJob` | the job id (the job track id). `POLICE` and `MEDIC` clock in as that duty role; any other id registers the place and shows no clock-in |
| `m_sPlaceId` | the stable place id; empty uses the entity name (name the entity) |
| `m_NpcPrefab`, `m_vNpcOffset`, `m_fNpcYaw` | an optional NPC (a clerk, a dispatcher) the server spawns beside the desk; leave the prefab empty for none |

The workplace registers in the place registry under the kind `Workplace`, so a system finds it by
`LF_Places.Find("Workplace", id)` and never by entity name. Extra rows go on the punch clock child
(`DutyClock`) in a derived prefab, in the `LF_DutyClock` context, the way the police desk adds the
fine and application rows. The server log names each workplace:

```
[LFDebug:Places] placed Workplace 'Town_Clinic_Desk'
```

`workplace for X has no place id and the entity is unnamed` means the entity needs a name;
`workplace ... has no job id` means `m_sJob` is empty.

## 5. Point the mission at the world

Open `addons/LF_Town/Missions/LF_Town.conf`. The scaffold leaves two lines out; add them, taking
the world path from Workbench (**right-click the world file > Copy Resource Path**):

```
 World "{GUID}Worlds/Town.ent"
 SystemsConfig "{2104C177A245B6D1}Configs/Systems/LifeFrameworkSystems.conf"
```

Keep `m_LF_PackManifest` and `m_aLF_ConfigOverlays`. Set `m_sName` to what the server browser shows
and `m_iPlayerCount` to your limit.

## 6. Tune XP and levels

The courier pay and the level at which fuel and taxi open are script constants. The XP, the level
table, the daily cap and the unemployment benefit are yours. Put this in
`$profile:LifeFramework/config.json` on the server (the path is in the
[install guide](install.md#5-know-the-two-folders)):

```json
{
  "schemaVersion": 1,
  "jobs": {
    "tracks": {
      "DELIVERY": {
        "levelThresholds": [400, 1200, 3000]
      }
    },
    "xpAwards": {
      "delivery_run": { "xp": 120 }
    }
  }
}
```

`levelThresholds` is the total XP that reaches level 2, 3 and so on; a character's level is 1 plus
the thresholds its XP has passed, so changing the table rebalances existing saves. The shipped table
is `500 1500 3500 7500 15500 31500` at 100 XP a run (five runs to fuel delivery, fifteen to taxi).
An action no shipped award has needs `job` as well: `{ "job": "DELIVERY", "xp": 25 }`.

Check the file before the restart, offline:

```
tools\cli config-check path\to\config.json
```

It must print `0 error(s)`. A key the server does not know is a warning; a value of the wrong type
or outside its range is an error and the server would keep the shipped value. The field-by-field
list is in the [config reference](../config/owner-config.md).

## 7. Run it

For a local check, start the dedicated server on the pack's mission. Use the mission header's
resource path (right-click it in Workbench):

```
ArmaReforgerServer.exe -server "{GUID}Missions/LF_Town.conf" -addonsDir C:\path\to\repo\addons -addons LifeFramework,LF_Town -profile C:\LFServer\profile -maxFPS 60
```

Or open `Worlds/Town.ent` and press **Play** in Workbench for a listen host. Players join the same
way as on any server ([install guide](install.md#7-join)).

The server log names each site as it comes up:

```
[LFDebug:Jobs] site Town_Depot kind 0
[LFDebug:Jobs] site Town_Drop kind 1
```

`0` is depot, `1` drop, `2` taxi, `3` fuel. A missing name logs `job site has no entity name`; a
missing fuel, taxi or depot prefab logs its own error.

What a player does: use the counter, sign up for courier work, use the counter again to pick up the
parcel, carry it to the drop, use the drop. The run pays 40 once. Abandoning pays nothing.

## Limits

The line a courier sees when picking up the parcel names the playtest town's street: "Carry the
parcel east along the street to the porch past the brick-walled lock-up". It is a shipped
localization string shared with the playtest town, so it cannot be changed from a pack. The marker
and the drop crate show where to go.

## Problems

| Log line or symptom | What it means |
|---|---|
| `[LFDebug:Jobs] job site has no entity name` | the entity is unnamed in the hierarchy; name it, Workbench saves the name into the layer |
| `[LFDebug:Jobs] site ... kind 0` on a fuel run | the component's `m_eSite` is the wrong kind; the action and the site must agree |
| The counter has no prompt | the `ActionsManagerComponent` has no context, or the action is not in the `default` context |
| `Cannot pick up the parcel` / the row is grey | the player is not signed up as a courier, or already has a run open; sign up first |
| The run does not complete at the crate | the player is not carrying the parcel, or the crate has no `LF_JobDropoffAction` |
| `fuel can prefab missing` / `taxi prefab missing` | the core's prefab failed to load; reinstall the core item rather than editing the pack |
| Editing the level table changed nothing | the server reads `config.json` at boot; restart it |
