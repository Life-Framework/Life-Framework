# Moderator guide

For trial moderators on the Life Framework flagship server (#313). It covers
what you can read, what you can type, who may do what, and how a report
becomes a ruling. Everything named here exists in the code today unless it is
marked **coming**. Do not use a coming item; the section says what to do until
it lands.

Where the rules themselves live, and the appeal path, are separate documents
(#312, #315). This guide is the working side of them.

## The short version

1. Only **administrators** have in-game powers. At opening that is the
   maintainer alone.
2. Moderators work from tickets and evidence, and rule with an administrator's
   incident view in front of them.
3. Players flag trouble with `/report`. An administrator closes a flag with
   `!handled <number>`. Both are logged.
4. Ticket evidence stays a week after the ruling. The in-game incident log
   keeps 14 days.
5. Bans are recorded by account uid, not by name.

## The ladder

Three levels. Escalate up one level at a time and say why in the ticket.

| Level | In-game powers | What the level does |
|---|---|---|
| Moderator | None | Takes tickets, collects evidence, proposes a ruling, escalates what it cannot settle. |
| Operator | None | Sits between the two. This guide gives operators no power the code does not, so anything in game still goes to an administrator. What the operator level adds is set when the roster is filled in. |
| Administrator | All of them (the server's `ADMINISTRATOR` role) | Reads the incident panel, uses Game Master EDIT mode, types `!handled`, `!wadd`, `#kick` and `#ban`. |

Only administrators have in-game powers. The code checks the server's
`ADMINISTRATOR` role in `AdminChatChannel.IsAdminPlayer`; a moderator or
operator who is not an administrator cannot open the incident panel, cannot use
the admin chat channel and cannot kick or ban. A request from anyone else gets
nothing and a `[LFDebug:IncidentGM]` refusal in the server log. A moderator reads
incidents through an administrator (see "Rulings without in-game power").

### Roster

Named people are decided by the maintainer and filled in here before opening.
The first moderators are trial moderators from the playtesters, on probation and
reviewed after a few sessions.

| Level | Name | Discord | Since | Trial ends |
|---|---|---|---|---|
| Administrator | the maintainer | | | |
| Operator | to be named | | | |
| Moderator | to be named | | | |

The server's own administrator list is a server setting, not something this
guide changes. A moderator is never made an administrator to "just try" a tool.

## What a player can do

`/report` in chat. It runs on the player's client and asks the server, which
picks the incidents. The server flags every incident of the reporter's, as
victim or other party, from the last 10 minutes that is not flagged yet. Then:

- The player sees "Staff were told about your recent incidents."
- Every online administrator gets a toast saying how many incidents were
  flagged.
- A second `/report` inside 120 seconds is refused with "You reported a moment
  ago." A report with nothing to flag says "Nothing of yours to report from the
  last few minutes." and does not start the 120 seconds.

A report is a flag on incident rows, not a message. Players still need to open a
ticket with what they saw. The flag saves with the server save and stays until
an administrator handles it.

Both numbers (10 minutes, 120 seconds) are owner settings in
`Configs/Moderation/LF_Incident.conf`.

## Reading the incident panel

The panel is for administrators and sits in vanilla Game Master EDIT mode. It is
titled "Incidents near the camera". There is no separate admin screen.

- Markers appear in the world at recent incidents near the Game Master camera.
  Incidents inside 25 m of each other share one marker.
- The marker nearest the middle of the screen is the selected one. Its rows
  show in the panel, newest first, with at most eight lines in total.
- A marker for a reported incident reads `!!` in yellow. Reported incidents head
  the list (up to four lines) and are sent even when they are far outside the
  camera radius or older than the time window.
- The server decides what an administrator may see: incidents within 400 m of
  the camera from the last hour (3600 s), at most 40 rows, plus up to 20 flagged
  rows from anywhere. The client sends only where its camera is.
- The panel asks again every 4 seconds. "Asking the server..." means no answer
  yet. "No incidents here in the last hour." means none in that view.

A row reads `<kind>, <n> min ago: <subject> / <other>`, with an extra detail for
some kinds (the damage or cash amount, the action for an admin row, the custody
note for a logout). A reported row reads
`REPORTED #<number> <kind>, <n> min ago: <subject> / <other> (type !handled
<number> when done)`. The number is the one you give to `!handled` and write in
the ticket.

Names show for connected players. A player who has left shows as their account
uid. Treat uids as private (see "Privacy").

### Incident kinds

| Panel text | What was recorded |
|---|---|
| Murder | A death that the murder rule counted as murder. |
| Downed | A player was downed; the other party is the one who downed them. |
| Cash | Cash moved between players or sources, with the amount. |
| Hold-up | A hold-up started on a player. |
| Weapon aimed | A weapon stayed on the same player for the aim window (3 s by default). |
| New Life Rule return | A player wearing a wristband came back near their death scene inside the window. Nothing blocks them; the row is the record. |
| Logout in custody | A player left while cuffed, escorted, downed, wanted or jailed. The row shows their state and what the disconnect rules did. |
| Admin action | An administrator did something (see the next section). |
| Death, Damage | A death, and sampled player-on-player damage (one damage record per pair per 5 s). |

The server owner can turn kinds off in `LF_Incident.conf` (`m_sKinds`), so a
missing kind can mean "not recorded here". Ask the operator before concluding
that nothing happened.

### Time and clips

A row's time is the server's real clock (`System.GetUnixTime`). A player can
show a clip stamp on screen with F9 (gamepad: tap RB and View together). It
prints the server name, the last six characters of their character id and the
server date and time in UTC, the same clock the incident rows use. Ask for
clips with the stamp on; it ties a clip to a row without anyone's word.

## The admin action log

Administrator actions are logged as `Admin action` rows, so a ruling can be
checked afterwards. These write a row, with the admin as the subject and the
target as the other party when there is one:

- Any Game Master editor action (teleport, heal, spawn, delete, whitelist,
  kick).
- The vanilla `#kick` and `#ban` chat commands. The target is recorded when the
  first argument is the id of a connected player; the full argument text is
  kept.
- The admin chat commands `!wadd` and `!handled`.

A failed log write never blocks the action. An action with no admin account id
is not logged and the server log says so (`[LFDebug:AdminAction]`).

Moderators read this log through an administrator, and administrators are held
to it too. If an admin row looks wrong, escalate it to the maintainer.

## Admin commands

Only administrators can use these. They go in the admin chat channel
(`AdminChatChannel`), which only administrators can send on; the server checks
the role again on every message.

| Command | What it does |
|---|---|
| `!handled <number>` | Marks a reported incident dealt with and removes its flags. Logs who did it. Ignored, with a log line, when the number is not flagged. |
| `!wadd <TYPE> <uid>` | Adds an account to a whitelist, for example `!wadd POLICE <uid>`. |
| `!preject <uid>` | Rejects the open police application of that account and starts the reapply cooldown. |
| `#kick <player id>` | Vanilla kick. Logged as an admin row. |
| `#ban <player id> ...` | Vanilla ban. Logged as an admin row. |

`!handled` only closes the flag. The ruling, the warning and any ban are separate
steps, and they go in the ticket.

## From report to ruling

The steps for one flagged incident. Moderators write the ticket and propose;
the administrator acts.

1. **Ticket.** The reporter writes a ticket (not a chat message) with: their
   in-game name, the time, the place, what happened, and a clip with the stamp
   on if they have one. No @-mentions of staff.
2. **Find the row.** The administrator opens Game Master EDIT mode, flies to
   the reported marker (`!!`) or reads the reported lines at the top of the
   panel, and tells the moderator the incident number, kind, minutes ago and the
   two names.
3. **Check the facts.** Rule from the rows and the clip, not from two players'
   accounts. Read the kind of each row, their order and how many minutes ago
   each happened, and match the clip's stamp to the row's time.
4. **Propose.** The moderator writes the proposed ruling in the ticket: what was
   found, which rule, what response. Anything in game (kick, ban, whitelist change) goes to an administrator.
5. **Act.** The administrator does the in-game part. The action is logged as an
   Admin action row.
6. **Close.** The administrator types `!handled <number>` in admin chat. The
   ticket records the incident number, the ruling and who acted.

### Rulings without in-game power

Until a staff relay exists, a moderator reads incidents through an administrator
as in step 2, or from the staff channel's pasted rows. The staff webhook (#1283)
and the relay of admin actions and flags to it (#1284) are **coming**; when they
land, they send only a case number and a place, never names. Until then, never
paste a row with account uids into a public channel.

## Evidence

- **Ticket evidence stays a week**: clips, screenshots, pasted rows and the
  ticket text are kept for a week and no longer than that by default. If a
  ban or appeal rests on the evidence, ask the maintainer before deleting it.
- **The in-game incident log** is separate. It keeps records for 336 hours (14
  days) and at most 2000 records, oldest first (`m_iRetentionHours`,
  `m_iMaxRecords` in `LF_Incident.conf`). A flag is dropped when its incident
  leaves the log, so a report left unhandled for more than two weeks loses its
  row.
- The log lives in the server save. A wipe starts it empty, and wipes happen on
  24 hours' notice, so save evidence from the log into the ticket before a
  wipe.

## Bans and appeals

- A ban is recorded by **account uid**, not by name: whitelists and bans key on
  the uid. Write the uid, the date, the reason and the staff member's name in
  the ticket. Get the uid from the incident row or from the administrator.
- Only an administrator bans (`#ban`). The command is logged as an Admin action
  row today.
- A banned player appeals through the appeals path. The appeal is reviewed by a
  different staff member than the one who banned.
- **Coming (#1285):** a ban record file keyed by account uid, written by `#ban`
  and `#unban` with the reason, the staff uid and the target's recent incident
  numbers, and a `!banrecord <uid>` admin command to read it. Until it ships,
  the ticket is the ban record. Do not invent the command.
- **Coming (#315):** the appeals channel and its template.

## Privacy

Incident rows carry account uids. Keep them in tickets and the staff channel.
Never post a uid, an IP or a full row in a public channel, a Discord thread
players can read, or a clip description. The alpha is password-gated and there is
no Discord account linking, so a player's Discord name and their in-game name
may differ; ask for the in-game name and match it to the row yourself rather
than trusting a Discord tag.

## Staged incident (for your first session)

An administrator or the operator can stage one to check you can follow this guide
alone.

1. Two testers in the world. One shoots the other, which writes Damage and
   Death or Downed rows for the pair.
2. The tester who was shot types `/report`. They should see "Staff were told
   about your recent incidents.", and the administrator should get a toast.
3. A second `/report` straight away shows "You reported a moment ago."
4. The administrator opens Game Master EDIT mode and flies to the `!!` marker.
   The panel's first lines read `REPORTED #<number> ...` for the pair's rows.
5. The moderator writes the proposed ruling in a test ticket from those rows,
   using the steps above.
6. The administrator types `!handled <number>`. The `!!` disappears on the
   next refresh (within 4 seconds), and an Admin action row `!handled <number>`
   appears.

## Where the code is

- `Feature/Moderation/LF_IncidentLog.c`, `LF_IncidentConfig.c`: the log and its
  settings.
- `Feature/Moderation/LF_IncidentGm.c` and `UI/GM/LF_IncidentGmPanel.c`: the
  server answer and the panel.
- `Feature/Moderation/LF_IncidentFlags.c` and `LF_IncidentReportCommand.c`:
  `/report` and `!handled`.
- `Feature/Moderation/LF_AdminAction.c`: the admin action log.
- `Feature/Chat/AdminChatChannel.c`: the admin chat commands and the role
  check.
- Design notes: [Moderation Feature.md](../../addons/LifeFramework/Scripts/Game/Feature/Moderation/Feature.md)
  and the Moderation rows in [features.md](../features.md).
