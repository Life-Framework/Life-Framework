# Appeals

How a banned player asks for a second look, who reviews it, and how staff read
the ban record and the incidents behind it (#315, epic #1006). It is the
appeal path the [moderator guide](moderator-guide.md) points to. Staff read the
guide first; players need only the first two sections.

Status 2026-11-15: **the text is written; the Discord channel is not created
yet.** The maintainer creates the appeals channel before opening (listed in
`docs/roadmap/m4-slice-plan.md`). Update this file in the same change as the
channel.

## For players

A ban is by account, not by name. If you think yours was wrong or too long, you
can appeal it.

1. **Where.** The appeals channel on the [Discord](https://discord.gg/MjMexJteqz),
   as a private post only staff can read. Do not post it in a public channel,
   and do not put your account id, IP or anyone else's name in public.
2. **When.** Within 14 days of the ban. The server keeps its incident log for 14
   days, so inside that window staff can still read the rows behind the ban.
   Later appeals are read too, but may have to be decided from the ticket and a
   clip alone.
3. **What.** Copy the template below and fill in what you know. Leave a line
   blank if you do not know it; staff look the rest up.
4. **After.** Staff answer in the same post. The reviewer is a different staff
   member than the one who banned you.

### The template

```
Appeal

In-game name:
When you were banned (date, time, time zone):
The reason you were given:
Incident numbers you were told, if any:
What happened, in your words:
What you think was wrong or too long:
What you want staff to check:
Clip or screenshot (with the F9 stamp on, if you have one):
```

A clip and what to put in the note are in [Record a clip](../player-clips.md).
One appeal per ban. Say new facts in the same post rather than opening another.

## For staff

### The ban record

Every vanilla `#ban create` and `#ban remove` (the unban) the server accepts
appends one row to the ban record, `$profile:LifeFramework/ban_record.txt`
(`Feature/Moderation/LF_BanRecord.c`, #1285). The file is on the server only;
read it through the admin command, not the file. The columns, which the template
above is built from:

| Column | In the record | In the template |
|---|---|---|
| Time | unix time of the action | "When you were banned" (you convert to UTC) |
| Action | `BAN` or `UNBAN` | not asked; staff read it |
| Account uid | the target's account uid | not asked; staff match it from the in-game name |
| Staff uid | the administrator who typed the command, or `rcon` | not asked; it decides who may review |
| Duration | seconds | not asked |
| Reason | what the administrator typed (`No reason provided` if empty) | "The reason you were given" |
| Incident ids | the target's recent incident numbers when the ban was made | "Incident numbers you were told" |

The ban record shape is the one in PR #1373 (`ws/slice-1285`) at the time this was
written; that PR was being fixed when this page was cut. If a column or the
command name changes there, change this table and the lookup below to match.
Everything about the record below is **coming** until #1285 merges. Until then,
the ticket is the ban record (the moderator guide says the same). Do not use a
command that is not on `main`.

### Look up a ban

Only an administrator can type the lookup; moderators read it from the
administrator's paste in the staff channel. A uid is private (see "Privacy" in
the moderator guide).

1. Get the account uid. A banned player is not connected, so a player id will
   not resolve: use the uid from the ticket, the ban record or the incident
   row, found by matching the in-game name the player gave.
2. In chat or RCON, type `#banrecord <uid>`. It answers with every ban and unban
   of that account, oldest first, one line each:
   `unix <time> <BAN|UNBAN> by <staff uid>, <seconds> s, incidents <ids or ->: <reason>`.
   `No ban record for <uid>` means the server has no row for it (a ban from
   before the record, or the wrong uid). Check the ticket.
3. Read the incidents. The numbers on the `BAN` row are the incident numbers of
   that account at the ban. In the Game Master incident panel, flagged incidents
   show as `REPORTED #<number>` and a row's time matches the ban's unix time and
   a clip's F9 stamp (see "Reading the incident panel" in the moderator guide).
   The incident log keeps 14 days and 2000 records, so after that the numbers
   name rows that are gone: copy the rows into the ticket when you ban, not when
   the appeal comes.
4. Paste the matching row and the incident rows (without account uids in any
   public channel) into the appeal post for the reviewer.

### Answering an appeal as the maintainer

At opening the maintainer is the only staff, so this is the whole path from the
appeal post to the answer. It reads the same record as "Look up a ban"; the host
command is the faster way to the row when you are at the host.

1. **Find the ban.** On the host, `tools\cli host bans --player <uid>` prints
   one line per ban and unban of that account with the reason, staff uid and
   incident ids; add `--open` to show only bans with no later unban. Away from
   the host, type `#banrecord <uid>` in chat or RCON instead. The uid comes from
   the ticket or the incident row, matched by the in-game name in the appeal.
2. **Read its incident.** Take the incident ids off the `BAN` row and read those
   rows in the Game Master incident panel (see "Look up a ban", step 3). The log
   keeps 14 days, so inside the window the rows are still there.
3. **Rule.** Uphold, shorten or lift, and name the rule. It is the maintainer's
   own ban if the staff uid is yours: say "no second reviewer yet" in the post and
   record a self-review (see "Who reviews").
4. **Unban.** To lift, type the in-game unban, `#ban remove <id>`, with the reason
   as the administrator who is lifting it. The server writes an `UNBAN` row beside
   the `BAN` row (reason and your staff uid), so the record shows both. Confirm
   with `tools\cli host bans --player <uid>` or `#banrecord <uid>`:
   the `BAN` row is followed by the `UNBAN` row.
5. **Answer.** Write the ruling in the appeal post as described in "Answering".

### Who reviews

The reviewer is a different staff member than the one who banned. The ban
record's staff uid is how you know who that was: the reviewer's own account uid
must not equal it. Roles follow the moderator guide:

- A moderator reads the appeal, the record row and the incidents, and writes the
  proposed ruling in the post: uphold, shorten, or lift, and which rule.
- An administrator acts on it. A lift is an unban in game (`#ban remove <id>`),
  which writes an `UNBAN` row beside the `BAN` row, so the record shows both.
  The row's staff uid is then the administrator who lifted it.
- A moderator never bans or unbans, and an appeal is never ruled on by the
  administrator who banned unless no one else can; see the next point.

**At opening only the maintainer is staff**, so the "different staff member"
rule cannot be met for the maintainer's own bans. Until a second staff member
exists, the maintainer says so in the post ("no second reviewer yet"), tells the
player, and records any ruling as a self-review. A self-reviewed appeal is looked
at again by the first other staff member named on the moderator roster, and the
player can ask for that. Do not hide a self-review.

### Answering

Write the ruling in the appeal post with: the ban row (time, action, staff uid
only if the reviewer needs it), the incident numbers read, what was found, the
rule, the result (upheld, shortened to X, lifted), and who reviewed. Do not post
a uid to the player's public view. Ticket evidence stays a week after the
ruling; if the appeal rests on it, ask the maintainer before deleting it.

### Staged check (for the first session)

An administrator stages this to confirm the record and the appeal path work end
to end (the manual step in #315).

1. Two testers in the world. One shoots the other, which writes Damage and Death
   or Downed rows for the pair.
2. The administrator types `#ban create <tester player id> ...` on the shooter
   with a reason.
3. The administrator types `#banrecord <the shooter's uid>`. One `BAN` row
   shows: the unix time of now, the administrator's uid, the reason and the
   incident numbers of the shooter's rows from step 1.
4. The banned tester files an appeal from the template above in the appeals
   channel.
5. A second person (a moderator, or the administrator if there is only one, as a
   marked self-review) proposes a ruling from the row and the incident rows.
6. The administrator lifts it with `#ban remove`, then types `#banrecord <uid>`
   again: a `BAN` row and an `UNBAN` row appear, in that order.
7. The tester can join again.

## Where it is written

- Who may ban and read incidents: [moderator guide](moderator-guide.md), "Bans
  and appeals".
- The ban record code and its rows: `Feature/Moderation/LF_BanRecord.c` and the
  Moderation section of `Feature.md` (#1285).
- Player-side: [Record a clip](../player-clips.md), "For an appeal".
