# Feedback dry run, November 2026

Slice #1678, acceptance of #316 as a rehearsal: one recorded session and five feedback
posts go through `toolscli host feedback`, and the drafts are read for their session link,
their label and the folded duplicate. **No issue was filed on GitHub**; the `gh issue create`
lines below were printed and not run. The command is
[`tools/host/feedback.mjs`](https://github.com/Life-Framework/Life-Framework/blob/main/tools/host/feedback.mjs);
how a moderator turns a session into issues is in [feedback-triage.md](./feedback-triage.md).

**Setup.** A scratch host folder holding one session report
(`sessions/2026-11-15_19-00-00.md`, an empty report: only its name matters to the link),
passed through `LF_HOST_DIR`. The five posts below are made up for this run. The two
Discord ids in them are the fake ids the command is meant to strip. The scratch folder and
its drafts lived in the worktree's `tmp/` and are not committed.

```
LF_HOST_DIR=<scratch>/host toolscli host feedback <scratch>/export.txt --out <scratch>/out
```

## Input posts

A bug, a duplicate of that bug, an idea, a question and a playtest report, in the copied
channel format the command reads.

```
Rook — Today at 8:02 PM
the bank door is stuck shut after the robbery, nobody can open it
Mara — Today at 8:05 PM
same here, the bank door stays stuck closed after the robbery <@123456789012345678>
Ivy — Today at 8:14 PM
It would be nice to have a way to rename my shop sign (my id is 234567890123456789)
Dex — Today at 8:16 PM
how do I get a driver licence? the clerk at the town hall only says come back later
Tomas — Today at 8:20 PM
Playtest report: 4 players, 45 min. The first hour of the shift was smooth, but the radio cut out twice near the harbor and a late joiner spawned with an empty inventory.
```

## Drafts as printed

The command printed this, then wrote `1.md` to `4.md`:

```
gh issue create --title "[Bug]: The bank door is stuck shut after the robbery, nobody can open it" --body-file "tmp/feedback/<date>/1.md" --label bug
gh issue create --title "[Feature]: It would be nice to have a way to rename my shop sign" --body-file "tmp/feedback/<date>/2.md" --label enhancement
gh issue create --title "[Bug]: How do I get a driver licence" --body-file "tmp/feedback/<date>/3.md" --label bug
gh issue create --title "[Bug]: Playtest report: 4 players, 45 min" --body-file "tmp/feedback/<date>/4.md" --label bug
SUMMARY host-feedback ok messages=5 thanks=0 duplicates=1 drafts=4
```

For every draft, the session link and the siblings listed under it (names and ids are gone):

| Draft | Title | Label | Session link line |
|---|---|---|---|
| 1 | [Bug]: The bank door is stuck shut after the robbery, nobody can open it | `bug` | Session: 2026-11-15; its `tools\cli host summary` report is `sessions/2026-11-15_19-00-00.md` in the host folder. |
| 2 | [Feature]: It would be nice to have a way to rename my shop sign | `enhancement` | Session: 2026-11-15; its `tools\cli host summary` report is `sessions/2026-11-15_19-00-00.md` in the host folder. |
| 3 | [Bug]: How do I get a driver licence | `bug` | Session: 2026-11-15; its `tools\cli host summary` report is `sessions/2026-11-15_19-00-00.md` in the host folder. |
| 4 | [Bug]: Playtest report: 4 players, 45 min | `bug` | Session: 2026-11-15; its `tools\cli host summary` report is `sessions/2026-11-15_19-00-00.md` in the host folder. |

Each draft also has a list headed `Other drafts from this session:` naming the other three
titles, and a footer `Reported by N player(s) ... no author names or Discord ids are kept`.
Draft 1, in full, as filed would read:

```
### What happened?

the bank door is stuck shut after the robbery, nobody can open it

Also reported:
> same here, the bank door stays stuck closed after the robbery

### Steps to reproduce

Not given in the feedback; ask the reporter.

### Expected behavior

Not given in the feedback.

### Logs or screenshots

None attached.

### Environment

A live session of the Life Framework server; build and client as in the session's announcement.

---
Session: 2026-11-15; its `tools\cli host summary` report is `sessions/2026-11-15_19-00-00.md` in the host folder.

Other drafts from this session:
- [Feature]: It would be nice to have a way to rename my shop sign
- [Bug]: How do I get a driver licence
- [Bug]: Playtest report: 4 players, 45 min

Reported by 2 players in the session's feedback channel. Drafted by `tools\cli host feedback`; no author names or Discord ids are kept. Check it against the session's server log before filing (`tools\cli host triage`).
```

## What the run showed

- **Session link: works.** All four drafts carry `Session: 2026-11-15` and the report name,
  and each lists the other three drafts (#1611).
- **Duplicate: folded.** Rook's and Mara's bank-door posts became one draft that quotes
  Mara's words under "Also reported" and says "Reported by 2 players"; the summary line
  says `duplicates=1 drafts=4`.
- **Ids and names: gone.** Neither fake Discord id, the `<@...>` mention nor any author name
  reaches a draft.
- **Label: only two kinds exist.** The command knows `bug` and `enhancement`. The idea was
  labelled `enhancement` and the bug `bug`, as it should be. The **question** (draft 3)
  and the **playtest report** (draft 4) were both drafted as `[Bug]` with the `bug` label.
  Nothing marks either one as a question or a playtest report, and the playtest report is
  titled by its first sentence ("Playtest report: 4 players, 45 min"), which says nothing
  about what went wrong.
- **Question: not an issue.** A moderator should answer it in the channel and not file draft 3
  (the [onboarding](./onboarding.md) bot rules say a question gets the FAQ answer or "a person will look"; `feedback-triage.md` does not say what a moderator does with one, so that is unwritten).
- **Playtest report: one draft, three findings.** The report holds two problems (radio cut
  out near the harbor, late joiner with an empty inventory) and a note that the shift was
  smooth. The command makes one draft; a person has to split it into issues before filing.

## Follow-up cards

None of these edit `tools/host/feedback.mjs` here; it is outside this slice.

- Recognise a question (a leading "how do I", a question mark and no failure words) and print
  "answer in the channel" instead of a `gh issue create` line.
- Take the forum tag (Bug, Idea, Question, Playtest) from the export when it is present, and
  use it for the label before guessing from the words.
- Draft a playtest report as a list of findings, one candidate issue each.
