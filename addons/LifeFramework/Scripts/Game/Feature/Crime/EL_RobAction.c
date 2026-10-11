class EL_RobAction : ScriptedUserAction
{
	static const string TILL_ENTITY_NAME = "Roadmap_Fuel_Till";
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

	protected float m_fLastRobTime;

	protected static string s_sLastRefusal;
	protected static int s_iCashPaid;
	protected static int s_iAlertCount;

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
	//! The cash rob path. Pays MoneyStack through EL_MoneyUtils, never the bank.
	//! A null user is the logic-tier seam: the returned amount is what AddCash would
	//! receive, and nothing is deposited to an account. Gates that refuse log
	//! EL_Debug.Error and leave cash, wanted, and the cooldown where they were.
	//! An accepted robbery starts the cooldown and alerts police once.
	static int CommitRobbery(string playerUid, IEntity user, int payout, int wantedIncrease, int policeOnDuty, int minPolice, float cooldownSeconds, float lastRobTimeMs, float nowMs, out float outLastRobTimeMs)
	{
		outLastRobTimeMs = lastRobTimeMs;

		if (IsCooldownActive(lastRobTimeMs, cooldownSeconds, nowMs))
		{
			NoteRefusal("robbery refused: cooldown");
			return 0;
		}

		EL_PlayerAccount account = FindAccount(playerUid);
		if (account && account.GetFaction() != EL_Faction.CIVILIAN)
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
			EL_Debug.Error("Crime", "robbery wanted increase was not configured; using 1");
			bump = 1;
		}

		if (account)
		{
			account.IncreaseWantedLevel(bump);
			EL_Debug.Log("Crime", string.Format("wanted level increased for %1", playerUid));
			EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
			if (accountManager)
				accountManager.SaveAndReleaseAccount(account);

			s_iAlertCount = s_iAlertCount + 1;
			vector origin = "0 0 0";
			if (user)
				origin = user.GetOrigin();

			EL_Utils.AlertPolice("#EL-Police_Alert_Robbery", origin);
		}
		else if (!playerUid.IsEmpty())
		{
			EL_Debug.Error("Crime", string.Format("robbery wanted skipped: no account for %1", playerUid));
		}

		if (paid > 0)
		{
			s_iCashPaid = s_iCashPaid + paid;
			EL_Debug.Log("Crime", string.Format("robbery +%1 cash to player %2", paid, playerUid));
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

		if (owner.GetName() != TILL_ENTITY_NAME)
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

		// Only civilians can rob. Offer checks do not log and do not alert.
		EL_PlayerAccount account = GetPlayerAccount(user);
		if (account && account.GetFaction() != EL_Faction.CIVILIAN)
		{
			SetCannotPerformReason("#EL-Rob_CiviliansOnly");
			return false;
		}

		int policeOnDuty = GetPoliceOnDutyCount();
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
		float nowMs = 0;
		if (GetGame().GetWorld())
			nowMs = GetGame().GetWorld().GetWorldTime();

		int payout = ResolvePayout(m_iMoneyAmount, m_iMinPayout, m_iMaxPayout, -1);
		string playerUid = EL_Utils.GetPlayerUid(pUserEntity);
		int policeOnDuty = GetPoliceOnDutyCount();
		float nextLast = m_fLastRobTime;
		int paid = CommitRobbery(playerUid, pUserEntity, payout, m_iWantedIncrease, policeOnDuty, m_iMinPoliceOnDuty, m_fCooldown, m_fLastRobTime, nowMs, nextLast);
		m_fLastRobTime = nextLast;

		if (paid > 0)
			EL_Utils.Notify(WidgetManager.Translate("#EL-Stole_Money", paid), "#EL-Robbery_Successful", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	protected static void NoteRefusal(string reason)
	{
		s_sLastRefusal = reason;
		EL_Debug.Error("Crime", reason);
	}

	//------------------------------------------------------------------------------------------------
	protected static EL_PlayerAccount FindAccount(string playerUid)
	{
		if (playerUid.IsEmpty())
			return null;

		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (!accountManager)
			return null;

		return accountManager.GetAccount(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	protected static int PayCash(IEntity user, int payout)
	{
		if (!user)
			return payout;

		int paid = EL_MoneyUtils.AddCash(user, payout);
		if (paid != payout)
		{
			if (paid > 0)
				EL_MoneyUtils.RemoveCash(user, paid);

			EL_Debug.Error("Crime", string.Format("robbery payout short: paid %1 of %2", paid, payout));
			return 0;
		}

		return paid;
	}

	//------------------------------------------------------------------------------------------------
	protected EL_PlayerAccount GetPlayerAccount(IEntity user)
	{
		return FindAccount(EL_Utils.GetPlayerUid(user));
	}

	//------------------------------------------------------------------------------------------------
	protected int GetPoliceOnDutyCount()
	{
		int count = 0;
		EL_PlayerControllerManagerCompat playerManager = EL_PlayerControllerManagerCompat.GetInstance();
		if (playerManager)
		{
			array<int> playerIds = {};
			playerManager.GetAllPlayerIds(playerIds);

			EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
			if (accountManager)
			{
				foreach (int playerId : playerIds)
				{
					PlayerController pc = playerManager.GetPlayerController(playerId);
					if (pc)
					{
						string playerUid = EL_Utils.GetPlayerUID(pc.GetControlledEntity());
						EL_PlayerAccount account = accountManager.GetAccount(playerUid);
						if (account && account.GetFaction() == EL_Faction.POLICE && account.IsOnDuty())
							count++;
					}
				}
			}
		}
		return count;
	}
};
