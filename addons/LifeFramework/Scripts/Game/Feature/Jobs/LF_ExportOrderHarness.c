//------------------------------------------------------------------------------------------------
//! Harness verb `exportorder take|give short|rest` for the export crane proof (agent-box/proofs/
//! export-crane-proof.mjs, #4580). Both ask the server; same gate as `levelupcard grant`
//! (development worlds and harness builds, or an administrator).
//!   take        the caller takes the open order with the smallest count, at the gate level (the
//!               DELIVERY track's level is not reached in a proof); the board's level refusal is a
//!               separate, real take.
//!   give short  the server puts the held order's count minus one of its goods in the caller's pockets;
//!   give rest   one more unit (overflow drops at the feet).
//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "exportorder")
			return super.Handle(cmd);

		if (cmd.Arg1 != "take" && cmd.Arg1 != "give")
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "exportorder take|give short|rest");
			return true;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no player controller");
			return true;
		}

		controller.LF_AskExportHarness(cmd.Arg1, cmd.Arg2 == "short");
		LF_AgentHarness.Answer(cmd.Id, "ok", "asked " + cmd.Arg1 + " " + cmd.Arg2);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_AskExportHarness(string what, bool shortCount)
	{
		if (Replication.IsServer())
			RpcAsk_LF_ExportHarness(what, shortCount);
		else
			Rpc(RpcAsk_LF_ExportHarness, what, shortCount);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_ExportHarness(string what, bool shortCount)
	{
		int playerId = GetPlayerId();
		bool allowed = LF_DevGate.HarnessBuild() || LF_DevGate.CheckPlayerRequest(playerId, LF_ExportOrderBook.FEATURE, "harness export order " + what);
		if (!allowed)
			return;

		IEntity user = GetControlledEntity();
		string uid = LF_Utils.GetPlayerUID(playerId);
		LF_ExportOrderConfig config = LF_ExportOrderConfig.Get();
		if (!user || uid.IsEmpty() || !config)
		{
			LF_Debug.Error(LF_ExportOrderBook.FEATURE, string.Format("harness export %1 refused: no body, uid or config for player %2", what, playerId));
			return;
		}

		LF_ExportOrderBook.Tick(LF_ExportOrderBook.CurrentMinute());
		if (what == "take")
		{
			LF_ExportOrder smallest;
			foreach (LF_ExportOrder order : LF_ExportOrderBook.List())
			{
				if (order && !order.IsTaken() && (!smallest || order.m_iCount < smallest.m_iCount))
					smallest = order;
			}

			if (!smallest)
			{
				LF_Debug.Warn(LF_ExportOrderBook.FEATURE, "harness export take: no open order");
				return;
			}

			string refusal = LF_ExportOrderBook.Take(uid, config.m_iMinLevel, smallest.m_iId);
			LF_Debug.Info(LF_ExportOrderBook.FEATURE, string.Format("harness export take %1 (%2 x %3) for %4: '%5'", smallest.m_iId, smallest.m_iCount, smallest.m_sGoodsName, uid, refusal));
			return;
		}

		LF_ExportOrder held = LF_ExportOrderBook.HeldBy(uid);
		if (!held)
		{
			LF_Debug.Warn(LF_ExportOrderBook.FEATURE, string.Format("harness export give refused: %1 holds no order", uid));
			return;
		}

		int count = 1;
		if (shortCount)
			count = held.m_iCount - 1;

		int given = LF_InventoryUtils.AddAmount(user, held.m_Goods, count, true);
		LF_Debug.Info(LF_ExportOrderBook.FEATURE, string.Format("harness export give: %1 of %2 x %3 to %4", given, count, held.m_sGoodsName, uid));
	}
}
