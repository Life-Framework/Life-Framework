# Moderator drill: the staged incident

The maintainer runs this with one trial moderator, to check the moderator can rule
from [moderator-guide.md](./moderator-guide.md) alone. It is the acceptance from #313
as a script, built for slice #1509. About 30 minutes.

The moderator is given the guide and nothing else. The answer key is the last
section, **Maintainer only**, and is not shown to the moderator.

## Who does what

- **Tester A** (the shooter) and **Tester B** (the target): two players on the
  server. Neither needs any staff power.
- **Trial moderator**: rules from the guide. Writes the ticket and proposes the
  ruling.
- **Administrator** (the maintainer): the only one with in-game powers. Opens the
  incident panel, types `!handled`, and does any in-game step.

## What this drill does not do

- No ban. `#ban` and the ban record are not used. The guide marks the ban record
  as coming (#1285) and says not to use it yet.
- No kick and no whitelist change. The ruling is a written warning, which needs
  no in-game action.

## Run the drill

1. Tester A shoots Tester B until Tester B is downed or dies. The incident log
   writes Damage rows for the pair, then a Downed row, or a Death or Murder row.
2. Tester B types `/report` in chat. Expect "Staff were told about your recent
   incidents." and a toast for the administrator.
3. Tester B types `/report` again straight away. Expect "You reported a moment
   ago."
4. The administrator opens Game Master EDIT mode, flies to the `!!` marker, and
   reads the `REPORTED #<number>` lines in the "Incidents near the camera" panel.
   The administrator tells the moderator each number, kind, minutes ago and both
   names.
5. The trial moderator writes a test ticket from the rows, following steps 1 to 4
   of [From report to ruling](./moderator-guide.md#from-report-to-ruling): in-game
   names, time, place, what happened, the clip with its F9 stamp if there is one,
   and the incident numbers. Then the moderator proposes a written warning, naming
   the rule and the response.
6. The administrator does the in-game part (none for a written warning). For each
   REPORTED number the ruling covers, the administrator types `!handled <number>`
   in admin chat. The `!!` clears within 4 seconds and an Admin action row
   `!handled <number>` appears.
7. The administrator checks the ticket records the incident numbers, the ruling
   and who acted. The moderator closes the ticket.

Agents can drive the same incident with `proof incident-handled` (#1451). That
proof is planned and not on main yet.

## Maintainer only: scoring sheet

Mark each line from the moderator's ticket and the log. A correct ruling logs the
rows below.

| Check | Correct | Mark |
|---|---|---|
| Names | Both in-game names, matched to the rows, not Discord tags | |
| Uids | Both account uids taken from the rows, kept in the ticket and not posted publicly | |
| Kinds and times | Each row's kind as the panel shows it, and its minutes ago | |
| Order | Damage before the Downed, Death or Murder row | |
| Clip | The F9 stamp matched to the row time, or noted as none | |
| Ruling | A written warning that names the rule and the response | |
| Scope | No kick, ban or whitelist change proposed (any of these is wrong for this drill) | |
| Escalation | The in-game part is left to the administrator | |
| Handled | `!handled <number>` for each REPORTED number the ruling covers; the Admin action row appears | |
| Record | The ticket holds the incident numbers, the ruling, who acted, and a note that evidence is kept a week | |

### Log formats the ticket and log use

These are the formats the drill checks against. Only the incident log and the
admin action row are live on main.

- **Incident row** (`LF_IncidentRecord`, `Feature/Moderation/LF_IncidentLog.c`):
  number, kind, unix time in seconds, subject uid (the player it happened to),
  other uid (the other party, empty if none), position, amount (damage or cash,
  rounded), how (weapon prefab or money source), and the murder flag.
- **Panel line**: `<kind>, <n> min ago: <subject> / <other>`. A reported row reads
  `REPORTED #<number> ...`.
- **Admin action row**: kind `admin`, subject the administrator, other the target
  when there is one, how the action text (for example `!handled <number>`).
- **Ban record row** (coming, #1285; format from open PR #1373, not on main; not
  used in this drill): `V1|ACTION|<account uid>|<unix>|<staff uid>|<duration s>|<incident ids>|<reason>`,
  where ACTION is `BAN` or `UNBAN`. Once #1285 lands, a real ban writes this row.
  Until then the ticket is the ban record.

## Source

- [moderator-guide.md](./moderator-guide.md): the only document the moderator reads.
- [Moderation design notes](../../addons/LifeFramework/Scripts/Game/Feature/Moderation/Feature.md).
- #313 (the drill's source), #1285 (ban record, open), #1373 (ban record PR, open), #1451 (`proof incident-handled`, planned).
