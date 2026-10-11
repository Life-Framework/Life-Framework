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

- Job experience does not advance (`AddExperience` is a no-op).
- Police jail position is not a real placed cell yet (see [jailing](../scopes/police/jailing.md)).
- Duty toggle can drop the account cache so the officer cannot act again until reload.
- Whitelist does not persist and does not gate the faction picker.
- A dedicated-server pass is still required for the death-screen respawn and the police RPCs.

## Then

Open the group wider only after the above holds. Release scopes start from that build, not from a second economy.
