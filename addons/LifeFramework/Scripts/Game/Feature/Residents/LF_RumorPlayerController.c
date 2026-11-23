//------------------------------------------------------------------------------------------------
//! The wire of a told rumor (#4569). The server tells the listener's own client which row to play and
//! from whom; the client finds the clip in its own copy of the rumor config, so only an entity id and a
//! row id travel. Nothing goes from a client to the server.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Server: play rumor `rumorId` from the entity at `speakerId` on this player's machine.
	void LF_RumorSay(RplId speakerId, string rumorId)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_RumorSay(speakerId, rumorId);
		else
			Rpc(RpcDo_LF_RumorSay, speakerId, rumorId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_RumorSay(RplId speakerId, string rumorId)
	{
		LF_RumorDef def = LF_RumorPool.Find(rumorId);
		if (!def)
		{
			LF_Debug.Warn(LF_RumorVoice.FEATURE, string.Format("rumor '%1' is not in this machine's rumor config: no clip", rumorId));
			return;
		}

		LF_RumorVoice.Speak(LF_NetworkUtils.FindEntityByRplId(speakerId), def);
	}
}
