//------------------------------------------------------------------------------------------------
//! Client <-> server bridge for the player interaction menu, on the PlayerController.
//!   server -> owner : LF_OpenGiveCash, LF_OfferItemInHand, LF_LearnName, LF_PlayInteractionGesture
//!   owner -> server : LF_AskGiveCash, LF_AskGiveItem
//! The server takes the giver from this controller's controlled entity, never from the client,
//! and LF_PlayerInteraction re-checks range, life, and what is really carried.
//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	protected bool LF_IsLocalInteraction()
	{
		return GetGame().GetPlayerController() == this;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: open the amount sheet on this player's machine.
	void LF_OpenGiveCash(RplId target, int carried)
	{
		if (LF_IsLocalInteraction())
			RpcDo_LF_OpenGiveCash(target, carried);
		else
			Rpc(RpcDo_LF_OpenGiveCash, target, carried);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_OpenGiveCash(RplId target, int carried)
	{
		LF_GiveCashSheet.Open(this, target, carried);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: hand amount to the character with RplId target.
	void LF_AskGiveCash(RplId target, int amount)
	{
		if (Replication.IsServer())
			RpcAsk_LF_GiveCash(target, amount);
		else
			Rpc(RpcAsk_LF_GiveCash, target, amount);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_GiveCash(RplId target, int amount)
	{
		IEntity receiver = EL_NetworkUtils.FindEntityByRplId(target);
		LF_PlayerInteraction.GiveCash(GetControlledEntity(), receiver, amount, LF_PlayerInteraction.GESTURE_POINT);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: ask this player's machine what it holds, to hand it to target.
	void LF_OfferItemInHand(RplId target)
	{
		if (LF_IsLocalInteraction())
			RpcDo_LF_OfferItemInHand(target);
		else
			Rpc(RpcDo_LF_OfferItemInHand, target);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_OfferItemInHand(RplId target)
	{
		IEntity item = LF_PlayerInteraction.ItemInHand(GetControlledEntity());
		if (!item)
		{
			EL_Debug.Info("Interaction", "give item: nothing in hand");
			EL_Utils.Notify("#LF-Interact_NothingInHand", "#LF-Interact_Title", 3.0);
			return;
		}

		LF_AskGiveItem(target, EL_NetworkUtils.GetRplId(item));
	}

	//------------------------------------------------------------------------------------------------
	//! Client: give the item with RplId item to the character with RplId target.
	void LF_AskGiveItem(RplId target, RplId item)
	{
		if (Replication.IsServer())
			RpcAsk_LF_GiveItem(target, item);
		else
			Rpc(RpcAsk_LF_GiveItem, target, item);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_GiveItem(RplId target, RplId item)
	{
		IEntity receiver = EL_NetworkUtils.FindEntityByRplId(target);
		IEntity given = EL_NetworkUtils.FindEntityByRplId(item);
		LF_PlayerInteraction.GiveItem(GetControlledEntity(), receiver, given, LF_PlayerInteraction.GESTURE_POINT);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: this player now knows playerId's name.
	void LF_LearnName(int playerId, string name)
	{
		if (LF_IsLocalInteraction())
			RpcDo_LF_LearnName(playerId, name);
		else
			Rpc(RpcDo_LF_LearnName, playerId, name);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_LearnName(int playerId, string name)
	{
		LF_KnownNames.Learn(playerId, name);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: play a base-game gesture on this player's body (the owner animates it).
	void LF_PlayInteractionGesture(int gestureId, int durationMs)
	{
		if (LF_IsLocalInteraction())
			RpcDo_LF_PlayInteractionGesture(gestureId, durationMs);
		else
			Rpc(RpcDo_LF_PlayInteractionGesture, gestureId, durationMs);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_PlayInteractionGesture(int gestureId, int durationMs)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(GetControlledEntity());
		if (!character)
			return;

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
		if (!controller)
			return;

		if (!controller.CanPlayItemGesture())
		{
			EL_Debug.Info("Interaction", "gesture skipped: the character cannot gesture right now");
			return;
		}

		if (!controller.TryStartCharacterGesture(gestureId, durationMs))
			EL_Debug.Info("Interaction", "gesture " + gestureId + " did not start");
	}
}

//------------------------------------------------------------------------------------------------
//! Names follow a spawn: the spawning player learns who they know, and those who know them
//! learn their name again (player ids change between sessions; uids do not).
modded class EL_GameModeRoleplay
{
	override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);
		if (Replication.IsServer())
			LF_PlayerInteraction.SyncNames(playerId);
	}
}
