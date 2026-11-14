//------------------------------------------------------------------------------------------------
//! The "Reset world" request of the debug menu and the travel globe (Core/LF_WorldReset.c). Hosted
//! on the owned character, the one entity a client may RPC on. The server decides: the mission
//! must be a playtest mode and the dev gate open for the sender (LF_WorldReset.OnRequest).
//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! Ask the server to reset the world for everyone.
	//! \param source "menu" or "globe", for the log.
	void LF_AskWorldReset(string source)
	{
		if (Replication.IsServer())
		{
			LF_WorldReset.OnRequest(this, source);
			return;
		}

		Rpc(RpcAsk_LF_WorldReset, source);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_WorldReset(string source)
	{
		if (!LF_DevGate.CheckHarnessRequest(this, "world reset"))
			return;

		// The source is only a log label; a client cannot make it anything else.
		if (source != "menu" && source != "globe")
			source = "request";

		LF_WorldReset.OnRequest(this, source);
	}
}
