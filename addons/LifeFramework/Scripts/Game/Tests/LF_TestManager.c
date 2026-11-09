enum LF_TestSuiteMode
{
	ALL,
	FAST,
	PERSISTENCE
}

class LF_TestManager
{
	protected static ref LF_TestManager s_Instance;
	protected ref array<ref LF_Test> m_aTests = {};
	protected ref array<string> m_aOnly = {};

	//------------------------------------------------------------------------------------------------
	static LF_TestManager GetInstance()
	{
		if (!s_Instance)
			s_Instance = new LF_TestManager();

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void Register(LF_Test test, LF_TestTier tier)
	{
		test.SetTier(tier);
		m_aTests.Insert(test);
	}

	//------------------------------------------------------------------------------------------------
	//! Restricts the next run to these tests, matched by class name or GetName().
	//! Set by LF_TestRunnerComponent for `tools\cli test --only`. The run then
	//! ignores tiers, reports tier=only, and fails every name that matched nothing.
	void SetOnly(array<string> names)
	{
		m_aOnly.Clear();
		foreach (string raw : names)
		{
			string name = raw.Trim();
			if (!name.IsEmpty() && m_aOnly.Find(name) == -1)
				m_aOnly.Insert(name);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool MatchesOnly(LF_Test test, array<string> matched)
	{
		string className = test.ClassName();
		string testName = test.GetName();
		bool hit = false;
		foreach (string want : m_aOnly)
		{
			if (want != className && want != testName)
				continue;

			hit = true;
			if (matched.Find(want) == -1)
				matched.Insert(want);
		}

		return hit;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs every registered test (LOGIC only when fast) and fills reportXml
	//! with a JUnit-style report. Prints [LFTEST] markers for the CLI.
	//! A test that throws is fatal: EnforceScript has no exception handling, so
	//! the whole suite dies with it. Tests must not deref null.
	//! \return the number of failed tests.
	int RunAll(bool fast, out string reportXml)
	{
		LF_TestSuiteMode mode = LF_TestSuiteMode.ALL;
		if (fast)
			mode = LF_TestSuiteMode.FAST;

		return RunSuite(mode, reportXml);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs only the tests of one suite mode. FAST runs LOGIC-tier tests only,
	//! PERSISTENCE runs PERSISTENCE-tier tests only, ALL runs every test.
	//! Markers and XML shape match RunAll.
	//! \return the number of failed tests.
	int RunSuite(LF_TestSuiteMode mode, out string reportXml)
	{
		CollectTests();

		string tierName = "all";
		if (mode == LF_TestSuiteMode.FAST)
			tierName = "fast";
		else if (mode == LF_TestSuiteMode.PERSISTENCE)
			tierName = "persistence";

		bool filtered = m_aOnly.Count() > 0;
		array<string> matched = {};
		if (filtered)
			tierName = "only";

		int failures = 0;
		int total = 0;
		int passed = 0;
		string cases = "";

		PrintFormat("[LFTEST] SUITE START tier=%1", tierName);

		foreach (LF_Test test : m_aTests)
		{
			if (filtered)
			{
				if (!MatchesOnly(test, matched))
					continue;
			}
			else
			{
				bool skip = false;
				if (mode == LF_TestSuiteMode.FAST && test.Tier() != LF_TestTier.LOGIC)
					skip = true;
				else if (mode == LF_TestSuiteMode.PERSISTENCE && test.Tier() != LF_TestTier.PERSISTENCE)
					skip = true;

				if (skip)
				{
					PrintFormat("[LFTEST] SKIP %1 (tier=%2, %3 run)", EscapeXml(test.GetName()), TierName(test.Tier()), tierName);
					continue;
				}
			}

			total++;
			string name = EscapeXml(test.GetName());
			PrintFormat("[LFTEST] START %1", name);

			LF_TestContext ctx = new LF_TestContext();
			test.Run(ctx);

			if (ctx.FailureCount() > 0)
			{
				failures++;
				PrintFormat("[LFTEST] FAIL %1", name);
				string body = "";
				foreach (string failure : ctx.GetFailures())
				{
					PrintFormat("[LFTEST]   - %1", failure);
					body += string.Format("%1\n", EscapeXml(failure));
				}
				cases += string.Format("<testcase name=\"%1\" time=\"0\"><failure message=\"assertion failed\">%2</failure></testcase>\n", name, body);
			}
			else if (ctx.AssertionCount() == 0)
			{
				failures++;
				PrintFormat("[LFTEST] FAIL %1 (no assertions executed)", name);
				cases += string.Format("<testcase name=\"%1\" time=\"0\"><failure message=\"no assertions executed\"/></testcase>\n", name);
			}
			else
			{
				passed++;
				PrintFormat("[LFTEST] PASS %1 (%2 assertions)", name, ctx.AssertionCount());
				cases += string.Format("<testcase name=\"%1\" time=\"0\"/>\n", name);
			}
		}

		foreach (string want : m_aOnly)
		{
			if (matched.Find(want) != -1)
				continue;

			total++;
			failures++;
			string missing = EscapeXml(want);
			PrintFormat("[LFTEST] FAIL %1 (no test matches --only)", missing);
			PrintFormat("[LFTEST]   - no registered test has class or name '%1'", missing);
			cases += string.Format("<testcase name=\"%1\" time=\"0\"><failure message=\"no test matches --only\"/></testcase>\n", missing);
		}

		PrintFormat("[LFTEST] SUMMARY tier=%1 passed=%2 failed=%3 total=%4", tierName, passed, failures, total);
		reportXml = string.Format(
			"<testsuites><testsuite name=\"LifeFramework-%1\" tests=\"%2\" failures=\"%3\" errors=\"0\" time=\"0\">%4</testsuite></testsuites>",
			tierName, total, failures, cases);

		return failures;
	}

	//------------------------------------------------------------------------------------------------
	static string TierName(LF_TestTier tier)
	{
		if (tier == LF_TestTier.WORLD)
			return "world";

		if (tier == LF_TestTier.PERSISTENCE)
			return "persistence";

		return "logic";
	}

	//------------------------------------------------------------------------------------------------
	//! Registrations live in the machine-generated LF_TestRegistrations.generated.c,
	//! derived from each test file's `// tier:` comment. Regenerate with
	//! `tools\cli regen-tests` after adding or re-tiering a test; never edit the
	//! generated file by hand.
	protected void CollectTests()
	{
		m_aTests.Clear();
		LF_TestRegistrations.RegisterAll(this);
	}

	//------------------------------------------------------------------------------------------------
	static string WriteReport(string reportXml)
	{
		FileIO.MakeDirectory("$profile:TestResults");

		int year, month, day, hour, minute, second;
		System.GetYearMonthDayUTC(year, month, day);
		System.GetHourMinuteSecondUTC(hour, minute, second);
		string fileName = string.Format("$profile:TestResults/Run %1-%2-%3 %4_%5_%6.xml", year, month, day, hour, minute, second);

		FileHandle handle = FileIO.OpenFile(fileName, FileMode.WRITE);
		if (!handle)
			return "";

		handle.Write(reportXml);
		handle.Close();
		return fileName;
	}

	//------------------------------------------------------------------------------------------------
	static string EscapeXml(string value)
	{
		string result = "";
		for (int i = 0; i < value.Length(); i++)
		{
			string ch = value.Substring(i, 1);
			if (ch == "&")
				result += "&amp;";
			else if (ch == "<")
				result += "&lt;";
			else if (ch == ">")
				result += "&gt;";
			else if (ch == "\"")
				result += "&quot;";
			else
				result += ch;
		}

		return result;
	}
}