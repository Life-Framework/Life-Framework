# Moderator drill run, November 2026 (desk run)

Slice #1677, acceptance of #313 as a desk run. One reader took the incident text of
[moderator-drill.md](./moderator-drill.md#desk-run-the-incident-text) and wrote the
ruling from [moderator-guide.md](./moderator-guide.md) alone; the scoring sheet was
scored afterwards. No game session was run: the in-game half (a real trial
moderator and an administrator) stays manual for the maintainer.

**Independence limit.** The reader was the agent that also wrote the incident text,
and had read the whole drill file, scoring sheet included, before the ruling. So
this run shows that the guide *can* carry a ruling and where it is silent; it does
not show that a fresh moderator would rule the same. The maintainer's live run is
the real acceptance.

## Ruling

Written as the trial moderator's ticket entry, from the incident text and the guide.

```
Ticket: Tomas Reil reports Mara Voss (in-game names, matched to the rows; no Discord tags)
Time: 21:14 UTC, outside the Morton general store (reporter's account)
Incidents: #41 Damage, #42 Damage, #43 Downed, all flagged REPORTED, read by the
  administrator at 21:20:40 UTC, each "6 min ago", so about 21:14 UTC
Order: two Damage rows, then the Downed row (#41, #42, then #43): Tomas Reil was
  hurt first and downed last, Mara Voss the other party on all three
Clip: reporter's F9 stamp 21:14:31 UTC, inside the minute the rows give; matches
Other rows: none for the pair in the last hour, so no Hold-up, no Cash, no Weapon
  aimed. Nothing in the log gives Mara Voss a reason.
Found: Mara Voss damaged and downed Tomas Reil with no earlier contact in the log.
Rule: the server rule against attacking a player with no reason (rule text not in
  the guide; named from the guide's pointer to #312, unverified)
Proposed response: a written warning. No kick, no ban, no whitelist change.
In game: nothing for the administrator to do beyond closing the flags.
Escalation: none. If Mara Voss has an earlier warning, ask the maintainer.
Administrator: type !handled 41, !handled 42 and !handled 43.
Uids: not on the panel lines; kept out of this ticket and out of public channels.
Evidence: the ticket, the clip and the pasted rows are kept a week after the ruling.
Closed by: <administrator>, once the three !handled rows appear.
```

## Guide sections used

- From report to ruling, steps 1 to 6: the ticket fields, reading the rows, the ruling
  and the `!handled` step.
- Reading the incident panel and Incident kinds: what a `REPORTED #n` line means and
  that the number goes in the ticket.
- Time and clips: the F9 stamp is UTC, the same clock as the rows.
- The ladder: moderators propose and administrators act.
- Admin commands: `!handled <number>` per flagged incident.
- Evidence: the one-week ticket rule.
- Privacy: no uids in public.

## Key verdict

Scored against the drill's scoring sheet after the ruling was written.

| Check | Verdict | Note |
|---|---|---|
| Names | Right | Both in-game names, from the rows |
| Uids | Wrong | The panel line shows names for connected players, so the reader had no uid to keep (see gap 5) |
| Kinds and times | Right | Each kind and "6 min ago" as the panel shows them |
| Order | Right | Damage rows before the Downed row |
| Clip | Right | The stamp 21:14:31 matched to the time the administrator read the panel (see gap 4) |
| Ruling | Partly right | A written warning with its response, but the rule is named from a pointer and unverified (gaps 1 and 2) |
| Scope | Right | No kick, ban or whitelist change |
| Escalation | Right | The in-game part left to the administrator |
| Handled | Right | `!handled` 41, 42 and 43 |
| Record | Right | The numbers, the ruling, who acts, the one-week note |

Seven right, one partly right, one wrong: the ruling stands, and the two misses
come from what the guide does not say.

## Where the guide was silent

1. **No rule text.** The guide says the rules live in separate documents (#312,
   #315) and links neither, and step 4 says to name "which rule". The reader could
   only name the rule from memory of the drill.
2. **No warnings.** The guide never says a written warning exists: where it is
   recorded, how it reaches the player, how many come before a kick. It only lists
   in-game responses (`#kick`, `#ban`, `!wadd`).
3. **No proportion.** Nothing says when shooting a player is a violation or how to
   choose between a warning and a kick; the reader leaned on the absence of Hold-up,
   Cash and Weapon aimed rows, which is the reader's own inference.
4. **Row time.** A panel line gives minutes ago, not a clock time. Matching the F9
   stamp to a row needs the minute the administrator read the panel, which step 2
   does not ask them to say.
5. **Uids.** The guide tells moderators to keep uids in tickets and to get one from
   the row or the administrator, but a row for a connected player shows a name; the
   uid only shows for a player who left. Where a moderator gets it is not said.
6. **Ticket record.** Step 6 lists the incident number, ruling and who acted; the
   one-week evidence note is in Evidence, not in the steps, so a reader who follows
   only the steps would leave it out.

## Cards to file under #231

The guide is #1557's to edit, so none of these edits it here.

- Link the rules (#312) from the guide and quote the rule each ruling cites.
- Add a warnings section: where a written warning is recorded and delivered, and the
  ladder from warning to kick to ban.
- Say when player-on-player damage is a violation, with the log rows that settle it.
- Step 2 asks the administrator to give the clock time the panel was read.
- Say where a moderator gets an account uid for a connected player.
- Step 6 lists the one-week evidence note.
