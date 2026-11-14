//------------------------------------------------------------------------------------------------
//! The wire of a held robbery (LF_RobHold): the server tells the robber's own client when the hold
//! begins and how it ended. Nothing goes the other way; the start is a dialogue reply, which the
//! dialogue already validates on the server.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Server: show the hold on this player's screen.
	void LF_RobHoldBegin(vector till, float radius, int holdMs)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_RobHoldBegin(till, radius, holdMs);
		else
			Rpc(RpcDo_LF_RobHoldBegin, till, radius, holdMs);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_RobHoldBegin(vector till, float radius, int holdMs)
	{
		LF_RobHoldHud.Begin(till, radius, holdMs);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the hold ended (an LF_ERobHoldEnd) and paid `paid` cash.
	void LF_RobHoldEnd(int end, int paid)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_RobHoldEnd(end, paid);
		else
			Rpc(RpcDo_LF_RobHoldEnd, end, paid);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_RobHoldEnd(int end, int paid)
	{
		LF_RobHoldHud.End(end, paid);
	}
}
