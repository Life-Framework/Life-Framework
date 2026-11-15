# Moderator rulings

The page a moderator opens at step 4 of
[From report to ruling](./moderator-guide.md#from-report-to-ruling), when the ticket
says what happened and the ruling needs a rule, a response and a reason. It closes
three gaps the [November desk run](./moderator-drill-run-2026-11.md) found (gaps 1 to
3, #1707, slice #1731). It is staff-only and not on the site.

It does not change what a moderator can do. Moderators propose; only an administrator
acts in game ([The ladder](./moderator-guide.md#the-ladder)).

**What is decided and what is not.** The rule text below is the published text and
nothing else. The usual response per rule is the proposal a moderator starts from,
not a verdict: the ticket's facts can move it either way. Where the maintainer has
not decided something, the section says **Not decided** and names who asks whom.

## The rules and the usual ruling

The rules are quoted word for word from the published town rules
(`website/content/rules.json`, the same text as the board sign, the `/rules` page and
the pinned Discord block). If a quote here differs from the published text, the
published text is right and this page is stale: say so in the ticket.

A first offence with no earlier warning on the record starts at **a written warning**
unless the row says otherwise. How the ladder continues is under
[Warnings](#warnings).

### Rule 1

> No random violence: hurt someone only for a reason your character has.

- **Usual ruling:** a written warning. Often seen as a Damage row, then Downed (or
  Death or Murder), with nothing before it for the pair. The test for "a reason" is
  [below](#when-damage-between-players-breaks-a-rule).
- **Rows that settle it:** Damage, Downed, Death, Murder, and the rows that would
  give a reason (Hold-up, Weapon aimed, Cash).
- **Not this rule:** a death with no player behind it, or a self-kill. The Murder row
  is only for a death the murder rule counted, so a Death row alone says nothing
  about intent.

### Rule 2

> Fear for your life: with a gun on you, comply, then report it.

- **Usual ruling:** a written warning for resisting a hold-up or an aimed weapon
  when the log shows no reason to fear for one's life, for example a Weapon aimed
  row on the player followed by their own Damage rows on the aimer. When the aimer
  carried on after the player complied, the aimer is the one in question, under
  rule 1.
- **Rows that settle it:** Hold-up, Weapon aimed (3 s on the same player by
  default), Cash, then Damage in either direction and who is subject and who is
  other on each.
- **The reporter** of a hold-up did what the rule asks: they complied and reported.
  Do not rule on the reporter for having complied.

### Rule 3

> New life: after a death, forget what led to it. Do not hunt the killer.

- **Usual ruling:** a written warning. The log has a row for it: **New Life Rule
  return**, written when a player wearing a wristband comes back near their death
  scene inside the window. Nothing blocks them, so the row is the record.
- **Rows that settle it:** New Life Rule return, and any Damage, Weapon aimed or
  Hold-up row for the same player against the person who killed them after it.
- **One row is not a hunt.** A single return near the scene with no later row
  against the killer is a conversation, not a ruling; say so in the ticket and take
  no step.

### Rule 4

> Keep disputes out of the streets. No stalking, no harassing.

- **Usual ruling:** a written warning, after a ticket with a clip or a described
  pattern. The incident log has no stalking row, so the log can only support it
  (repeated Weapon aimed rows on the same player, repeated reports of the same pair)
  and cannot settle it.
- **Evidence that settles it:** the reporter's clips with the F9 stamp on, kept in
  the ticket for the week
  ([Evidence](./moderator-guide.md#evidence)). Two accounts of a dispute are not
  evidence ([guide step 3](./moderator-guide.md#from-report-to-ruling)).

### Rule 5

> No combat logging: play out an arrest, a robbery or a fight.

- **Usual ruling:** a written warning for the first, with the row attached. The log
  has a row: **Logout in custody**, written when a player leaves while cuffed,
  escorted, downed, wanted or jailed. The row shows their state and what the
  disconnect rules did.
- **Rows that settle it:** Logout in custody (read the state it shows), and the
  Damage or Hold-up rows just before it. A logout with no row before it is a
  disconnect, not a rule.
- **Ask the operator first** if the row is missing: the server owner can switch a
  kind off, so a missing row can mean "not recorded here"
  ([Incident kinds](./moderator-guide.md#incident-kinds)).

## Warnings

Until now the guide listed only in-game responses (`#kick`, `#ban`, `!wadd`) and never
said a warning exists. This section says what one is. The warning policy below was
decided on 2026-11-15.

### What a written warning is

A ruling, written in the ticket by the moderator and agreed by an administrator,
that names the player, the rule, and the incident numbers behind it. It needs no
in-game action: no kick, no ban and no whitelist change. It is not a mark on the
account in game, and nothing in the code counts warnings.

### Where it is recorded

In the **ticket**, in the ruling field of
[From report to ruling](./moderator-guide.md#from-report-to-ruling) step 4:

```
Warning: <in-game name>, rule <number>, incidents #<n>[, #<n>], <date UTC>.
Earlier warnings found: <none | ticket reference>.
```

Use the in-game name from the rows and keep the account uid in the ticket only
([Privacy](./moderator-guide.md#privacy)). The ticket is the record, as it is the
ban record until #1285 lands.

**A warning counts for 30 days** from its date in the ticket. Ticket evidence is kept a
week by default, so keep a ticket that holds a warning for the full 30 days, and ask
before deleting one.

### How it is delivered

There is no in-game way to send a warning: the server has no warn command. The
administrator or the moderator tells the player **privately**, outside the public
channels, in words that say the rule number, what was found, and what the next step
is. A direct message is how staff reach a player today
([trial moderators](./trial-moderators.md): nothing about a person goes in a public
channel). The alpha has no account linking, so ask the reporter or the player which
Discord name is theirs, and do not trust a Discord tag to match an in-game name
([Privacy](./moderator-guide.md#privacy)).

Put a line in the ticket: sent, by whom, when. A warning not sent has not been given.

**Use this fixed warning text**, naming the rule:

```
Warning: you broke rule <N> (<rule title>). This stays on your record for 30 days.
```

### The ladder

One level at a time, and say why in the ticket.

| Step | Response | Who acts | In game |
|---|---|---|---|
| 1 | Written warning | Moderator proposes, administrator agrees | Nothing |
| 2 | Kick (`#kick <player id>`) | Administrator | Logged as an Admin action row |
| 3 | Ban (`#ban <player id> ...`) | Administrator | Logged as an Admin action row, recorded by account uid |

- **Skipping a step.** A moderator may propose a kick or a ban before a warning when
  the rows show it, and says so in the ticket. Nothing here lets a moderator act
  alone: the proposal goes to an administrator.
- **A ban** needs the uid, the date, the reason and the staff member's name in the
  ticket, and it opens the appeal path ([appeals](./appeals.md)).
- **A third warning within 30 days means a kick.** After the kick, the moderator logs
  it and the maintainer reviews whether a ban is needed.

## When damage between players breaks a rule

Damage between players is not a violation by itself: Life Framework is a town of
people with reasons, and a fight, a robbery and an arrest all involve damage. The
rule is **rule 1**: hurt someone only for a reason your character has.

### What the log can tell you

- **Damage** rows are sampled: one record per pair per 5 seconds. Count them as
  "lasted at least this long", not as one hit.
- **Downed**, **Death** and **Murder** rows follow. The other party on a Downed row
  is the one who downed the subject.
- Read **who is subject and who is other** on each row, the order, and the minutes
  ago, the same way as the guide's step 3.
- A shot that is discarded or absorbed by a **safe zone** or a **stun weapon** is
  not damage: a stun downs without wounding and starts no murder, and the spawn safe
  zone blocks player damage. Damage rows at a spot that is a safe zone mean the
  ruling is for a different reason.

### Rows that give a reason

If the log shows one of these **before** the Damage rows, in the same pair and the
same scene, there may be a reason. Read it, do not assume it:

| Row before the damage | What it can mean |
|---|---|
| Hold-up on the shooter, or Weapon aimed on the shooter | The shooter was acting against a threat; this is where rule 2 applies. |
| Weapon aimed by the damaged player on the shooter | The target started the threat. |
| Cash between the pair | A trade or a robbery: read the amount and direction. |
| Earlier Damage rows from the damaged player on the shooter | Who started it. |
| Hold-up started by the shooter on the damaged player | A robbery, which is a crime in game and not by itself a rule. |

### The three outcomes

1. **Rows show a reason** (a Hold-up, Weapon aimed or earlier Damage that gives the
   shooter one). No ruling on the damage. Say in the ticket which row.
2. **Rows show nothing before the first Damage** for the pair in the last hour
   (the panel's window), and the reporter's account matches. That is the drill
   incident: a written warning under rule 1. Name the missing rows in the ticket:
   "no Hold-up, Cash, Weapon aimed or earlier Damage for the pair in the last hour."
3. **The rows are not enough**, for example the incident is older than the log
   window or a kind is off. Do not rule. Ask for the clip, and ask the operator
   before saying nothing happened.

### What does not settle it

- A Murder or Death row alone: it says a player died, not why.
- Two accounts of the fight ([guide step 3](./moderator-guide.md#from-report-to-ruling)).
- An absent row when `LF_Incident.conf`'s `m_sKinds` may have turned that kind off.
- The reporter's wish for a ban. The ruling follows the rows.

## Source

- [Town rules](https://github.com/Life-Framework/Life-Framework/blob/main/website/content/rules.json):
  the published text the quotes match (#312).
- [moderator-guide.md](./moderator-guide.md): the ladder, the incident kinds, evidence
  and privacy.
- [moderator-drill-run-2026-11.md](./moderator-drill-run-2026-11.md): gaps 1 to 3.
- [appeals.md](./appeals.md): bans and the ban record.
