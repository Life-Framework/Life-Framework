//------------------------------------------------------------------------------------------------
//! Publishes LF_OnPlayerSpawnedEvent from the game mode's spawn hook, so no feature has to edit
//! Core/LF_GameModeRoleplay.c to react to a spawn.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);

		if (Replication.IsServer())
			LF_Session.OnPlayerSpawned(playerId, LF_Utils.GetAccountUID(playerId));
	}
}
