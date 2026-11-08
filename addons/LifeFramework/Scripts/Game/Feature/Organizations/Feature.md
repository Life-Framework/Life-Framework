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

- `EL_OrganizationManager` / `EL_Organization` — the persisted register (save
  payload unchanged by M1-17). Names checked by `LF_OrgRules.RefuseName` and
  `IsNameTaken`.
- `EL_OrganizationPlayer` — cash boundary: `Found` (fee from carried MoneyStack),
  deposit, withdraw, buy; `MemberLines` for the ledger.
- `EL_OrganizationCharacter.c` — ledger RPCs; every register change is refused
  away from the desk (`DESK_RANGE` 5 m).
- `LF_OrgRules` — fee, name rules, rights bits, `ModeFor` (which entry a pair sees).
- `LF_OrgInvites` — session offers (not saved, 2 minute lapse);
  `LF_OrgHideoutLease` — the yard lease.
- `LF_OrgReplication` — the organization copy on each character for the offer side.
- `LF_OrgInviteRole` — the `org_invite` handler. `LF_OrgLedger` — the page.

## Iteration path

- A membership card item handed over on Join (the card prop and inventory slot).
- Ranks beyond founder and member; a fee and rent for the yard.
- The stash (M1-15) reads membership the same way the gate does.
