//------------------------------------------------------------------------------------------------
//! The give-up action of the downed state (#256): hold the key while downed. The client only
//! asks; the server (LF_BleedOut.GiveUp) checks that the controlled character is downed and that
//! the delay has passed. The request rides the player's own controller, which the client owns.
modded class SCR_PlayerController
{
	protected bool m_bLF_GiveUpListening;

	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		LF_RegisterGiveUpListener();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: listen for the key once. No local input on a dedicated server.
	protected void LF_RegisterGiveUpListener()
	{
		if (m_bLF_GiveUpListening || GetGame().GetPlayerController() != this)
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		input.AddActionListener("LF_GiveUp", EActionTrigger.DOWN, LF_OnGiveUpKey);
		m_bLF_GiveUpListening = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnGiveUpKey()
	{
		if (!LF_Downed.IsDowned(GetControlledEntity()))
			return;

		LF_AskGiveUp();
	}

	//------------------------------------------------------------------------------------------------
	//! Client entry (also the harness): ask the server to let this player die.
	void LF_AskGiveUp()
	{
		if (Replication.IsServer())
		{
			LF_BleedOut.GiveUp(GetControlledEntity());
			return;
		}

		Rpc(RpcAsk_LF_GiveUp);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_GiveUp()
	{
		LF_BleedOut.GiveUp(GetControlledEntity());
	}
}
