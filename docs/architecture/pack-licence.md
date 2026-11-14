# Licence note for pack authors

What you need to know about licences when you build a pack on the Life Framework core (#967).
This states the project's licences and the obvious obligations that follow. **It is not legal
advice.** When money, a company or a dispute is involved, read the licence texts and ask someone
qualified.

The rule for what goes in a pack is [core-vs-pack.md](core-vs-pack.md). The commands are
`tools\cli pack new` and `pack check` ("Starting a pack", "Checking a pack" there).

## Two licences, one core

| Copy | Licence | Where it is stated |
|---|---|---|
| The source on GitHub (`addons/LifeFramework/`, `addons/LF_Everon1989/`, `tools/`) | MIT | [`LICENSE`](../../LICENSE) |
| The build on the Arma Reforger Workshop | Arma Public License (APL) | the Workshop description, [`README.md`](../../README.md) "License" |

The Workshop Terms of Use require an APL-family licence (or a custom non-commercial one) on every
upload, and MIT is not on that list. So the Workshop copy is APL and the GitHub copy stays MIT
(decision D2, [decisions.md](../decisions.md)). Which one applies to you depends on where you took the
files from, not on which one is "better".

## Your pack's own licence

Your pack is your work. Choose its licence yourself; Life Framework asks for none in particular.

- **Depending on the core does not change your licence.** A pack lists the core's Workshop id as a
  dependency (`pack new` writes it in the `.gproj`). The core is downloaded beside your pack. Nothing
  of the core is inside your files, so nothing of the core's licence is carried by them.
- **A Workshop upload needs an APL-family or non-commercial licence.** That is Bohemia's rule for
  the Workshop, not ours. Pick it when you publish the pack there.
- **A pack holds no script.** `validate-addon-boundary` rejects a `.c` file in a pack. What you could copy
  from the core is therefore data and resources: prefabs, configs, layouts, strings, textures.

## What you may copy from the core

Prefer inheriting. A pack prefab that inherits a core prefab (`pack new --job` does this) copies
nothing and keeps working when the core updates. Copy only when inheriting cannot do the job.

| You copy from | You may | You must |
|---|---|---|
| The GitHub source (MIT) | use, change, publish and sell the copy, in a pack under any licence | keep the copyright and permission notice from [`LICENSE`](../../LICENSE) with the copied files |
| The Workshop build (APL) | use and change it as the APL allows, which includes non-commercial use of your pack | follow the APL's terms on attribution, on commercial use and on what you pass on (read its text) |

If you want MIT terms on what you copy, take it from GitHub. The Workshop copy is always APL.

Three limits apply to both copies:

- **Third-party files keep their own licence.** The core and the flagship contain work from other
  authors (Everon Life and its forks, LNGCLifeRP, Overthrow, CC0 and CC-BY assets). Their notices are in
  [`THIRD_PARTY.md`](../../THIRD_PARTY.md) and [`assets-source/LICENSES.md`](../../assets-source/LICENSES.md).
  When you copy a file that is listed there, its licence and credit come with it.
- **The flagship's content is the flagship's.** The names, flavour copy and 1989 Everon setting in
  `addons/LF_Everon1989/` are the pack's content ([core-vs-pack.md](core-vs-pack.md)). The decision
  of 2026-11-08 keeps the right to license future versions of that pack differently; the core stays MIT.
  Copy the flagship's setting only if you are sure of its licence at that commit.
- **No official endorsement.** Do not present your pack as made or backed by Life Framework or by
  Bohemia Interactive.

## Credit

Credit is a short, true statement of what you built on.

- **The core.** Name it in the pack's README, in its Workshop description and in its credits: "Built on
  Life Framework (https://github.com/Life-Framework/Life-Framework), MIT source and APL Workshop build."
- **Copied files.** Keep the notice the file arrived with. Where the format allows a comment, add a line
  that names the source, as the core does: `// Ported from <url>`. Resource files and `.conf` files get the
  credit in your credits file instead.
- **A credits file.** Follow [`docs/credits.md`](../credits.md): one table row per project, with its author, its
  licence, what you took and where the notice is recorded. A per-file list goes in a `THIRD_PARTY.md`
  beside your addon; assets go one per line as `<asset> | <source URL> | <author> | <licence> | <resource path>`
  in your equivalent of `assets-source/LICENSES.md`.
- **Do this in the same change that adds the file,** as the core does. A credit added later is a credit
  that gets missed.

## Base-game assets

Prefabs, models, textures and sounds from Arma Reforger belong to Bohemia Interactive.

- **Reference and inherit them.** A pack names a base-game prefab by `{GUID}path` and inherits it. The
  player already has the file from the base game. Your pack ships a reference, not a copy.
- **Do not copy base-game files into your pack.** Re-uploading Bohemia's files is not covered by
  anything in this note. Bohemia's terms (the Arma Reforger EULA, the Workshop Terms of Use and the APL)
  decide what you may do with them; read them before you take a base-game file out of the game.
- **Bohemia's other samples.** Material from the Arma Reforger Samples repository is APL. If you build on
  it, credit Bohemia Interactive as [`docs/credits.md`](../credits.md) does.
- **Monetisation.** Server and mod monetisation is limited by Bohemia's policy. The core is always free,
  and nothing here changes what that policy allows you.

## CC-BY and other attribution assets

An asset under CC-BY (a texture, a sound, a model) may sit in a pack under any licence you choose, with
conditions.

1. Name the author, the title, the source URL and the licence, in your credits file and in the pack's
   README. CC-BY says credit must be visible; a line in a file nobody opens is thin.
2. Say if you changed it.
3. Keep a copy of the licence link in your credits file.
4. Do not use CC-BY-SA, GPL or APL-SA files in a pack you want under a different licence. Those carry
   their licence onto what you build with them. The core does not copy them either
   (decision 2026-11-08, [decisions.md](../decisions.md)).
5. CC0 needs no credit. Give it anyway, one line in the credits file. It tells the next author where it
   came from.

An asset with no licence is not free to use. Leave it out.

## Checklist before you publish a pack

- [ ] The pack's README names its licence and the core's two licences.
- [ ] A credits file lists every borrowed file and asset, with author and licence.
- [ ] No base-game file is copied in; base-game content is referenced by `{GUID}path`.
- [ ] No CC-BY-SA, GPL or APL-SA file is in a pack that is not under the same terms.
- [ ] The Workshop description names an APL-family or non-commercial licence.
- [ ] The README says the pack is not official and not backed by Bohemia Interactive.

Not legal advice. The licence texts are the authority: [`LICENSE`](../../LICENSE) (MIT) and Bohemia's
[Arma Public License](https://www.bohemia.net/community/licenses/arma-public-license).
