//------------------------------------------------------------------------------------------------
//! Harness verb `exportorder take|give short|rest` for the export crane proof (agent-box/proofs/
//! export-crane-proof.mjs, #4580). Both ask the server; same gate as `levelupcard grant`
//! (development worlds and harness builds, or an administrator).
//!   take        the caller takes the open order with the smallest count, at the gate level (the
//!               DELIVERY track's level is not reached in a proof); the board's level refusal is a
//!               separate, real take.
//!   give short  the server puts the held order's count minus one of its goods in the caller's pockets
//!               (what does not fit drops at the feet, and the log line counts the drops as given);
//!   give rest   one more unit;
//!   draw <kind> opens an order of the template whose id starts with <kind> (planks) as the board's first
//!               line, so the board's first take row takes it (the truck branch of the proof needs it);
//!   give truck  the held order's whole count into the storage of the nearest vehicle within 12 m, what a
//!               player does with the inventory screen when loading the truck (the harness cannot drag).
//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "exportorder")
			return super.Handle(cmd);

		if (cmd.Arg1 != "take" && cmd.Arg1 != "give" && cmd.Arg1 != "draw")
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "exportorder take|give short|rest|truck|draw <kind>");
			return true;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no player controller");
			return true;
		}

		controller.LF_AskExportHarness(cmd.Arg1, cmd.Arg2);
		LF_AgentHarness.Answer(cmd.Id, "ok", "asked " + cmd.Arg1 + " " + cmd.Arg2);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_AskExportHarness(string what, string mode)
	{
		if (Replication.IsServer())
			RpcAsk_LF_ExportHarness(what, mode);
		else
			Rpc(RpcAsk_LF_ExportHarness, what, mode);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_ExportHarness(string what, string mode)
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
		if (what == "draw")
		{
			LF_ExportOrder drawn = LF_ExportOrderBook.OpenFirst(mode, LF_ExportOrderBook.CurrentMinute());
			if (drawn)
				LF_Debug.Info(LF_ExportOrderBook.FEATURE, string.Format("harness export draw %1: order %2 (%3 x %4) is the first line of the book", mode, drawn.m_iId, drawn.m_iCount, drawn.m_sGoodsName));
			else
				LF_Debug.Warn(LF_ExportOrderBook.FEATURE, string.Format("harness export draw refused: no priced order kind starts with '%1'", mode));
			return;
		}

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

		if (mode == "truck")
		{
			GiveToTruck(user, held, uid);
			return;
		}

		int count = 1;
		if (mode == "short")
			count = held.m_iCount - 1;

		// The count is what AddAmount spawned: the carry limit drops the pockets' excess a moment later (119 of
		// 119 here, then the crane counts 3), so the proof reads what the crane counts from its refusal line.
		int given = LF_InventoryUtils.AddAmount(user, held.m_Goods, count, true);
		LF_Debug.Info(LF_ExportOrderBook.FEATURE, string.Format("harness export give: %1 of %2 x %3 to %4", given, count, held.m_sGoodsName, uid));
	}

	//------------------------------------------------------------------------------------------------
	protected void GiveToTruck(IEntity user, LF_ExportOrder held, string uid)
	{
		IEntity truck;
		float best = float.MAX;
		array<IEntity> nearby = LF_Utils.GetNearbyVehicles(user.GetOrigin(), 12.0);
		foreach (IEntity vehicle : nearby)
		{
			float distance = vector.Distance(vehicle.GetOrigin(), user.GetOrigin());
			if (distance < best && LF_FeedFromVehicle.StorageOf(vehicle))
			{
				best = distance;
				truck = vehicle;
			}
		}

		InventoryStorageManagerComponent storage = LF_FeedFromVehicle.StorageOf(truck);
		if (!storage)
		{
			LF_Debug.Warn(LF_ExportOrderBook.FEATURE, string.Format("harness export give truck refused: no vehicle with storage within 12 m of %1", uid));
			return;
		}

		int given = LF_InventoryUtils.AddAmount(storage, held.m_Goods, held.m_iCount, false);
		LF_Debug.Info(LF_ExportOrderBook.FEATURE, string.Format("harness export give truck: %1 of %2 x %3 into the vehicle's storage for %4", given, held.m_iCount, held.m_sGoodsName, uid));
	}
}
