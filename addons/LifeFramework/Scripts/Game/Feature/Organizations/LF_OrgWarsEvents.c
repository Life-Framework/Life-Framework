//------------------------------------------------------------------------------------------------
//! Organization war events on the server bus (docs/architecture/events.md; M7 gang war, #2666).
//! Fields are plain values: org ids, war ids, unix seconds. Published on the server only, after the
//! transition in LF_OrgWars happened. A new event of this feature is appended to this file.

//! An org declared war on another (state DECLARED, waiting for the answer).
class LF_OnOrgWarDeclared : LF_Event
{
	string m_sWarId;
	string m_sDeclarerOrgId;
	string m_sTargetOrgId;
	int m_iDeclaredAt;
	//! Unix second the declaration lapses when nobody answers.
	int m_iAnswerBy;

	static LF_OnOrgWarDeclared Create(string warId, string declarerOrgId, string targetOrgId, int declaredAt, int answerBy)
	{
		LF_OnOrgWarDeclared e = new LF_OnOrgWarDeclared();
		e.m_sWarId = warId;
		e.m_sDeclarerOrgId = declarerOrgId;
		e.m_sTargetOrgId = targetOrgId;
		e.m_iDeclaredAt = declaredAt;
		e.m_iAnswerBy = answerBy;
		return e;
	}
}

//! The target accepted: the two orgs are at war until m_iEndsAt.
class LF_OnOrgWarAccepted : LF_Event
{
	string m_sWarId;
	string m_sDeclarerOrgId;
	string m_sTargetOrgId;
	int m_iAcceptedAt;
	int m_iEndsAt;

	static LF_OnOrgWarAccepted Create(string warId, string declarerOrgId, string targetOrgId, int acceptedAt, int endsAt)
	{
		LF_OnOrgWarAccepted e = new LF_OnOrgWarAccepted();
		e.m_sWarId = warId;
		e.m_sDeclarerOrgId = declarerOrgId;
		e.m_sTargetOrgId = targetOrgId;
		e.m_iAcceptedAt = acceptedAt;
		e.m_iEndsAt = endsAt;
		return e;
	}
}

//! A war closed: it ran out (reason "time"), a declaration went unanswered (reason "lapsed"), or an
//! org gave up (reason "forfeit"). m_bWasAtWar is false for a declaration that never became a war.
class LF_OnOrgWarEnded : LF_Event
{
	string m_sWarId;
	string m_sDeclarerOrgId;
	string m_sTargetOrgId;
	string m_sReason;
	int m_iEndedAt;
	int m_iCooldownUntil;
	bool m_bWasAtWar;

	static LF_OnOrgWarEnded Create(string warId, string declarerOrgId, string targetOrgId, string reason, int endedAt, int cooldownUntil, bool wasAtWar)
	{
		LF_OnOrgWarEnded e = new LF_OnOrgWarEnded();
		e.m_sWarId = warId;
		e.m_sDeclarerOrgId = declarerOrgId;
		e.m_sTargetOrgId = targetOrgId;
		e.m_sReason = reason;
		e.m_iEndedAt = endedAt;
		e.m_iCooldownUntil = cooldownUntil;
		e.m_bWasAtWar = wasAtWar;
		return e;
	}
}
