// Server-side stand for the agent harness. The client names an entity that
// already exists in the loaded world; the server moves only the requesting
// player's character to just beside it. That is how a play session reaches a
// DebugWorld layer without injecting movement keys. The interaction that
// follows still goes through the normal action checks.

modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! \param entityName World entity to stand beside. Resolved on the server.
	void EL_AskHarnessStand(string entityName)
	{
		Rpc(RpcAsk_EL_HarnessStand, entityName);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_HarnessStand(string entityName)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		IEntity target = world.FindEntityByName(entityName);
		if (!target)
		{
			EL_Debug.Error("AgentHarness", string.Format("stand refused: %1 is not in the world", entityName));
			return;
		}

		// One metre to the side keeps the character inside a 3m action radius
		// without planting them inside the prop.
		SetOrigin(target.GetOrigin() + Vector(1.2, 0, 0));
		EL_Debug.Log("AgentHarness", string.Format("stood beside %1", entityName));
	}
}
