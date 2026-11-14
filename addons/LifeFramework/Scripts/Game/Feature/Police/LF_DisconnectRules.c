//------------------------------------------------------------------------------------------------
//! Disconnect rules (#691): what logging out does to a player who is cuffed, escorted, downed,
//! wanted or jailed, so leaving never escapes an arrest, a death or a sentence.
//!
//! - Cuffed (or escorted on foot): "jail" (default) starts the sentence at logout and the rejoin
//!   spawns in the cell (LF_Jail.OnPlayerSpawned); "fine" takes the fine from the bank; "release"
//!   only frees the cuffs. Jail and fine both add the leaving-custody wanted charge. Jail with no
//!   placed cell falls back to the fine.
//! - Downed (not stunned): dies at logout when downedDies is on (LF_BleedOut reads it).
//! - Wanted: the warrant stays on the character (it is saved with the account); wantedLogoutAdd
//!   raises it.
//! - Jailed: the remaining sentence stays (the jail payload); jailClockOffline lets it count down
//!   while the prisoner is away.
//!
//! The owner config section is "disconnect" (docs/config/owner-config.md). Every decision logs a
//! [LFDebug:Disconnect] line and a flagged logout writes an incident row (kind "logout").
//! Persistence: nothing new. The sentence is the jail payload, the warrant the account, the fine
//! the bank.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_DisconnectConfig
{
	static const string SECTION = "disconnect";
	static const string RULE_JAIL = "jail";
	static const string RULE_FINE = "fine";
	static const string RULE_RELEASE = "release";

	static const int WANTED_ADD_MAX = 5;	// lf-fixed: the schema limit, the wanted cap
	static const int JAIL_SECONDS_MAX = 86400;	// lf-fixed: the schema limit, one day
	static const int FINE_MAX = 10000000;	// lf-fixed: the schema limit, mirrors police fineCap

	[Attribute("jail", UIWidgets.Auto, "Logging out while cuffed: jail (the rejoin spawns in the cell), fine (taken from the bank) or release (only the cuffs come off)")]
	string m_sCuffedRule;

	[Attribute("1", UIWidgets.CheckBox, "A player escorted on foot by an officer counts as cuffed")]
	bool m_bEscortedCountsAsCuffed;

	[Attribute("1", UIWidgets.Auto, "Wanted levels added for leaving custody (jail and fine rules)", params: "0 5")]
	int m_iCuffedWantedAdd;

	[Attribute("300", UIWidgets.Auto, "Shortest sentence in seconds for leaving custody under the jail rule", params: "0 86400")]
	int m_iCuffedMinJailSeconds;

	[Attribute("1000", UIWidgets.Auto, "Fine taken from the bank for leaving custody under the fine rule", params: "0 10000000")]
	int m_iCuffedFine;

	[Attribute("1", UIWidgets.CheckBox, "The fine may take the bank below zero (debt). Off: only what the bank holds")]
	bool m_bCuffedFineAllowsDebt;

	[Attribute("1", UIWidgets.CheckBox, "Logging out while downed is death. Off: the body is left to the base game, and the rejoin is alive")]
	bool m_bDownedDies;

	[Attribute("0", UIWidgets.Auto, "Wanted levels added for logging out while wanted and not in custody. The warrant itself always stays", params: "0 5")]
	int m_iWantedLogoutAdd;

	[Attribute("0", UIWidgets.CheckBox, "A jail sentence counts down while the prisoner is offline. Off: only online time is served")]
	bool m_bJailClockOffline;

	protected static ref LF_DisconnectConfig s_Config;

	//------------------------------------------------------------------------------------------------
	//! The shipped values; the members carry no initializers (#1077).
	static LF_DisconnectConfig CreateDefault()
	{
		LF_DisconnectConfig config = new LF_DisconnectConfig();
		config.m_sCuffedRule = RULE_JAIL;
		config.m_bEscortedCountsAsCuffed = true;
		config.m_iCuffedWantedAdd = 1;
		config.m_iCuffedMinJailSeconds = 300;
		config.m_iCuffedFine = 1000;
		config.m_bCuffedFineAllowsDebt = true;
		config.m_bDownedDies = true;
		config.m_iWantedLogoutAdd = 0;
		config.m_bJailClockOffline = false;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped values with the owner's "disconnect" section over them.
	static LF_DisconnectConfig Get()
	{
		if (s_Config)
			return s_Config;

		s_Config = CreateDefault();
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (reader)
		{
			s_Config.ApplyOwner(reader);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("disconnect: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
		}

		LF_Debug.Info(LF_DisconnectRules.FEATURE, string.Format("rules: cuffed=%1 escorted-as-cuffed=%2 downed-dies=%3 wanted-add=%4 jail-clock-offline=%5", s_Config.m_sCuffedRule, s_Config.m_bEscortedCountsAsCuffed, s_Config.m_bDownedDies, s_Config.m_iWantedLogoutAdd, s_Config.m_bJailClockOffline));
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests and the config reload.
	static void Reload()
	{
		s_Config = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: use this config until Reload.
	static void Use(LF_DisconnectConfig config)
	{
		s_Config = config;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsRule(string rule)
	{
		return rule == RULE_JAIL || rule == RULE_FINE || rule == RULE_RELEASE;
	}

	//------------------------------------------------------------------------------------------------
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"cuffedRule", "escortedCountsAsCuffed", "cuffedWantedAdd", "cuffedMinJailSeconds", "cuffedFine", "cuffedFineAllowsDebt", "downedDies", "wantedLogoutAdd", "jailClockOffline"};
		reader.WarnUnknownKeys(ownerKeys);

		string rule = m_sCuffedRule;
		if (reader.ReadString("cuffedRule", rule))
		{
			rule.ToLower();
			if (IsRule(rule))
				m_sCuffedRule = rule;
			else
				LF_Debug.Error(LF_DisconnectRules.FEATURE, string.Format("disconnect.cuffedRule = %1 is not jail, fine or release; keeping %2", rule, m_sCuffedRule));
		}

		reader.ReadBool("escortedCountsAsCuffed", m_bEscortedCountsAsCuffed);
		reader.ReadInt("cuffedWantedAdd", m_iCuffedWantedAdd, 0, WANTED_ADD_MAX);
		reader.ReadInt("cuffedMinJailSeconds", m_iCuffedMinJailSeconds, 0, JAIL_SECONDS_MAX);
		reader.ReadInt("cuffedFine", m_iCuffedFine, 0, FINE_MAX);
		reader.ReadBool("cuffedFineAllowsDebt", m_bCuffedFineAllowsDebt);
		reader.ReadBool("downedDies", m_bDownedDies);
		reader.ReadInt("wantedLogoutAdd", m_iWantedLogoutAdd, 0, WANTED_ADD_MAX);
		reader.ReadBool("jailClockOffline", m_bJailClockOffline);
	}
}

//------------------------------------------------------------------------------------------------
//! What a leaving player was, read on the server just before the base game removes the body.
class LF_DisconnectState : Managed
{
	string m_sPlayerUid;
	bool m_bCuffed;
	bool m_bEscorted;
	bool m_bDowned;
	bool m_bStunned;
	int m_iWanted;
	bool m_bServing;
	bool m_bCellPlaced;

	//------------------------------------------------------------------------------------------------
	//! "cuffed,downed,wanted" style; "clean" when nothing applies.
	string Describe()
	{
		array<string> parts = {};
		if (m_bCuffed)
			parts.Insert("cuffed");
		if (m_bEscorted)
			parts.Insert("escorted");
		if (m_bDowned)
			parts.Insert("downed");
		if (m_bStunned)
			parts.Insert("stunned");
		if (m_iWanted > 0)
			parts.Insert("wanted" + m_iWanted);
		if (m_bServing)
			parts.Insert("jailed");

		if (parts.IsEmpty())
			return "clean";

		string text = parts[0];
		for (int i = 1; i < parts.Count(); i++)
			text += "," + parts[i];

		return text;
	}
}

//------------------------------------------------------------------------------------------------
//! What the rules decided for one logout.
class LF_DisconnectDecision : Managed
{
	//! LF_DisconnectConfig.RULE_*, or empty when the player was not in custody.
	string m_sCustody;
	//! The configured jail rule fell back to the fine (no cell placed).
	bool m_bJailFellBack;
	int m_iWantedAdd;
	int m_iMinJailSeconds;
	int m_iFine;
	bool m_bFineAllowsDebt;
	bool m_bDownedDies;
	bool m_bKeepWarrant;
	bool m_bKeepSentence;

	//------------------------------------------------------------------------------------------------
	//! True when the logout changes or keeps anything worth a log line and an incident row.
	bool IsFlagged()
	{
		return !m_sCustody.IsEmpty() || m_iWantedAdd > 0 || m_bDownedDies || m_bKeepWarrant || m_bKeepSentence;
	}

	//------------------------------------------------------------------------------------------------
	string Describe()
	{
		array<string> parts = {};
		if (m_sCustody == LF_DisconnectConfig.RULE_JAIL)
			parts.Insert(string.Format("jail min=%1s", m_iMinJailSeconds));
		else if (m_sCustody == LF_DisconnectConfig.RULE_FINE)
			parts.Insert(string.Format("fine %1", m_iFine));
		else if (m_sCustody == LF_DisconnectConfig.RULE_RELEASE)
			parts.Insert("release");
		if (m_bJailFellBack)
			parts.Insert("(no cell: fine instead of jail)");
		if (m_iWantedAdd > 0)
			parts.Insert("wanted+" + m_iWantedAdd);
		if (m_bDownedDies)
			parts.Insert("dies");
		if (m_bKeepWarrant)
			parts.Insert("warrant kept");
		if (m_bKeepSentence)
			parts.Insert("sentence kept");

		if (parts.IsEmpty())
			return "nothing";

		string text = parts[0];
		for (int i = 1; i < parts.Count(); i++)
			text += ", " + parts[i];

		return text;
	}
}

//------------------------------------------------------------------------------------------------
class LF_DisconnectRules
{
	static const string FEATURE = "Disconnect";

	//------------------------------------------------------------------------------------------------
	//! Pure: the rules for one leaving player.
	static LF_DisconnectDecision Decide(notnull LF_DisconnectState state, notnull LF_DisconnectConfig config)
	{
		LF_DisconnectDecision decision = new LF_DisconnectDecision();
		bool inCustody = state.m_bCuffed || (state.m_bEscorted && config.m_bEscortedCountsAsCuffed);

		// A serving prisoner is already where custody leads; the sentence rule covers them.
		if (inCustody && !state.m_bServing)
		{
			decision.m_sCustody = config.m_sCuffedRule;
			if (!LF_DisconnectConfig.IsRule(decision.m_sCustody))
				decision.m_sCustody = LF_DisconnectConfig.RULE_JAIL;

			if (decision.m_sCustody == LF_DisconnectConfig.RULE_JAIL && !state.m_bCellPlaced)
			{
				decision.m_sCustody = LF_DisconnectConfig.RULE_FINE;
				decision.m_bJailFellBack = true;
			}

			if (decision.m_sCustody != LF_DisconnectConfig.RULE_RELEASE)
				decision.m_iWantedAdd = config.m_iCuffedWantedAdd;

			decision.m_iMinJailSeconds = config.m_iCuffedMinJailSeconds;
			decision.m_iFine = config.m_iCuffedFine;
			decision.m_bFineAllowsDebt = config.m_bCuffedFineAllowsDebt;
		}
		else if (state.m_iWanted > 0 && !state.m_bServing)
		{
			decision.m_iWantedAdd = config.m_iWantedLogoutAdd;
		}

		// A stun is not a death in waiting: it never kills.
		decision.m_bDownedDies = state.m_bDowned && !state.m_bStunned && config.m_bDownedDies;
		decision.m_bKeepWarrant = state.m_iWanted > 0 || decision.m_iWantedAdd > 0;
		decision.m_bKeepSentence = state.m_bServing;
		return decision;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the sentence for leaving custody under the jail rule.
	static int JailSeconds(int sentenceForWanted, int minSeconds)
	{
		return Math.Max(sentenceForWanted, minSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the game mode before any other disconnect cleanup (the cuffs, the downed death
	//! and the escort are still in place). Reads the state, decides and applies the custody rule.
	//! The downed death itself is LF_BleedOut's, which asks DownedDies.
	static LF_DisconnectDecision OnPlayerDisconnected(int playerId)
	{
		if (!Replication.IsServer())
			return null;

		string uid = LF_Utils.GetPlayerUID(playerId);
		if (uid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("player %1 left with no uid: no rule applied", playerId));
			return null;
		}

		IEntity body;
		if (GetGame() && GetGame().GetPlayerManager())
			body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);

		LF_DisconnectState state = ReadState(uid, body);
		return Apply(state, body);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the state of uid with body as the character (may be null: a test, a body already gone).
	static LF_DisconnectState ReadState(string uid, IEntity body)
	{
		LF_DisconnectState state = new LF_DisconnectState();
		state.m_sPlayerUid = uid;
		state.m_bCuffed = LF_Restraint.IsRestrained(body) || LF_Restraint.HasHold(uid);
		state.m_bEscorted = LF_JailEscort.IsEscorted(uid);
		if (body)
		{
			state.m_bDowned = LF_Downed.IsDowned(body);
			state.m_bStunned = LF_Stun.IsStunned(body);
		}

		state.m_bServing = LF_Jail.IsServing(uid);
		state.m_bCellPlaced = LF_Jail.HasCell();

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts)
		{
			LF_PlayerAccount account = accounts.GetAccount(uid);
			if (account)
				state.m_iWanted = account.GetWantedLevel();
		}

		return state;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: decide for state and carry out the custody and wanted parts.
	static LF_DisconnectDecision Apply(notnull LF_DisconnectState state, IEntity body)
	{
		string uid = state.m_sPlayerUid;
		LF_DisconnectDecision decision = Decide(state, LF_DisconnectConfig.Get());
		if (!decision.IsFlagged())
			return decision;

		LF_Debug.Info(FEATURE, string.Format("%1 left %2: %3", uid, state.Describe(), decision.Describe()));

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account;
		if (accounts)
			account = accounts.GetAccount(uid);

		int wanted = state.m_iWanted;
		if (decision.m_iWantedAdd > 0)
		{
			if (account)
			{
				account.IncreaseWantedLevel(decision.m_iWantedAdd);
				wanted = account.GetWantedLevel();
				LF_Debug.Info(FEATURE, string.Format("%1 wanted %2 -> %3 for logging out", uid, state.m_iWanted, wanted));
			}
			else
			{
				LF_Debug.Error(FEATURE, string.Format("%1 wanted not raised: no account", uid));
			}
		}

		int amount;
		if (decision.m_sCustody == LF_DisconnectConfig.RULE_JAIL)
		{
			int seconds = JailSeconds(LF_CrimeCatalog.SentenceFor(uid, LF_Jail.SentenceSeconds(wanted)), decision.m_iMinJailSeconds);
			if (LF_Jail.ImprisonOnReturn(uid, seconds, wanted))
			{
				amount = seconds;
				LF_Debug.Info(FEATURE, string.Format("%1 jailed for leaving custody: %2 s, served from the rejoin in the cell", uid, seconds));
			}
			else
			{
				LF_Debug.Warn(FEATURE, string.Format("%1 could not be jailed at logout: fine instead", uid));
				decision.m_sCustody = LF_DisconnectConfig.RULE_FINE;
				decision.m_bJailFellBack = true;
			}
		}

		if (decision.m_sCustody == LF_DisconnectConfig.RULE_FINE)
			amount = TakeFine(uid, decision.m_iFine, decision.m_bFineAllowsDebt);
		else if (decision.m_sCustody == LF_DisconnectConfig.RULE_RELEASE)
			LF_Debug.Info(FEATURE, string.Format("%1 left custody: released, no penalty (cuffedRule=release)", uid));

		if (decision.m_bKeepSentence)
			LF_Debug.Info(FEATURE, string.Format("%1 left jail: %2 s of the sentence kept", uid, LF_Jail.GetRemaining(uid)));

		if (decision.m_bKeepWarrant)
			LF_Debug.Info(FEATURE, string.Format("%1 left wanted %2: the warrant stays", uid, wanted));

		if (accounts && account && decision.m_iWantedAdd > 0)
			accounts.SaveAndReleaseAccount(account);

		vector position;
		if (body)
			position = body.GetOrigin();

		LF_EventBus.Publish(LF_OnFlaggedLogout.Create(uid, state.Describe(), decision.Describe(), position, amount));
		return decision;
	}

	//------------------------------------------------------------------------------------------------
	//! The logout fine from uid's bank. \return The amount taken.
	protected static int TakeFine(string uid, int fine, bool allowDebt)
	{
		if (fine <= 0)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 left custody: the fine is 0, nothing taken", uid));
			return 0;
		}

		LF_BankAccount bank = LF_ATMManager.GetOrCreate(uid);
		if (!bank)
		{
			LF_Debug.Error(FEATURE, string.Format("%1 left custody: no bank account, fine %2 not taken", uid, fine));
			return 0;
		}

		int taken = bank.Charge(fine, allowDebt, "disconnect fine");
		LF_Debug.Info(FEATURE, string.Format("%1 left custody: fined %2 of %3 from the bank, balance %4", uid, taken, fine, bank.GetBalance()));
		return taken;
	}

	//------------------------------------------------------------------------------------------------
	//! LF_BleedOut: does a downed logout die?
	static bool DownedDies()
	{
		return LF_DisconnectConfig.Get().m_bDownedDies;
	}

	//------------------------------------------------------------------------------------------------
	//! LF_Jail: does an offline prisoner serve time?
	static bool JailClockOffline()
	{
		return LF_DisconnectConfig.Get().m_bJailClockOffline;
	}
}
