// red-proof: `tools\cli test --no-build --red LF_Test_NewspaperEdition` printed RED PROOF OK (2026-11-23): with the hook on
// the facts are never noted, and the arrest, reward, opening, price and robbery assertions fail.

[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_NewspaperEdition : LF_Test
{
	static const int NOW = 1800000000; // lf-fixed: a frozen clock mid-week
	static const string OPEN_CASE_NAME = "Vic Opencase";
	static const string HIDDEN_NAME = "Hal Hiddenrecord";
	static const string CROOK_NAME = "Rex Robber";
	static const string SECRET_PLACE = "Old Still Barn";

	override string GetName()
	{
		return "newspaper/edition";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_NewspaperEdition.ForgetFacts();
		LF_NewspaperEdition.SetClock(NOW);

		ExpectConfig(ctx);
		ExpectBusFacts(ctx);
		ExpectFrontPage(ctx);
		ExpectWire(ctx);

		LF_NewspaperEdition.ForgetFacts();
		LF_Newspaper.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void ExpectConfig(LF_TestContext ctx)
	{
		LF_NewspaperSectionsConfig config = LF_NewspaperSectionsConfig.Get();
		ctx.True(config.m_aSections.Count() >= 5, "the sections config loads its five sections");
	}

	//------------------------------------------------------------------------------------------------
	//! The bus events the log reads become facts; an open case and a hidden record never do.
	protected void ExpectBusFacts(LF_TestContext ctx)
	{
		LF_NewspaperSectionsConfig config = LF_NewspaperSectionsConfig.Get();

		ctx.NotNull(LF_NewspaperEdition.FactFor(LF_OnArrested.Create("test-courier-a", 720, 2), config), "an arrest is a fact");
		ctx.NotNull(LF_NewspaperEdition.FactFor(LF_OnMarketPriceMoved.Create("{00000000DEADBEEF}Prefabs/Test/Apple.et", 12, 9), config), "a price move is a fact");

		LF_OnMoneyMoved reward = LF_OnMoneyMoved.CreateFull("test-courier-b", "cash", 150, true, "police-call", 150, "");
		LF_NewspaperFact rewardFact = LF_NewspaperEdition.FactFor(reward, config);
		ctx.True(rewardFact && rewardFact.m_eKind == LF_ENewspaperFact.REWARD, "a call reward paid is a reward fact");

		LF_OnMoneyMoved plot = LF_OnMoneyMoved.CreateFull("test-courier-c", "bank", 4000, false, "property_buy", 100, "");
		LF_NewspaperFact plotFact = LF_NewspaperEdition.FactFor(plot, config);
		ctx.True(plotFact && plotFact.m_eKind == LF_ENewspaperFact.OPENING, "a business plot bought is an opening fact");

		LF_OnMoneyMoved wage = LF_OnMoneyMoved.CreateFull("test-courier-b", "cash", 90, true, "job-pay", 240, "");
		ctx.True(!LF_NewspaperEdition.FactFor(wage, config), "a wage is private, not a fact");

		LF_NewspaperFact crime = LF_NewspaperEdition.FactFor(LF_OnCrimeCommitted.Create("test-courier-suspect", LF_WantedReports.OFFENCE_ROBBERY), config);
		ctx.True(crime && crime.m_sName.IsEmpty() && crime.m_sKey != "test-courier-suspect", "a crime fact keeps the offence and never who did it");

		ctx.True(!LF_NewspaperEdition.FactFor(LF_OnWantedReportFiled.Create("test-courier-suspect", 1, SECRET_PLACE), config), "a wanted report (an open case) is never a fact");
		ctx.True(!LF_NewspaperEdition.FactFor(new LF_OnSuspectIdentified(), config), "an identified suspect (an open case) is never a fact");
		ctx.True(!LF_NewspaperEdition.FactFor(new LF_OnFenceSale(), config), "a fence sale (a hidden record) is never a fact");
		ctx.True(!LF_NewspaperEdition.FactFor(new LF_OnHideoutLeaseChanged(), config), "a hideout lease (a hidden record) is never a fact");
	}

	//------------------------------------------------------------------------------------------------
	//! The week's page lists the public facts with names only where N31 allows them.
	protected void ExpectFrontPage(LF_TestContext ctx)
	{
		if (!RedProof())
		{
			LF_NewspaperEdition.Note(LF_NewspaperEdition.Make(LF_ENewspaperFact.ARREST, "uid-a", "Ada Arrested", "", 12, 0));
			LF_NewspaperEdition.Note(LF_NewspaperEdition.Make(LF_ENewspaperFact.REWARD, "uid-b", "Bea Officer", "police-call", 150, 0));
			LF_NewspaperEdition.Note(LF_NewspaperEdition.Make(LF_ENewspaperFact.REWARD, "uid-b", "Bea Officer", "police-call", 100, 0));
			LF_NewspaperEdition.Note(LF_NewspaperEdition.Make(LF_ENewspaperFact.OPENING, "uid-c", "Cal Baker", "property_buy", 4000, 0));
			LF_NewspaperEdition.Note(LF_NewspaperEdition.Make(LF_ENewspaperFact.MARKET, "apple", "", "Apples", 12, 10));
			LF_NewspaperEdition.Note(LF_NewspaperEdition.Make(LF_ENewspaperFact.MARKET, "apple", "", "Apples", 10, 9));
			LF_NewspaperEdition.Note(LF_NewspaperEdition.Make(LF_ENewspaperFact.CRIME, LF_WantedReports.OFFENCE_ROBBERY, CROOK_NAME, LF_WantedReports.OFFENCE_ROBBERY, 1, 0));
		}

		// Facts that must never print, even if they reached the log.
		LF_NewspaperFact openCase = LF_NewspaperEdition.Make(LF_ENewspaperFact.OPEN_CASE, "uid-v", OPEN_CASE_NAME, SECRET_PLACE, 1, 0);
		openCase.m_sName = OPEN_CASE_NAME;
		LF_NewspaperEdition.Note(openCase);
		LF_NewspaperFact hidden = LF_NewspaperEdition.Make(LF_ENewspaperFact.HIDDEN, "uid-h", HIDDEN_NAME, SECRET_PLACE, 3, 0);
		hidden.m_sName = HIDDEN_NAME;
		LF_NewspaperEdition.Note(hidden);

		// Last week's arrest is old news.
		LF_NewspaperEdition.SetClock(NOW - LF_NewspaperEdition.WEEK_SECONDS);
		LF_NewspaperFact old = LF_NewspaperEdition.Make(LF_ENewspaperFact.ARREST, "uid-o", "Olly Oldnews", "", 5, 0);
		LF_NewspaperEdition.SetClock(NOW);
		LF_NewspaperEdition.Note(old);

		// A pack row that tries to print open cases with names prints nothing.
		LF_NewspaperSectionsConfig config = new LF_NewspaperSectionsConfig();
		config.m_aSections = {};
		foreach (LF_NewspaperSectionDef def : LF_NewspaperSectionsConfig.Get().m_aSections)
			config.m_aSections.Insert(def);

		LF_NewspaperSectionDef rogue = new LF_NewspaperSectionDef();
		rogue.m_sId = "rogue";
		rogue.m_eKind = LF_ENewspaperFact.OPEN_CASE;
		rogue.m_sHeadline = "Open cases";
		rogue.m_sLine = "%1 at %2";
		rogue.m_bNames = true;
		rogue.m_iMaxLines = 5;
		config.m_aSections.Insert(rogue);

		LF_NewspaperIssue issue = LF_NewspaperEdition.Build(LF_NewspaperEdition.Facts(), LF_NewspaperEdition.WeekOf(NOW), config);
		string page = LF_Newspaper.PageText(LF_Newspaper.BuildParts(issue, LF_NewspaperEditionStyle.Newsprint()));
		Print("[LF_Test_NewspaperEdition] page:\n" + page);

		ctx.True(page.Contains("Ada Arrested"), "the page names the arrested character");
		ctx.True(page.Contains("12 minutes"), "the page prints the sentence");
		ctx.True(page.Contains("Bea Officer") && page.Contains("$250"), "the page names the rewarded officer with the week's total");
		ctx.True(page.Contains("Cal Baker") && page.Contains("$4,000"), "the page names the new business owner");
		ctx.True(page.Contains("Apples") && page.Contains("$12") && page.Contains("$9"), "the page prints the week's price move, first to last");
		ctx.True(page.Contains(LF_Terms.Text(LF_WantedReports.OFFENCE_ROBBERY)), "the page prints the robbery");
		ctx.False(page.Contains(CROOK_NAME), "the robbery is printed with no name");
		ctx.False(page.Contains(OPEN_CASE_NAME), "an open case never prints");
		ctx.False(page.Contains(HIDDEN_NAME), "a hidden record never prints");
		ctx.False(page.Contains(SECRET_PLACE), "an open case's place never prints");
		ctx.False(page.Contains("Open cases"), "a section of a hidden kind never prints");
		ctx.False(page.Contains("Olly Oldnews"), "last week's facts are left off");
	}

	//------------------------------------------------------------------------------------------------
	//! What the server sends is what the client prints.
	protected void ExpectWire(LF_TestContext ctx)
	{
		LF_NewspaperIssue issue = LF_NewspaperEdition.Build(LF_NewspaperEdition.Facts(), LF_NewspaperEdition.WeekOf(NOW), LF_NewspaperSectionsConfig.Get());
		LF_NewspaperIssue back = LF_NewspaperEdition.Decode(LF_NewspaperEdition.Encode(issue));
		ctx.True(back && back.m_aStories.Count() == issue.m_aStories.Count(), "the edition survives the wire");
		if (back && back.m_aStories.Count() > 0 && issue.m_aStories.Count() > 0)
			ctx.EqualStr(issue.m_aStories[0].m_sBody, back.m_aStories[0].m_sBody, "a story's words survive the wire");
	}
}
