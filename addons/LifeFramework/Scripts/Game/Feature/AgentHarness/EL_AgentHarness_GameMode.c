// Hooks the harness onto the roleplay game mode without editing
// EL_GameModeRoleplay.c. EnsureLoop() itself no-ops when there is no local
// player controller, so the dedicated test server does not poll.

modded class EL_GameModeRoleplay
{
	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);
		// Menus (faction, spawn, debug) exist before the pawn does. EnsureLoop
		// no-ops until this process has a local controller, and again on spawn.
		EL_AgentHarness.EnsureLoop();
	}

	// OnPlayerConnected is server-only. A client joined to a dedicated server
	// learns about players here, before its faction menu needs the poll.
	protected override void OnPlayerRegistered(int playerId)
	{
		super.OnPlayerRegistered(playerId);
		EL_AgentHarness.EnsureLoop();
	}

	override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);
		EL_AgentHarness.EnsureLoop();
	}
}

// A joining client of a dedicated server gets its controller from the
// server; start the poll when that controller takes its first entity too.
modded class SCR_PlayerController
{
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		EL_AgentHarness.EnsureLoop();
	}
}
