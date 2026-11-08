# Overthrow survey

Surveyed [Overthrow.Arma4](https://github.com/ArmaOverthrow/Overthrow.Arma4) `main` at `9dbb8fa`. This is the Reforger mod. Nothing here was ported.

`LICENSE.md` is the Arma Public License Share Alike: attribution, noncommercial, Arma-only, and share-alike. The README badge, `docs/technical-design.md`, and `docs/mission-statement.md` still say MIT. Those pages are not the license file. A later copy needs a grant that covers Life Framework, or the copied text stays under APL-SA.

## Recommendation

Copy the ownership map, the versioned player record, the vehicle record, and the tab row. Seat a passenger from the server when vehicle escort is built. Leave the war in Overthrow.

The structure that made their persistence refactors cheap is one serializer per manager, version written first, new fields only appended, applied through a public mutator. `OVT_PlayerManagerSerializer.Deserialize` is the one to copy: versions 2 through 5 each clear the field an older payload never wrote, then `OVT_PlayerManagerComponent.ApplyPersistedPlayers` writes onto the live record instead of replacing it.

The workspace-widget menu (`OVT_UIContext.ShowLayout`) is already in `EL_FactionSelectionMenu`. It is not listed again.

## Matches

### Whitelist persistence

- Overthrow path: `Scripts/Game/Persistence/Serializers/Components/OVT_PlayerManagerSerializer.c`, `Serialize`, applied by `OVT_PlayerManagerComponent.ApplyPersistedPlayers`.
- Life Framework scope: the connect whitelist, which has to survive a restart as a list of persistent ids.
- Copy this: a versioned array of records keyed by persistent id, with an empty unfinished row dropped on save and a missing id skipped on load.

Overthrow has no connect whitelist. The player table is the list shape, not a gate.

### Vehicle escort

- Overthrow path: `Scripts/Game/GameMode/Deployments/Modules/OVT_VehicleSpawningDeploymentModule.c`, `SeatAgent`.
- Life Framework scope: the jailing follow-up that puts a prisoner in a vehicle.
- Copy this: from the server, `CompartmentAccessComponent.MoveInVehicle` into the next free compartment, and skip anyone already seated.

`SeatAgent` lives inside occupation crew spawning. Take the seating call only.

### Garage store and retrieve

- Overthrow path: `Scripts/Game/Persistence/Serializers/Components/OVT_VehicleManagerSerializer.c`, `OVT_PersistedPlayerVehicle`, filled by `OVT_VehicleManagerComponent.CaptureVehicleRecord`.
- Life Framework scope: garage store and retrieve, on top of buy, lock, and register.
- Copy this: one append-only record (prefab, owner, lock, transform) that the manager keeps live, so a retrieve spawns from that record.

`docs/features/resistance/vehicle-storage/requirements.md` describes a stored vehicle as a data record with no entity. `OVT_VehicleStorageComponent` is not in this tree. `ReserveVehicle` hides the live entity in place because a released vanilla record dies within minutes; that is not a garage.

### Housing ownership

- Overthrow path: `Scripts/Game/GameMode/Managers/OVT_OwnerManagerComponent.c`, `SetOwner`, checked by `OVT_RealEstateRequestComponent.RpcAsk_BuyBuilding`.
- Life Framework scope: private housing, the ownership loop after hideouts.
- Copy this: key owner and renter by persistent id and a position string, and have the server choose the building with `GetNearestBuilding` from the caller's feet.

The client never names the building. Money is taken on the server before `SetOwner`. The resistance-funds branch stays in Overthrow.

### A screen assembled from parts

- Overthrow path: `Scripts/Game/UI/Context/OVT_ShopContext.c`, `RebuildTabs`, with one `OVT_ShopMenuTabComponent` per category on `OVT_TabHostContext`.
- Life Framework scope: a world-anchored menu (shop, housing, garage) that grows a section without a new layout file.
- Copy this: clear the container and instantiate one small layout per entry, then paint the selected part after the rebuild.

### A save that appends payloads

- Overthrow path: `Scripts/Game/Persistence/Serializers/Components/OVT_PlayerManagerSerializer.c`, `Deserialize`.
- Life Framework scope: any persistence change that adds a field to a save that already shipped.
- Copy this: write the version first, append the new field at the end of the record, and on an older version set that field to the value the old save promised.

`OVT_PersistedPlayerVehicle` states the same rule on the record itself: a new member is appended, never inserted or reordered. `OVT_OverthrowGameModeSerializer.Serialize` keeps the inherited vanilla payload in a nested `base` object and writes Overthrow's fields after it.

## Leave behind

- Occupation, resistance war, NATO and FIA, capture, radio command, wanted level, QRF, and high command.
- The resistance-account and officer branches on housing and garages.
- `ReserveVehicle` as a substitute for a garage record.
- Abstract wallet money. Life Framework cash stays a physical stack, and the bank stays the only cash-to-account boundary.
- The MIT wording in the README and the design docs, until it matches `LICENSE.md` or a separate grant exists.
