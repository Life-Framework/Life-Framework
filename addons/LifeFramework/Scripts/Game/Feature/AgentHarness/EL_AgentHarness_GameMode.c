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

	override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);
		EL_AgentHarness.EnsureLoop();
	}
}
