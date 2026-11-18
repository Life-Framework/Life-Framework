//------------------------------------------------------------------------------------------------
//! /policerank <account uid> <rank> (#2838): an administrator sets an officer's police rank, up or
//! down (demotion has no other path). Who may is decided on the server, never from the payload: the
//! engine administrator flag or the staff roster level `administrator`, as for application decisions
//! (LF_PoliceApplicationGm.MayDecide). The rank must be on the ladder. Every change and refusal is
//! logged with both uids.
//!
//! The rank is the target account's active character's (#331). A running duty clock is settled first,
//! so the seconds earned at the old rank count at the old rank. After a demotion the character's
//! on-duty seconds restart at the start of the new rank, so the next accrual does not promote them
//! straight back. Medic ranks are not covered: they are not on main yet (#2837).
//------------------------------------------------------------------------------------------------
class LF_PoliceRankAdmin
{
	static const string REFUSED_DENIED = "#LF-PoliceRankSet_Denied";
	static const string REFUSED_TARGET = "#LF-PoliceRankSet_NoTarget";
	static const string REFUSED_RANK = "#LF-PoliceRankSet_BadRank";

	//------------------------------------------------------------------------------------------------
	//! Server: one rank set. \return "" when it went through, else the refusal key.
	static string SetRank(string callerUid, bool engineAdmin, string targetUid, int rank)
	{
		if (!LF_PoliceApplicationGm.MayDecide(callerUid, engineAdmin))
		{
			LF_Debug.Warn("Ranks", string.Format("rank set refused for %1: not an administrator (target %2, rank %3)", callerUid, targetUid, rank));
			return REFUSED_DENIED;
		}

		LF_PlayerAccount account;
		LF_PlayerAccountManager manager = LF_PlayerAccountManager.GetInstance();
		if (manager && LF_PoliceApplicationStaff.IsValidUid(targetUid))
			account = manager.GetAccount(targetUid);

		if (!account)
		{
			LF_Debug.Warn("Ranks", string.Format("rank set refused for %1: no account '%2'", callerUid, targetUid));
			return REFUSED_TARGET;
		}

		if (!LF_PoliceRanks.IsRankUsable(rank))
		{
			LF_Debug.Warn("Ranks", string.Format("rank set refused for %1: rank %2 is not on the ladder (0..%3)", callerUid, rank, LF_PoliceRanks.TopRank()));
			return REFUSED_RANK;
		}

		// Seconds earned at the old rank count at the old rank.
		LF_PoliceRanks.SettleNow(account);
		int oldRank = account.GetPoliceRank();
		account.SetPoliceRank(rank);

		string change = "unchanged";
		if (rank > oldRank)
		{
			change = "promotion";
			LF_PoliceApplications.EndTrial(targetUid);
		}
		else if (rank < oldRank)
		{
			change = "demotion";
		}

		// Rank thresholds are running totals of on-duty seconds. A demoted officer starts the new rank
		// over (else the next accrual promotes them straight back); a promoted one starts it at least.
		int oldSeconds = account.GetOnDutySeconds();
		int newSeconds = RankStartSeconds(rank);
		if (rank > oldRank)
			newSeconds = LF_Utils.MaxInt(oldSeconds, newSeconds);
		else if (rank == oldRank)
			newSeconds = oldSeconds;

		if (newSeconds != oldSeconds)
		{
			account.SetOnDutySeconds(newSeconds);
			LF_Debug.Info("Ranks", string.Format("rank set: %1 on-duty seconds %2 -> %3 (start of rank %4)", targetUid, oldSeconds, newSeconds, rank));
		}

		LF_Debug.Info("Ranks", string.Format("rank set by administrator %1: %2 (character %3) rank %4 -> %5 (%6)", callerUid, targetUid, account.GetActiveCharacterKey(), oldRank, rank, change));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The on-duty seconds at which an officer reaches this rank: the threshold of the rung below, 0 for patrol.
	static int RankStartSeconds(int rank)
	{
		if (rank <= LF_PoliceRanks.PATROL)
			return 0;

		return LF_PoliceRanks.DutySecondsForNext(rank - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: "<uid> <rank>" from the chat line. False when it is not exactly that.
	static bool ParseArgs(string data, out string targetUid, out int rank)
	{
		targetUid = "";
		rank = -1;
		array<string> parts = {};
		data.Trim().Split(" ", parts, true);
		if (parts.Count() != 2)
			return false;

		string rankText = parts[1];
		if (rankText.IsEmpty() || rankText.ToInt().ToString() != rankText)
			return false;

		targetUid = parts[0];
		rank = rankText.ToInt();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the RPC body. The caller is the player the RPC arrived from.
	static void ServerRun(int playerId, string data)
	{
		if (!Replication.IsServer())
			return;

		string targetUid;
		int rank;
		string refusal;
		if (!ParseArgs(data, targetUid, rank))
		{
			LF_Debug.Warn("Ranks", string.Format("rank set refused for player %1: '%2' is not <uid> <rank>", playerId, data));
			refusal = "#LF-PoliceRankSet_Usage";
		}
		else
		{
			refusal = SetRank(LF_Utils.GetPlayerUID(playerId), AdminChatChannel.IsAdminPlayer(playerId), targetUid, rank);
		}

		if (!refusal.IsEmpty())
		{
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Police_Title", refusal, 5.0, LF_ENotificationType.WARNING, rank.ToString());
			return;
		}

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Police_Title", "#LF-PoliceRankSet_Done", 5.0, LF_ENotificationType.INFO, rank.ToString());
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
modded class SCR_ChatPanelManager
{
	//------------------------------------------------------------------------------------------------
	override protected void InitDefaultChatCommands()
	{
		super.InitDefaultChatCommands();
		ChatCommandInvoker invoker = GetCommandInvoker("policerank");
		if (invoker)
			invoker.Insert(LF_OnPoliceRankCommand);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnPoliceRankCommand(SCR_ChatPanel panel, string data)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error("Ranks", "/policerank not sent: no local player controller");
			return;
		}

		controller.LF_AskPoliceRankSet(data);
	}
};

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Asks the server to set a police rank. The player owns their controller, so the RPC is allowed;
	//! on the server Rpc() runs the Server handler at once.
	void LF_AskPoliceRankSet(string data)
	{
		Rpc(RpcAsk_LF_PoliceRankSet, data);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PoliceRankSet(string data)
	{
		LF_PoliceRankAdmin.ServerRun(GetPlayerId(), data);
	}
};
