# Quorum sign-off and the tester roster

Status: **proposed, not in force.** The maintainer decided on 2026-11-10 that a
quorum of trusted testers replaces the single maintainer at the release gate
(`docs/decisions.md`). The numbers and rules below are our proposal for them to
approve or change. The tooling is built and switched off: `quorum.json` says
`"enabled": false`, so `tools\cli promote` behaves as before until the
maintainer flips it.

Issue: #575. Gate it feeds: `docs/worktrees.md`, "Release gates". Who runs the
sessions: [session playbook](session-playbook.md).

## The rule

A feature is `verified` when two things are true:

1. Its automated proofs passed (the PR merged, so `validate` and the suite were green).
2. If its PR lists manual steps, at least **3 distinct trusted testers** ran
   them in a session on the commit being promoted and reported **pass**.

A PR without manual steps needs no testers; its automated proofs are the whole
gate, as today.

| Question | Proposal | Why |
| --- | --- | --- |
| Quorum size | 3 testers per feature | One person can miss a bug or mis-click. Three agreeing is a signal without making a session impossible when the roster is small. |
| Same tester twice | Counts once. A later result from the same tester replaces their earlier one | A quorum is people, not clicks |
| A tester who is also the author | Does not count for that feature | The author already knows the expected path |
| One `fail` from a roster tester | Blocks the feature: `promote --fail N`, a `regression` issue with their report, fix, retest | One real failure outweighs three passes. The next session retests it with a fresh quorum |
| Result on a different commit | Does not count | Confirmations name the commit (`sha`) and `promote` checks it |

## Who is a trusted tester

All of these:

- Has played at least two sessions and filed at least one report, in
  `#playtest-feedback` or as a GitHub issue, that someone could reproduce.
- Has used their own Steam name in every session (the server keys saves to it).
- Has no moderation strike on the server.
- The maintainer admitted them. Until the roster has five people, only the
  maintainer admits. After that, two existing testers can propose one and the
  maintainer confirms.

Someone leaves the roster by asking, by going 90 days without a session, or by
the maintainer's decision. Their past confirmations stay on record.

## The roster

Two copies, kept in step by the bot:

- **File:** [`testers.json`](testers.json), one entry per person (`id`, `discord`, `since`).
  It is the copy `promote` reads. `id` is the name that appears in confirmations.
- **Discord role:** `Playtester` (Trusted-level, see `docs/community/discord.md`
  and [onboarding](../community/onboarding.md)). It gates nothing but pings and a
  hint on posts. The file is the source of truth; if they disagree, the file wins.

The roster starts empty. The maintainer admits the first testers; the bot adds
the entry in a PR so the history is visible.

## Disputes

- A tester says a feature passed and another says it failed: it counts as failed
  until a retest. A retest is the next session with the quorum rule applied again.
- A tester disputes a decision about them (admission, removal): the maintainer decides.
- A report that cannot be reproduced after two tries is marked `needs info` on
  its issue, not counted as a failure. It is not a pass either.

## What stays with the maintainer

- Admitting and removing testers, and the quorum size.
- Turning the rule on (`"enabled": true`) or back off.
- Any dispute that survives one more session.
- Anything that needs a human anyway: bans, kicks, money, legal questions, and
  the Workshop account.
- The decision to promote when the roster is too small to reach quorum (see
  "When the quorum cannot be met").

## How `promote` applies it

With `"enabled": true` in [`quorum.json`](quorum.json):

```
tools\cli promote --to <sha> --confirm --confirmations tmp/promote/confirmations-<sha8>.json
```

The file is written by whoever ran the session (the bot or the maintainer):

```json
{
  "sha": "8c824c08",
  "results": [
    { "issue": 612, "tester": "ana", "result": "pass" },
    { "issue": 612, "tester": "ben", "result": "pass" },
    { "issue": 612, "tester": "cy",  "result": "pass" }
  ]
}
```

`promote` then, for every issue whose PR lists manual steps:

- ignores any `tester` not in the roster, and counts each person once;
- refuses with `N of 3 confirmations` when a feature is short;
- refuses with `failed it` when a roster tester reported `fail`;
- refuses when `sha` names a different commit than `--to`;
- when every feature passes, moves `stable`, tags it, and sets `verified` as it
  does now.

With the rule off, `--confirmations` is ignored and `--confirm` works as before.
`tools/validation/quorum-selftest.mjs` proves each refusal and the pass path in a
scratch repo. Without a confirmations file, `--confirm` refuses when the rule is on.

AGENTS.md says agents never run `promote`. That rule stands until the maintainer
approves this one; then it changes to "an agent runs `promote` only when the
quorum rule is on and the confirmations file meets it".

## When the quorum cannot be met

- **Too few testers on the roster:** the maintainer promotes with `--confirm`
  after turning the rule off, or tests it themselves. Say so in the PR.
- **A feature never reaches three confirmations** because nobody tries it: the
  next session's announcement lists it first. After three sessions, the
  maintainer is asked whether to drop its manual steps or test it personally.
- **A session produces confirmations on an older commit than `main`:** promote
  that older commit with `--to`; `main`'s newer PRs wait for the next session.
