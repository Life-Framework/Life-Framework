//------------------------------------------------------------------------------------------------
//! One contract row (#2702, docs/architecture/reputation.md "Spending standing: contracts"): an
//! ordinary LF_CivilianJob run of `m_sJob` with a standing gate, a pay multiplier, the points it
//! awards when finished and the stake it takes when abandoned or timed out. Standing is a stake,
//! never a currency: a stake moves points only, never money.
[BaseContainerProps()]
class LF_ReputationContractDef : LF_ConfigRecord
{
	static const float MIN_MULTIPLIER = 1.3;	// lf-fixed: reputation.md section 8, contracts pay 1.3 to 1.6 times the base run
	static const float MAX_MULTIPLIER = 1.6;	// lf-fixed: keeps a top contract hour under police duty pay

	[Attribute("", UIWidgets.EditBox, "Contract id. The record key")]
	string m_sId;

	[Attribute("PARCEL", UIWidgets.EditBox, "LF_EJobType name of the run the contract wraps: PARCEL, FUEL_DELIVERY or TAXI")]
	string m_sJob;

	[Attribute("", UIWidgets.EditBox, "Standing gate id (a row in LF_Reputation.conf m_aGates). Empty or no row is open")]
	string m_sStandingGate;

	[Attribute("TOWN", UIWidgets.EditBox, "Faction the award goes to and the stake comes from")]
	string m_sFaction;

	[Attribute("1.3", UIWidgets.Auto, "Pay multiplier on the run's base pay; clamped to 1.3..1.6", params: "1 3")]
	float m_fPayMultiplier;

	[Attribute("0", UIWidgets.Auto, "Standing points awarded when the contract is finished", params: "0 1000")]
	int m_iAwardPoints;

	[Attribute("0", UIWidgets.Auto, "Standing points taken when the contract is abandoned or times out", params: "0 1000")]
	int m_iStakePoints;

	[Attribute("0", UIWidgets.Auto, "Seconds to finish before the contract times out; 0 for no limit", params: "0 86400")]
	float m_fTimeLimitSeconds;

	//------------------------------------------------------------------------------------------------
	static LF_ReputationContractDef Make(string id, string job, string gate, string faction, float multiplier, int award, int stake, float timeLimit = 0)
	{
		LF_ReputationContractDef def = new LF_ReputationContractDef();
		def.m_sId = id;
		def.m_sJob = job;
		def.m_sStandingGate = gate;
		def.m_sFaction = faction;
		def.m_fPayMultiplier = multiplier;
		def.m_iAwardPoints = award;
		def.m_iStakePoints = stake;
		def.m_fTimeLimitSeconds = timeLimit;
		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The multiplier clamped to 1.3..1.6.
	float Multiplier()
	{
		return Math.Clamp(m_fPayMultiplier, MIN_MULTIPLIER, MAX_MULTIPLIER); // lf-float: a ratio near 1, not a large int
	}

	//------------------------------------------------------------------------------------------------
	//! \return The job this contract runs, or UNEMPLOYED when the name is not a delivery job.
	LF_EJobType Job()
	{
		int job = typename.StringToEnum(LF_EJobType, m_sJob);
		if (job < 0 || !LF_CivilianJob.IsDeliveryJob(job))
			return LF_EJobType.UNEMPLOYED;

		return job;
	}
}

//------------------------------------------------------------------------------------------------
//! The contract table: Configs/Reputation/LF_ReputationContracts.conf. Core ships one neutral test
//! row; the pack's contracts (the harbour master's, Vince's) come with the pack overlay card.
[BaseContainerProps(configRoot: true)]
class LF_ReputationContractsConfig
{
	static const ResourceName CONFIG_PATH = "{4AC9E6E85B924B9F}Configs/Reputation/LF_ReputationContracts.conf";

	[Attribute("", UIWidgets.Object, "Contracts: a run with a standing gate, a pay multiplier, an award and a stake")]
	ref array<ref LF_ReputationContractDef> m_aContracts;

	[Attribute("1800", UIWidgets.Auto, "Seconds after a contract ends (finished or failed) before the same character may take another", params: "0 86400")]
	float m_fCooldownSeconds;

	protected static ref LF_ReputationContractsConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	static LF_ReputationContractsConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_Reputation.FEATURE, string.Format("contract config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_ReputationContractsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(LF_Reputation.FEATURE, string.Format("contract config has an unexpected root class: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_ReputationContractsConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	LF_ReputationContractDef Find(string id)
	{
		if (!m_aContracts || id.IsEmpty())
			return null;

		foreach (LF_ReputationContractDef def : m_aContracts)
		{
			if (def && def.m_sId == id)
				return def;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! A character's contract in progress (session state, like the run it wraps).
class LF_ReputationContractRun : Managed
{
	string m_sId;
	float m_fTakenAt;
}

//------------------------------------------------------------------------------------------------
//! Contracts on the server: one at a time per character, a cooldown after each, the pay multiplier
//! and award on finish, the stake on abandon or time-out. LF_CivilianJob calls Settle from
//! CompleteRun and Forfeit from AbandonRun; Begin is the entry point an offer calls. Session state:
//! a restart refunds the run's deposit (LF_CivilianJob.ApplyDeposits) and drops the contract with
//! no stake taken.
class LF_ReputationContracts
{
	static const string KEY_UNKNOWN = "#LF-Contract_Unknown";
	static const string KEY_BUSY = "#LF-Contract_Busy";
	static const string KEY_COOLDOWN = "#LF-Contract_Cooldown";
	static const string KEY_NOT_ABLE = "#LF-Contract_NotAble";
	static const string KEY_NO_RUN = "#LF-Contract_NoRun";
	static const string REASON_BONUS = "standing-bonus";

	protected static ref map<string, ref LF_ReputationContractRun> s_mActive;
	protected static ref map<string, float> s_mCooldownUntil;
	protected static ref map<string, int> s_mLastBonus;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mActive = null;
		s_mCooldownUntil = null;
		s_mLastBonus = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_ReputationContractRun> Active()
	{
		if (!s_mActive)
			s_mActive = new map<string, ref LF_ReputationContractRun>();

		return s_mActive;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, float> Cooldowns()
	{
		if (!s_mCooldownUntil)
			s_mCooldownUntil = new map<string, float>();

		return s_mCooldownUntil;
	}

	//------------------------------------------------------------------------------------------------
	static LF_ReputationContractDef FindDef(string contractId)
	{
		LF_ReputationContractsConfig config = LF_ReputationContractsConfig.Get();
		if (!config)
			return null;

		return config.Find(contractId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The id of the character's contract in progress, or "".
	static string ActiveId(string playerUid)
	{
		DropStale(playerUid);
		LF_ReputationContractRun run = Active().Get(playerUid);
		if (!run)
			return string.Empty;

		return run.m_sId;
	}

	//------------------------------------------------------------------------------------------------
	//! A contract whose run ended outside the run paths (a wipe, a failed start) is dropped with no stake.
	protected static void DropStale(string playerUid)
	{
		if (!s_mActive || !s_mActive.Contains(playerUid))
			return;

		if (LF_CivilianJob.HasActiveRun(playerUid))
			return;

		LF_Debug.Warn(LF_Reputation.FEATURE, string.Format("contract %1 of %2 dropped: its run is gone (no stake taken)", s_mActive.Get(playerUid).m_sId, playerUid));
		s_mActive.Remove(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! \return "" when the character may take the contract now, else the refusal `#` key (a standing
	//! gate refusal is the gate's own faction key, `tierWord` its argument).
	static string Refusal(string playerUid, string contractId, float now, out string tierWord)
	{
		tierWord = string.Empty;
		LF_ReputationContractDef def = FindDef(contractId);
		if (!def || def.Job() == LF_EJobType.UNEMPLOYED)
		{
			LF_Debug.Warn(LF_Reputation.FEATURE, string.Format("contract refused for %1: '%2' is not a contract with a delivery job", playerUid, contractId));
			return KEY_UNKNOWN;
		}

		if (!ActiveId(playerUid).IsEmpty())
			return KEY_BUSY;

		float until;
		if (Cooldowns().Find(playerUid, until) && now < until)
			return KEY_COOLDOWN;

		return LF_Reputation.Refusal(playerUid, def.m_sStandingGate, tierWord);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: binds the contract to the character's active run of the contract's job. Refused (false,
	//! logged) when Refusal refuses or there is no matching run.
	static bool Take(string playerUid, string contractId, float now)
	{
		if (Replication.IsRunning() && !Replication.IsServer())
			return false;

		if (playerUid.IsEmpty())
		{
			LF_Debug.Error(LF_Reputation.FEATURE, "contract refused: empty player id");
			return false;
		}

		string tierWord;
		string refusal = Refusal(playerUid, contractId, now, tierWord);
		if (!refusal.IsEmpty())
		{
			LF_Debug.Info(LF_Reputation.FEATURE, string.Format("contract %1 refused for %2: %3", contractId, playerUid, refusal));
			return false;
		}

		LF_ReputationContractDef def = FindDef(contractId);
		if (!LF_CivilianJob.HasActiveRun(playerUid) || LF_CivilianJob.GetActiveJob(playerUid) != def.Job())
		{
			LF_Debug.Info(LF_Reputation.FEATURE, string.Format("contract %1 refused for %2: no active %3 run", contractId, playerUid, def.m_sJob));
			return false;
		}

		LF_ReputationContractRun run = new LF_ReputationContractRun();
		run.m_sId = contractId;
		run.m_fTakenAt = now;
		Active().Set(playerUid, run);
		LF_Debug.Info(LF_Reputation.FEATURE, string.Format("contract %1 taken by %2 (x%3, stake %4)", contractId, playerUid, def.Multiplier(), def.m_iStakePoints));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a living, unrestricted player starts the contract's run (the deposit and cargo of
	//! LF_CivilianJob.BeginRun) and takes the contract. Rate limited as a purchase; a dead or downed
	//! body is refused before any money moves.
	//! \param[out] refusal The `#` key to show when refused before the run, else "".
	static LF_ERunStart Begin(IEntity user, string playerUid, string contractId, float now, out int deposit, out string refusal, out string tierWord)
	{
		deposit = 0;
		refusal = string.Empty;
		tierWord = string.Empty;
		if (!user || playerUid.IsEmpty())
		{
			LF_Debug.Error(LF_Reputation.FEATURE, "contract start refused: no player or no account id");
			return LF_ERunStart.REFUSED;
		}

		if (!LF_Utils.IsAliveState(user) || LF_Downed.IsDowned(user))
		{
			LF_Debug.Warn(LF_Reputation.FEATURE, string.Format("contract start refused for %1: dead or downed", playerUid));
			refusal = KEY_NOT_ABLE;
			return LF_ERunStart.REFUSED;
		}

		if (!LF_RateLimit.AllowCharacter(user, LF_ERequestKind.PURCHASE, "contract start"))
			return LF_ERunStart.REFUSED;

		refusal = Refusal(playerUid, contractId, now, tierWord);
		if (!refusal.IsEmpty())
			return LF_ERunStart.REFUSED;

		LF_ReputationContractDef def = FindDef(contractId);
		LF_ERunStart started = LF_CivilianJob.BeginRun(user, playerUid, def.Job(), now, deposit);
		if (started != LF_ERunStart.STARTED)
			return started;

		if (!Take(playerUid, contractId, now))
		{
			// Unreachable after Refusal passed and the run started; the run stays a plain run.
			LF_Debug.Error(LF_Reputation.FEATURE, string.Format("contract %1 not bound to %2's started run; it runs as a plain run", contractId, playerUid));
			refusal = KEY_NO_RUN;
		}

		return started;
	}

	//------------------------------------------------------------------------------------------------
	protected static void End(string playerUid, float now)
	{
		Active().Remove(playerUid);
		float cooldown = 0;
		LF_ReputationContractsConfig config = LF_ReputationContractsConfig.Get();
		if (config)
			cooldown = config.m_fCooldownSeconds;

		Cooldowns().Set(playerUid, now + Math.Max(0, cooldown)); // lf-float: seconds of world time
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_CivilianJob.CompleteRun: the pay of a finished run. With no contract it is
	//! `basePay`. A contract in time pays basePay times its multiplier (once: the contract ends here)
	//! and awards its points. A contract past its time limit takes the stake and pays only basePay.
	static int Settle(string playerUid, int basePay, float now)
	{
		if (s_mLastBonus)
			s_mLastBonus.Remove(playerUid);

		if (!s_mActive || !s_mActive.Contains(playerUid))
			return basePay;

		LF_ReputationContractRun run = s_mActive.Get(playerUid);
		LF_ReputationContractDef def = FindDef(run.m_sId);
		if (!def)
		{
			LF_Debug.Warn(LF_Reputation.FEATURE, string.Format("contract %1 of %2 is no longer in the config; the run pays its base", run.m_sId, playerUid));
			s_mActive.Remove(playerUid);
			return basePay;
		}

		if (def.m_fTimeLimitSeconds > 0 && now - run.m_fTakenAt > def.m_fTimeLimitSeconds)
		{
			Forfeit(playerUid, "timed out", now);
			return basePay;
		}

		int pay = Math.Round(basePay * def.Multiplier());
		int bonus = LF_Utils.MaxInt(0, pay - basePay);
		if (!s_mLastBonus)
			s_mLastBonus = new map<string, int>();

		s_mLastBonus.Set(playerUid, bonus);
		End(playerUid, now);
		if (def.m_iAwardPoints > 0)
			LF_Reputation.Add(playerUid, def.m_sFaction, def.m_iAwardPoints, "contract:" + def.m_sId);

		LF_Debug.Info(LF_Reputation.FEATURE, string.Format("contract %1 finished by %2: pay %3 (base %4, bonus %5), award %6 %7", def.m_sId, playerUid, pay, basePay, bonus, def.m_iAwardPoints, def.m_sFaction));
		return pay;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The bonus part of the character's last Settle (for the ledger reason), and clears it.
	static int TakeLastBonus(string playerUid)
	{
		int bonus;
		if (!s_mLastBonus || !s_mLastBonus.Find(playerUid, bonus))
			return 0;

		s_mLastBonus.Remove(playerUid);
		return bonus;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_CivilianJob.AbandonRun (and a time-out): takes the stake, logs why, ends the
	//! contract and starts the cooldown. No money moves. Nothing happens with no contract.
	//! \return The points actually taken (0 or negative).
	static int Forfeit(string playerUid, string reason, float now)
	{
		if (!s_mActive || !s_mActive.Contains(playerUid))
			return 0;

		string contractId = s_mActive.Get(playerUid).m_sId;
		LF_ReputationContractDef def = FindDef(contractId);
		End(playerUid, now);
		if (!def)
		{
			LF_Debug.Warn(LF_Reputation.FEATURE, string.Format("contract %1 of %2 %3 but is no longer in the config; no stake taken", contractId, playerUid, reason));
			return 0;
		}

		int moved = 0;
		if (def.m_iStakePoints > 0)
			moved = LF_Reputation.Add(playerUid, def.m_sFaction, -def.m_iStakePoints, "contract:" + contractId + " " + reason);

		LF_Debug.Info(LF_Reputation.FEATURE, string.Format("contract %1 %2 by %3: stake %4 %5 taken (%6 moved)", contractId, reason, playerUid, def.m_iStakePoints, def.m_sFaction, moved));
		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the character's contract and cooldown with no stake (tests, admin wipe).
	static void Forget(string playerUid)
	{
		if (s_mActive)
			s_mActive.Remove(playerUid);

		if (s_mCooldownUntil)
			s_mCooldownUntil.Remove(playerUid);

		if (s_mLastBonus)
			s_mLastBonus.Remove(playerUid);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ReputationContracts' session state.
class LF_ReputationContracts_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ReputationContracts";
	}

	override void Apply()
	{
		LF_ReputationContracts.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ReputationContractsConfig's cache.
class LF_ReputationContractsConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ReputationContractsConfig";
	}

	override void Apply()
	{
		LF_ReputationContractsConfig.Reload();
	}
}
