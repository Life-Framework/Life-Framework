// LF_QuestHooks.c - additive seams that route existing gameplay signals into the
// quest system. Gather/process flow through EL_JobManager already; a modded
// override forwards them without touching the job logic. The arrival chain's
// SIGNAL keys (sign read, parcel run paid, ATM deposit) come from modded
// overrides here too, so the sign, job, and ATM files stay untouched.

//------------------------------------------------------------------------------------------------
//! SIGNAL keys and the server checks behind them.
class LF_QuestSignals
{
	static const string SIGN_READ = "sign.read:";
	static const string JOB_RUN_COMPLETE = "job.run_complete:";
	static const string ATM_DEPOSIT = "atm.deposit";

	protected static string s_sWantedSign;
	protected static bool s_bSignFound;

	//------------------------------------------------------------------------------------------------
	//! "sign.read:<id>" for a sign id in Configs/Signs/LF_Signs.conf.
	static string SignRead(string signId)
	{
		return SIGN_READ + signId;
	}

	//------------------------------------------------------------------------------------------------
	//! "job.run_complete:<job>", the job enum name in lower case ("parcel", "fuel_delivery", "taxi").
	static string JobRunComplete(EL_EJobType job)
	{
		string name = typename.EnumToString(EL_EJobType, job);
		name.ToLower();
		return JOB_RUN_COMPLETE + name;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether a sign prop with this id stands within radius of the user.
	static bool IsSignNear(IEntity user, string signId, float radius)
	{
		if (!user || signId.IsEmpty() || !GetGame() || !GetGame().GetWorld())
			return false;

		s_sWantedSign = signId;
		s_bSignFound = false;
		GetGame().GetWorld().QueryEntitiesBySphere(user.GetOrigin(), radius, CollectSign, null, EQueryEntitiesFlags.ALL);
		return s_bSignFound;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectSign(IEntity entity)
	{
		if (!entity)
			return true;

		LF_SignComponent sign = LF_SignComponent.Cast(entity.FindComponent(LF_SignComponent));
		if (sign && sign.GetSignId() == s_sWantedSign)
		{
			s_bSignFound = true;
			return false;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Rewards a giver hands over beyond cash, XP, and items.
class LF_QuestRewards
{
	//------------------------------------------------------------------------------------------------
	//! The account of the player controlling character, or null (an AI or a test body).
	static EL_PlayerAccount AccountOf(IEntity character)
	{
		string uid = EL_Utils.GetPlayerUID(character);
		EL_PlayerAccountManager manager = EL_PlayerAccountManager.GetInstance();
		if (uid.IsEmpty() || !manager)
			return null;

		return manager.GetAccount(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: prints the account's active character on an ID card in the character's pockets.
	//! A character already carrying their card gets no second one (LF_IdCard.Grant refuses).
	//! \return True when the character now carries a newly printed card.
	static bool HandIdCard(IEntity character, EL_PlayerAccount account)
	{
		if (!account || !account.GetActiveCharacter())
		{
			EL_Debug.Error("Quests", "ID card not handed over: no registered character for this player");
			return false;
		}

		EL_PlayerCharacter identity = account.GetActiveCharacter();
		return LF_IdCard.Grant(character, identity.GetId(), identity.GetFirstName(), identity.GetLastName(), identity.GetAge());
	}
}

//------------------------------------------------------------------------------------------------
modded class EL_JobManager
{
	//------------------------------------------------------------------------------------------------
	override void OnGatherCompleted(IEntity user, ResourceName gatheredItem)
	{
		super.OnGatherCompleted(user, gatheredItem);
		LF_QuestManager questManager = LF_QuestManager.GetInstance();
		if (questManager)
			questManager.ReportGather(user, gatheredItem);
	}

	//------------------------------------------------------------------------------------------------
	override void OnProcessCompleted(IEntity user, ResourceName processedItem)
	{
		super.OnProcessCompleted(user, processedItem);
		LF_QuestManager questManager = LF_QuestManager.GetInstance();
		if (questManager)
			questManager.ReportProcess(user, processedItem);
	}
}

//------------------------------------------------------------------------------------------------
//! "job.run_complete:<job>" when a drop-off pays a run.
modded class LF_JobDropoffAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		string uid;
		bool hadRun = false;
		EL_EJobType job = EL_EJobType.UNEMPLOYED;
		if (Replication.IsServer())
		{
			uid = EL_Utils.GetPlayerUid(pUserEntity);
			if (!uid.IsEmpty() && LF_CivilianJob.HasActiveRun(uid))
			{
				hadRun = true;
				job = LF_CivilianJob.GetActiveJob(uid);
			}
		}

		super.PerformAction(pOwnerEntity, pUserEntity);

		// Abandoning is its own action, so a run that ended here was paid.
		if (hadRun && !LF_CivilianJob.HasActiveRun(uid))
			LF_QuestManager.GetInstance().ReportSignal(pUserEntity, LF_QuestSignals.JobRunComplete(job));
	}
}

//------------------------------------------------------------------------------------------------
//! "atm.deposit" when cash lands in the account.
modded class EL_CharacterATMComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void ReplyResult(bool deposit, LF_EATMRefusal reason, int amount, int balance, int cash)
	{
		super.ReplyResult(deposit, reason, amount, balance, cash);

		if (deposit && reason == LF_EATMRefusal.NONE && Replication.IsServer())
			LF_QuestManager.GetInstance().ReportSignal(GetOwner(), LF_QuestSignals.ATM_DEPOSIT);
	}
}

//------------------------------------------------------------------------------------------------
//! "sign.read:<id>" when the local player reads a sign. Read is local only (static card), so
//! the reader's own quest tracker asks the server, which checks the sign is beside them.
modded class LF_SignReadAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		super.PerformAction(pOwnerEntity, pUserEntity);

		if (!pOwnerEntity || !pUserEntity || pUserEntity != EL_Utils.GetLocalCharacter())
			return;

		LF_SignDef sign = GetSign();
		if (!sign)
			return;

		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(pUserEntity.FindComponent(LF_QuestTrackerComponent));
		if (!tracker)
		{
			EL_Debug.Error("Quests", "sign read not reported: the reader has no quest tracker");
			return;
		}

		tracker.AskSignRead(sign.m_sId);
	}
}
