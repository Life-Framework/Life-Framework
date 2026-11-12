// red-proof: observed 2026-11-12 with `toolscli test --no-build --only LF_Test_CrimeCatalogBackcompat`: setting the murder entry m_iWantedAdd to 2 in Configs/Crime/LF_CrimeCatalog.conf failed `- wanted for #LF-Offence_Murder (expected 3, got 2)` and `- a murder still adds 3 (expected 3, got 2)`. Restored; green.
// tier: LOGIC
//! #685a: the crime catalog (Configs/Crime/LF_CrimeCatalog.conf) gives today's offences the numbers
//! the call sites carried before it: wanted per offence, the ticket form, and a sentence that stays
//! the wanted level's unless a catalog entry asks for more. An id the catalog does not list falls back to
//! one wanted level and does not VME.
class LF_Test_CrimeCatalogBackcompat : LF_Test
{
	protected static const string UID = "crime-catalog-backcompat";

	override string GetName()
	{
		return "crime/catalog-backcompat";
	}

	//------------------------------------------------------------------------------------------------
	protected void ExpectWanted(LF_TestContext ctx, string crimeId, int wanted)
	{
		ctx.Equal(wanted, LF_CrimeCatalog.BaseWanted(crimeId), "wanted for " + crimeId);
		LF_PlayerAccount account = LF_PlayerAccount.Create(UID);
		ctx.Equal(wanted, LF_CrimeCatalog.ApplyWanted(account, UID, crimeId), "an account committing " + crimeId + " ends at that wanted level");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_CrimeCatalogConfig.Reload();
		LF_CrimeCatalog.ResetStatics();
		array<ref LF_CrimeRecordRow> savedRows = LF_CrimeRecord.ExportAll();
		LF_CrimeRecord.ApplyAll(null);

		// What each call site added before the catalog.
		ExpectWanted(ctx, LF_WantedReports.OFFENCE_ROBBERY, 1);
		ExpectWanted(ctx, LF_WantedReports.OFFENCE_VEHICLE_THEFT, 1);
		ExpectWanted(ctx, LF_WantedReports.OFFENCE_CARJACKING, 3);
		ExpectWanted(ctx, LF_WantedReports.OFFENCE_WEAPON_THEFT, 2);
		ExpectWanted(ctx, LF_WantedReports.OFFENCE_BREAK_IN, 1);
		ExpectWanted(ctx, LF_WantedReports.OFFENCE_HOSPITAL, 1);
		ExpectWanted(ctx, LF_WantedReports.OFFENCE_MURDER, 3);
		ExpectWanted(ctx, LF_WantedReports.OFFENCE_CASH_VAN, 1);
		ExpectWanted(ctx, LF_WantedReports.OFFENCE_ARMORY, 1);
		ExpectWanted(ctx, LF_WantedReports.OFFENCE_SHOPLIFTING, 1);
		ExpectWanted(ctx, LF_FineTickets.OFFENCE_UNPAID, 1);
		// Reported with no wanted level of their own.
		ExpectWanted(ctx, LF_WantedReports.OFFENCE_SCANNER, 0);
		ExpectWanted(ctx, LF_WantedReports.OFFENCE_LOCKPICK, 0);
		ExpectWanted(ctx, LF_WantedReports.OFFENCE_CHOP, 0);
		ExpectWanted(ctx, LF_WantedReports.OFFENCE_JAILBREAK, 0);
		ExpectWanted(ctx, LF_WantedReports.OFFENCE_CHAIN_GOODS, 0);
		ctx.Equal(3, LF_Murder.WantedAmount(), "a murder still adds 3");

		// The ticket form: five offences in the shipped order, fixed amounts.
		array<ref LF_TicketOffence> offences = {};
		LF_PoliceConfig.TicketOffences(offences);
		ctx.Equal(5, offences.Count(), "the ticket form lists five offences");
		if (offences.Count() == 5)
		{
			ctx.EqualStr("#LF-Ticket_OffenceSpeeding", offences[0].m_sName, "form line 1");
			ctx.Equal(200, offences[0].m_iAmount, "speeding is 200");
			ctx.Equal(150, offences[1].m_iAmount, "disturbance is 150");
			ctx.Equal(500, offences[2].m_iAmount, "reckless driving is 500");
			ctx.Equal(800, offences[3].m_iAmount, "brandishing is 800");
			ctx.EqualStr("#LF-Ticket_OffenceCrash", offences[4].m_sName, "form line 5");
			ctx.Equal(1500, offences[4].m_iAmount, "a crash is 1500");
		}

		// No catalog entry sets a sentence: the wanted level's stays.
		LF_WantedReports.ResetStatics();
		LF_WantedReports.Add(UID, LF_WantedReports.OFFENCE_ROBBERY, "", "", "", false);
		LF_WantedReports.Add(UID, LF_WantedReports.OFFENCE_MURDER, "", "", "", false);
		ctx.Equal(450, LF_CrimeCatalog.SentenceFor(UID, 450), "the wanted-scaled sentence is unchanged");
		LF_WantedReports.ResetStatics();

		// An id the catalog does not list: one wanted level, a warning, no crash.
		ctx.Equal(1, LF_CrimeCatalog.BaseWanted("test-not-in-catalog"), "an unknown crime adds 1 wanted");
		ctx.True(LF_CrimeCatalog.Lookup("test-not-in-catalog") == null, "an unknown crime has no entry");

		LF_CrimeRecord.ApplyAll(savedRows);
		LF_CrimeCatalog.ResetStatics();
	}
}
