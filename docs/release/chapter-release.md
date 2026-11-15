# Chapter release: Workshop update and the live server

How the lead ships a finished chapter or milestone to players with one command. The maintainer decided this
on 2026-11-15:

- The one 0grind server (service 298627, `144.76.90.77:2023`) is both the live alpha and the playtest server.
- When a chapter or milestone is done, the lead releases it. The maintainer is told afterwards.
- A wipe is allowed whenever one is needed (`tools\cli host wipe`, with its own 24 hour notice).

Both Workshop items already exist under the maintainer's account, so every release is a headless update:

| Item | Addon | Workshop id | Page |
|---|---|---|---|
| Life Framework - Core | `LifeFramework` | `4550A2B656E1B569` | https://reforger.armaplatform.com/workshop/4550A2B656E1B569 |
| Life Framework - Everon 1989 | `LF_Everon1989` | `B1E84D5687E11724` | https://reforger.armaplatform.com/workshop/B1E84D5687E11724 |

## The command

```
tools\cli release chapter <x.y.z> --dry-run          always first: every check, nothing changes
tools\cli release chapter <x.y.z> [--notes <file>]   the release
tools\cli release chapter <x.y.z> --now              restart the server even with players on
tools\cli release chapter <x.y.z> --verify           step 5 alone, after a scheduled restart
tools\cli release chapter <x.y.z> --published        both items already published by hand: skip build and publish
```

`--published` is for a release the maintainer publishes through the Workbench GUI, where Visibility can be set to
Public (the headless `-publishAddon` run keeps the item's last visibility). Publish core, then pack, at version
`<x.y.z>` in Workbench first. With the flag, preflight requires **both** Workshop pages to show exactly `<x.y.z>`
(a lower or higher version is refused, so a half-finished publish stops here). Steps 2 and 3 are skipped: no
worktree is made, nothing is packed or stamped, nothing is uploaded. Steps 4 to 6 run as usual: config push and
restart (or deferral with players on), verify, the Discord post, then the tag. The tag goes on the `origin/main`
commit itself (`--at <sha>`), since no release commit with version stamps exists, and the command runs from the
current clean worktree. Verify only passes if the uploaded build says `<x.y.z>` in `LF_PackManifest.CORE_VERSION`;
the preflight warns when the commit does not. Add `--dry-run` first, as always.

Other flags: `--at <sha>` releases a commit on `origin/main` other than its tip. `--since <ref>` sets where the
generated notes start. `--no-gproj-stamp` leaves the `.gproj` files unstamped (see "Open questions").

Run it from a clean worktree (`tools\cli wt new <slug>`), never the main checkout. Run it in the foreground. A
real release takes about 30 to 60 minutes: two packs, two uploads, the Workshop processing them and a server
boot. Each wait has its own limit, so the command never hangs.

The command stops at the first failure and prints `SUMMARY release-chapter fail step=<n>`. Nothing after that
step runs. The steps:

1. **Preflight.** The tree is clean. The commit is on `origin/main` (its tip unless `--at`). The tag `v<x.y.z>`
   does not exist. The version is newer than the newest `v*` tag and than the version on each Workshop page. The
   pack manifest (`addons/LF_Everon1989/Configs/LF_PackManifest.conf`) accepts the new core version: a version
   at or past `m_sCoreBelow` would make the pack refuse the core, so move that bound in a PR first. Every GitHub
   check on the commit passed and `validate` is among them. Without `--notes`, the notes are built from the PR
   titles merged since the last `v*` tag and written to `tmp\release\notes-<x.y.z>.md`. With no tag yet, the
   last 30 merges are used; pass `--since <ref>` to choose. The Workshop change note is the same text,
   `tmp\release\changenote-<x.y.z>.txt`.
2. **Build.** `tools\cli wt new release-<x-y-z>` and a detached checkout of the commit. The version is stamped
   into both `.gproj` files and `LF_PackManifest.CORE_VERSION`, and a release commit with those stamps is made
   on top of the commit, not on any branch. Then `tools\cli pack` packs each addon headless into
   `<worktree>\tmp\release\pack\<addon>`.
3. **Publish.** The core first, then the pack, because the pack depends on the core. Each upload is one headless
   Workbench run (below). The command then reads the Workshop page until it shows the new version, for up to
   600 s.
4. **Server.** `tools\cli host remote 0grind config push --yes`. The mods are on "latest", so 0grind downloads
   the update when the server next starts. If A2S shows 0 players (or no answer) or you passed `--now`, the
   command runs `host remote 0grind restart --yes`. Otherwise it says so and leaves the update for the next
   scheduled restart (06:00 and 18:00).
5. **Verify** (after a restart from step 4). Waits up to 900 s for A2S to answer, then up to 300 s for a pulled
   log that shows the new version. Each try runs `host remote 0grind logs pull`, looks for
   `[LFDebug:Packs] core <x.y.z> accepted by pack LF_Everon1989`, and runs `tools\cli server-doctor` on the log,
   which must PASS. That line is logged only when the pack's manifest accepts the running core, so it shows
   that both new items loaded together. After a scheduled restart, run `--verify` yourself.
6. **Announce and tag.** Posts a short changelog to the announcements webhook (`announceWebhookUrl` in the 0grind
   host's `secrets.json`; the URL is never printed). The post fits under Discord's 2000 characters and ends with
   "...and N more" when the notes run longer. Then it tags `v<x.y.z>` on the release commit from step 2, so the
   tag holds exactly what was uploaded, and pushes only that tag.

Afterwards, tell the maintainer (version, the Workshop links, whether the server restarted) and run
`tools\cli wt prune release-<x-y-z>`.

## The publish flags

Read from the Workbench exe (`ArmaReforgerWorkbenchSteamDiag.exe` contains every flag below) and the BIKI page
"Arma Reforger: Startup Parameters" (`tools\cli call wiki_read '{"title":"Startup Parameters"}'`):

```
ArmaReforgerWorkbenchSteamDiag.exe -gproj <worktree>\addons\<addon>\<addon>.gproj
  -wbModule=ResourceManager -publishAddon -publishAddonDir <packed dir>
  -publishAddonVersion <x.y.z> -publishAddonChangeNoteFile <notes> -noSplash -addonsDir <addons>,<core+data>
```

- `-publishAddon` is for updates only. The wiki says the fields it is not given (title, description, preview,
  tags, visibility) come "from last session", meaning the item's previous publish. The page text therefore stays
  as it is. A page text change is still done by hand in Workbench.
- `-publishAddonDir` takes the folder `-packAddon` wrote (`tools\cli pack --out`).
- **No `-profile`.** The Bohemia login Workbench saved lives in its default profile
  (`Documents\My Games\ArmaReforgerWorkbench`), and that is also where the publish log goes. **Never
  `-wbBackendLogin`**: that flag takes an email and a password on the command line.
- The command passes when the log says `Publishing successful`. It fails on Workbench's own error lines:
  `Publish Addon: Game Identity is not linked with Bohemia Account!`, `Publish Addon: Backend authentication
  timeout!`, `Failed to publish bundle`, `Validating bundle failed`, `Creating bundle failed`.
- Not used: `-publishAddonPreviewImage` and `-publishAddonScreenshots` (the gallery stays as it is) and
  `-publishAddonChangeNote` (the file is used instead).

**This PC's Workbench.** This PC has no `workbench` pool cap and agents never start a headless Workbench on it.
The release is the one exception, because the saved login is here (maintainer, 2026-11-15). Pack and publish
run in the release worktree, which has its own addons folder, and the pack sets `LF_WB_LOCAL=1` for that one
run. If the maintainer has Workbench open, it keeps working, but the publish run shares the default profile
with it, so prefer a time when the editor is closed.

## Testing

- `node tools/validation/release-chapter-selftest.mjs` runs the whole flow against stubs for Workbench, git,
  gh, 0grind, A2S, the Workshop page and Discord. It checks the order (core before pack), the dry run touching
  nothing, the deferred restart, `--now`, and that each refusal stops before the later steps run.
- `--dry-run` runs step 1 for real (git, gh, both Workshop pages) and the read-only half of step 4 (the
  `config push` diff and A2S). It prints the pack and publish commands and the Discord post, and checks that the
  webhook URL exists. It writes only the notes files in `tmp\release\`.

## Open questions for the first real run

- **The `.gproj` stamp.** The release commit adds `Version "<x.y.z>"` to each `.gproj`. The Workbench exe
  contains the string `Version`, but no published `.gproj` seen so far has that line. If the pack step fails on
  it, rerun with `--no-gproj-stamp`. The Workshop version comes from `-publishAddonVersion` either way.
- **Workshop processing time.** The 600 s wait for the page to show the version is a guess. If it runs out
  while the upload itself succeeded, check the page, then continue by hand: `host remote 0grind config push
  --yes`, the restart, then `release chapter <v> --verify`.

Related: [release-process.md](../release-process.md), [workshop-upload-0.1.0.md](workshop-upload-0.1.0.md) (the
first, by-hand upload), [owner/host-the-flagship.md](../owner/host-the-flagship.md) (0grind).
