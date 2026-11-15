# Trial moderators: recruitment and probation

The decision of 2026-11-09, written out. Issue #1510 (slice of #1006). The
guide a trial moderator works from is [moderator-guide.md](moderator-guide.md).

## Where things stand

- **The roster holds only the maintainer today.** `server/flagship/staff.txt`
  has one line, a pending administrator entry for the maintainer. No trial
  moderator is in it.
- **At opening only the maintainer is staff.** The maintainer is the one
  administrator, and nobody else holds a level until a trial moderator is added
  under the steps below.
- Trial moderators are added before the public opening, not before the alpha
  (see the 2026-11-14 decision in the #1006 plan).

## Who they are

Trial moderators come from the playtesters. A playtester is someone who has
joined the playtest sessions and reports in `#playtest-feedback`. Nobody is
recruited from outside the playtest group, and nobody starts above the
moderator level.

They are on probation. A trial moderator has the moderator level in the roster
and nothing more: no administrator powers in game, and no operator level.

## How one is asked

1. The maintainer picks the candidate. The candidate has joined at least two
   playtest sessions and has given usable feedback there.
2. The maintainer asks them privately, in a direct message. Nothing about it
   goes in a public channel.
3. The candidate may say no, and nothing is recorded if they do.
4. If they say yes, they take the drill below before they are added to the
   roster.

## The drill

The drill checks that the candidate can rule from the guide alone.

1. Two players stage an incident in the playtest mission: some damage, then a
   `/report`.
2. The candidate reads the incident the way the guide says, using
   [moderator-guide.md](moderator-guide.md) and nothing else.
3. The candidate proposes a ruling and says what they would escalate. The
   maintainer, who holds the administrator view, carries out any in-game action.
4. A scoring sheet says what a correct ruling logs: the incident read, the
   ruling, the ticket evidence kept, and the escalation for anything the
   candidate could not settle.

They pass when the ruling and the log match the sheet. A miss is talked through
and the drill can be run once more, with a different staged incident.

## Probation

Probation runs for three playtest sessions after the drill. The maintainer
reviews the trial moderator after the third session, looking at the tickets
they took and the rulings they proposed.

- If the review is good, the maintainer confirms the moderator level in the
  roster and fills the "Trial ends" column of the roster table in
  [moderator-guide.md](moderator-guide.md) with the review date.
- If the review is not yet clear, probation can run one more session. It is
  extended once, and the ticket says why.
- If it is not working, the trial moderator is removed from the roster. That is
  not a ban, and nothing is said in public.

Probation does not change what a moderator can do in game. Nothing in game
depends on probation; the roster entry is what gives the level.

## Adding them to the roster

The roster is `server/flagship/staff.txt`. Its format is in its header: one
account uid per line, then a level (`moderator`, `operator` or `administrator`),
then an optional `# display name`.

- Add a new trial moderator with `tools\cli staff add <uid> moderator --name "<display>"`
  (#1466). The display name is one the person agrees to publish.
- If the roster already has a `#pending` line for them, promote it with
  `tools\cli staff promote <name-of-pending> <uid>` so the line keeps its place.
- Remove someone with `tools\cli staff remove <uid>`.
- Check the file after any change with `tools/cli staff roster check`.

The uid is a human step. The maintainer gets it from the candidate and puts it
in the roster. An agent never looks up or invents a uid.

Only administrators have in-game powers (#313). A trial moderator is never
given the administrator level to "just try" a tool.
