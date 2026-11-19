//------------------------------------------------------------------------------------------------
//! The bus subscriber that books a conviction's licence bans. LF_Jail publishes LF_OnArrested when a
//! sentence starts; it never calls the seizure itself.
class LF_LicenceSeizureSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnArrested arrest = LF_OnArrested.Cast(busEvent);
		if (!arrest)
			return false;

		LF_LicenceSeizure.OnArrest(arrest.m_sPlayerUid);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Beat 3 of the licence chapter (#3432): booking a conviction takes the licences its crime codes
//! map to (LF_LicenceSeizureRules, #3428) through LF_LicenceBans.Ban (#3430), tells the arrested
//! player, and the licence desk refuses the buyer with the days left (LF_LicenceDesk.VisitFor).
//!
//! The crime codes of a conviction are the offences on the suspect's open crime reports
//! (LF_WantedReports) when the sentence starts: the report offence is the same id the seizure rules
//! and the crime catalog carry. A code with no rule changes nothing.
//! An arrest that seizes the firearm permit also drops an open permit application (the fee is not
//! refunded), so the ban cannot be walked around by collecting an application filed before it.
//! Server only. Nothing here persists: the ban rides the "licencebans" section.
class LF_LicenceSeizure
{
	static const string FEATURE = "License";
	static const string KEY_TITLE = "#LF-LicenceDesk_Title";
	static const string KEY_FIREARM = "#LF-LicenceSeized_Firearm";
	static const string KEY_DRIVING = "#LF-LicenceSeized_Driving";
	static const string KEY_GENERIC = "#LF-LicenceSeized_Generic";
	static const string KEY_PLAIN = "#LF-LicenceSeized_Plain";

	protected static ref LF_LicenceSeizureSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes to LF_OnArrested once. Called when the game mode starts.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		// A bus reset on its own (tests do it) drops the subscriber while s_Subscriber stays set.
		if (s_Subscriber && LF_EventBus.IsSubscribed(LF_OnArrested, s_Subscriber))
			return true;

		if (!s_Subscriber)
			s_Subscriber = new LF_LicenceSeizureSubscriber();

		return LF_EventBus.Subscribe(LF_OnArrested, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	//! A sentence started for uid: books the bans its open crime reports map to, on the live body
	//! when the player is connected.
	//! \return How many licences were seized.
	static int OnArrest(string uid)
	{
		if (uid.IsEmpty() || LF_Systems.Refuses(LF_LicenseGate.SYSTEM, "licence seizure"))
			return 0;

		array<string> crimeIds = {};
		CollectCrimeIds(uid, crimeIds);
		LF_PlayerAccount account;
		return Book(uid, crimeIds, LF_PoliceUtils.FindPlayerEntityByUid(uid, account));
	}

	//------------------------------------------------------------------------------------------------
	//! The offence ids on a character's open crime reports.
	static void CollectCrimeIds(string uid, notnull array<string> crimeIds)
	{
		crimeIds.Clear();
		array<ref LF_WantedReportRecord> reports = LF_WantedReports.GetFor(uid);
		if (!reports)
			return;

		foreach (LF_WantedReportRecord report : reports)
		{
			if (report && !report.m_sOffence.IsEmpty())
				crimeIds.Insert(report.m_sOffence);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Books a conviction for these crime codes: every licence they map to is taken from the saved
	//! record and from `body` (the arrested body: pass it, or it keeps the licence until the next
	//! respawn) and banned for the rule's days; the player is told. A conviction with no mapped code
	//! changes nothing.
	//! \return How many licences were seized.
	static int Book(string uid, notnull array<string> crimeIds, IEntity body)
	{
		if (!Replication.IsServer() || uid.IsEmpty())
			return 0;

		array<ref LF_LicenceBan> bans = {};
		LF_LicenceSeizureRules.BansFor(crimeIds, bans);
		if (bans.IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("conviction of %1 maps to no licence ban (%2 offence(s))", uid, crimeIds.Count()));
			return 0;
		}

		bool permitTaken;
		foreach (LF_LicenceBan ban : bans)
		{
			LF_LicenceBans.Ban(uid, ban.m_eLicence, ban.m_iDays, body);
			if (ban.m_eLicence == LF_LicenceDesk.PERMIT)
				permitTaken = true;

			Tell(body, ban);
		}

		if (permitTaken)
			LF_LicenceDesk.RevokeFor(body, uid, "arrest");

		LF_Debug.Info(FEATURE, string.Format("conviction of %1 seized %2 licence(s)", uid, bans.Count()));
		return bans.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tell(IEntity body, notnull LF_LicenceBan ban)
	{
		if (!body)
			return;

		string arg;
		if (ban.m_iDays > 0)
			arg = ban.m_iDays.ToString();

		LF_Utils.NotifyEntity(body, SeizedKey(ban.m_eLicence, ban.m_iDays), KEY_TITLE, 6.0, arg);
	}

	//------------------------------------------------------------------------------------------------
	//! The player's line for a seized licence: "Your firearm permit has been seized for %1 days."
	//! A seizure with no ban has no days to say.
	static string SeizedKey(LF_ELicenseType licence, int days)
	{
		if (days <= 0)
			return KEY_PLAIN;

		if (licence == LF_ELicenseType.FIREARM_PERMIT)
			return KEY_FIREARM;

		if (licence == LF_ELicenseType.DRIVING_LICENCE)
			return KEY_DRIVING;

		return KEY_GENERIC;
	}

	//------------------------------------------------------------------------------------------------
	//! In-game minutes left on a character's ban of a licence; 0 with no ban in force.
	static int MinutesLeft(string uid, LF_ELicenseType licence)
	{
		int end = LF_LicenceBans.EndOf(uid, licence);
		if (end < 0)
			return 0;

		int left = end - LF_LicenceBans.Now();
		if (left < 1)
			return 1;

		return left;
	}

	//------------------------------------------------------------------------------------------------
	//! In-game days left on a ban, rounded up so the last hours still say 1; 0 with no ban in force.
	static int DaysLeft(string uid, LF_ELicenseType licence)
	{
		int minutes = MinutesLeft(uid, licence);
		if (minutes <= 0)
			return 0;

		return (minutes + LF_LicenceDesk.MINUTES_PER_DAY - 1) / LF_LicenceDesk.MINUTES_PER_DAY;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LicenceSeizure's statics (LF_StaticReset finds it by reflection).
class LF_LicenceSeizure_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LicenceSeizure";
	}

	override void Apply()
	{
		LF_LicenceSeizure.ResetStatics();
	}
}
