class LF_RobAction : ScriptedUserAction
{
	static const string TILL_ACTION_NAME = "#EL-Fuel_Till_Rob";
	static const string REFUSED_COOLDOWN = "robbery refused: cooldown";
	static const string REFUSED_NO_ACCOUNT = "robbery refused: no account";
	static const string REFUSED_NOT_CIVILIAN = "robbery refused: only civilians can rob";
	static const string REFUSED_POLICE = "robbery refused: not enough police on duty";
	static const string REFUSED_PAYOUT = "robbery refused: payout is not configured";

	[Attribute(defvalue: "100", uiwidget: UIWidgets.Auto, desc: "Amount of money to steal when no range is set")]
	protected int m_iMoneyAmount;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.Auto, desc: "Minimum cash payout. 0 with max 0 uses m_iMoneyAmount")]
	protected int m_iMinPayout;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.Auto, desc: "Maximum cash payout. Equal to min pays that fixed amount")]
	protected int m_iMaxPayout;

	[Attribute(defvalue: "10", uiwidget: UIWidgets.Auto, desc: "Cooldown in seconds")]
	protected float m_fCooldown;

	[Attribute(defvalue: "2", uiwidget: UIWidgets.Auto, desc: "Minimum police on duty required")]
	protected int m_iMinPoliceOnDuty;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Show the fuel till action name (TILL_ACTION_NAME) from script. Set on the fuel till and its annex twin.")]
	protected bool m_bTillActionName;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Robbed only through the attendant's dialogue (LF_RobHold): the world action is hidden and refused. The fuel station till (#936).")]
	protected bool m_bDialogueOnly;

	[Attribute(defvalue: "30", uiwidget: UIWidgets.Auto, desc: "Dialogue robbery: seconds the robber must keep the attendant at gunpoint before the till opens")]
	protected float m_fHoldSeconds;

	[Attribute(defvalue: "6", uiwidget: UIWidgets.Auto, desc: "Dialogue robbery: metres from the till the robber must stay within")]
	protected float m_fHoldRadius;

	[Attribute(defvalue: "30", uiwidget: UIWidgets.Auto, desc: "Dialogue robbery: an on-duty officer this close to the till ends it")]
	protected float m_fPoliceRadius;

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
	static int CommitRobbery(string playerUid, IEntity user, int payout, int policeOnDuty, int minPolice, float cooldownSeconds, float lastRobTimeMs, float nowMs, out float outLastRobTimeMs)
	{
		outLastRobTimeMs = lastRobTimeMs;

		string refusal = Gate(playerUid, payout, policeOnDuty, minPolice, cooldownSeconds, lastRobTimeMs, nowMs);
		if (!refusal.IsEmpty())
		{
			NoteRefusal(refusal);
			return 0;
		}

		int paid = PayCash(user, payout);
		outLastRobTimeMs = nowMs;
		s_sLastRefusal = "";
		Report(playerUid, user, policeOnDuty);
		NotePaid(playerUid, paid);
		return paid;
	}

	//------------------------------------------------------------------------------------------------
	//! Why a robbery may not happen now (one of the REFUSED_* reasons), or empty when it may.
	//! Reads only; the caller logs. No account means no civilian gate and no wanted bump, so it
	//! must not pay either.
	static string Gate(string playerUid, int payout, int policeOnDuty, int minPolice, float cooldownSeconds, float lastRobTimeMs, float nowMs)
	{
		if (IsCooldownActive(lastRobTimeMs, cooldownSeconds, nowMs))
			return REFUSED_COOLDOWN;

		LF_PlayerAccount account = FindAccount(playerUid);
		if (!account)
			return REFUSED_NO_ACCOUNT;

		if (account.GetFaction() != LF_Faction.CIVILIAN)
			return REFUSED_NOT_CIVILIAN;

		if (policeOnDuty < minPolice)
			return REFUSED_POLICE;

		if (payout <= 0)
			return REFUSED_PAYOUT;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The crime is on record: wanted level, then the alarm at the till and the radio call to
	//! on-duty officers (LF_Dispatch, which also opens the witness and evidence scene).
	protected static void Report(string playerUid, IEntity user, int policeOnDuty)
	{
		LF_PlayerAccount account = FindAccount(playerUid);
		if (account)
		{
			LF_CrimeCatalog.ApplyWanted(account, playerUid, LF_WantedReports.OFFENCE_ROBBERY);
			LF_Debug.Log("Crime", string.Format("wanted level increased for %1", playerUid));
			LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
			if (accountManager)
				accountManager.SaveAndReleaseAccount(account);
		}

		s_iAlertCount = s_iAlertCount + 1;
		vector origin = "0 0 0";
		if (user)
			origin = user.GetOrigin();

		LF_Dispatch.Report(playerUid, user, LF_WantedReports.OFFENCE_ROBBERY, origin, policeOnDuty);
	}

	//------------------------------------------------------------------------------------------------
	protected static void NotePaid(string playerUid, int paid)
	{
		if (paid <= 0)
			return;

		s_iCashPaid = s_iCashPaid + paid;
		LF_Debug.Log("Crime", string.Format("robbery +%1 cash to player %2", paid, playerUid));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: why a held robbery of this till by `user` cannot start now, or empty. Logs nothing:
	//! the dialogue asks it to decide which reply to offer.
	string HeldRefusal(IEntity user)
	{
		int payout = ResolvePayout(m_iMoneyAmount, m_iMinPayout, m_iMaxPayout, 0);
		return Gate(LF_Utils.GetPlayerUid(user), payout, LF_AccountReplication.CountPoliceOnDuty(), m_iMinPoliceOnDuty, m_fCooldown, m_fLastRobTime, NowMs());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a held robbery of this till begins (LF_RobHold). The gates of the instant robbery,
	//! then the cooldown starts and the crime is reported at once: the attendant trips the alarm
	//! as the gun comes out, so officers can arrive before the till opens. The cash waits for
	//! PayHeld. \return The refusal (logged), or empty when it began.
	string BeginHeld(IEntity user)
	{
		string refusal = HeldRefusal(user);
		if (!refusal.IsEmpty())
		{
			NoteRefusal(refusal);
			return refusal;
		}

		m_fLastRobTime = NowMs();
		s_sLastRefusal = "";
		Report(LF_Utils.GetPlayerUid(user), user, LF_AccountReplication.CountPoliceOnDuty());
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the hold was kept; the till opens and pays the robber as the instant robbery does.
	//! \return Cash paid (0 when the inventory could not take it; logged).
	int PayHeld(IEntity user)
	{
		int paid = PayCash(user, ResolvePayout(m_iMoneyAmount, m_iMinPayout, m_iMaxPayout, -1));
		NotePaid(LF_Utils.GetPlayerUid(user), paid);
		return paid;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDialogueOnly()
	{
		return m_bDialogueOnly;
	}

	//------------------------------------------------------------------------------------------------
	float GetHoldSeconds()
	{
		return m_fHoldSeconds;
	}

	//------------------------------------------------------------------------------------------------
	float GetHoldRadius()
	{
		return m_fHoldRadius;
	}

	//------------------------------------------------------------------------------------------------
	float GetPoliceRadius()
	{
		return m_fPoliceRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! The dialogue-only till nearest to `origin` within `maxDistance` metres, or null. This is how an
	//! attendant finds the till behind its counter; there is no link to keep in step.
	static LF_RobAction FindDialogueTill(vector origin, float maxDistance)
	{
		if (!s_aTills)
			return null;

		LF_RobAction best;
		float bestDistance = maxDistance;
		foreach (LF_RobAction till : s_aTills)
		{
			if (!till || !till.m_bDialogueOnly || !till.GetOwner())
				continue;

			float distance = vector.Distance(origin, till.GetOwner().GetOrigin());
			if (distance > bestDistance)
				continue;

			best = till;
			bestDistance = distance;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected static float NowMs()
	{
		if (GetGame() && GetGame().GetWorld())
			return GetGame().GetWorld().GetWorldTime();

		return 0;
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
	override bool CanBeShownScript(IEntity user)
	{
		// A dialogue-only till is robbed by talking to its attendant with a weapon drawn.
		return !m_bDialogueOnly;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (m_bDialogueOnly)
			return false;

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

		if (m_bDialogueOnly)
		{
			LF_Debug.Warn("Crime", "the world action of a dialogue-only till was performed and refused; rob it through the attendant");
			return;
		}

		float nowMs = 0;
		if (GetGame().GetWorld())
			nowMs = GetGame().GetWorld().GetWorldTime();

		int payout = ResolvePayout(m_iMoneyAmount, m_iMinPayout, m_iMaxPayout, -1);
		string playerUid = LF_Utils.GetPlayerUid(pUserEntity);
		int policeOnDuty = LF_AccountReplication.CountPoliceOnDuty();
		float nextLast = m_fLastRobTime;
		int paid = CommitRobbery(playerUid, pUserEntity, payout, policeOnDuty, m_iMinPoliceOnDuty, m_fCooldown, m_fLastRobTime, nowMs, nextLast);
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
