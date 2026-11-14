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
	//! Server: RefuseSubmit plus the owner's gates (#803). A gate set to 0 hours is off.
	//! \param playtimeSeconds The account's play time over every job (LF_Playtime.Total).
	//! \param lastRejectedAt Unix time of the account's latest rejection, 0 for none.
	//! \param[out] secondsLeft The wait that remains when a gate refuses, else 0.
	static string RefuseSubmitGated(bool onRoster, bool hasOpenApplication, int playtimeSeconds, int minPlaytimeHours, int lastRejectedAt, int cooldownHours, int now, out int secondsLeft)
	{
		secondsLeft = 0;
		string refusal = RefuseSubmit(onRoster, hasOpenApplication);
		if (!refusal.IsEmpty())
			return refusal;

		if (lastRejectedAt > 0 && cooldownHours > 0)
		{
			secondsLeft = lastRejectedAt + cooldownHours * 3600 - now;
			if (secondsLeft > 0)
				return "#LF-PoliceApp_Error_Cooldown";
		}

		secondsLeft = 0;
		if (minPlaytimeHours > 0)
		{
			secondsLeft = minPlaytimeHours * 3600 - playtimeSeconds;
			if (secondsLeft > 0)
				return "#LF-PoliceApp_Error_Playtime";
		}

		secondsLeft = 0;
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Unix time a trial that starts at now ends, 0 when there is no trial.
	static int TrialEndsAt(int now, int trialHours)
	{
		if (trialHours <= 0)
			return 0;

		return now + trialHours * 3600;
	}

	//------------------------------------------------------------------------------------------------
	//! A trial (trialEndsAt > 0) is over once the clock reaches its end.
	static bool TrialExpired(int trialEndsAt, int now)
	{
		return trialEndsAt > 0 && now >= trialEndsAt;
	}

	//------------------------------------------------------------------------------------------------
	//! "2h 15m", "40m" or "<1m": the wait a refusal names. Rounds up to the minute.
	static string FormatDuration(int seconds)
	{
		if (seconds <= 0)
			return "<1m";

		int minutes = (seconds + 59) / 60;
		int hours = minutes / 60;
		minutes = minutes - hours * 60;
		if (hours <= 0)
			return string.Format("%1m", minutes);

		if (minutes <= 0)
			return string.Format("%1h", hours);

		return string.Format("%1h %2m", hours, minutes);
	}

	//------------------------------------------------------------------------------------------------
	//! "!preject UID" -> the applicant's account uid. False for any other shape.
	static bool TryParseReject(string message, out string uid)
	{
		array<string> strs = new array<string>();
		message.Split(" ", strs, true);
		if (strs.Count() != 2 || strs[0] != "!preject")
			return false;

		uid = strs[1];
		return true;
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
