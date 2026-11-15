# Feedback triage after a playtest session

A moderator runs this after each playtest session. The feedback channel holds the raw reports. This page turns them into GitHub issues that the team can track.

## Steps

1. **Copy the feedback channel.** Select the session's posts in `#feedback` and paste them into a text file, for example `tmp/feedback-<date>.txt`.
2. **Draft the issues.** `tools\cli host feedback tmp/feedback-<date>.txt` drops thanks, merges posts that report the same thing ("Reported by 2 players"), strips Discord ids and names, and writes one draft per report to `tmp/feedback/<date>/`. Each draft carries the session line and lists its siblings. The command prints one `gh issue create` line per draft and files nothing. A question gets "answer in the channel" instead of a line; a playtest report gets the `playtest` label and often holds several problems, so split it before filing.
3. **Read each draft, then file it.** Fix the title if it is vague, keep the player's words, then run the printed `gh issue create` line. A report of a bug or idea can also be filed by hand with the Playtest form, `.github/ISSUE_TEMPLATE/playtest.yml`, which adds the `playtest` label.
4. **Link the issues under the announcement.** Add the new issue links under the session announcement on Discord, so players can see that their report became an issue.
5. **Run the log side.** `tools\cli host triage` goes through the session's server log (on 0grind, pull it first with `tools\cli host remote 0grind logs pull`). Match what the log shows to the reports. Add anything the log shows that no player reported; `host triage --draft` drafts an issue for each new line group the same way.

The announcement before the session is `tools\cli host announce` and `tools\cli host post session --send` ([session runbook](../ops/session-runbook.md), template in [announce-template.md](announce-template.md)). A rehearsal of steps 1 to 4 on made-up posts is in [feedback-dryrun-2026-11.md](feedback-dryrun-2026-11.md).

## Notes

- A report is read by a person. Filing an issue does not promise a fix. Say so if a player asks.
- Feedback that is not about the game (account help, server access) goes to the [support page](support.md), not to an issue.
- The [moderator guide](moderator-guide.md) covers the rest of a moderator's work.
