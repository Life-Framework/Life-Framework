// red-proof: observed 2026-11-12 with `toolscli test --no-build --only LF_Test_CrimeCatalogOwner`: making LF_CrimeCatalog.WantedFor return before the repeat-threshold check failed `- from the threshold on a repeat adds the repeat amount (expected 3, got 1)` and `- ApplyWanted raises the account by the repeat amount (expected 3, got 1)`. Restored; green.
// tier: LOGIC
//! #685a: the owner's `crime` section changes the catalog field by field, keyed by crime id; a number
//! outside the schema range is refused and the shipped value kept; the repeat rule reads the saved
//! crime record count; and a sentence entry lengthens a sentence but never shortens it.
class LF_Test_CrimeCatalogOwner : LF_Test
{
	protected static const string UID = "crime-catalog-owner";
	protected static const string ROBBERY = "#LF-Offence_Robbery";

	override string GetName()
	{
		return "crime/catalog-owner";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_CrimeCatalogConfig MakeCore()
	{
		LF_CrimeCatalogConfig config = new LF_CrimeCatalogConfig();
		config.m_aCrimes = {};
		config.m_aCrimes.Insert(LF_CrimeEntry.Make(ROBBERY, 1, 0));
		config.m_aCrimes.Insert(LF_CrimeEntry.Make(LF_WantedReports.OFFENCE_MURDER, 3, 0));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_CrimeCatalogConfig Merge(string sectionJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"crime\":" + sectionJson + "}", "test");
		LF_CrimeCatalogConfig config = MakeCore();
		config.ApplyLayers();
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Fields merge per id; an id the core lacks is added; unknown keys do not stop the rest.
		LF_CrimeCatalogConfig config = Merge("{\"crimes\":{\"" + ROBBERY + "\":{\"wantedAdd\":2,\"fine\":900,\"bogus\":1},\"test-custom-offence\":{\"wantedAdd\":4}}}");
		LF_CrimeEntry robbery = config.Find(ROBBERY);
		ctx.NotNull(robbery, "the robbery entry is still there");
		if (robbery)
		{
			ctx.Equal(2, robbery.m_iWantedAdd, "the owner's wanted amount applies");
			ctx.Equal(900, robbery.m_iFine, "the owner's fine applies");
			ctx.Equal(0, robbery.m_iSentenceSeconds, "a field the owner left out keeps the shipped value");
		}
		LF_CrimeEntry murder = config.Find(LF_WantedReports.OFFENCE_MURDER);
		ctx.True(murder && murder.m_iWantedAdd == 3, "an offence the owner left out is untouched");
		ctx.Equal(3, config.m_aCrimes.Count(), "an unlisted id adds an offence");
		LF_CrimeEntry custom = config.Find("test-custom-offence");
		ctx.True(custom && custom.m_iWantedAdd == 4, "the added offence has the owner's wanted amount");

		// Range: a value outside the attribute limits is refused, the shipped value stays, the rest applies.
		config = Merge("{\"crimes\":{\"" + ROBBERY + "\":{\"wantedAdd\":9,\"fine\":-5,\"sentenceSeconds\":120}}}");
		robbery = config.Find(ROBBERY);
		if (robbery)
		{
			ctx.Equal(1, robbery.m_iWantedAdd, "wanted above 5 is refused");
			ctx.Equal(0, robbery.m_iFine, "a negative fine is refused");
			ctx.Equal(120, robbery.m_iSentenceSeconds, "the valid field of the same record applies");
		}

		// Through the live singleton: owner values change the wanted an account gets, and the repeat rule counts.
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"crime\":{\"crimes\":{\"" + ROBBERY + "\":{\"wantedAdd\":1,\"repeatThreshold\":2,\"repeatWantedAdd\":2,\"sentenceSeconds\":900}}}}", "test");
		LF_CrimeCatalogConfig.Reload();
		LF_CrimeCatalog.ResetStatics();
		array<ref LF_CrimeRecordRow> savedRows = LF_CrimeRecord.ExportAll();
		LF_CrimeRecord.ApplyAll(null);

		ctx.Equal(1, LF_CrimeCatalog.WantedFor(UID, ROBBERY), "a first robbery adds the base amount");
		LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID, ROBBERY));
		ctx.Equal(1, LF_CrimeCatalog.WantedFor(UID, ROBBERY), "with one completed, below the threshold of 2");
		LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID, ROBBERY));
		ctx.Equal(3, LF_CrimeCatalog.WantedFor(UID, ROBBERY), "from the threshold on a repeat adds the repeat amount");
		ctx.Equal(1, LF_CrimeCatalog.WantedFor("crime-catalog-owner-other", ROBBERY), "another account has its own count");

		LF_PlayerAccount account = LF_PlayerAccount.Create(UID);
		ctx.Equal(3, LF_CrimeCatalog.ApplyWanted(account, UID, ROBBERY), "ApplyWanted raises the account by the repeat amount");

		LF_WantedReports.ResetStatics();
		LF_WantedReports.Add(UID, ROBBERY, "", "", "", false);
		ctx.Equal(900, LF_CrimeCatalog.SentenceFor(UID, 300), "a catalog sentence longer than the scaled one applies");
		ctx.Equal(1200, LF_CrimeCatalog.SentenceFor(UID, 1200), "a shorter catalog sentence never shortens it");
		LF_WantedReports.ResetStatics();

		LF_CrimeRecord.ApplyAll(savedRows);
		LF_ConfigService.Reset();
		LF_CrimeCatalogConfig.Reload();
		LF_CrimeCatalog.ResetStatics();
	}
}
