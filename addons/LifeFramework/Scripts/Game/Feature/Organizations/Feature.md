# Organizations — Design Requirements

> Context file for the organization registry. Written against `docs/decisions.md`
> ("In-world first", "One player interaction menu", gamepad-friendly, PC-first).

## Intent

A group of players registers a name at the registry desk in the village (site 8,
`Roadmap_Org_Desk`), pays the fee in cash, and from then on recruits face to
face. Members open the hideout yard gate down the alley. The fund and the
processing upgrade are kept at the desk.

## Interaction pattern (in-world)

- The desk opens a ledger page (`LF_OrgLedger` on the UI kit). It is UI because
  founding needs a typed name and the members are a list; it looks like the paper
  on the desk and plays a page turn, a pencil on Sign, and a close.
- Invite, join, and remove are entries in the player interaction menu (`org_invite`
  slot, `LF_OrgInviteRole`): "Invite to <org>" for a leader facing someone,
  "Join <org>" for that person facing the leader, "Remove from <org>" for a leader
  facing a member. The inviter points, the new member raises a hand and hears the
  hand-over. No menu, no typed name.
- The yard gate is a door: a member opens it; a non-member sees the reason.

## Current state

- `LF_OrganizationManager` / `LF_Organization` — the persisted register (save
  payload unchanged by M1-17). Names checked by `LF_OrgRules.RefuseName` and
  `IsNameTaken`.
- `LF_OrganizationPlayer` — cash boundary: `Found` (fee from carried MoneyStack),
  deposit, withdraw, buy; `MemberLines` for the ledger.
- `LF_OrganizationCharacter.c` — ledger RPCs; every register change is refused
  away from the desk (`DESK_RANGE` 5 m).
- `LF_OrgRules` — fee, name rules, rights bits, `ModeFor` and `RankModeFor` (which entry a pair sees).
- `LF_OrgRights` — the rights bitmask (#750a): bit N is `LF_EOrganizationPermission` N, bits 0-3
  are the four bools a rank saved before the mask. Ranks are a ladder (0 = founder); promote,
  demote and kick only go down it. `LF_OrgRankRole` fills `org_promote` / `org_demote`.
  The mask and order are saved in `orgranks` (save version 27); a version 1 record migrates on load.
- `LF_OrgInvites` — session offers (not saved, 2 minute lapse);
  `LF_OrgHideoutLease` — the yard lease.
- `LF_OrgReplication` — the organization copy on each character for the offer side.
- `LF_OrgInviteRole` — the `org_invite` handler. `LF_OrgLedger` — the page.

## Iteration path

- A membership card item handed over on Join (the card prop and inventory slot).
- A rank table editor at the desk (rename, set rights, add rungs) on top of `ApplyRankRow`;
  pack ladders from config. A fee and rent for the yard. The stash gated by `USE_STASH`.
- The stash (M1-15) reads membership the same way the gate does.

## Test org sign and the door's words (#934)

- **Player sees:** a chalkboard beside the registry desk (`Sign_TestOrg`, about 291, 255.5) with
  *Join test organisation* and *Leave organisation*, and a Read card that says what it is for.
  The hideout door names itself: *Open* for a member, *Members only (Test Organisation)* for
  everyone else, with the reason under it; a yard nobody holds says how to get it.
- **Owner and side:** `LF_TestOrgSign` decides on the server, playtest only like the travel
  globe (`LF_SpawnHub.IsEnabled()` and `LF_DevGate`), and a body within 5 m of the sign. The first
  one in founds the shared "Test Organisation" as its founder and takes the hideout lease; later
  ones join at the entry rank; Leave takes anyone out, founder included (the organisation stays on
  the register so it can be joined again). `LF_OrganizationManager.FindByName`, `JoinDirect` and
  `LeaveDirect` are the playtest seams; the normal paths (found, invite, leave) are unchanged.
- **Door:** `LF_HideoutDoorComponent` carries the owner's name as an `RplProp` (written by
  `LF_Hideout.PushLock`); `LF_HideoutDoorRules` picks the wording and `LF_DoorUserAction`
  applies it on the prompt (`GetActionNameScript`) and the refusal.
- **Data:** keys `LF-TestOrgSign_*` and `LF-Hideout_*`; the sign card is the `test_org` row of
  `Configs/Signs/LF_Signs.conf`. Nothing new persists: the register and the hideout lease already do.
- **Proof:** `organizations/test-org-sign` (LOGIC), `organizations/test-org-sign-world` (WORLD),
  `node agent-box/session.mjs test-org-sign-proof`.
