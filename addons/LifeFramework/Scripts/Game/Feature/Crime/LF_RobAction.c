class LF_RobAction : ScriptedUserAction
{
	static const string TILL_ACTION_NAME = "#EL-Fuel_Till_Rob";

	[Attribute(defvalue: "100", uiwidget: UIWidgets.Auto, desc: "Amount of money to steal when no range is set")]
	protected int m_iMoneyAmount;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.Auto, desc: "Minimum cash payout. 0 with max 0 uses m_iMoneyAmount")]
	protected int m_iMinPayout;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.Auto, desc: "Maximum cash payout. Equal to min pays that fixed amount")]
	protected int m_iMaxPayout;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.Auto, desc: "Wanted levels added when the robbery is accepted")]
	protected int m_iWantedIncrease;

	[Attribute(defvalue: "10", uiwidget: UIWidgets.Auto, desc: "Cooldown in seconds")]
	protected float m_fCooldown;

	[Attribute(defvalue: "2", uiwidget: UIWidgets.Auto, desc: "Minimum police on duty required")]
	protected int m_iMinPoliceOnDuty;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Show the fuel till action name (TILL_ACTION_NAME) from script. Set on the fuel till and its annex twin.")]
	protected bool m_bTillActionName;

	protected float m_fLastRobTime;

	protected static string s_sLastRefusal;
	protected static int s_iCashPaid;
	protected static int s_iAlertCount;
	//! Every till in the world, so the harness's per-proof reset can end their cooldowns (#867). Weak refs.
	protected static ref array<LF_RobAction> s_aTills;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		if (!s_aTills)
			s_aTills = {};

		if (s_aTills.Find(this) < 0)
			s_aTills.Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: ends every till's robbery cooldown (the harness's per-proof reset, #867).
	//! @return How many tills were reset.
	static int ResetCooldowns()
	{
		if (!s_aTills)
			return 0;

		int reset = 0;
		foreach (LF_RobAction till : s_aTills)
		{
			if (!till)
				continue;

			till.m_fLastRobTime = 0;
			reset++;
		}

		return reset;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetSession()
	{
		s_sLastRefusal = "";
		s_iCashPaid = 0;
		s_iAlertCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	static string GetLastRefusal()
	{
		return s_sLastRefusal;
	}

	//------------------------------------------------------------------------------------------------
	static int GetCashPaid()
	{
		return s_iCashPaid;
	}

	//------------------------------------------------------------------------------------------------
	static int GetAlertCount()
	{
		return s_iAlertCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Cash the server will hand to MoneyStack. A negative roll asks the server to pick
	//! inside the range. Min and max of 0 keep the single configured amount. Min equal to
	//! max is the fixed test payout. An inverted range pays nothing.
	static int ResolvePayout(int fixedAmount, int minAmount, int maxAmount, int roll)
	{
		int min = minAmount;
		int max = maxAmount;
		if (min <= 0 && max <= 0)
		{
			min = fixedAmount;
			max = fixedAmount;
		}

		if (min <= 0 || max < min)
			return 0;

		if (min == max)
			return min;

		int span = max - min + 1;
		int offset = roll;
		if (offset < 0)
			offset = Math.RandomInt(0, span);

		if (offset < 0)
			offset = 0;

		if (offset >= span)
			offset = offset % span;

		return min + offset;
	}

	//------------------------------------------------------------------------------------------------
	//! Cooldown is configured in seconds. World time is milliseconds. A last time of 0
	//! means this till has not been robbed yet.
	static bool IsCooldownActive(float lastRobTimeMs, float cooldownSeconds, float nowMs)
	{
		if (cooldownSeconds <= 0)
			return false;

		if (lastRobTimeMs <= 0)
			return false;

		float elapsedMs = nowMs - lastRobTimeMs;
		return elapsedMs < cooldownSeconds * 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! The cash rob path. Pays MoneyStack through LF_MoneyUtils, never the bank.
	//! A null user is the logic-tier seam: the returned amount is what AddCash would
	//! receive, and nothing is deposited to an account. Gates that refuse log
	//! LF_Debug.Error and leave cash, wanted, and the cooldown where they were.
	//! An accepted robbery starts the cooldown and alerts police once.
	static int CommitRobbery(string playerUid, IEntity user, int payout, int wantedIncrease, int policeOnDuty, int minPolice, float cooldownSeconds, float lastRobTimeMs, float nowMs, out float outLastRobTimeMs)
	{
		outLastRobTimeMs = lastRobTimeMs;

		if (IsCooldownActive(lastRobTimeMs, cooldownSeconds, nowMs))
		{
			NoteRefusal("robbery refused: cooldown");
			return 0;
		}

		// No account means no civilian gate and no wanted bump, so it must not pay either.
		LF_PlayerAccount account = FindAccount(playerUid);
		if (!account)
		{
			NoteRefusal("robbery refused: no account");
			return 0;
		}

		if (account.GetFaction() != LF_Faction.CIVILIAN)
		{
			NoteRefusal("robbery refused: only civilians can rob");
			return 0;
		}

		if (policeOnDuty < minPolice)
		{
			NoteRefusal("robbery refused: not enough police on duty");
			return 0;
		}

		if (payout <= 0)
		{
			NoteRefusal("robbery refused: payout is not configured");
			return 0;
		}

		int paid = PayCash(user, payout);
		outLastRobTimeMs = nowMs;
		s_sLastRefusal = "";

		int bump = wantedIncrease;
		if (bump < 1)
		{
			LF_Debug.Error("Crime", "robbery wanted increase was not configured; using 1");
			bump = 1;
		}

		account.IncreaseWantedLevel(bump);
		LF_Debug.Log("Crime", string.Format("wanted level increased for %1", playerUid));
		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (accountManager)
			accountManager.SaveAndReleaseAccount(account);

		s_iAlertCount = s_iAlertCount + 1;
		vector origin = "0 0 0";
		if (user)
			origin = user.GetOrigin();

		// The alarm sounds at the till and on-duty officers are radioed (LF_Dispatch).
		LF_Dispatch.Report(playerUid, user, LF_WantedReports.OFFENCE_ROBBERY, origin, policeOnDuty);

		if (paid > 0)
		{
			s_iCashPaid = s_iCashPaid + paid;
			LF_Debug.Log("Crime", string.Format("robbery +%1 cash to player %2", paid, playerUid));
		}

		return paid;
	}

	//------------------------------------------------------------------------------------------------
	int GetConfiguredPayout()
	{
		return ResolvePayout(m_iMoneyAmount, m_iMinPayout, m_iMaxPayout, 0);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		if (!m_bTillActionName)
			return false;

		outName = TILL_ACTION_NAME;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		float nowMs = 0;
		if (GetGame().GetWorld())
			nowMs = GetGame().GetWorld().GetWorldTime();

		if (IsCooldownActive(m_fLastRobTime, m_fCooldown, nowMs))
		{
			SetCannotPerformReason("#EL-Rob_Cooldown");
			return false;
		}

		// Only civilians can rob. Offer checks do not log and do not alert. On a client the
		// account and the police count are the replicated copies (LF_AccountReplication).
		if (!LF_AccountStateBits.IsCivilian(LF_AccountReplication.GetState(user)))
		{
			SetCannotPerformReason("#EL-Rob_CiviliansOnly");
			return false;
		}

		int policeOnDuty = LF_AccountReplication.GetPoliceOnDutyCount();
		if (policeOnDuty < m_iMinPoliceOnDuty)
		{
			SetCannotPerformReason("#EL-Rob_NeedPolice");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// The server pays, raises wanted, and alerts. A client copy of the action does nothing.
		if (!Replication.IsServer())
			return;

		float nowMs = 0;
		if (GetGame().GetWorld())
			nowMs = GetGame().GetWorld().GetWorldTime();

		int payout = ResolvePayout(m_iMoneyAmount, m_iMinPayout, m_iMaxPayout, -1);
		string playerUid = LF_Utils.GetPlayerUid(pUserEntity);
		int policeOnDuty = LF_AccountReplication.CountPoliceOnDuty();
		float nextLast = m_fLastRobTime;
		int paid = CommitRobbery(playerUid, pUserEntity, payout, m_iWantedIncrease, policeOnDuty, m_iMinPoliceOnDuty, m_fCooldown, m_fLastRobTime, nowMs, nextLast);
		m_fLastRobTime = nextLast;

		if (paid > 0)
			LF_Utils.NotifyEntity(pUserEntity, "#EL-Stole_Money", "#EL-Robbery_Successful", 3.0, paid.ToString());
	}

	//------------------------------------------------------------------------------------------------
	protected static void NoteRefusal(string reason)
	{
		s_sLastRefusal = reason;
		LF_Debug.Error("Crime", reason);
	}

	//------------------------------------------------------------------------------------------------
	//! The civilian gate the weapon and vehicle robberies offer through. A missing account is
	//! allowed, the same as their original inline check; a non-civilian account is refused.
	static bool IsFactionAllowedToRob(string playerUid)
	{
		LF_PlayerAccount account = FindAccount(playerUid);
		if (account && account.GetFaction() != LF_Faction.CIVILIAN)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static LF_PlayerAccount FindAccount(string playerUid)
	{
		if (playerUid.IsEmpty())
			return null;

		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (!accountManager)
			return null;

		return accountManager.GetAccount(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	protected static int PayCash(IEntity user, int payout)
	{
		if (!user)
			return payout;

		int paid = LF_MoneyUtils.AddCash(user, payout, "robbery");
		if (paid != payout)
		{
			if (paid > 0)
				LF_MoneyUtils.RemoveCash(user, paid, "robbery-rollback");

			LF_Debug.Error("Crime", string.Format("robbery payout short: paid %1 of %2", paid, payout));
			return 0;
		}

		return paid;
	}
};
