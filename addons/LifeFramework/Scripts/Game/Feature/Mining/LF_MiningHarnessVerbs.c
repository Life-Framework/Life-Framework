//------------------------------------------------------------------------------------------------
//! Session proof verb for the coal chain (#3244): `coalgive [n]` puts n coal lumps (one by default, at most
//! 20) in the caller's inventory, the way `nuggets` does for iron. A proof that mines a coal rock for real
//! still needs a known amount at the smelter, and a refusal below the smelting level needs the inputs
//! in hand without a trip to the mine. Same gate as `repgrant` and /licence: development worlds and
//! harness builds, or an administrator. The client sends the count; the server decides and logs
//! `[LFDebug:Mining] harness coal <uid> added=A held=H`.
//------------------------------------------------------------------------------------------------
class LF_MiningHarness
{
	static const ResourceName COAL_LUMP = "{C2AE9B7E62F0369F}Prefabs/Resources/Mining/CoalLump.et";
	static const int MAX_COAL = 20;	// lf-fixed: a dev harness bound, not a tunable

	//------------------------------------------------------------------------------------------------
	//! Server: puts \p count coal lumps (1 to MAX_COAL) in \p character's inventory; what does not fit
	//! stays out (no drop on the ground, so a refusal is visible in the count).
	//! \return Lumps actually added.
	static int GiveCoal(IEntity character, int count)
	{
		if (!character)
			return 0;

		int wanted = Math.ClampInt(count, 1, MAX_COAL);
		return LF_InventoryUtils.AddAmount(character, COAL_LUMP, wanted);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_AskCoalGive(int count)
	{
		if (Replication.IsServer())
			RpcAsk_LF_CoalGive(count);
		else
			Rpc(RpcAsk_LF_CoalGive, count);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_CoalGive(int count)
	{
		int playerId = GetPlayerId();
		bool allowed = LF_DevGate.HarnessBuild() || LF_DevGate.CheckPlayerRequest(playerId, "Mining", "harness coal");
		if (!allowed)
			return;

		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!body)
		{
			LF_Debug.Error("Mining", string.Format("harness coal refused: player %1 has no body", playerId));
			return;
		}

		int added = LF_MiningHarness.GiveCoal(body, count);
		LF_Debug.Info("Mining", string.Format("harness coal %1 added=%2 held=%3", LF_Utils.GetPlayerUID(playerId), added, LF_InventoryUtils.GetAmount(body, LF_MiningHarness.COAL_LUMP)));
	}
}

//------------------------------------------------------------------------------------------------
//! Harness `coalgive [n]`.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "coalgive")
			return super.Handle(cmd);

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_controller");
			return true;
		}

		int count = 1;
		if (!cmd.Arg1.IsEmpty())
			count = cmd.Arg1.ToInt();

		controller.LF_AskCoalGive(count);
		LF_AgentHarness.Answer(cmd.Id, "ok", "asked coalgive");
		return true;
	}
}
