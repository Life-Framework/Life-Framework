//------------------------------------------------------------------------------------------------
//! How an officer closed a dispatch call (#4203, PO-D1). NONE while the call is open or was closed
//! without an officer (the suspect is no longer wanted).
enum LF_CallOutcomeKind
{
	NONE,
	//! A fine, an arrest or a revive settled it.
	RESOLVED,
	//! The suspect had left by the time the officer came.
	NO_ONE_FOUND,
	//! Nothing happened: the caller was wrong or trolling.
	UNFOUNDED
}

//------------------------------------------------------------------------------------------------
//! The outcome lives on the call it closed. LF_PoliceCall is a plain record in LF_PoliceCalls.c; the
//! field is added here so the outcome code stays in its own file. Session only, like the call list.
modded class LF_PoliceCall
{
	int m_iOutcome;
}

//------------------------------------------------------------------------------------------------
//! Server: an officer closes a call they hold as resolved, no one found or unfounded (#4203). The
//! outcome counts only when the officer stands within the call radius of the scene
//! (LF_PoliceCallPay.SiteOf: the crime's position, else the report's grid square); a far close is
//! refused with a localized reason and the call stays open. This only records and closes: pay, XP
//! and the caller's unfounded mark belong to the cards that read LF_PoliceCall.m_iOutcome.
class LF_CallOutcome
{
	static const string FEATURE = "CallOutcome";

	//------------------------------------------------------------------------------------------------
	//! Pure: the name of an outcome for logs.
	static string Name(LF_CallOutcomeKind outcome)
	{
		if (outcome == LF_CallOutcomeKind.RESOLVED)
			return "resolved";

		if (outcome == LF_CallOutcomeKind.NO_ONE_FOUND)
			return "no one found";

		if (outcome == LF_CallOutcomeKind.UNFOUNDED)
			return "unfounded";

		return "none";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the toast key the officer reads when the call closed with this outcome.
	static string ClosedKey(LF_CallOutcomeKind outcome)
	{
		if (outcome == LF_CallOutcomeKind.RESOLVED)
			return "#LF-CallOutcome_Resolved";

		if (outcome == LF_CallOutcomeKind.NO_ONE_FOUND)
			return "#LF-CallOutcome_NoOneFound";

		return "#LF-CallOutcome_Unfounded";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: officerUid, standing at officerPosition, closes call callId with outcome.
	//! \param messageKey The localization key to show the officer: the closing line on success, the
	//! reason on a refusal.
	//! \param messageArg The first argument of that key ("" when it takes none).
	//! \return True when the call closed with the outcome recorded on it.
	static bool Close(int callId, string officerUid, LF_CallOutcomeKind outcome, vector officerPosition, out string messageKey, out string messageArg)
	{
		messageArg = "";
		LF_PoliceCall call = LF_PoliceCalls.Get(callId);
		if (!call || call.m_bClosed)
		{
			messageKey = "#LF-CallOutcome_NoCall";
			LF_Debug.Warn(FEATURE, string.Format("close of call #%1 by %2 refused: no open call", callId, officerUid));
			return false;
		}

		if (outcome == LF_CallOutcomeKind.NONE)
		{
			messageKey = "#LF-CallOutcome_NoCall";
			LF_Debug.Warn(FEATURE, string.Format("close of call #%1 by %2 refused: no outcome chosen", callId, officerUid));
			return false;
		}

		if (officerUid.IsEmpty() || call.m_sOfficerUid != officerUid)
		{
			messageKey = "#LF-CallOutcome_NotHeld";
			LF_Debug.Warn(FEATURE, string.Format("close of call #%1 by %2 refused: held by '%3'", callId, officerUid, call.m_sOfficerUid));
			return false;
		}

		// The crime's own position first (#2578), then the pay site (the alarm's position or a grid square).
		vector scene = call.m_vPosition;
		float radius = LF_PoliceCallPay.Radius();
		if (!call.m_bHasPosition && !LF_PoliceCallPay.SiteOf(call, scene, radius))
		{
			messageKey = "#LF-CallOutcome_NoScene";
			LF_Debug.Warn(FEATURE, string.Format("close of call #%1 by %2 refused: site '%3' has no position", callId, officerUid, call.m_sSite));
			return false;
		}

		float distance = vector.DistanceXZ(officerPosition, scene);
		if (distance > radius)
		{
			messageKey = "#LF-CallOutcome_TooFar";
			messageArg = Math.Round(radius).ToString();
			LF_Debug.Info(FEATURE, string.Format("close of call #%1 by %2 refused: %3 m from the scene, radius %4", callId, officerUid, Math.Round(distance), radius));
			return false;
		}

		if (!LF_PoliceCalls.Complete(callId, officerUid))
		{
			messageKey = "#LF-CallOutcome_NoCall";
			return false;
		}

		call.m_iOutcome = outcome;
		messageKey = ClosedKey(outcome);
		LF_Debug.Info(FEATURE, string.Format("call #%1 closed %2 by %3", callId, Name(outcome), officerUid));
		return true;
	}
}
