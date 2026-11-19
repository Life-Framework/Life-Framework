//------------------------------------------------------------------------------------------------
//! Which police actions need a qualification (#3427, slice Q4 of #1035). The quick vehicle search
//! stays open to every officer; the full search (the hidden compartment too) and opening a case on
//! the case board need the `detective` unlock, which the detective course grants.
//!
//! Server rule. The action classes (LF_VehicleSearchFullAction, LF_CaseBoardPinAction) ask `Allows`
//! on the server before they act and tell the player when it says no; the qualifications are server
//! state (LF_Qualifications), so there is no client-side offer check.
class LF_QualificationGate
{
	static const string UNLOCK_DETECTIVE = "detective";
	static const string MSG_DETECTIVE_WORK = "#LF-Qual_DetectiveWork";
	static const string TITLE = "#LF-Training_Title";

	static const string ACT_QUICK_SEARCH = "quick-search";
	static const string ACT_FULL_SEARCH = "full-search";
	static const string ACT_OPEN_CASE = "open-case";

	//------------------------------------------------------------------------------------------------
	//! The unlock id an action needs; empty when every officer may do it.
	static string UnlockFor(string action)
	{
		if (action == ACT_FULL_SEARCH || action == ACT_OPEN_CASE)
			return UNLOCK_DETECTIVE;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the character (record key) may do the action. An action that needs no
	//! unlock is always allowed; one that does needs a held course granting it. A refusal is logged.
	static bool Allows(string character, string action)
	{
		string unlock = UnlockFor(action);
		if (unlock.IsEmpty())
			return true;

		if (LF_Qualifications.Has(character, unlock))
			return true;

		LF_Debug.Info(LF_QualificationConfig.FEATURE, string.Format("gate: %1 refused '%2': needs '%3'", character, action, unlock));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether the officer in `user` may do the action. A refusal is toasted to them
	//! ("That's detective work. The training desk runs the course.").
	static bool AllowsUser(IEntity user, string action)
	{
		if (UnlockFor(action).IsEmpty())
			return true;

		if (!user)
			return false;

		if (Allows(CharacterKey(user), action))
			return true;

		LF_Utils.NotifyEntity(user, MSG_DETECTIVE_WORK, TITLE, 4.0);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The record key the held courses are stored under, from the body; empty when the body has no
	//! account or character (then nothing is held).
	static string CharacterKey(IEntity user)
	{
		if (!user)
			return "";

		string uid = LF_PoliceRanks.ResolveUid(user);
		LF_PlayerAccountManager manager = LF_PlayerAccountManager.GetInstance();
		if (!manager || uid.IsEmpty())
			return "";

		LF_PlayerAccount account = manager.GetAccount(uid);
		if (!account)
			return "";

		return account.GetActiveCharacterKey();
	}
}
