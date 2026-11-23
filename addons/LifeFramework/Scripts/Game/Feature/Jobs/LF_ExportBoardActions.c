//------------------------------------------------------------------------------------------------
//! "Take the order" rows on the export order board (#4411). The prefab carries LF_ExportBoard.MAX_ROWS
//! of these, slot 0 up; slot N sits on the board's Nth printed line and shows only while that line is
//! an open order. The action runs on the reader's machine and names the order by its id (never by the
//! slot, which shifts when an order closes); the server decides: range, level, one order per hauler.
//------------------------------------------------------------------------------------------------
class LF_ExportBoardTakeAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.EditBox, "Which printed line of the board this row takes (0 = the first line)")]
	protected int m_iSlot;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled(LF_CivilianJob.SYSTEM))
			return false;

		return LF_ExportBoard.OpenOrderIdAt(m_iSlot) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		string label = LF_ExportBoard.LabelAt(m_iSlot);
		if (label.IsEmpty())
			return false;

		outName = WidgetManager.Translate("#LF-ExportBoard_Take", label);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses(LF_CivilianJob.SYSTEM, "LF_ExportBoardTakeAction"))
			return;

		if (!pOwnerEntity || !pUserEntity)
			return;

		int orderId = LF_ExportBoard.OpenOrderIdAt(m_iSlot);
		if (orderId < 0)
			return;

		// A server-driven use (the agent harness): the server takes it for that reader directly.
		if (pUserEntity != LF_Utils.GetLocalCharacter())
		{
			if (!Replication.IsServer())
				return;

			LF_ExportBoardTake.ServerTake(pUserEntity, orderId);
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_ExportBoard.FEATURE, "export order take not sent: no SCR_PlayerController");
			return;
		}

		controller.LF_AskExportTake(orderId);
	}
}

//------------------------------------------------------------------------------------------------
class LF_ExportBoardTake
{
	//! Client: the board asks again this long after a take, so the line changes at once.
	static const int REFRESH_DELAY_MS = 1200;	// lf-fixed: one RPC round trip with room, not a tunable

	//------------------------------------------------------------------------------------------------
	//! Client: ask the server for the board face's rows again (a plain class, so it sees the board
	//! bridge in the Signs folder, which loads after this folder's modded classes).
	static void Refresh(SCR_PlayerController controller)
	{
		if (controller)
			controller.LF_AskBoard(LF_ExportBoard.KIND, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `user` asks for order `orderId` at the board. The reader must stand at a board, the
	//! hauler's DELIVERY level and the one-order rule are the book's (LF_ExportOrderBook.Take). The
	//! reason for a refusal goes to that player as a toast.
	//! \return True when the order is now theirs.
	static bool ServerTake(IEntity user, int orderId)
	{
		if (!user || !Replication.IsServer())
			return false;

		if (LF_Systems.Refuses(LF_CivilianJob.SYSTEM, "export order take"))
			return false;

		if (!LF_ActionSite.IsNear(user.GetOrigin(), LF_PoliceConfig.BoardReadDistance(), LF_ExportBoard.READ_ACTION))
		{
			LF_Debug.Warn(LF_ExportBoard.FEATURE, string.Format("export order take refused: player is not at the board (order %1)", orderId));
			return false;
		}

		LF_ExportOrderConfig config = LF_ExportOrderConfig.Get();
		string uid = LF_Utils.GetPlayerUid(user);
		if (!config || uid.IsEmpty())
		{
			LF_Utils.NotifyEntity(user, LF_ExportOrderBook.REFUSE_GONE);
			return false;
		}

		LF_ExportOrderBook.Tick(LF_ExportOrderBook.CurrentMinute());
		string refusal = LF_ExportOrderBook.Take(uid, LF_JobLevels.LevelOf(uid, config.m_sJob), orderId);
		if (!refusal.IsEmpty())
		{
			LF_Utils.NotifyEntity(user, refusal, "", 3.0, config.m_iMinLevel.ToString());
			return false;
		}

		LF_ExportOrder order = LF_ExportOrderBook.Find(orderId);
		if (order)
			LF_Utils.NotifyEntity(user, "#LF-ExportOrder_Taken", "", 4.0, order.DeadlineText());

		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: ask the server to give this player export order `orderId`; then re-read the board face.
	void LF_AskExportTake(int orderId)
	{
		if (Replication.IsServer())
			RpcAsk_LF_ExportTake(orderId);
		else
			Rpc(RpcAsk_LF_ExportTake, orderId);

		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(LF_ExportBoardTake.Refresh, LF_ExportBoardTake.REFRESH_DELAY_MS, false, this);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_ExportTake(int orderId)
	{
		// The shared board comms limit comes first: a spamming client cannot flood the log or the book.
		if (!LF_BoardProviders.AllowReadAt(GetPlayerId(), System.GetTickCount()))
			return;

		LF_ExportBoardTake.ServerTake(GetControlledEntity(), orderId);
	}
}
