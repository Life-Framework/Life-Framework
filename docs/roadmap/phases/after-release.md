# After release

Play the release cut, fix what breaks, then add the larger loops that are still missing. Order is by dependency, not by size.

| Order | Scope | Why it waits |
| --- | --- | --- |
| 1 | Bugs from alpha and release play | A public session is the source of the list |
| 2 | [Private housing](../scopes/later/housing.md) | Doors and keys exist; buying a home is a second ownership loop on top of hideouts |
| 3 | [Crafting](../scopes/later/crafting.md) | Machines already process fixed recipes; a general bench is the wider version |
| 4 | [Tasks](../scopes/later/tasks.md) | The quest tracker exists; workplace and faction tasks are content on top |
| 5 | [Revive](../scopes/later/revive.md) | Death today is the death screen and respawn, not a downed state |
| 6 | [Police missions](../scopes/later/police-missions.md) | Needs ranks, jailing, and stable crime reports |
| 7 | [Events](../scopes/later/events.md) | World events sit on a server that already stays up |

The [UI kit](../scopes/later/ui-kit.md) is in the tree (`Scripts/Game/Feature/UIKit/`, test `EL_Test_UIKit.c`); `LF_Garage` uses it. Migrating the release menus onto it is still out of scope and does not change the order in that table.

Skill points, factions, and the quest tracker are already in the tree. Later scopes use them; they are not new projects.

Vehicle escort to jail is done: `LF_JailEscort` and `LF_JailPutInVehicleAction` on `Roadmap_Escort_Car`, test `EL_Test_Roadmap_Escort.c` (see [jailing](../scopes/police/jailing.md)).
