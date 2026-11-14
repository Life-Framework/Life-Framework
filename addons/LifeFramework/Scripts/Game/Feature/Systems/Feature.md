# Systems

> Every system can be turned on or off from config (#960, epic #223). Rule:
> `docs/architecture/core-vs-pack.md`: the core decides what the game can do, the pack and the
> owner decide what this server is.

## Intent

A pack or an owner chooses which systems the server has: a PvE server turns robbery off, a
modern pack turns the pager off, a pack without fields turns farming off. A player on that server
never meets the system: no prompt, no item in the kit, no request that goes through.

## Interaction pattern

Nothing to walk up to: the switch removes things. An action of a system that is off is hidden,
not greyed out. A request that still reaches the server (a stale prompt, a dialogue line picked
just before a reload) is refused with a log line and, where the player sees a result, the
`#LF-Systems_Off` caption ("That is turned off on this server.").

## Systems (#960)

- **Catalog:** `Configs/Systems/LF_SystemCatalog.conf` (`LF_SystemCatalog`) lists every system once:
  each `Feature/<Area>` folder, plus named parts of one (`Robbery` in `Crime`, `Pager` in `Comms`).
  An entry says how it switches (`m_eSwitch`), whether it is opt-in, its parent, what it needs
  (`m_aRequires`) and the items only it uses (`m_aItems`). `validate-systems-list` fails a folder
  with no entry, an unknown parent or requirement, script naming an unknown id, and a
  `SWITCHABLE` system no script checks.
  - `SWITCHABLE`: its entry points check the switch. Today: `Farming` (plot actions, plant,
    harvest, the growth clock), `Robbery` (the till, weapon and vehicle robbery actions, the till
    hold and its dialogue line, a hold's payout), `Pager` (paging, the read action, loadout items).
    Bank vault and heist robbery are not part of `Robbery` yet (they belong to `Crime`, `NOT_YET`).
  - `NOT_YET`: not gated yet; a config that turns it off is refused with a logged reason and it
    stays on. Follow-ups move them to `SWITCHABLE` one by one.
  - `LOCKED`: framework plumbing (Account, Character, Interaction, UIKit, ...); always on.
- **Layers:** the catalog default (on unless opt-in), then the pack's `LF_SystemsConfig` overlays
  from the mission header, then the owner's `systems.switches.<id>.enabled`. A client takes the
  server's effective values from the config stream (`systems` section) and needs no layer itself.
- **Dependencies:** a parent that is off turns its parts off. A config that turns off a system an
  enabled one needs is refused at boot and the needed system keeps its shipped value (#279); when
  that value is off (opt-in), the system that needs it is turned off instead. Declared today:
  ATM and Organizations need Banking, Police needs Crime, Investigation needs Police.
- **Read path:** `LF_Systems.IsEnabled(id)` (actions, offers, the growth clock),
  `LF_Systems.Refuses(id, what)` (server entry points: logs `<what> refused: <id> is off on this
  server (<reason>)`), `LF_Systems.IsItemAllowed(prefab)` (the loadout).
- **Logging:** each system that is off logs one `LF_Debug.Info` line under its own id when the
  switch resolves (boot, `/configreload`, a client's stream), plus one summary under `Systems`.
- **Run-time switch-off (#1047):** `LF_Systems.SwitchOff(id, reason)` (server only) turns a non-LOCKED system off
  until the world ends (a map with no place for it). It logs `[LFDebug:<id>] <reason>: off` once,
  survives a config reload, and goes out in the config stream.
- **Saved data:** never touched. Load and save run as before while a system is off, so turning it
  on again finds its data. Farming also stops its growth clock while off, so a crop comes back as
  it was saved.
- **Gotcha:** a `.conf` value does not beat a member initializer. With `bool m_bEnabled = true;`
  an overlay's `m_bEnabled 0` read as on (red-proof in `LF_Test_SystemsWorld.c`); the attribute
  default is the only default.

## Proof

- `systems/toggle` (LOGIC): catalog in script, owner section, refusals, dependencies, opt-in,
  run-time switch-off, loadout items.
- `systems/world-gates` (WORLD): the shipped catalog; the DebugWorld farm plot, robbery seams and a
  pager with the systems off and on; a pack overlay under the owner; the stream to a client.
- `systems/keep-data` (PERSISTENCE): a crop saved and loaded while Farming is off; across a restart
  with `tools\cli wt e2e` (the save boot writes a config.json with Farming off).
