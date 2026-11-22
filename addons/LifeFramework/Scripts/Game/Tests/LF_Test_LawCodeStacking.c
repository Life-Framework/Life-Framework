// red-proof: observed 2026-11-22 with `tools/cli i test --no-build --red LF_Test_LawCodeStacking`: RED-OK, RedProof() leaves the stacking groups and jail-only flag unset and `a jail-only code adds nothing to the fine total (expected 400, got 1300)` and `sentences stack by group too (expected 1800, got 2100)` fail. Green with the hook off.
//! #4202: law-book codes carry a section, a stacking group and a jail-only flag. Two codes in one group
//! stack to the higher; codes in different groups (or with no group) add; a jail-only code refuses a
//! fine but still adds its time; the owner's `crime` section sets the three fields and refuses a bad
//! section; every code of the shipped catalog reads under one of the four sections.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_LawCodeStacking : LF_Test
{
	protected static const string GROUP_SPEED = "test-speed";
	protected static const string GROUP_THEFT = "test-theft";

	override string GetName()
	{
		return "law/code-stacking";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_CrimeEntry Code(string id, int fine, int sentenceSeconds, string group, bool jailOnly)
	{
		LF_CrimeEntry code = LF_CrimeEntry.Make(id, 0, fine);
		code.m_iSentenceSeconds = sentenceSeconds;
		if (!RedProof())
		{
			code.m_sStackGroup = group;
			code.m_bJailOnly = jailOnly;
		}

		return code;
	}

	//------------------------------------------------------------------------------------------------
	protected array<LF_CrimeEntry> Sheet(LF_CrimeEntry a, LF_CrimeEntry b = null, LF_CrimeEntry c = null, LF_CrimeEntry d = null)
	{
		array<LF_CrimeEntry> sheet = {};
		sheet.Insert(a);
		if (b)
			sheet.Insert(b);
		if (c)
			sheet.Insert(c);
		if (d)
			sheet.Insert(d);
		return sheet;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_CrimeCatalogConfig Merge(string sectionJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"crime\":" + sectionJson + "}", "test");
		LF_CrimeCatalogConfig config = new LF_CrimeCatalogConfig();
		config.m_aCrimes = {};
		config.m_aCrimes.Insert(LF_CrimeEntry.Make("test-code-a", 1, 100));
		config.ApplyLayers();
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// A fresh code is criminal, groupless and finable.
		LF_CrimeEntry fresh = LF_CrimeEntry.Make("test-fresh", 1, 50);
		ctx.True(fresh.m_eLawSection == LF_LawSection.CRIMINAL, "a new code is in the criminal section");
		ctx.True(fresh.m_sStackGroup.IsEmpty(), "a new code has no stacking group");
		ctx.True(!fresh.m_bJailOnly, "a new code is not jail only");

		// Two codes in one group stack to the higher; the order does not matter.
		LF_CrimeEntry speedLow = Code("test-speed-low", 60, 0, GROUP_SPEED, false);
		LF_CrimeEntry speedHigh = Code("test-speed-high", 200, 0, GROUP_SPEED, false);
		array<LF_CrimeEntry> sheet = Sheet(speedLow, speedHigh);
		ctx.Equal(200, LF_LawCodeRules.TotalFine(sheet), "same group: the higher fine counts, not the sum");
		sheet = Sheet(speedHigh, speedLow);
		ctx.Equal(200, LF_LawCodeRules.TotalFine(sheet), "same group: the order of the charges does not change the total");
		array<LF_CrimeEntry> counted = {};
		LF_LawCodeRules.Counted(sheet, true, counted);
		ctx.Equal(1, counted.Count(), "same group: one code counts, the other is merged");
		if (counted.Count() == 1)
			ctx.True(counted[0] == speedHigh, "same group: the code that counts is the higher one");
		ctx.True(LF_LawCodeRules.SameGroup(speedLow, speedHigh), "two codes of one group stack");

		// Codes in different groups add; a code with no group always adds.
		LF_CrimeEntry theft = Code("test-theft", 400, 0, GROUP_THEFT, false);
		LF_CrimeEntry loose = Code("test-loose", 75, 0, "", false);
		sheet = Sheet(speedLow, speedHigh, theft, loose);
		ctx.Equal(675, LF_LawCodeRules.TotalFine(sheet), "different groups add: 200 + 400 and the ungrouped 75");
		ctx.True(!LF_LawCodeRules.SameGroup(speedHigh, theft), "codes of different groups do not stack");
		ctx.True(!LF_LawCodeRules.SameGroup(loose, loose), "a code with no group stacks with nothing, not even itself");
		LF_CrimeEntry loose2 = Code("test-loose-2", 25, 0, "", false);
		sheet = Sheet(loose, loose2);
		ctx.Equal(100, LF_LawCodeRules.TotalFine(sheet), "two groupless codes both add");

		// A jail-only code refuses a fine but its time counts, stacked by group.
		LF_CrimeEntry robbery = Code("test-robbery", 900, 1200, "", true);
		ctx.True(!LF_LawCodeRules.CanFine(robbery), "a jail-only code refuses a fine");
		ctx.True(LF_LawCodeRules.CanFine(theft), "a finable code accepts a fine");
		sheet = Sheet(robbery, theft);
		ctx.Equal(400, LF_LawCodeRules.TotalFine(sheet), "a jail-only code adds nothing to the fine total");
		ctx.Equal(1200, LF_LawCodeRules.TotalSentenceSeconds(sheet), "a jail-only code's time still counts");
		LF_CrimeEntry brawlShort = Code("test-brawl-short", 0, 300, GROUP_THEFT, false);
		LF_CrimeEntry brawlLong = Code("test-brawl-long", 0, 600, GROUP_THEFT, true);
		sheet = Sheet(brawlShort, brawlLong, robbery);
		ctx.Equal(1800, LF_LawCodeRules.TotalSentenceSeconds(sheet), "sentences stack by group too: 600 for the group plus the robbery's 1200");
		ctx.Equal(0, LF_LawCodeRules.TotalFine(sheet), "every code on the sheet is jail only or has no fine");
		array<LF_CrimeEntry> empty = {};
		ctx.Equal(0, LF_LawCodeRules.TotalFine(empty), "an empty sheet totals nothing");

		// The owner's section sets the three fields; a bad section is refused and the shipped one stays.
		LF_CrimeCatalogConfig config = Merge("{\"crimes\":{\"test-code-a\":{\"lawSection\":\"road\",\"stackGroup\":\"speed\",\"jailOnly\":true}}}");
		LF_CrimeEntry ownerCode = config.Find("test-code-a");
		ctx.NotNull(ownerCode, "the owner's record finds the code");
		if (ownerCode)
		{
			ctx.True(ownerCode.m_eLawSection == LF_LawSection.ROAD, "the owner's section applies");
			ctx.EqualStr("speed", ownerCode.m_sStackGroup, "the owner's stacking group applies");
			ctx.True(ownerCode.m_bJailOnly, "the owner's jail-only flag applies");
			ctx.Equal(100, ownerCode.m_iFine, "a field the owner left out keeps the shipped value");
		}

		config = Merge("{\"crimes\":{\"test-code-a\":{\"lawSection\":\"tax\",\"stackGroup\":\"x\"}}}");
		ownerCode = config.Find("test-code-a");
		if (ownerCode)
		{
			ctx.True(ownerCode.m_eLawSection == LF_LawSection.CRIMINAL, "an unknown section is refused, the shipped one stays");
			ctx.EqualStr("x", ownerCode.m_sStackGroup, "the valid field of the same record still applies");
		}

		ctx.EqualStr(LF_LawCode.KEY_NARCOTICS, LF_LawCode.SectionKey(LF_LawSection.NARCOTICS), "a section has a stable owner-config key");
		LF_LawSection parsed = LF_LawSection.CRIMINAL;
		ctx.True(LF_LawCode.ParseSection(LF_LawCode.KEY_WEAPONS, parsed) && parsed == LF_LawSection.WEAPONS, "the weapons key parses");
		ctx.True(!LF_LawCode.IsSection(4) && !LF_LawCode.IsSection(-1) && LF_LawCode.IsSection(3), "only 0 to 3 are sections");

		// The shipped catalog: every code has a section the book can list it under.
		LF_ConfigService.Reset();
		LF_CrimeCatalogConfig.Reload();
		LF_CrimeCatalogConfig live = LF_CrimeCatalogConfig.Get();
		ctx.NotNull(live, "the shipped catalog loads");
		if (live && live.m_aCrimes)
		{
			int sectioned;
			foreach (LF_CrimeEntry entry : live.m_aCrimes)
			{
				if (entry && LF_LawCode.IsSection(entry.m_eLawSection))
					sectioned++;
			}

			ctx.Equal(live.m_aCrimes.Count(), sectioned, "every shipped code has a law-book section");
			int listed;
			array<LF_CrimeEntry> inSection = {};
			for (int s = LF_LawSection.CRIMINAL; s <= LF_LawSection.WEAPONS; s++)
			{
				LF_LawSection sec = s;
				LF_LawCode.CollectSection(sec, inSection);
				listed += inSection.Count();
			}

			ctx.Equal(live.m_aCrimes.Count(), listed, "the four sections list every shipped code once");
		}

		LF_ConfigService.Reset();
		LF_CrimeCatalogConfig.Reload();
		LF_CrimeCatalog.ResetStatics();
	}
}
