enum EL_TestSuiteMode
{
	ALL,
	FAST,
	PERSISTENCE
}

class EL_TestManager
{
	protected static ref EL_TestManager s_Instance;
	protected ref array<ref EL_Test> m_aTests = {};

	//------------------------------------------------------------------------------------------------
	static EL_TestManager GetInstance()
	{
		if (!s_Instance)
			s_Instance = new EL_TestManager();

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void Register(EL_Test test, EL_TestTier tier)
	{
		test.SetTier(tier);
		m_aTests.Insert(test);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs every registered test (LOGIC only when fast) and fills reportXml
	//! with a JUnit-style report. Prints [ELTEST] markers for the CLI.
	//! A test that throws is fatal: EnforceScript has no exception handling, so
	//! the whole suite dies with it. Tests must not deref null.
	//! \return the number of failed tests.
	int RunAll(bool fast, out string reportXml)
	{
		EL_TestSuiteMode mode = EL_TestSuiteMode.ALL;
		if (fast)
			mode = EL_TestSuiteMode.FAST;

		return RunSuite(mode, reportXml);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs only the tests of one suite mode. FAST runs LOGIC-tier tests only,
	//! PERSISTENCE runs PERSISTENCE-tier tests only, ALL runs every test.
	//! Markers and XML shape match RunAll.
	//! \return the number of failed tests.
	int RunSuite(EL_TestSuiteMode mode, out string reportXml)
	{
		CollectTests();

		string tierName = "all";
		if (mode == EL_TestSuiteMode.FAST)
			tierName = "fast";
		else if (mode == EL_TestSuiteMode.PERSISTENCE)
			tierName = "persistence";

		int failures = 0;
		int total = 0;
		int passed = 0;
		string cases = "";

		PrintFormat("[ELTEST] SUITE START tier=%1", tierName);

		foreach (EL_Test test : m_aTests)
		{
			bool skip = false;
			if (mode == EL_TestSuiteMode.FAST && test.Tier() != EL_TestTier.LOGIC)
				skip = true;
			else if (mode == EL_TestSuiteMode.PERSISTENCE && test.Tier() != EL_TestTier.PERSISTENCE)
				skip = true;

			if (skip)
			{
				PrintFormat("[ELTEST] SKIP %1 (tier=%2, %3 run)", EscapeXml(test.GetName()), TierName(test.Tier()), tierName);
				continue;
			}

			total++;
			string name = EscapeXml(test.GetName());
			PrintFormat("[ELTEST] START %1", name);

			EL_TestContext ctx = new EL_TestContext();
			test.Run(ctx);

			if (ctx.FailureCount() > 0)
			{
				failures++;
				PrintFormat("[ELTEST] FAIL %1", name);
				string body = "";
				foreach (string failure : ctx.GetFailures())
				{
					PrintFormat("[ELTEST]   - %1", failure);
					body += string.Format("%1\n", EscapeXml(failure));
				}
				cases += string.Format("<testcase name=\"%1\" time=\"0\"><failure message=\"assertion failed\">%2</failure></testcase>\n", name, body);
			}
			else if (ctx.AssertionCount() == 0)
			{
				failures++;
				PrintFormat("[ELTEST] FAIL %1 (no assertions executed)", name);
				cases += string.Format("<testcase name=\"%1\" time=\"0\"><failure message=\"no assertions executed\"/></testcase>\n", name);
			}
			else
			{
				passed++;
				PrintFormat("[ELTEST] PASS %1 (%2 assertions)", name, ctx.AssertionCount());
				cases += string.Format("<testcase name=\"%1\" time=\"0\"/>\n", name);
			}
		}

		PrintFormat("[ELTEST] SUMMARY tier=%1 passed=%2 failed=%3 total=%4", tierName, passed, failures, total);
		reportXml = string.Format(
			"<testsuites><testsuite name=\"LifeFramework-%1\" tests=\"%2\" failures=\"%3\" errors=\"0\" time=\"0\">%4</testsuite></testsuites>",
			tierName, total, failures, cases);

		return failures;
	}

	//------------------------------------------------------------------------------------------------
	static string TierName(EL_TestTier tier)
	{
		if (tier == EL_TestTier.WORLD)
			return "world";

		if (tier == EL_TestTier.PERSISTENCE)
			return "persistence";

		return "logic";
	}

	//------------------------------------------------------------------------------------------------
	//! Registrations live in the machine-generated EL_TestRegistrations.generated.c,
	//! derived from each test file's `// tier:` comment. Regenerate with
	//! `tools\cli regen-tests` after adding or re-tiering a test; never edit the
	//! generated file by hand.
	protected void CollectTests()
	{
		m_aTests.Clear();
		EL_TestRegistrations.RegisterAll(this);
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