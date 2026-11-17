//------------------------------------------------------------------------------------------------
//! Organization war record (M7 gang war W1, #2666; #766, #858). A war moves DECLARED -> ACCEPTED ->
//! ENDED, or DECLARED -> LAPSED when nobody answers. Server state in memory only: persistence is W6
//! (ExportAll / ApplyAll). Every time is real unix seconds the caller passes in, so a restart mid-war
//! keeps its clock. Keyed by organization id, never name. No RPC, no UI, no money: rights and the fee
//! are W3, `LF_OrgWarRules` (from `LF_OrgWarConfig.Rules()`, W2 #2667) is the rules value.
//------------------------------------------------------------------------------------------------
enum LF_EOrgWarState
{
	DECLARED,
	ACCEPTED,
	ENDED,
	LAPSED
}

//------------------------------------------------------------------------------------------------
//! One war. Flat on purpose: W6 saves these fields as they are.
class LF_OrgWarRecord : Managed
{
	string m_sWarId;
	string m_sDeclarerOrgId;
	string m_sTargetOrgId;
	LF_EOrgWarState m_eState;
	int m_iDeclaredAt;
	int m_iAcceptedAt;
	int m_iEndsAt;
	//! Answer window (seconds) the declaration was made under; the lapse time is declaredAt plus this.
	int m_iAnswerWindowSec;
	//! Post-war cooldown (seconds) the war was accepted under; applied from the end of the war.
	int m_iCooldownSec;
	//! A new declaration on this pair is refused before this unix second (0 when none).
	int m_iCooldownUntil;

	//------------------------------------------------------------------------------------------------
	bool Involves(string orgId)
	{
		return m_sDeclarerOrgId == orgId || m_sTargetOrgId == orgId;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPair(string orgA, string orgB)
	{
		return (m_sDeclarerOrgId == orgA && m_sTargetOrgId == orgB) || (m_sDeclarerOrgId == orgB && m_sTargetOrgId == orgA);
	}
}

//------------------------------------------------------------------------------------------------
class LF_OrgWars
{
	static const string FEATURE = "OrgWars";

	//! Refusal keys (the UI slice maps them to text). "" means accepted.
	static const string REFUSE_SELF = "self";
	static const string REFUSE_UNKNOWN_ORG = "unknown_org";
	static const string REFUSE_PENDING = "pending";
	static const string REFUSE_COOLDOWN = "cooldown";
	static const string REFUSE_UNKNOWN_WAR = "unknown_war";
	static const string REFUSE_NOT_PENDING = "not_pending";
	static const string REFUSE_EXPIRED = "expired";

	//! End reasons carried by LF_OnOrgWarEnded.
	static const string REASON_TIME = "time";
	static const string REASON_LAPSED = "lapsed";
	static const string REASON_FORFEIT = "forfeit";

	protected static ref array<ref LF_OrgWarRecord> s_aWars;
	protected static int s_iNextId = 1;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_OrgWarRecord> Table()
	{
		if (!s_aWars)
			s_aWars = new array<ref LF_OrgWarRecord>();

		return s_aWars;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aWars = null;
		s_iNextId = 1;
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return Table().Count();
	}

	//------------------------------------------------------------------------------------------------
	static LF_OrgWarRecord Get(string warId)
	{
		foreach (LF_OrgWarRecord war : Table())
		{
			if (war && war.m_sWarId == warId)
				return war;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: orgA declares war on orgB at unix second `now`. Returns "" and the new record in `war`,
	//! or a REFUSE_ key (logged) and no record.
	static string Declare(string declarerOrgId, string targetOrgId, int now, notnull LF_OrgWarRules rules, out LF_OrgWarRecord war)
	{
		war = null;
		Tick(now);

		if (declarerOrgId == targetOrgId)
			return Refused("declare", REFUSE_SELF, declarerOrgId, targetOrgId);

		if (!LF_OrganizationManager.Get(declarerOrgId) || !LF_OrganizationManager.Get(targetOrgId))
			return Refused("declare", REFUSE_UNKNOWN_ORG, declarerOrgId, targetOrgId);

		foreach (LF_OrgWarRecord other : Table())
		{
			if (!other || !other.IsPair(declarerOrgId, targetOrgId))
				continue;

			if (other.m_eState == LF_EOrgWarState.DECLARED || other.m_eState == LF_EOrgWarState.ACCEPTED)
				return Refused("declare", REFUSE_PENDING, declarerOrgId, targetOrgId);

			if (other.m_eState == LF_EOrgWarState.ENDED && now < other.m_iCooldownUntil)
				return Refused("declare", REFUSE_COOLDOWN, declarerOrgId, targetOrgId);
		}

		LF_OrgWarRecord record = new LF_OrgWarRecord();
		record.m_sWarId = string.Format("war-%1", s_iNextId);
		s_iNextId++;
		record.m_sDeclarerOrgId = declarerOrgId;
		record.m_sTargetOrgId = targetOrgId;
		record.m_eState = LF_EOrgWarState.DECLARED;
		record.m_iDeclaredAt = now;
		record.m_iAnswerWindowSec = rules.m_iAnswerSeconds;
		record.m_iCooldownSec = rules.m_iCooldownSeconds;
		Table().Insert(record);
		war = record;

		LF_Debug.Info(FEATURE, string.Format("%1 declared: %2 on %3, answer by %4", record.m_sWarId, declarerOrgId, targetOrgId, now + rules.m_iAnswerSeconds));
		LF_EventBus.Publish(LF_OnOrgWarDeclared.Create(record.m_sWarId, declarerOrgId, targetOrgId, now, now + rules.m_iAnswerSeconds));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the target accepts. Returns "" or a REFUSE_ key. An accept at or after the window's end
	//! lapses the declaration instead (REFUSE_EXPIRED).
	static string Accept(string warId, int now, notnull LF_OrgWarRules rules)
	{
		LF_OrgWarRecord war = Get(warId);
		if (!war)
			return Refused("accept", REFUSE_UNKNOWN_WAR, warId, string.Empty);

		if (war.m_eState != LF_EOrgWarState.DECLARED)
			return Refused("accept", REFUSE_NOT_PENDING, warId, string.Empty);

		int answerBy = war.m_iDeclaredAt + war.m_iAnswerWindowSec;
		if (now >= answerBy)
		{
			Finish(war, LF_EOrgWarState.LAPSED, REASON_LAPSED, answerBy);
			return Refused("accept", REFUSE_EXPIRED, warId, string.Empty);
		}

		war.m_eState = LF_EOrgWarState.ACCEPTED;
		war.m_iAcceptedAt = now;
		war.m_iEndsAt = now + rules.m_iLengthSeconds;
		war.m_iCooldownSec = rules.m_iCooldownSeconds;
		LF_Debug.Info(FEATURE, string.Format("%1 accepted: %2 and %3 are at war until %4", warId, war.m_sDeclarerOrgId, war.m_sTargetOrgId, war.m_iEndsAt));
		LF_EventBus.Publish(LF_OnOrgWarAccepted.Create(warId, war.m_sDeclarerOrgId, war.m_sTargetOrgId, now, war.m_iEndsAt));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: move every war the clock has passed. A DECLARED war past its answer window becomes
	//! LAPSED; an ACCEPTED war past endsAt becomes ENDED. The end time recorded is the deadline, not
	//! `now`, so a late tick does not stretch the cooldown.
	static void Tick(int now)
	{
		// Snapshot: Finish publishes, and a handler may declare.
		array<LF_OrgWarRecord> snapshot = {};
		foreach (LF_OrgWarRecord war : Table())
		{
			snapshot.Insert(war);
		}

		foreach (LF_OrgWarRecord w : snapshot)
		{
			if (!w)
				continue;

			if (w.m_eState == LF_EOrgWarState.DECLARED)
			{
				int answerBy = w.m_iDeclaredAt + w.m_iAnswerWindowSec;
				if (now >= answerBy)
					Finish(w, LF_EOrgWarState.LAPSED, REASON_LAPSED, answerBy);
			}
			else if (w.m_eState == LF_EOrgWarState.ACCEPTED && now >= w.m_iEndsAt)
			{
				Finish(w, LF_EOrgWarState.ENDED, REASON_TIME, w.m_iEndsAt);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: close a war now with a reason (forfeit, or anything W-later adds). An ACCEPTED war becomes
	//! ENDED and starts the pair cooldown; a DECLARED one becomes LAPSED. A war already closed is left alone.
	static void End(string warId, string reason, int now)
	{
		LF_OrgWarRecord war = Get(warId);
		if (!war)
		{
			Refused("end", REFUSE_UNKNOWN_WAR, warId, string.Empty);
			return;
		}

		if (war.m_eState == LF_EOrgWarState.ACCEPTED)
		{
			Finish(war, LF_EOrgWarState.ENDED, reason, now);
			return;
		}

		if (war.m_eState == LF_EOrgWarState.DECLARED)
		{
			Finish(war, LF_EOrgWarState.LAPSED, reason, now);
			return;
		}

		Refused("end", REFUSE_NOT_PENDING, warId, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	//! True while an ACCEPTED war between the two orgs is running. Reads state; call Tick(now) first
	//! when the clock has moved.
	static bool IsAtWar(string orgA, string orgB)
	{
		foreach (LF_OrgWarRecord war : Table())
		{
			if (war && war.m_eState == LF_EOrgWarState.ACCEPTED && war.IsPair(orgA, orgB))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The ACCEPTED wars an org is in.
	static array<ref LF_OrgWarRecord> ActiveFor(string orgId)
	{
		array<ref LF_OrgWarRecord> result = {};
		foreach (LF_OrgWarRecord war : Table())
		{
			if (war && war.m_eState == LF_EOrgWarState.ACCEPTED && war.Involves(orgId))
				result.Insert(war);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! W6 persistence hook: the records as they are. Left for W6 to wrap in a payload.
	static array<ref LF_OrgWarRecord> ExportAll()
	{
		array<ref LF_OrgWarRecord> result = {};
		foreach (LF_OrgWarRecord war : Table())
		{
			result.Insert(war);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! W6 persistence hook: replace the table with loaded records. Idempotent; the id counter moves past
	//! the highest "war-N" loaded. Dropping a record whose org is gone is W6's job.
	static void ApplyAll(notnull array<ref LF_OrgWarRecord> records)
	{
		s_aWars = new array<ref LF_OrgWarRecord>();
		s_iNextId = 1;
		foreach (LF_OrgWarRecord war : records)
		{
			if (!war)
				continue;

			s_aWars.Insert(war);
			if (war.m_sWarId.StartsWith("war-"))
			{
				int n = war.m_sWarId.Substring(4, war.m_sWarId.Length() - 4).ToInt();
				if (n >= s_iNextId)
					s_iNextId = n + 1;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Finish(LF_OrgWarRecord war, LF_EOrgWarState state, string reason, int at)
	{
		bool wasAtWar = war.m_eState == LF_EOrgWarState.ACCEPTED;
		war.m_eState = state;
		if (state == LF_EOrgWarState.ENDED)
			war.m_iCooldownUntil = at + war.m_iCooldownSec;

		LF_Debug.Info(FEATURE, string.Format("%1 %2 (%3) at %4", war.m_sWarId, typename.EnumToString(LF_EOrgWarState, state), reason, at));
		LF_EventBus.Publish(LF_OnOrgWarEnded.Create(war.m_sWarId, war.m_sDeclarerOrgId, war.m_sTargetOrgId, reason, at, war.m_iCooldownUntil, wasAtWar));
	}

	//------------------------------------------------------------------------------------------------
	protected static string Refused(string what, string key, string a, string b)
	{
		LF_Debug.Warn(FEATURE, string.Format("%1 refused: %2 (%3, %4)", what, key, a, b));
		return key;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OrgWars' statics (found by reflection; Core/LF_StaticReset.c).
class LF_OrgWars_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OrgWars";
	}

	override void Apply()
	{
		LF_OrgWars.ResetStatics();
	}
}
