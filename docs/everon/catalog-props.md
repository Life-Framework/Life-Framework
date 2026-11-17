# Everon props catalog

Generated 2026-11-16. The vanilla props most useful for roleplay fit-out, grouped by use. Each group has its own file with per-category tables, sizes (`tools\cli measure`), how many exist on vanilla Everon, which towns use them and whether the prefab already carries a user action. Merged data: [catalog-props.json](catalog-props.json) (409 items). Buildings: [catalog-buildings.md](catalog-buildings.md).

| group | file | items | with a user action |
|---|---|---:|---:|
| retail | [props/retail.md](props/retail.md) | 53 | 1 |
| office-police | [props/office-police.md](props/office-police.md) | 63 | 5 |
| residential | [props/residential.md](props/residential.md) | 63 | 7 |
| medical-services | [props/medical-services.md](props/medical-services.md) | 51 | 7 |
| signs-lights-street | [props/signs-lights-street.md](props/signs-lights-street.md) | 80 | 4 |
| vehicles-dock-industrial | [props/vehicles-dock-industrial.md](props/vehicles-dock-industrial.md) | 99 | 19 |

## Reading the flags

- **hasAction / actions**: a user-action entry found in the vanilla prefab (sit, open, pick up, play). A prop with `false` is a mesh only: anything interactive must be added by the mod (Life Framework's own sit and bed actions are in `docs/features/`).
- **everonWorldCount**: instances placed on vanilla Everon (whole map for non-structure props; structure props counted from the structure dump).
- **usedInTowns**: from the 450 m dumps around Saint Philippe, Saint Pierre, Morton and Montignac only.
- Items whose GUID the pak data does not expose carry null in `guid`/`res`; find them with `tools\cli call asset_search`.
