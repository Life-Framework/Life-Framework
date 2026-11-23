// red-proof: 2026-11-23 `tools/cli test --no-build --red LF_Test_NewspaperPersist`: RED PROOF OK; "the arrest comes back with its name", "the crime comes back, unnamed" (the section read skipped). Green with --only.
//! The Courier's save section (WO-W2.3, #4464): an edition is built, saved, the statics cleared and the
//! section read back; the edition number, the dateline and the front page come back equal, loading twice
//! ends in the same log, and the saved facts keep #4303's name rule (a name only for an arrest, a reward or
//! an opening; never on a crime; an open case or a hidden record never saved). A save from before the
//! section takes OnAbsent() and the builder prints the current week. Restart: `wt e2e` brings the e2e
//! edition back.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_NewspaperPersist : LF_Test
{
	static const int NOW = 1800000000; // lf-fixed: a frozen clock mid-week, above 2^24 so float rounding would show
	static const string ARRESTED = "Ada Persisted";
	static const string CROOK = "Rex Leakedname";
	static const string OPEN_CASE = "Vic Opencase";
	static const string CROOK_UID = "uid-crook-secret";
	static const string E2E_ARRESTED = "Eve Restarted";
	static const string E2E_CROOK = "Rob Neverprinted";
	static const string E2E_ARREST_KEY = "e2e-courier-arrest";

	override string GetName()
	{
		return "newspaper/persist";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

#ifdef LF_E2E_LOAD_BOOT
		if (LF_E2E.RequiresSection(LF_NewspaperSavePayload.NAME))
			ExpectRestarted(ctx);

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_NewspaperSavePayload.NAME), "newspaper is a registered save section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_NewspaperSavePayload.NAME), "newspaper is not a legacy positional payload");

		// Build an edition: an arrest with a name, a crime someone tried to name, an open case slipped into
		// the log by hand (the bus never adds one; the section must still refuse it).
		LF_NewspaperEdition.ForgetFacts();
		LF_NewspaperBackIssues.ResetStatics();
		LF_Newspaper.ResetStatics();
		LF_NewspaperEdition.SetClock(NOW);
		int week = LF_NewspaperEdition.WeekOf(NOW);
		LF_NewspaperEdition.Note(LF_NewspaperEdition.Make(LF_ENewspaperFact.ARREST, "uid-persist-a", ARRESTED, "", 12, 0));
		LF_NewspaperEdition.Note(LF_NewspaperEdition.Make(LF_ENewspaperFact.REWARD, "uid-persist-b", "Bea Rewarded", "police-call", 150, 0));
		LF_NewspaperEdition.Note(LF_NewspaperEdition.Make(LF_ENewspaperFact.CRIME, LF_WantedReports.OFFENCE_ROBBERY, "", LF_WantedReports.OFFENCE_ROBBERY, 1, 0));
		LF_NewspaperEdition.Facts().Insert(RawFact(LF_ENewspaperFact.CRIME, CROOK, LF_WantedReports.OFFENCE_ROBBERY));
		LF_NewspaperEdition.Facts().Insert(RawFact(LF_ENewspaperFact.OPEN_CASE, OPEN_CASE, "uid-open"));
		LF_NewspaperIssue printed = LF_NewspaperEdition.Publish();
		ctx.True(printed && !printed.m_aStories.IsEmpty(), "the edition prints its stories");
		string printedWire = LF_NewspaperEdition.Encode(printed);
		string printedDateline;
		if (printed)
			printedDateline = printed.m_sDateline;

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_NewspaperSavePayload.NAME), "the save carries the newspaper section");
		ctx.True(saved.Contains(ARRESTED), "the arrest keeps its name in the save");
		ctx.False(saved.Contains(CROOK), "a crime's name is never written to the save");
		ctx.False(saved.Contains(OPEN_CASE), "an open case is never written to the save");

		// Restart: the statics are gone, the clock runs, the paper is the placeholder.
		LF_NewspaperEdition.ForgetFacts();
		LF_NewspaperBackIssues.ResetStatics();
		LF_Newspaper.ResetStatics();
		ctx.True(LF_NewspaperEdition.Facts().IsEmpty(), "the cleared session holds no fact");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(loadContext, ctx);

		LF_NewspaperIssue current = LF_Newspaper.Current();
		ctx.Equal(week, LF_NewspaperSavePayload.EditionWeek(), "the edition number is back");
		ctx.EqualStr(printedDateline, current.m_sDateline, "the dateline reads the same after the load");
		ctx.EqualStr(printedWire, LF_NewspaperEdition.Encode(current), "the front page reads the same after the load");
		ctx.NotNull(LF_NewspaperBackIssues.Issue(week), "the edition is filed with the back issues");
		ctx.Equal(4, LF_NewspaperEdition.Facts().Count(), "the four printable facts are back, the open case is not");
		ExpectNameRule(ctx, ARRESTED, CROOK);
		ctx.EqualStr(printedWire, LF_NewspaperEdition.Encode(LF_NewspaperEdition.Build(LF_NewspaperEdition.Facts(), week, LF_NewspaperSectionsConfig.Get())), "the next rebuild from the loaded facts prints the same page");

		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		ReadSection(again, ctx);
		ctx.Equal(4, LF_NewspaperEdition.Facts().Count(), "loading twice keeps the same log, not double");
		ctx.Equal(1, LF_NewspaperBackIssues.Rows().Count(), "loading twice keeps one edition per week");

		// A crime keyed by a uid (a bug the bus never makes) is saved and loaded under its offence.
		LF_NewspaperFact keyed = RawFact(LF_ENewspaperFact.CRIME, CROOK, LF_WantedReports.OFFENCE_ROBBERY);
		keyed.m_sKey = CROOK_UID;
		LF_NewspaperFactRow keyedRow = LF_NewspaperSavePayload.ToRow(keyed);
		ctx.True(keyedRow && keyedRow.m_sKey != CROOK_UID && keyedRow.m_sName.IsEmpty(), "a crime row never keeps who did it, as a name or a key");
		LF_NewspaperFactRow edited = new LF_NewspaperFactRow();
		edited.m_iKind = LF_ENewspaperFact.CRIME;
		edited.m_sName = CROOK;
		edited.m_sKey = CROOK_UID;
		edited.m_sDetail = LF_WantedReports.OFFENCE_ROBBERY;
		LF_NewspaperFact loaded = LF_NewspaperSavePayload.FromRow(edited);
		ctx.True(loaded && loaded.m_sKey != CROOK_UID && loaded.m_sName.IsEmpty(), "an edited save cannot put a name or a uid on a crime");
		edited.m_iKind = LF_ENewspaperFact.OPEN_CASE;
		ctx.True(!LF_NewspaperSavePayload.FromRow(edited), "an edited save cannot load an open case");

		// A save from before the section: no newspaper section, so OnAbsent() and the paper starts fresh.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_NewspaperSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		ctx.False(oldContext.SaveToString().Contains(LF_SaveSections.OBJECT_PREFIX + LF_NewspaperSavePayload.NAME), "the older save has no newspaper section");
		LF_NewspaperEdition.ForgetFacts();
		LF_NewspaperBackIssues.ResetStatics();
		LF_Newspaper.ResetStatics();
		LF_SaveSections.Get(LF_NewspaperSavePayload.NAME).OnAbsent();
		LF_NewspaperEdition.SetClock(NOW);
		LF_NewspaperIssue fresh = LF_NewspaperEdition.Build(LF_NewspaperEdition.Facts(), LF_NewspaperEdition.WeekOf(LF_NewspaperEdition.Now()), LF_NewspaperSectionsConfig.Get());
		ctx.EqualStr(LF_Terms.Text(LF_NewspaperEdition.KEY_DATELINE, LF_NewspaperEdition.WeekNumber(week).ToString()), fresh.m_sDateline, "after an older save the builder prints the current week");
		ctx.EqualStr(LF_Newspaper.KEY_DATELINE, LF_Newspaper.Current().m_sDateline, "after an older save the paper is the placeholder until a fact comes in");

		// Every non-reload run (the e2e save boot among them) leaves the e2e edition in the log, at the real
		// clock so later bus facts of the same week do not drop it.
		LF_NewspaperEdition.ForgetFacts();
		LF_NewspaperBackIssues.ResetStatics();
		LF_Newspaper.ResetStatics();
		LF_NewspaperEdition.Note(LF_NewspaperEdition.Make(LF_ENewspaperFact.ARREST, E2E_ARREST_KEY, E2E_ARRESTED, "", 12, 0));
		LF_NewspaperEdition.Facts().Insert(RawFact(LF_ENewspaperFact.CRIME, E2E_CROOK, LF_WantedReports.OFFENCE_ROBBERY));
		LF_NewspaperEdition.Facts().Insert(RawFact(LF_ENewspaperFact.HIDDEN, E2E_CROOK, "uid-hidden"));
		LF_NewspaperEdition.Publish();
	}

	//------------------------------------------------------------------------------------------------
	//! Pass 2 of `wt e2e`: the edition pass 1 left is back after a real restart, names as N31 allows.
	protected void ExpectRestarted(LF_TestContext ctx)
	{
		int week = LF_NewspaperSavePayload.EditionWeek();
		ctx.True(week > 0, "the e2e edition's week survives the restart");
		LF_NewspaperIssue filed = LF_NewspaperBackIssues.Issue(week);
		ctx.NotNull(filed, "the e2e edition survives the restart");
		if (filed)
			ctx.EqualStr(LF_NewspaperEdition.Encode(filed), LF_NewspaperEdition.Encode(LF_Newspaper.Current()), "the paper on sale after the restart is the saved edition");

		ctx.True(LF_Newspaper.Current().m_sDateline != LF_Newspaper.KEY_DATELINE, "the paper after the restart is not the placeholder");
		ExpectNameRule(ctx, E2E_ARRESTED, E2E_CROOK);
		foreach (LF_NewspaperFact fact : LF_NewspaperEdition.Facts())
			ctx.True(fact.m_eKind != LF_ENewspaperFact.HIDDEN, "a hidden record never survives the restart");
	}

	//------------------------------------------------------------------------------------------------
	//! The arrest kept its name; the crime came back with none; no open case or hidden record came back.
	protected void ExpectNameRule(LF_TestContext ctx, string arrested, string crook)
	{
		bool arrestNamed;
		bool crimeSeen;
		foreach (LF_NewspaperFact fact : LF_NewspaperEdition.Facts())
		{
			if (!fact)
				continue;

			if (fact.m_eKind == LF_ENewspaperFact.ARREST && fact.m_sName == arrested)
				arrestNamed = true;

			if (fact.m_eKind == LF_ENewspaperFact.CRIME)
			{
				crimeSeen = true;
				ctx.True(fact.m_sName.IsEmpty(), "a crime fact comes back with no name");
			}

			ctx.True(fact.m_eKind != LF_ENewspaperFact.OPEN_CASE && fact.m_eKind != LF_ENewspaperFact.HIDDEN, "an open case or hidden record never comes back");
			ctx.True(fact.m_sName != crook, "the crook's name never comes back");
		}

		ctx.True(arrestNamed, "the arrest comes back with its name");
		ctx.True(crimeSeen, "the crime comes back, unnamed");
	}

	//------------------------------------------------------------------------------------------------
	//! A fact put in the log by hand, past Make's name rule: what a bug or an edited save could hold.
	protected static LF_NewspaperFact RawFact(LF_ENewspaperFact kind, string name, string key)
	{
		LF_NewspaperFact fact = new LF_NewspaperFact();
		fact.m_eKind = kind;
		fact.m_sName = name;
		fact.m_sKey = key;
		fact.m_sDetail = key;
		fact.m_iAmount = 1;
		fact.m_iUnixTime = LF_NewspaperEdition.Now();
		return fact;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ReadSection(notnull JsonLoadContext context, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_NewspaperSavePayload.NAME);
		if (!section)
			return;

		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_NewspaperSavePayload.NAME), "the newspaper section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the newspaper section reads");
		context.EndObject();
	}
}
