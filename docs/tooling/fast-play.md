# LF Fast Play (Workbench)

Play the open world in Workbench as a dev character who is already registered and in town. There is
no rules quiz and no registration form (R-89), and you start with the starting bank and an ID card.

## Use it

1. Open the project in Workbench and open a world (DebugWorld or Everon1989Town) in the World Editor.
2. **Plugins > Life Framework > LF Fast Play**, or **Ctrl+Shift+F5**.
3. Play starts. About two seconds after the world is up you are standing in town as *Dev Tester*,
   age 30. Your bank has the starting balance (or the debug top-up on a debug game mode), and your
   pockets hold your ID card.

Stop play as usual. The next ordinary **Play** is the full arrival flow (quiz, form R-89, clerk),
so use that to test onboarding.

**Play from camera** keeps you at the camera on both buttons (`LF_PlayFromCamera`, #1834).

## How it works

- The plugin (`Scripts/WorkbenchGame/LFFastPlay/LF_FastPlayPlugin.c`) writes `fast` to
  `$profile:lf-fastplay/request.txt` and enters play mode (`WorldEditor.SwitchToGameMode`).
- On the first player's arrival, `LF_WorkbenchPlay.IsFastPlay()` reads the file once and overwrites
  it with `used`. It is overwritten rather than deleted because Workbench has been seen to leave a
  deleted file in place.
- `LF_CharacterCreationManager.FastPlay` registers the dev character if the account has none, pays
  the starting bank, spawns through `LF_SpawnLogic`, and hands the ID card a second later.
- The quiz is skipped, not passed: the account's quiz state is untouched.

## Safety

The decision is `LF_WorkbenchPlay.FastPlayDecision(IsWorkbench(), request)`. `IsWorkbench()` is
`#ifdef WORKBENCH`, which only Workbench compiles, so a dedicated server or a published build never
takes the path, whatever files are in its profile. The plugin folder is the only committed one under
`Scripts/WorkbenchGame/` (`.gitignore`), and Workbench is the only thing that compiles it.
`LF_Test_WorkbenchPlay` (LOGIC) holds these decisions.

## Also fixed for Workbench play

Workbench play loads the world with no mission header. Two consequences are handled under `WORKBENCH`:

- **Pack overlays:** signs, dialogue, quiz and sound slots come from the mission header's
  `m_aLF_ConfigOverlays`. With no header, `LF_MissionConfigPackSource` uses the header for the open
  world that lists the most overlays (`LF_WorkbenchPlay.FallbackHeader`), so the pack's signs
  (`rules`, `registry`, ...) read and the clerk speaks (the core clerk is text only; its voice is the
  pack's dialogue patch). DebugWorld has no header that lists overlays (the village is a SubScene of
  it), so there the pack's header with the most overlays stands in.
- **Persistence:** the engine's default systems config has no `SCR_PersistenceSystem`, and the World
  Editor cannot name one, so nothing saves in Workbench play. That line is a warning there, not an error.
