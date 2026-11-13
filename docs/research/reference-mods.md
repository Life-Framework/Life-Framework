# Reference mods and tools

Public Reforger projects the maintainer pointed us at, read on 2026-11-12 and
2026-11-13. We take ideas and facts only. Licences decide whether code could
ever move: MIT code can be ported with the `THIRD_PARTY.md` header; APL-ND,
APL-SA without the author's permission, GPL-2.0 and unlicensed code cannot.
Read it with the [integration spec](integration-spec.md).

| Project | Licence | What we took | Filed |
| --- | --- | --- | --- |
| Reforger Mapper (`benjibossk/mappingenfusiontest`) | MIT | Workbench scripting pitfalls; entity budget and prefab bounds plugins | #912, #913 |
| `reforge-mcp` (`WannabeCoderLuc/reforge-mcp`) | MIT | Pak-level API lookup stamped with the build; handler and automation crash notes | #914, #912 |
| DarcMods (`mokdevel/DarcMods`) | APL-ND | Convoy that mixes AI driving with virtual movement; mission state machine | comments on #772, #757 |
| Kexanone mods (`Kexanone/ArmaReforgerMods`) | APL-SA | Time-sorted cleanup queue with withdraw; base-game vehicle deletion check; scenario jobs | comments on #690, #757 |
| TrainWreckLooting (`TrainWreckers/TrainWreckLooting`) | none stated | Tagged containers filled on first open, refilled after a cooldown | #916, #917 |
| NIRE mods (`Typ2011/NIRE-Mods`) | GPL-2.0 | Rules quiz gate; crate cargo cost by volume; server-checked delete grant | #915; comments on #752, #312, #794 |

## Notes per project

- **Reforger Mapper.** A map-design web tool with a local bridge and a
  Workbench plugin set. Terrain, island and collaboration features do not fit
  (we build on vanilla Everon). Its `docs/FINDINGS.md` lists traps found by
  trial: `CreateEntity` ignores its angles argument; some house prefabs count
  the passed height above ground; integers read into a `float` request field
  become 0; deleting an entity class a saved world still holds stops the world
  loading.
- **reforge-mcp.** A rewrite of `enfusion-mcp-bk`, which we run. It builds its
  indexes from the installed game's paks and names the build each answer came
  from, and reports the shipped API dump as two versions behind. Handler
  rules: one bad symbol takes the whole `WorkbenchGame` module offline, and
  handler code loads only at process start.
- **DarcMods.** The convoy snaps start and end to roads, builds waypoints for
  the crew, and when no player is within about 2 km it moves the vehicle along
  the road in 100 m steps instead. Not used: helicopters and drones.
- **Kexanone mods.** The cleanup patch queues root entities by delete time,
  works 32 per 2 s tick, and can withdraw an entity. A companion mod exists
  only to stop the base game deleting abandoned vehicles. Not used: winter
  terrain, scoring table, map markers.
- **TrainWreckLooting.** Archived by its author; it no longer compiles as is.
  Tagged containers, a loot table built from the game's catalogue, refill
  after a cooldown only when no player is near, batch processing, a grid that
  activates containers near players.
- **NIRE mods.** The rules quiz is passed once per account, with a retry limit
  and admin reset. The crate mod costs cargo by mesh volume and guards delete
  with a hold action plus a single-use server grant. Not used: the ACE medical
  interface, saline bags, walking speed indicator.
