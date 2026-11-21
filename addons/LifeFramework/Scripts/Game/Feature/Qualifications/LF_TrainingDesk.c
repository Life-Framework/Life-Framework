//------------------------------------------------------------------------------------------------
//! The training desk (#3426, slice Q3 of #1035): an officer on duty takes a course there. The hold on
//! the action is the study; when it completes, the server checks the course rules, takes the fee from
//! the officer's carried cash and grants the course (LF_Qualifications.Grant), then toasts
//! "You've qualified: Detective".
//!
//! The fee is a cost to the officer, never a raise. It is taken when the hold completes, in the same
//! server call that grants: OnActionStart only runs on the acting client, so a leave-mid-study never
//! reaches the server and nothing is charged, held or saved for it. Server logic only; the action
//! class below asks, the rules (LF_QualificationRules) decide.
//------------------------------------------------------------------------------------------------
class LF_TrainingDesk
{
	static const string TITLE = "#LF-Training_Title";
	static const string MSG_QUALIFIED = "#LF-Training_Qualified";
	static const string MSG_NOT_ON_DUTY = "#LF-Training_NotOnDuty";
	//! A course's display name is the key `LF-Training_Course_<id>`; built in two parts because the key is composed.
	static const string COURSE_NAME_PREFIX = "#" + "LF-Training_Course_";

	//------------------------------------------------------------------------------------------------
	//! The account rule's faction name for a course row (`police`, `medic`, `civilian`).
	static string FactionName(LF_Faction faction)
	{
		if (faction == LF_Faction.POLICE)
			return "police";

		if (faction == LF_Faction.MEDIC)
			return "medic";

		return "civilian";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the officer finished studying `courseId`. Checks that the character is on duty and the
	//! course rules pass (faction, rank, not already held, purse), takes the fee and grants the course,
	//! all or nothing. A refusal changes nothing and is told to the player.
	//! \return The verdict; `m_bOk` is true when the course was granted.
	static LF_QualificationVerdict Take(IEntity user, string courseId)
	{
		LF_QualificationVerdict refused = new LF_QualificationVerdict();
		refused.m_sCourseId = courseId;

		if (!user)
		{
			LF_Debug.Error(LF_QualificationConfig.FEATURE, "training desk: no user");
			return refused;
		}

		string uid = LF_PoliceRanks.ResolveUid(user);
		LF_PlayerAccount account;
		LF_PlayerAccountManager manager = LF_PlayerAccountManager.GetInstance();
		if (manager && !uid.IsEmpty())
			account = manager.GetAccount(uid);

		if (!account || !account.IsOnDuty())
		{
			refused.m_sReason = MSG_NOT_ON_DUTY;
			LF_Debug.Info(LF_QualificationConfig.FEATURE, string.Format("training desk: %1 refused for '%2': not on duty", uid, courseId));
			LF_Utils.NotifyEntity(user, refused.m_sReason, TITLE, 4.0);
			return refused;
		}

		string key = account.GetActiveCharacterKey();
		array<string> held = {};
		LF_Qualifications.GetHeld(key, held);
		int cash = LF_MoneyUtils.GetCash(user);

		LF_QualificationVerdict verdict = LF_QualificationRules.Check(LF_QualificationConfig.Get(), courseId, FactionName(account.GetFaction()), account.GetPoliceRank(), held, cash);
		if (!verdict.m_bOk)
		{
			LF_Utils.NotifyEntity(user, verdict.m_sReason, TITLE, 4.0, verdict.m_iNumber.ToString());
			return verdict;
		}

		int taken = LF_MoneyUtils.RemoveCash(user, verdict.m_iFee, "training");
		if (taken < verdict.m_iFee)
		{
			// The purse was checked a moment ago; a short take means the cash moved. Give back what came out.
			LF_Debug.Warn(LF_QualificationConfig.FEATURE, string.Format("training desk: fee %1 for '%2' only took %3, refunded", verdict.m_iFee, courseId, taken));
			if (taken > 0)
				LF_MoneyUtils.AddCash(user, taken, "training-refund");
			verdict.m_bOk = false;
			verdict.m_sReason = LF_QualificationRules.REASON_PURSE;
			verdict.m_iNumber = verdict.m_iFee;
			LF_Utils.NotifyEntity(user, verdict.m_sReason, TITLE, 4.0, verdict.m_iNumber.ToString());
			return verdict;
		}

		if (!LF_Qualifications.Grant(key, courseId))
		{
			// The rules said it was free to take, so a refused grant is a bug; do not keep the fee for it.
			LF_Debug.Error(LF_QualificationConfig.FEATURE, string.Format("training desk: grant of '%1' to %2 failed after the fee was taken, fee refunded", courseId, key));
			LF_MoneyUtils.AddCash(user, taken, "training-refund");
			verdict.m_bOk = false;
			return verdict;
		}

		LF_Debug.Info(LF_QualificationConfig.FEATURE, string.Format("training desk: %1 qualified for '%2', fee %3", key, courseId, verdict.m_iFee));
		LF_Utils.NotifyEntity(user, MSG_QUALIFIED, TITLE, 5.0, LF_Terms.Text(COURSE_NAME_PREFIX + courseId));
		return verdict;
	}
}

//------------------------------------------------------------------------------------------------
//! "Take the <course> course" at the training desk: a held action, one per course on the prefab
//! (`m_sCourseId`). The hold is the study; its length is the prefab's `Duration`, a
//! quarter of a second per in-game study minute (detective 240 min = 60 s). Hidden for a character already holding the course and
//! for anyone off duty; the server re-checks everything when the hold completes.
class LF_TrainingDeskAction : ScriptedUserAction
{
	[Attribute("", UIWidgets.EditBox, "The course id this action takes (LF_QualificationCourse.m_sId)")]
	protected string m_sCourseId;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_QualificationConfig.Get() || !LF_QualificationConfig.Get().Find(m_sCourseId))
			return false;

		return LF_AccountStateBits.IsOnDuty(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_AccountStateBits.IsOnDuty(LF_AccountReplication.GetState(user)))
		{
			SetCannotPerformReason(LF_TrainingDesk.MSG_NOT_ON_DUTY);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_TrainingDesk.Take(pUserEntity, m_sCourseId);
	}
}
