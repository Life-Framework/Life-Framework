# Terms

> A pack or an owner renames the framework's own words (Police to Sheriff, the currency, the town)
> in every language. Issue #958 (M3 Addons, epic #1005).

## Model

A **term** is a stable id (`police`, `medic`, `town`, `currency`, `org`, `fine`, `job`) with **forms**: `singular`,
`plural`, and any other a sentence needs (`sym` for the currency symbol). A form's value is a string key
(`#LF-Term_Police`), so translators translate "Sheriff" like any other string, or literal text.

Core text names a term in a placeholder: `{police}` (singular), `{Police}` (capital first letter),
`{medic:pl}` (plural, `pl` is short for `plural`), `{currency:sym}` (any form). Braces that are not a
placeholder stay as written; an unknown term stays as written and logs once; a form the term lacks falls
back to its singular and logs once.

Layers, lowest first (`LF_Terms.Get`):

1. **Core**: `Configs/Terms/LF_Terms.conf` (`LF_TermsConfig`).
2. **Pack**: an overlay `.conf` of the same class, patched by term id through the shared override model
   (`LF_ConfigOverlay`, #962). A pack form is a string key in the pack's own table, or literal text.
3. **Owner**: the `terms` section of `config.json`. Plain text (`"police": "Sheriff"`) is the singular and
   the plural in every language; `{"singular": "Gang", "plural": "Gangs"}` sets forms; `{"de_de":
   "Sheriff"}` or `{"de_de": {"singular": ..., "plural": ...}}` sets a language. A language key beats an
   every-language key.
4. **Client**: the server's owner text rides the config stream (`terms` section); a client has no owner file.

## The one text path

`LF_Terms.Text(key, p1..p4)` translates the key, expands the placeholders, then fills `%1..%4` (so a player
name that reads `{police}` stays as typed). It is called by: the toast path
(`LF_NotificationManagerComponent`), the dialogue scene's captions and replies, the wanted board title, the
dispatch service board, the ID request card, and `LF_MoneyFormat` (`{currency:sym}`). `LF_Terms.Form(id, form)`
is the same for script that builds text itself.

Also on the path (#1136): `SetCannotPerformReason` (wrap the key: `LF_Terms.Text("#Key")`), the UI kit's titles, lines and
buttons (`LF_Terms.Shown`: a bare key is translated and expanded, other text is expanded), and the script that
builds a line itself (ticket sheet, ledger, wanted poster). All-capitals placeholders print in capitals (`{POLICE} TICKET`).

**Not on the path, and why.** The engine translates these itself from the key, so a placeholder would show raw:
prefab `Name` and `Description` (item and prop names, the dispatch and phone actions), quest title, description
and turn-in keys, license and job config keys (`m_sLicenseName`, `m_sJobName`), mission details, spawn-menu names,
`m_sCrimeId` (the key is also the offence's identity), and a layout label that carries a `#Key` in the `.layout`.
Those keys stay in `terms-baseline.json` (44 now, from 130). A fix for one is a script that sets the text from
`LF_Terms.Text` at runtime, or a pack that overrides the string.
**Voiced lines** (the clerk, the fuel attendant) keep the recorded word: the caption must match the audio
(`dialogue-pipeline-selftest`). `validate-terms` exempts a key whose comment says `(voiced`. Their replies are
not voiced and use placeholders.

The terms: `police`, `medic`, `town`, `currency`, `org`, `fine`, `job`. `job` has a form per job id
(`{job:miner}`) whose value is the job's own name key. `validate-terms` enforces the first four words only; `org`,
`fine` and `job` are defined and used by the keys migrated in #1136 but not yet enforced (that would add about 25
older keys to the baseline).

## Checks

`validate-terms.mjs`: a placeholder in any table names a term and a form that exist; a core key's English
text does not hard-code a listed term's word. Older keys sit in `tools/validation/terms-baseline.json`, which
only shrinks. `tools\cli terms` lists every term, its forms and the keys that use it.

## Tests

`LF_Test_Terms` (`strings/terms`, LOGIC): each form, pack overlay, owner over pack, per-language owner text,
fallback logging once, the stream, the shipped table, the text path, the money symbol.
