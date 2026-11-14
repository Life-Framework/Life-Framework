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
	protected bool m_bRed;

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
	//! Batched red proof (`tools\cli test --red`, LF_TEST_RED): the --only tests run with their
	//! red-proof hook on (LF_Test.RedProof()), the run reports tier=red, and a test PASSES the
	//! proof only by FAILING. A test that still passes is hollow and is counted as failed.
	void SetRed(bool on)
	{
		m_bRed = on;
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
		bool red = m_bRed && filtered;
		if (red)
			tierName = "red";
		else if (filtered)
			tierName = "only";

		int failures = 0;
		int total = 0;
		int passed = 0;
		int skippedNeedsPack = 0;
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

			if (CoreOnlyRun() && test.NeedsPack() != "")
			{
				skippedNeedsPack++;
				PrintFormat("[LFTEST] SKIP %1 (core-only run: needs the pack: %2)", EscapeXml(test.GetName()), EscapeXml(test.NeedsPack()));
				continue;
			}

			total++;
			string name = EscapeXml(test.GetName());
			PrintFormat("[LFTEST] START %1", name);

			LF_TestContext ctx = new LF_TestContext();
			test.SetRedProof(red);
			test.Run(ctx);
			test.SetRedProof(false);

			if (red)
			{
				// passed = failed as the proof wants; failed = still passes (hollow) or asserts nothing.
				if (ctx.FailureCount() > 0)
				{
					passed++;
					PrintFormat("[LFTEST] RED-OK %1 (fails with the hook on, %2 assertions)", name, ctx.AssertionCount());
					string redBody = "";
					foreach (string redFailure : ctx.GetFailures())
					{
						PrintFormat("[LFTEST]   red: %1", redFailure);
						redBody += string.Format("%1\n", EscapeXml(redFailure));
					}
					cases += string.Format("<testcase name=\"%1\" time=\"0\"><system-out>red proof ok: fails with the hook on\n%2</system-out></testcase>\n", name, redBody);
				}
				else
				{
					failures++;
					PrintFormat("[LFTEST] FAIL %1 (HOLLOW: still passes with the red-proof hook on, %2 assertions; no RedProof() hook, or the hook does not break what the assertions check)", name, ctx.AssertionCount());
					cases += string.Format("<testcase name=\"%1\" time=\"0\"><failure message=\"hollow: passes under red proof\"/></testcase>\n", name);
				}

				continue;
			}

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

		PrintFormat("[LFTEST] SUMMARY tier=%1 passed=%2 failed=%3 total=%4 skipped=%5", tierName, passed, failures, total, skippedNeedsPack);
		reportXml = string.Format(
			"<testsuites><testsuite name=\"LifeFramework-%1\" tests=\"%2\" failures=\"%3\" errors=\"0\" time=\"0\">%4</testsuite></testsuites>",
			tierName, total, failures, cases);

		return failures;
	}

	//------------------------------------------------------------------------------------------------
	//! True on a core-only run (-scrDefine LF_CORE_ONLY): only LifeFramework is loaded.
	static bool CoreOnlyRun()
	{
		#ifdef LF_CORE_ONLY
		return true;
		#else
		return false;
		#endif
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