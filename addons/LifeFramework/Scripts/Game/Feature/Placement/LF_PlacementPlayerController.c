//------------------------------------------------------------------------------------------------
//! The keys of aimed placement (#1028) and its request to the server. The controller is the entity
//! the client owns for the whole session, so the listeners and the RPC ride it. The listeners do
//! nothing while LF_PlacementAim is not running. The client only proposes the ghost's spot; the
//! server decides again in LF_PlacementService.Confirm (who, where the item is, rate limit, reach,
//! probes), exactly as for the Deploy action.
//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	protected bool m_bLF_PlacementKeysListening;

	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		LF_RegisterPlacementKeys();
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_RegisterPlacementKeys()
	{
		if (m_bLF_PlacementKeysListening || GetGame().GetPlayerController() != this)
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		input.AddActionListener("LF_PlacementPlace", EActionTrigger.DOWN, LF_OnPlacementPlace);
		input.AddActionListener("LF_PlacementCancel", EActionTrigger.DOWN, LF_OnPlacementCancel);
		input.AddActionListener("LF_PlacementTurnLeft", EActionTrigger.DOWN, LF_OnPlacementTurnLeft);
		input.AddActionListener("LF_PlacementTurnRight", EActionTrigger.DOWN, LF_OnPlacementTurnRight);
		m_bLF_PlacementKeysListening = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnPlacementPlace()
	{
		LF_PlacementAim.Place();
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnPlacementCancel()
	{
		LF_PlacementAim.Cancel();
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnPlacementTurnLeft()
	{
		LF_PlacementAim.Turn(-1, LF_PlacementFineHeld());
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnPlacementTurnRight()
	{
		LF_PlacementAim.Turn(1, LF_PlacementFineHeld());
	}

	//------------------------------------------------------------------------------------------------
	protected bool LF_PlacementFineHeld()
	{
		InputManager input = GetGame().GetInputManager();
		return input && input.GetActionValue("LF_PlacementFine") > 0.5;
	}

	//------------------------------------------------------------------------------------------------
	//! Client entry (the mode's Place key, the harness): proposes the ghost's spot for `item`. The
	//! listen host confirms directly.
	void LF_AskPlaceAimed(IEntity item, vector origin, vector forward)
	{
		RplComponent rpl = LF_Component<RplComponent>.Find(item);
		if (!rpl)
		{
			LF_Debug.Error("Placement", "aimed place dropped: the item has no RplComponent");
			return;
		}

		if (Replication.IsServer())
		{
			LF_ServerPlaceAimed(rpl.Id(), origin, forward);
			return;
		}

		Rpc(RpcAsk_LF_PlaceAimed, rpl.Id(), origin, forward);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PlaceAimed(RplId itemId, vector origin, vector forward)
	{
		LF_ServerPlaceAimed(itemId, origin, forward);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_ServerPlaceAimed(RplId itemId, vector origin, vector forward)
	{
		IEntity player = GetControlledEntity();
		IEntity item = LF_NetworkUtils.FindEntityByRplId(itemId);
		if (!player || !item)
		{
			LF_Debug.Warn("Placement", "aimed place refused: no controlled character or the item does not resolve");
			return;
		}

		LF_PlacementService.ConfirmAimed(player, item, origin, forward);
	}
}
