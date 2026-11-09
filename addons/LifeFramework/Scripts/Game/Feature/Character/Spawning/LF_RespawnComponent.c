//------------------------------------------------------------------------------------------------
//! Client->server bridge for the death-screen respawn, plus the hook that opens the
//! death screen when the server announces the player is ready to spawn again.
//! Lives on the PlayerController (SCR_RespawnComponent), which persists across death,
//! so the request always has an entity the client owns.
modded class SCR_RespawnComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		GetOnRespawnReadyInvoker_O().Insert(LF_OnReadyForRespawn);
	}

	//------------------------------------------------------------------------------------------------
	//! Fired on the owner (client) when the server notifies this player they are ready to
	//! spawn again (LF_SpawnLogic.OnPlayerKilled_S). Opens the death screen.
	protected void LF_OnReadyForRespawn()
	{
		if (!GetGame().GetWorkspace())
			return;

		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return;

		// Already controlling a living character: the ready notification is stale.
		IEntity controlled = playerController.GetControlledEntity();
		if (controlled)
		{
			CharacterControllerComponent characterController = CharacterControllerComponent.Cast(controlled.FindComponent(CharacterControllerComponent));
			if (characterController && characterController.GetLifeState() != ECharacterLifeState.DEAD)
				return;
		}

		LF_DeathScreen.Open(playerController);
	}

	//------------------------------------------------------------------------------------------------
	//! Client entry: request the server to respawn the account's active character.
	void LF_AskRespawn()
	{
		Rpc(RpcAsk_LF_Respawn);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: re-run the account-aware spawn for the requesting player. The player's dead
	//! body stays in the world untouched; only the character spawn is re-created.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_Respawn()
	{
		if (!m_PlayerController)
			return;

		int playerId = m_PlayerController.GetPlayerId();
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		if (!respawnSystem)
		{
			LF_Debug.Error("Death", string.Format("respawn denied: no respawn system for player %1", playerId));
			return;
		}

		LF_SpawnLogic spawnLogic = LF_SpawnLogic.Cast(respawnSystem.GetSpawnLogic());
		if (!spawnLogic)
		{
			LF_Debug.Error("Death", string.Format("respawn denied: no LF_SpawnLogic for player %1", playerId));
			return;
		}

		LF_Debug.Info("Death", string.Format("respawn player %1", playerId));
		spawnLogic.RespawnPlayer_S(playerId);
	}
}