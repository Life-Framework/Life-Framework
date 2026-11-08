//------------------------------------------------------------------------------------------------
//! Unemployment benefit, collected in person at the job counter. The safety net that replaced
//! the passive paycheck (decision 2026-11-08).
//!
//! Rule: an account may collect when its bank balance is strictly below the configured
//! threshold, it is not on duty as police, it has no active delivery run, and its last
//! collection is at least the configured cooldown ago. Everything is read on the server.
//! The cooldown ledger is session state: a restart lets a broke player collect once again.
class LF_UnemploymentBenefit
{
	static const string REFUSED_BALANCE = "#EL-Benefit_Refused_Balance";
	static const string REFUSED_WORKING = "#EL-Benefit_Refused_Working";
	static const string REFUSED_COOLDOWN = "#EL-Benefit_Refused_Cooldown";
	static const string REFUSED_DISABLED = "#EL-Benefit_Refused_Disabled";

	protected static ref map<string, float> s_mLastCollectedAt;

	//------------------------------------------------------------------------------------------------
	//! Pure check. \return "" when the account may collect, else the refusal message key.
	static string Refusal(string uid, int bankBalance, bool working, float now, int threshold, int amount, float cooldownSeconds)
	{
		if (uid.IsEmpty() || amount <= 0)
			return REFUSED_DISABLED;

		if (working)
			return REFUSED_WORKING;

		if (bankBalance >= threshold)
			return REFUSED_BALANCE;

		if (s_mLastCollectedAt && s_mLastCollectedAt.Contains(uid))
		{
			if (now - s_mLastCollectedAt.Get(uid) < cooldownSeconds)
				return REFUSED_COOLDOWN;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Records a collection and returns the amount owed, or 0 with outReason set when refused.
	static int Claim(string uid, int bankBalance, bool working, float now, int threshold, int amount, float cooldownSeconds, out string outReason)
	{
		outReason = Refusal(uid, bankBalance, working, now, threshold, amount, cooldownSeconds);
		if (!outReason.IsEmpty())
		{
			EL_Debug.Info("Benefit", string.Format("benefit refused for %1: %2 (balance %3)", uid, outReason, bankBalance));
			return 0;
		}

		if (!s_mLastCollectedAt)
			s_mLastCollectedAt = new map<string, float>();

		s_mLastCollectedAt.Set(uid, now);
		EL_Debug.Info("Benefit", string.Format("benefit granted to %1: %2 (balance %3)", uid, amount, bankBalance));
		return amount;
	}

	//------------------------------------------------------------------------------------------------
	//! Undoes a recorded collection the player never received (a full inventory).
	static void ForgetCollection(string uid)
	{
		if (s_mLastCollectedAt)
			s_mLastCollectedAt.Remove(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! Working means on duty as police or holding an active delivery run.
	static bool IsWorking(string uid)
	{
		if (LF_CivilianJob.HasActiveRun(uid))
			return true;

		EL_PlayerAccountManager manager = EL_PlayerAccountManager.GetInstance();
		if (!manager)
			return false;

		EL_PlayerAccount account = manager.GetAccount(uid);
		if (!account)
			return false;

		return account.GetFaction() == EL_Faction.POLICE && account.IsOnDuty();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the counter action. Hands the benefit over as cash with the pick-up sound, then
	//! a toast. A refusal toasts its reason.
	//! \return Cash placed on the player.
	static int CollectAtCounter(IEntity user)
	{
		if (!Replication.IsServer() && Replication.IsRunning())
			return 0;

		if (!user)
		{
			EL_Debug.Error("Benefit", "benefit refused: no player entity");
			return 0;
		}

		string uid = EL_Utils.GetPlayerUID(user);
		if (uid.IsEmpty())
		{
			EL_Debug.Error("Benefit", "benefit refused: player has no account id");
			return 0;
		}

		LF_JobsConfig config = LF_JobsConfig.Get();
		if (!config)
		{
			EL_Utils.NotifyEntity(user, REFUSED_DISABLED, "#EL-Benefit_Title", 4.0);
			return 0;
		}

		EL_BankAccount bank = EL_ATMManager.GetOrCreate(uid);
		int balance = 0;
		if (bank)
			balance = bank.GetBalance();

		string reason;
		int owed = Claim(uid, balance, IsWorking(uid), LF_JobGuards.Now(), config.m_iBenefitBalanceThreshold, config.m_iBenefitAmount, config.m_fBenefitCooldownSeconds, reason);
		if (owed <= 0)
		{
			EL_Utils.NotifyEntity(user, reason, "#EL-Benefit_Title", 4.0);
			return 0;
		}

		int given = EL_MoneyUtils.GiveCash(user, owed);
		if (given <= 0)
		{
			ForgetCollection(uid);
			EL_Debug.Warn("Benefit", string.Format("benefit not handed over: inventory full (player %1)", uid));
			EL_Utils.NotifyEntity(user, "#EL-Inventory_Full", "#EL-Benefit_Title", 4.0);
			return 0;
		}

		if (given != owed)
			EL_Debug.Warn("Benefit", string.Format("benefit short: paid %1 of %2 (player %3)", given, owed, uid));

		SCR_InventoryStorageManagerComponent inventory = EL_Component<SCR_InventoryStorageManagerComponent>.Find(user);
		if (inventory)
			inventory.RpcAsk_PlaySound(EL_NetworkUtils.GetRplId(user), "SOUND_PICK_UP");

		EL_Utils.NotifyEntity(user, "#EL-Benefit_Paid", "#EL-Benefit_Title", 4.0, given.ToString());
		return given;
	}
}

//------------------------------------------------------------------------------------------------
//! "Collect benefit" at the job counter. The server reads the balance and decides.
class LF_CollectBenefitAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_UnemploymentBenefit.CollectAtCounter(pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Benefit_Collect";
		return true;
	}
}
