//------------------------------------------------------------------------------------------------
//! Server-validated opening of a hideout stash. The player controller is the entity the client
//! owns for the whole session, so the request and the answer ride it.
//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	protected static const float LF_STASH_REACH = 5.0;

	//------------------------------------------------------------------------------------------------
	//! Client: asks the server to open the stash. The listen host handles it directly.
	void LF_RequestOpenStash(IEntity stash)
	{
		RplComponent rpl = EL_Component<RplComponent>.Find(stash);
		if (!rpl)
		{
			EL_Debug.Error("Hideout", "stash open dropped: the stash has no RplComponent");
			return;
		}

		if (Replication.IsServer())
		{
			RpcAsk_LF_OpenStash(rpl.Id());
			return;
		}

		Rpc(RpcAsk_LF_OpenStash, rpl.Id());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OpenStash(RplId stashId)
	{
		RplComponent stashRpl = RplComponent.Cast(Replication.FindItem(stashId));
		IEntity stash;
		if (stashRpl)
			stash = stashRpl.GetEntity();

		IEntity user = GetControlledEntity();
		if (!stash || !user)
		{
			EL_Debug.Warn("Hideout", string.Format("stash open refused for player %1: no stash or no character", GetPlayerId()));
			return;
		}

		LF_HideoutStashStorageManagerComponent manager = EL_Component<LF_HideoutStashStorageManagerComponent>.Find(stash);
		if (!manager)
		{
			EL_Debug.Error("Hideout", string.Format("stash open refused: %1 has no LF_HideoutStashStorageManagerComponent", EL_Utils.GetPrefabName(stash)));
			return;
		}

		if (vector.Distance(user.GetOrigin(), stash.GetOrigin()) > LF_STASH_REACH)
		{
			EL_Debug.Warn("Hideout", string.Format("stash open refused for player %1: out of reach", GetPlayerId()));
			return;
		}

		if (!manager.AllowsUser(user, true))
		{
			EL_Debug.Info("Hideout", string.Format("stash %1 refused player %2: not a member", manager.GetHideoutId(), GetPlayerId()));
			EL_NotificationManagerComponent.NotifyPlayer(GetPlayerId(), "#LF-HideoutStash_Title", "#LF-HideoutStash_NotMember", 4.0, EL_ENotificationType.WARNING);
			if (GetGame().GetPlayerController() == this)
				RpcDo_LF_StashRefused();
			else
				Rpc(RpcDo_LF_StashRefused);
			return;
		}

		EL_Debug.Info("Hideout", string.Format("stash %1 opened by player %2", manager.GetHideoutId(), GetPlayerId()));
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_OpenStash(stashId);
		else
			Rpc(RpcDo_LF_OpenStash, stashId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_OpenStash(RplId stashId)
	{
		RplComponent stashRpl = RplComponent.Cast(Replication.FindItem(stashId));
		IEntity user = GetControlledEntity();
		if (!stashRpl || !stashRpl.GetEntity() || !user)
		{
			EL_Debug.Warn("Hideout", "stash open dropped on the client: the stash or the character is gone");
			return;
		}

		SCR_InventoryStorageManagerComponent inventory = EL_Component<SCR_InventoryStorageManagerComponent>.Find(user);
		if (!inventory)
		{
			EL_Debug.Error("Hideout", "stash open dropped on the client: the character has no inventory manager");
			return;
		}

		inventory.SetStorageToOpen(stashRpl.GetEntity());
		inventory.OpenInventory();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_StashRefused()
	{
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ACTION_FAILED);
	}
}
