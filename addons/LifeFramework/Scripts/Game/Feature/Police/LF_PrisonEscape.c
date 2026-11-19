//------------------------------------------------------------------------------------------------
//! The search afterwards (#3422, #798). Beat 4 of the prison break (#3421): the police go looking.
//! - A prisoner who walks out in a break window is posted on the wanted board as "Escaped custody"
//!   (OnEscaped, from LF_Jail.NoteEscape): a crime report with a dispatch call, and the escape is
//!   on their record. They stay wanted while they are out.
//! - The member who forced the door has the jailbreak on their record and is wanted (OnOpened,
//!   from LF_PrisonBreak.Start), so an officer can arrest them like any wanted player.
//! - An officer who arrests an escapee (LF_Jail.Arrest) puts them back in the cell: the rest of the
//!   sentence plus the escape term (TermSeconds, from LF_PrisonBreakConfig), the escaped status cleared (LF_Jail.Recapture).
//!
//! No state of its own: the status is the "prisonbreak" section (LF_PrisonBreak), the sentence is the
//! "jail" payload, the reports are the wanted store, so there is nothing new to save.
class LF_PrisonEscape
{
	static const string FEATURE = "PrisonEscape";
	static const string OFFENCE_ESCAPED = "#LF-Offence_EscapedCustody";

	//------------------------------------------------------------------------------------------------
	//! Seconds added to the rest of the sentence when an escapee is arrested again
	//! (LF_PrisonBreakConfig.m_iEscapeTermSeconds; the shipped default when the config is missing).
	static int TermSeconds()
	{
		LF_PrisonBreakConfig config = LF_PrisonBreakConfig.Get();
		if (!config)
			return LF_PrisonBreakConfig.DEFAULT_ESCAPE_TERM_SECONDS;

		return config.EscapeTermSeconds();
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_Jail.NoteEscape once LF_PrisonBreak set the escaped status: posts "Escaped
	//! custody" for the prisoner and keeps them wanted.
	//! \return True when the wanted row was filed.
	static bool OnEscaped(string playerUid, IEntity body)
	{
		if (playerUid.IsEmpty())
			return false;

		RaiseToWanted(playerUid);
		LF_DispatchRecord call = LF_Dispatch.Report(playerUid, body, OFFENCE_ESCAPED, vector.Zero);
		if (!call)
		{
			LF_Debug.Warn(FEATURE, string.Format("escape of %1 not filed: dispatch refused or no listener", playerUid));
			return false;
		}

		LF_Debug.Warn(FEATURE, string.Format("%1 escaped custody: on the wanted board", playerUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_PrisonBreak.Start before it reports the break: makes sure the jailbreak lands
	//! on the opener's record (the crime record listens on the bus) and that they are wanted.
	static void OnOpened(string starterUid)
	{
		if (starterUid.IsEmpty())
			return;

		LF_CrimeRecord.EnsureSubscribed();
		RaiseToWanted(starterUid);
		LF_Debug.Info(FEATURE, string.Format("%1 opened the jail door: jailbreak on the record, wanted", starterUid));
	}

	//------------------------------------------------------------------------------------------------
	//! At least one wanted level, so the person can be arrested and stays on the board.
	protected static void RaiseToWanted(string playerUid)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return;

		LF_PlayerAccount account = accounts.GetAccount(playerUid);
		if (account && account.GetWantedLevel() < 1)
			account.SetWantedLevel(1);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the wanted store holds a report of `offence` for the person.
	static bool HasWantedRow(string playerUid, string offence)
	{
		array<ref LF_WantedReportRecord> reports = LF_WantedReports.GetFor(playerUid);
		if (!reports)
			return false;

		foreach (LF_WantedReportRecord report : reports)
		{
			if (report && report.m_sOffence == offence)
				return true;
		}

		return false;
	}
}
