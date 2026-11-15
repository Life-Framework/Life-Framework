# Opening-gate checklist

The #555 gate as a sheet the maintainer fills in. Copy the block below into a
comment on #555 and tick a line only when its evidence is in hand. The rules
behind the gate are in [the invite-only alpha page](alpha.md#the-opening-gate).
Opening the server to the public is removing the password, and that is the
maintainer's call once every line is ticked.

A command marked *(planned)* is in code font as its card names it and has not
merged. Until it does, the line names the check to do by hand.

## The checklist

```
## Opening gate (#555)

- [ ] Sessions at the player target
      Evidence: several sessions in a row with a peak of 16 players, a full server (the
      alpha runs on 16 slots).
      List them: `tools\cli host history` *(planned, #1409)*; until it merges, open the
      `<host>/sessions/*.md` files that `tools\cli host summary` wrote after each
      session and read the unique-player peak and the perf verdict in each.
      Sessions: <date, peak, perf verdict> x <how many>

- [ ] No save loss
      Evidence: the `tools\cli host summary` restarts-and-crashes line for every alpha
      session, plus the players' own report, shows no lost or corrupted save and every
      restart brought progress back;
      and one restore drill passes on a recent backup: `tools\cli host drill`
      (it passes only when the save loads with its accounts).
      Latest drill: <the SUMMARY line>

- [ ] No blocking regression
      Evidence: `gh issue list --label regression --state open` names no issue that
      stops a player from joining, playing a job, or keeping progress.
      Open regressions: <numbers, or none>

- [ ] Trial moderators in place
      Evidence: `server/flagship/staff-roster.json` names the trial moderators,
      chosen from the playtesters, on probation (#313), working from
      `docs/community/moderator-guide.md`. Not a condition for the first invites:
      at the start of the alpha the maintainer is the only administrator, so this
      line is ticked after opening begins, before the public announcement.
      Roster checked: <date>

- [ ] Gate check
      `tools\cli host gate` *(planned, #1410)* reads the four lines above in one
      run. Until it merges, the four lines are the check.

- [ ] Password removed
      `tools\cli host open` *(planned, #1512)* removes the alpha password once the
      gate passes. Until it merges, the maintainer removes it on the host by hand.
      The password never appears in this repository or in a public channel.
```

## Where each piece of evidence lives

| Line | Command or query | Where it reports |
| --- | --- | --- |
| Sessions at the player target | `tools\cli host history` *(planned, #1409)*, `tools\cli host summary` | `<host>/sessions/<stamp>.md` |
| No save loss | `tools\cli host summary`, `tools\cli host drill` | The session file and the drill's SUMMARY line |
| No blocking regression | `gh issue list --label regression --state open` | The issue list |
| Trial moderators in place | Read `server/flagship/staff-roster.json` | The roster file |
| Gate check | `tools\cli host gate` *(planned, #1410)* | One verdict line |
| Password removal | `tools\cli host open` *(planned, #1512)* | The host's server config |
