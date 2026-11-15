// LF_PoliceApplicationRelay.c - a filed police application tells the staff channel (#1289).
// One line through LF_StaffWebhook.Send: the last characters of the account id, the applicant's
// playtime and the time. Never the full uid, the name or the answers; staff read those in the
// application list. Server only: the caller is the server-side RPC handler that files the form.

//------------------------------------------------------------------------------------------------
class LF_PoliceApplicationRelay
{
	static const int TAIL_LENGTH = 4;

	//------------------------------------------------------------------------------------------------
	//! Pure: the last TAIL_LENGTH characters of uid (at most half of a short one), so the full id never
	//! leaves the server.
	static string UidTail(string uid)
	{
		int length = uid.Length();
		int keep = length / 2;
		if (keep > TAIL_LENGTH)
			keep = TAIL_LENGTH;

		return uid.Substring(length - keep, keep);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the staff-channel line for one filed application.
	static string Line(string uid, int playtimeSeconds, int now)
	{
		return string.Format("Police application filed: account ...%1, playtime %2, %3 UTC", UidTail(uid), LF_PoliceApplicationRules.FormatDuration(playtimeSeconds), LF_ClipStamp.FormatTime(now));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: files the application through LF_PoliceApplications.SubmitGated and, when it is filed,
	//! sends one line to the staff channel. A refused application sends nothing.
	//! \return "" when filed, else the refusal key.
	static string File(string uid, string name, string why, string availability, bool onRoster, int playtimeSeconds, int minPlaytimeHours, int cooldownHours, int now)
	{
		string refusal = LF_PoliceApplications.SubmitGated(uid, name, why, availability, onRoster, playtimeSeconds, minPlaytimeHours, cooldownHours, now);
		if (!refusal.IsEmpty())
			return refusal;

		LF_StaffWebhook.Send(Line(uid, playtimeSeconds, now));
		return "";
	}
}
