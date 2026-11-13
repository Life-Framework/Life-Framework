//------------------------------------------------------------------------------------------------
//! The wire of a conversation. Requests ride the player's own controller (the only entity a
//! client may RPC on); answers come back to its owner. The client sends an NPC and later a reply
//! index, nothing else; the server sends ids of nodes and lines.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: ask to talk to an NPC.
	void LF_AskDialogueStart(IEntity npc)
	{
		if (Replication.IsServer())
		{
			LF_Dialogue.Start(this, npc);
			return;
		}

		Rpc(RpcAsk_LF_DialogueStart, LF_NetworkUtils.GetRplId(npc));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_DialogueStart(RplId npcId)
	{
		LF_Dialogue.Start(this, LF_NetworkUtils.FindEntityByRplId(npcId));
	}

	//------------------------------------------------------------------------------------------------
	//! Client: pick a reply by its index at the current node.
	void LF_AskDialogueChoose(int index)
	{
		if (Replication.IsServer())
		{
			LF_Dialogue.Choose(this, index);
			return;
		}

		Rpc(RpcAsk_LF_DialogueChoose, index);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_DialogueChoose(int index)
	{
		LF_Dialogue.Choose(this, index);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the player left the conversation.
	void LF_AskDialogueLeave()
	{
		if (Replication.IsServer())
		{
			LF_Dialogue.End(this, false);
			return;
		}

		Rpc(RpcAsk_LF_DialogueLeave);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_DialogueLeave()
	{
		LF_Dialogue.End(this, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: show a node on this player's screen.
	void LF_DialogueShow(RplId npcId, string nodeId, string spoken, int mask)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_DialogueShow(npcId, nodeId, spoken, mask);
		else
			Rpc(RpcDo_LF_DialogueShow, npcId, nodeId, spoken, mask);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_DialogueShow(RplId npcId, string nodeId, string spoken, int mask)
	{
		LF_DialogueScene.Show(LF_NetworkUtils.FindEntityByRplId(npcId), nodeId, spoken, mask);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: close the scene on this player's screen.
	void LF_DialogueClose()
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_DialogueClose();
		else
			Rpc(RpcDo_LF_DialogueClose);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_DialogueClose()
	{
		LF_DialogueScene.Close("the server ended it", false);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: this player stands near an NPC that is speaking to someone else.
	void LF_DialogueHear(RplId npcId, string spoken)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_DialogueHear(npcId, spoken);
		else
			Rpc(RpcDo_LF_DialogueHear, npcId, spoken);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_DialogueHear(RplId npcId, string spoken)
	{
		LF_DialogueVoice.SayList(LF_NetworkUtils.FindEntityByRplId(npcId), spoken);
	}
}
