# Alpha

A small group plays the systems already in the repository. This phase adds no new crime types, jobs, or assets. It proves the skeleton is stable enough to build the release scopes on.

## What the group exercises

- Create a character, pick civilian or police, spawn at a faction point.
- Earn cash: gather or process, sell to a trader, buy from a shop, deposit and withdraw at an ATM.
- Take a legal job and receive a paycheck as cash.
- As a civilian, rob cash under the existing police-on-duty gate and gain wanted level.
- As police, go on duty, fine, arrest, and confiscate.
- Save, restart the server, and load the same account, bank balance, and character.

## Done when

- The group can finish that session on a dedicated server with a client.
- A restart restores account, cash-in-bank, and character.
- A broken prefab or empty config logs an error and skips that interaction. It does not take the server down.

## Known holes to close before calling alpha done

These are gaps in systems that already exist, from [features.md](../../features.md):

- Closed: job experience advances. `EL_PlayerJobComponent.AddExperience` calls `LF_CivilianJob.AddExperience`, which adds XP and levels the career.
- Closed: the jail position is the placed cell. `EL_PoliceMenu.GetJailPosition` returns `LF_Jail.GetCellOrigin()` (`Roadmap_Jail_Cell` on `Roadmap.layer`); see [jailing](../scopes/police/jailing.md).
- Closed: the duty toggle keeps the account cached. `EL_DutyAction` calls `SaveAndReleaseAccount`, which re-caches the account instead of evicting it.
- Closed: the whitelist saves to `$profile:LifeFramework/whitelist.txt` on every add/remove and loads lazily, and `EL_FactionSelectionMenu` calls `EL_WhitelistManager.MayJoinFaction` before police is selected.
- Open: a dedicated-server pass is still required for the death-screen respawn and the police RPCs.

## Then

Open the group wider only after the above holds. Release scopes start from that build, not from a second economy.
