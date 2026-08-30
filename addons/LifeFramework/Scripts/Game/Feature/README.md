# Feature Area — Index & Conventions

This folder (`Scripts/Game/Feature/`) holds the gameplay features. Each feature
has its own folder and a `Feature.md` context file that records the *design
requirements* — the intended player experience, the interaction pattern, the
V1 slice, and the iteration path.

**Read first:** the shared design philosophy is
[`docs/design-philosophy.md`](../../../../../docs/design-philosophy.md). Every
feature's `Feature.md` is written against it.

## Feature map

| Feature | Folder | Player-facing intent | Interaction today → target |
| --- | --- | --- | --- |
| Account | `Account/` | Persistent per-player identity (faction, characters, wanted) | background → background |
| ATM | `ATM/` | Self-service cash ↔ bank machine | pure UI → physical ATM at bank |
| Banking | `Banking/` | Bank branch, teller, transfers | entity menu → world-anchored counter |
| Character | `Character/` | Spawning, inventory, hand-carry | physical → physical |
| CharacterCreation | `CharacterCreation/` | Intake / first-join gate | pure UI → world-anchored intake |
| Chat | `Chat/` | Communication | pure UI (accepted) |
| Crime | `Crime/` | Robbery, wanted system | physical → physical |
| Gathering | `Gathering/` | Pick, mine, chop in the world | physical → physical + state |
| Houses | `Houses/` | Owned property, locks, keys | physical → physical |
| Jobs | `Jobs/` | Employment, paychecks, licenses | pure UI → world-anchored workplace |
| Level | `Level/` | XP / level / skill points | background → world-anchored trainer |
| License | `License/` | Licenses (weapon, vehicle, etc.) | pure UI → DMV counter |
| LicensePlate | `LicensePlate/` | Vehicle registration plates | physical → physical + DMV |
| Money | `Money/` | Physical cash inventory item | physical → physical |
| Notifications | `Notifications/` | Player notifications | pure UI (accepted) |
| Police | `Police/` | Duty, arrest, fine, confiscate | physical + station terminal |
| Processing | `Processing/` | Turn raw into product | physical → physical machine |
| Quantity | `Quantity/` | Item stacks, splits, transfer | engineering (no direct player interaction) |
| Resources | `Resources/` | Destructible gather nodes | physical → physical |
| Shop | `Shop/` | Physical stores — clothing, supermarket, weapons, vehicles | world-anchored menu → physical stores |
| SirenLights | `SirenLights/` | Vehicle emergency lights | physical → physical |
| Survival | `Survival/` | Hunger, thirst, health | physical + HUD readout |
| Trader | `Trader/` | Sell goods to a merchant | world-anchored menu → physical trader NPC |
| VehicleLock | `VehicleLock/` | Vehicle keys and locks | physical → physical |
| Whitelist | `Whitelist/` | Faction / job access control | admin (no player interaction) |

## Where a feature's files live

A feature spans more than its scripts. The convention:

- Scripts → `Scripts/Game/Feature/<Name>/`
- Prefabs → `Prefabs/<Name>/` (e.g. `Prefabs/Trader/`, `Prefabs/Resources/Mining/`)
- Configs → `Configs/<Name>/` (e.g. `Configs/Shop/ShopPrices.conf`)
- UI layouts → `UI/Layouts/<Name>/` when the feature earns them

This is mostly true today; outliers are called out in the feature's `Feature.md`
rather than moved blindly. **Moving a prefab or config breaks every `{GUID}path`
reference to it** (AGENTS.md golden rule 3) — reorganize only with a reference
map, never as a side effect of doc work.

## Writing a new feature

1. Create `Scripts/Game/Feature/<Name>/`.
2. Write `Feature.md` first — intent, interaction rung (today → target), V1
   slice, iteration path, dependencies. This is the contract.
3. Only then write code against it.