//------------------------------------------------------------------------------------------------
//! A reply that offers a contract (#2913). The dialogue file names the hook "Contract" and the
//! contract id as its param ("test_parcel"). Same shape as LF_DialogueHook_JobSignUp (#2740).
//! As a condition: the reply is offered only when the contract's standing gate passes (Offers runs
//! on the server, where the points live, so this is the authoritative tier, not ClientTier).
//! As an action: the server signs the player up for the contract's job when they are not on it
//! yet (through the JobSignUp hook, with its career and standing checks), then calls
//! LF_ReputationContracts.Begin, which refuses a dead or downed body, rate limits as a purchase,
//! re-checks the gate, the one-at-a-time rule and the cooldown, takes the run's deposit and binds
//! the contract. Every refusal is told to the player in words; nothing moves on a refusal.
class LF_DialogueHook_Contract : LF_DialogueHook
{
	static const string KEY_STARTED = "#LF-Contract_Started";
	static const string KEY_RUN_OPEN = "#LF-Contract_RunOpen";

	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		LF_ReputationContractDef def = LF_ReputationContracts.FindDef(param);
		if (!def || def.Job() == LF_EJobType.UNEMPLOYED)
			return false;

		string uid = LF_Utils.GetPlayerUid(player);
		if (uid.IsEmpty())
			return false;

		return LF_Reputation.Passes(uid, def.m_sStandingGate);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)
	{
		// A local copy: the nested JobSignUp Run below overwrote this hook's `param` in the engine (seen
		// in LF_Test_DialogueContract: Begin was handed "PARCEL"). Read `param` here only.
		string contractId = param;
		if (LF_Systems.Refuses(LF_CivilianJob.SYSTEM, "dialogue contract"))
			return;

		string uid = LF_Utils.GetPlayerUid(player);
		if (!player || uid.IsEmpty())
		{
			LF_Debug.Error(LF_Reputation.FEATURE, "dialogue contract refused: no player or no account id");
			return;
		}

		LF_ReputationContractDef def = LF_ReputationContracts.FindDef(contractId);
		if (!def || def.Job() == LF_EJobType.UNEMPLOYED)
		{
			LF_Debug.Error(LF_Reputation.FEATURE, string.Format("dialogue contract refused: '%1' is not a contract with a delivery job", contractId));
			Tell(player, spoken, LF_ReputationContracts.KEY_UNKNOWN, LF_EJobType.UNEMPLOYED, "");
			return;
		}

		LF_EJobType job = def.Job();
		float now = LF_JobGuards.Now();
		string tierWord;
		string refusal = LF_ReputationContracts.Refusal(uid, contractId, now, tierWord);
		if (!refusal.IsEmpty())
		{
			// Refused before any sign-up or money: the same check Begin makes again below.
			LF_Debug.Info(LF_Reputation.FEATURE, string.Format("dialogue contract %1 refused for %2: %3", contractId, uid, refusal));
			Tell(player, spoken, refusal, job, tierWord);
			return;
		}

		// A run already under way (any job) is finished or abandoned first: signing up for the contract's
		// job would change the job under that open run.
		if (LF_CivilianJob.HasActiveRun(uid))
		{
			LF_Debug.Info(LF_Reputation.FEATURE, string.Format("dialogue contract %1 refused for %2: a run is already under way", contractId, uid));
			Tell(player, spoken, KEY_RUN_OPEN, job, "");
			return;
		}

		if (LF_CivilianJob.GetSignedUpJob(uid) != job)
		{
			array<string> signUpSaid = {};
			LF_DialogueHook_JobSignUp signUp = new LF_DialogueHook_JobSignUp();
			signUp.Run(player, npc, def.m_sJob, signUpSaid);
			if (LF_CivilianJob.GetSignedUpJob(uid) != job)
			{
				// The sign-up hook already told the player why (standing or career level).
				LF_Debug.Info(LF_Reputation.FEATURE, string.Format("dialogue contract %1 refused for %2: not signed up for %3", contractId, uid, def.m_sJob));
				return;
			}
		}

		int deposit;
		LF_ERunStart started = LF_ReputationContracts.Begin(player, uid, contractId, now, deposit, refusal, tierWord);
		if (started == LF_ERunStart.NO_DEPOSIT)
		{
			LF_Utils.NotifyEntity(player, LF_CivilianJob.KEY_NEED_DEPOSIT, LF_CivilianJob.NameKey(job), 6.0, deposit.ToString());
			return;
		}

		if (started != LF_ERunStart.STARTED)
		{
			if (!refusal.IsEmpty())
				Tell(player, spoken, refusal, job, tierWord);

			LF_Debug.Info(LF_Reputation.FEATURE, string.Format("dialogue contract %1 not started for %2 (%3)", contractId, uid, typename.EnumToString(LF_ERunStart, started)));
			return;
		}

		LF_JobMapMarks.OnRunStarted(uid, job, player.GetOrigin());
		if (!refusal.IsEmpty())
		{
			// The run started but the contract did not bind (Begin logged it): an ordinary run.
			Tell(player, spoken, refusal, job, "");
			return;
		}

		LF_Debug.Info(LF_Reputation.FEATURE, string.Format("dialogue contract %1 started by %2 (deposit %3)", contractId, uid, deposit));
		spoken.Insert(KEY_STARTED);
		LF_Utils.NotifyEntity(player, KEY_STARTED, LF_CivilianJob.NameKey(job), 6.0);
	}

	//------------------------------------------------------------------------------------------------
	//! A refusal in words: a toast always (it carries the tier word), and the NPC's answer when the
	//! key needs no argument.
	protected void Tell(IEntity player, notnull array<string> spoken, string key, LF_EJobType job, string tierWord)
	{
		if (tierWord.IsEmpty())
			spoken.Insert(key);

		string title;
		if (job != LF_EJobType.UNEMPLOYED)
			title = LF_CivilianJob.NameKey(job);

		LF_Utils.NotifyEntity(player, key, title, 6.0, tierWord);
	}
}
