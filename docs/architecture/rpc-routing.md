# RPC routing on the server and on a listen host

Settled by in-game tests on build 26266485 (#2641). Short version: **on the server, a bare `Rpc()` with
`RplRcver.Server` always runs its handler right away, inside the call. An `Owner` Rpc does the same when
the server owns the entity. A `Broadcast` Rpc never runs on the server.** So a listen host needs no branch
for its own requests or its own owner replies. It needs a local call only for broadcasts its own player
must see.

## The engine's table

`scripts/Core/generated/Replication/RplRcver.c` (read it with `tools\cli call game_read`) documents the routing:

| caller | caller owns the item | Server | Owner | Broadcast |
|---|---|---|---|---|
| server | yes | direct call | direct call | routed to relevant clients |
| server | no | direct call | routed to the owner client | routed to relevant clients |
| client | yes | routed to the server | direct call | direct call |
| client | no | dropped | dropped | direct call |

A listen host is the server row. Its own player's body, controller and components are owned by the
server, so they are the "yes" row.

## What the tests observed

Probe: `LF_TestRpcProbeComponent` (one Rpc per receiver; each handler counts its runs and whether it ran
inside the `Rpc()` call), on `Prefabs/Debug/LF_TestRpcProbe.et` (with `RplComponent`) and
`LF_TestRpcProbe_NoRpl.et` (without). The suite boot is the server, with no clients connected.

| case | test | Server | Owner | Broadcast |
|---|---|---|---|---|
| (a) server owns it, valid RplId (the host's own body) | `LF_Test_RpcRoutingOwned` | runs here, inside the call | runs here, inside the call | does not run here |
| (b) owner handed to `RplIdentity.Invalid()` (no connection) | `LF_Test_RpcRoutingNotOwned` | runs here, inside the call | runs here, inside the call | does not run here |
| (c) no `RplComponent`, so no RplId | `LF_Test_RpcRoutingNoRplId` | dropped | dropped | dropped |
| (c') replicated prefab spawned local-only on the server | `LF_Test_RpcRoutingNoRplId` | runs here, inside the call | runs here, inside the call | does not run here |

- **When:** every handler that ran locally ran synchronously inside `Rpc()`. None ran later: each probe
  logs its counts again 100 ms after the test (`[LFDebug:RpcProbe] <case>: after the delay ...`), and they
  were unchanged.
- **(b) caveat:** the suite has no client, so it cannot make a real remote owner. `Give(RplIdentity.Invalid())`
  is unreliable: `IsOwner()` read false on one run and true (with `FindOwner` valid) on the next. The
  Owner handler ran locally on both runs. When a real client owns the entity, the engine table says the call
  goes to that client only. A two-process session proof for that case is a follow-up. No verdict below
  depends on it, because nobody wants the host to run a remote client's reply.
- **(c):** with no `RplComponent`, the engine logs `RPL (E): Attempting to send an RPC through unregistred
  item. itemType='...', rpc='...'` and drops the call for every receiver. Nothing runs, on the host or
  anywhere else. That is a broken entity, not a listen-host problem: a client's Rpc is dropped there too.
- **(c'):** `SpawnEntityPrefabLocal` on the server still registers an entity that has an `RplComponent` (valid
  RplId), so "local-only" does not mean "unregistered" on the server.
- **PR #2639 (`LF_Test_QuestLogHostReply`)** is case (a) with an Owner reply. It agrees.

## Rules that follow

1. A client-to-server request (`RpcAsk_*`, `RplRcver.Server`) needs **no** `Replication.IsServer()` branch.
   On a listen host, `Rpc(RpcAsk_X)` runs `RpcAsk_X` right away. A branch that calls the handler directly
   does exactly the same thing; it is harmless but redundant.
2. An owner reply (`RpcDo_*`, `RplRcver.Owner`) to an entity the host owns runs on the host. Adding a direct
   call **beside** the Rpc makes the handler run twice.
3. A broadcast (`RpcDo_*`, `RplRcver.Broadcast`) never runs on the server. When the host's own player must
   see it (a visual, a sound, a UI list), call the handler or its effect locally as well, guarded so a
   dedicated server skips work it cannot show. Existing examples:
   `LF_OreNodeComponent` (`!System.IsConsoleApp()`), `LF_EventShell.LF_AnnounceEventPhase`
   (`RplSession.Mode() != RplMode.Dedicated`), `LF_WantedPosterBoard.ServerSet` (`m_bClient`).
4. An entity whose Rpcs must work needs an `RplComponent`. Without one, every Rpc is dropped with the
   `unregistred item` error.

## Verdicts on earlier changes

| item | what it assumed | verdict |
|---|---|---|
| #2511 / PR #2526: `LF_AskDebugAction`, `LF_AskSpawnHubTravel` (and `LF_AskWorldReset`, PR #2501) host branches | a host's `Rpc(RpcAsk_)` is dropped | **simplify**: redundant, harmless. Remove in a follow-up slice, along with the "never reaches its handler" comments |
| #2511 / PR #2526: `validate-rpc-host-path` + baseline + selftest | the same | **retire**: it forces a redundant branch on every new request (about 58 direct `RpcAsk_` calls in 35 files now). Remove in a follow-up slice |
| #2534 / PR #2556: police arrest, quest log ask host branches | the same | **simplify**, as above. `LF_Test_RpcHostPathPoliceQuest` stays green either way (the bare Rpc runs the handler too) |
| PR #2550: organisation ledger asks (7 in `LF_OrganizationCharacter.c`) | the same | **simplify**, as above |
| #2620: check for owner replies with no host path | an Owner reply never reaches the host | **drop as written**. Reshape it into a check for `Broadcast` Rpcs with no local call (rule 3). Today 4 of 13 broadcast sites have no nearby local call; 3 of those already run their effect locally for the host, and `LF_GameModeRoleplay.LF_PublishConfigStream` (the host is the publisher) needs a look |
| AGENTS.md "RPC direction" host-check line | the same | **fixed in this PR** |

Follow-up slices (sub-issues of #2641): #2715 retire `validate-rpc-host-path`; #2716 drop the branches named
above; #2717 and #2718 drop the remaining duplicate branches; #2719 a two-process proof of case (b) with a
real remote owner.
