//------------------------------------------------------------------------------------------------
//! Bail. A prisoner in custody pays from their bank account to leave early: the bail is
//! LF_JailBreakConfig.BailPerMinute for each started minute left, never below BailMinimum or above
//! BailMaximum. Paid, the sentence ends like a served one (LF_Jail.EndOnBail: released at the jail's
//! exit spot on the floor, wanted reduced); the crime record is not touched. Refused when bail is
//! off, the payer is not in custody (an escapee on the run is not), or the bank holds less than the
//! bail; nothing is taken then. The money moves once: Withdraw is the check, and a second request
//! finds nobody serving.
//!
//! Cash is not taken: the bank is the only purse (cash on the body may have been seized at the arrest).
class LF_JailBailRules
{
	//------------------------------------------------------------------------------------------------
	//! Bail for `remainingSeconds` left: per started minute, clamped to the minimum and maximum.
	//! 0 when nothing is left to serve.
	static int Quote(float remainingSeconds, int perMinute, int minimum, int maximum)
	{
		if (remainingSeconds <= 0)
			return 0;

		int minutes = Math.Ceil(remainingSeconds / 60);
		int bail = minutes * LF_Utils.MaxInt(perMinute, 0);
		bail = LF_Utils.MaxInt(bail, LF_Utils.MaxInt(minimum, 0));
		if (maximum > 0)
			bail = LF_Utils.MinInt(bail, LF_Utils.MaxInt(maximum, minimum));

		return bail;
	}
}

//------------------------------------------------------------------------------------------------
enum LF_EJailBailResult
{
	PAID,
	OFF,
	NOT_IN_CUSTODY,
	NO_ACCOUNT,
	TOO_POOR
}

//------------------------------------------------------------------------------------------------
class LF_JailBail
{
	static const string FEATURE = "JailBail";
	static const string REASON = "jail-bail";

	//------------------------------------------------------------------------------------------------
	//! Bail the prisoner would pay now, 0 when they are not in custody.
	static int QuoteFor(string playerUid)
	{
		if (!LF_Jail.IsInCustody(playerUid))
			return 0;

		LF_JailBreakConfig config = LF_JailBreakConfig.Get();
		return LF_JailBailRules.Quote(LF_Jail.GetRemaining(playerUid), config.BailPerMinute(), config.BailMinimum(), config.BailMaximum());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the prisoner pays their own bail from the bank and is released.
	//! \param[out] bail The bail asked (0 when not in custody).
	static LF_EJailBailResult Pay(string playerUid, out int bail)
	{
		bail = 0;
		if (!LF_JailBreakConfig.Get().m_bBailEnabled)
		{
			LF_Debug.Info(FEATURE, string.Format("bail refused for %1: bail is off", playerUid));
			return LF_EJailBailResult.OFF;
		}

		if (playerUid.IsEmpty() || !LF_Jail.IsInCustody(playerUid))
		{
			LF_Debug.Info(FEATURE, string.Format("bail refused for %1: not in custody", playerUid));
			return LF_EJailBailResult.NOT_IN_CUSTODY;
		}

		bail = QuoteFor(playerUid);
		LF_BankAccount account = LF_ATMManager.GetOrCreate(playerUid);
		if (!account)
		{
			LF_Debug.Error(FEATURE, string.Format("bail refused for %1: no bank account", playerUid));
			return LF_EJailBailResult.NO_ACCOUNT;
		}

		if (!account.Withdraw(bail, REASON))
		{
			LF_Debug.Info(FEATURE, string.Format("bail refused for %1: bank %2 < bail %3", playerUid, account.GetBalance(), bail));
			return LF_EJailBailResult.TOO_POOR;
		}

		LF_Jail.EndOnBail(playerUid);
		LF_Debug.Info(FEATURE, string.Format("%1 paid bail %2 from the bank (left %3)", playerUid, bail, account.GetBalance()));
		return LF_EJailBailResult.PAID;
	}

	//------------------------------------------------------------------------------------------------
	//! The toast for a result.
	static string MessageFor(LF_EJailBailResult result)
	{
		switch (result)
		{
			case LF_EJailBailResult.PAID:
				return "#LF-JailBail_Paid";
			case LF_EJailBailResult.OFF:
				return "#LF-JailBail_Off";
			case LF_EJailBailResult.NOT_IN_CUSTODY:
				return "#LF-JailBail_NotServing";
			case LF_EJailBailResult.TOO_POOR:
				return "#LF-JailBail_TooPoor";
		}

		return "#LF-JailBail_NoAccount";
	}
}

//------------------------------------------------------------------------------------------------
//! "Pay bail" on the jail's bail phone. The server quotes and decides; the toast names the bail.
class LF_JailBailAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_Jail.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_JailBreakConfig.Get().m_bBailEnabled)
		{
			SetCannotPerformReason("#LF-JailBail_Off");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || LF_Systems.Refuses(LF_Jail.SYSTEM, "LF_JailBailAction"))
			return;

		string playerUid = LF_Utils.GetPlayerUid(pUserEntity);
		int bail;
		LF_EJailBailResult result = LF_JailBail.Pay(playerUid, bail);
		LF_ENotificationType kind = LF_ENotificationType.WARNING;
		if (result == LF_EJailBailResult.PAID)
			kind = LF_ENotificationType.SUCCESS;

		int playerId = 0;
		if (GetGame() && GetGame().GetPlayerManager())
			playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);

		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Police_Title", LF_JailBail.MessageFor(result), 4.0, kind, LF_MoneyFormat.FormatMoney(bail));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-JailBail_Action";
		return true;
	}
}
