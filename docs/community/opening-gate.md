# Opening-gate checklist

The #555 gate as a sheet the maintainer fills in. Copy the block below into a
comment on #555 and tick a line only when its evidence is in hand. The rules
behind the gate are in [the invite-only alpha page](alpha.md#the-opening-gate).
Opening the server to the public is removing the password, and that is the
maintainer's call once every line is ticked.

A command marked *(planned)* is in code font as its card names it and has not
merged. Until it does, the line names the check to do by hand. The first four
lines are the four checks of `tools\cli host gate` (#1410) and carry its PASS/FAIL
names; the lines after them are the ones the command cannot read.

## The checklist

```
## Opening gate (#555)

- [ ] Sessions at the player target
      Evidence: several sessions in a row with a peak of 16 players, a full server (the
      alpha runs on 16 slots). The gate defaults to 40 players and 3 sessions, so run it
      with `--target 16`.
      List them: `tools\cli host history`, one row per finished session with its peak
      players and how it ended.
      Sessions: <date, peak, perf verdict> x <how many>

- [ ] No save loss
      Evidence: the `tools\cli host summary` restarts-and-crashes line for every alpha
      session, plus the players' own report, shows no lost or corrupted save and every
      restart brought progress back;
      and one restore drill passes on a recent backup: `tools\cli host drill`
      (it passes only when the save loads with its accounts).
      Latest drill: <the SUMMARY line>

- [ ] No open regression
      Evidence: `gh issue list --label regression --state open` is empty. The gate
      counts every open `regression` issue, so read the list before ticking; one that
      does not stop a player from joining, playing a job or keeping progress is closed
      or relabelled first, not waved through.
      Open regressions: <numbers, or none>

- [ ] Administrator uid in the roster
      Evidence: `server/flagship/staff.txt` names an administrator with a real uid, not
      `#pending` (the maintainer is the only administrator at the start of the alpha;
      `tools\cli staff promote` fills the uid in and `tools\cli staff roster check`
      reads the file).
      Trial moderators listed under it, on probation (#313) and working from
      `docs/community/moderator-guide.md`, are information only and not a condition:
      the maintainer adds them after opening begins, before the public announcement.
      Roster checked: <date, administrators, moderators>

- [ ] Gate check
      `tools\cli host gate --target 16` reads the four lines above in one run and prints
      `SUMMARY host-gate open|closed`; exit 0 only when all four pass.
      Latest run: <the SUMMARY line>

- [ ] Password refusal (manual)
      Evidence: with the alpha password set, a client that joins with no password or a
      wrong one is refused and one with the right password gets in. Bohemia's backend
      enforces the password (#1520) and the automated proof is held, so a person does
      this once from a second machine.
      Checked: <date, what was refused>

- [ ] Password removed
      `tools\cli host open` *(planned, #1512)* removes the alpha password once the
      gate passes. Until it merges, the maintainer removes it on the host by hand.
      The password never appears in this repository or in a public channel.
```

## Where each piece of evidence lives

| Line | Command or query | Where it reports |
| --- | --- | --- |
| Sessions at the player target | `tools\cli host history`, `tools\cli host summary` | One row per session; the file `<host>/sessions/<stamp>.md` |
| No save loss | `tools\cli host summary`, `tools\cli host drill` | The session file and the drill's SUMMARY line |
| No open regression | `gh issue list --label regression --state open` | The issue list |
| Administrator uid in the roster | `tools\cli staff roster check` | `server/flagship/staff.txt` |
| Gate check | `tools\cli host gate --target 16` | Four PASS/FAIL lines and one `SUMMARY host-gate` line |
| Password refusal | A join with no password or a wrong one | The client's refusal, by hand |
| Password removal | `tools\cli host open` *(planned, #1512)* | The host's server config |
