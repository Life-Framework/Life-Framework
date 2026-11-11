//------------------------------------------------------------------------------------------------
//! What the station clerk accepts on the police application form, and who may approve one.
//! Pure checks shared by the client form (its error line) and the server (which decides).
//! Each Refuse* returns "" to accept or the localization key written on the form.
//------------------------------------------------------------------------------------------------
class LF_PoliceApplicationRules
{
	static const int NAME_MAX_LENGTH = 48;
	static const int WHY_MAX_LENGTH = 240;
	static const int AVAILABILITY_MAX_LENGTH = 80;

	//------------------------------------------------------------------------------------------------
	//! \param name, why, availability Already trimmed.
	static string RefuseForm(string name, string why, string availability)
	{
		if (name.IsEmpty() || why.IsEmpty() || availability.IsEmpty())
			return "#LF-PoliceApp_Error_Fields";

		if (name.Length() > NAME_MAX_LENGTH || why.Length() > WHY_MAX_LENGTH || availability.Length() > AVAILABILITY_MAX_LENGTH)
			return "#LF-PoliceApp_Error_Length";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether a player may file now. One open application per player; a player already on
	//! the police roster has nothing to apply for.
	static string RefuseSubmit(bool onRoster, bool hasOpenApplication)
	{
		if (onRoster)
			return "#LF-PoliceApp_Error_OnRoster";

		if (hasOpenApplication)
			return "#LF-PoliceApp_Error_AlreadyOpen";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: who approves. Only an administrator. A playtest mission skips this on purpose by
	//! approving on submit (LF_MissionHeader.m_bLF_InstantPoliceApproval).
	static bool CanApprove(bool isAdmin)
	{
		return isAdmin;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the characters the file format uses so a field cannot forge a record.
	static string Clean(string text)
	{
		string clean = text.Trim();
		clean.Replace("|", "/");
		clean.Replace("\r", " ");
		clean.Replace("\n", " ");
		clean.Replace("\t", " ");
		return clean;
	}
}
