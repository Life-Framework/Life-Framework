# Smoke list: the fixed checks of every promotion

Run these before the per-issue checklist (`tools\cli playtest checklist` puts them at its top, read from this file).
They are the things that always break, whatever changed (#403). The promotion rule:

- **Only a regression in an already-verified feature blocks promotion.** A failed smoke check, or a `regression`
  label on an issue that was `verified` before, holds `stable` where it is.
- **An unfinished new feature does not block.** Cut it: revert its PR (`tools\cli promote --revert <PR>`) or leave it
  behind its switch, then promote the rest.
- A smoke fail is fixed or reverted on `main` and the smoke list runs again from the top.

The list is parsed by `tools/lib/playtest-checklist.mjs` (`parseSmokeList`): keep one line per check, in the form
`- S<n> (<minutes> min): text`. Add a check by appending a line with the next number; never renumber, so earlier
checklists stay readable.

## Checks

- S1 (3 min): Boot. `tools\cli playtest` (or `host start` and join) reaches the world with no `SCRIPT (E)` and no `VME:` in the server console log (`tools\cli logs "SCRIPT \(E\)|VME:"` is empty).
- S2 (2 min): Join. A fresh profile signs the registration form and spawns in town with a character, cash, and the HUD. A second client joins the running server and spawns too.
- S3 (2 min): Save. Take an item and some cash (apple from the general store, a bank deposit), then wait for or force the autosave; the server log names the save and no persistence error.
- S4 (3 min): Restart. Stop the dedicated server cleanly and start it with `-loadSessionSave`: the world loads, no `Unable to find`, no `Wrong GUID/name`.
- S5 (2 min): Reload. Rejoin after the restart: same character, same cash, same item and bank balance as before the restart.
- S6 (3 min): Late joiner. With the first player mid-action (a half-grown plot, a downed or wanted player), a second client joins late and sees the same state, not a blank or default one.

## Delta rule

The per-issue checklist is the delta: it lists only the open `needs-playtest` and `in-dev` issues, which are the merged
PRs since the last `verified` sign-off. A change to persistence, replication, or shared UI means re-running the
neighbouring verified features of that area on top of the smoke list; the maintainer adds those to the pass by hand
(`tools\cli playtest checklist --all` shows everything still open).
