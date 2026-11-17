enum LF_TestSuiteMode
{
	ALL,
	FAST,
	PERSISTENCE,
	PERF
}

class LF_TestManager
{
	protected static ref LF_TestManager s_Instance;
	protected ref array<ref LF_Test> m_aTests = {};
	protected ref array<string> m_aOnly = {};
	protected bool m_bRed;
	protected bool m_bLogicWorld;
	//! Self-registered tests (LF_TestRegistration), filled from static initializers at script load.
	//! No initializer on purpose: those may run before this class's own; Enqueue creates it on first use.
	protected static ref array<ref LF_TestRegistration> s_aSelfRegistered;
	//! Duplicate class names already logged, so each is logged once per process.
	protected static ref array<string> s_aLoggedDuplicates;

	//! The minimal world tools\cli test boots when every selected test is LOGIC (tools/lib/test-world.mjs).
	static const string LOGIC_WORLD = "Worlds/TestLogic/TestLogic.ent";

	//------------------------------------------------------------------------------------------------
	static LF_TestManager GetInstance()
	{
		if (!s_Instance)
			s_Instance = new LF_TestManager();

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Called from a test file's LF_TestRegistration static initializer (LF_TestRegistration.c).
	//! Returns true so a static bool initializer can call it.
	static bool Enqueue(LF_TestRegistration registration)
	{
		if (!registration)
			return false;

		if (!s_aSelfRegistered)
			s_aSelfRegistered = new array<ref LF_TestRegistration>();

		s_aSelfRegistered.Insert(registration);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Collects now (generated registry, self-registered merged in by class name, pack) and returns the list a run
	//! walks. Call it on a fresh manager: on the running singleton it would rebuild the list mid-run.
	array<ref LF_Test> Collected()
	{
		CollectTests();
		return m_aTests;
	}

	//------------------------------------------------------------------------------------------------
	void Register(LF_Test test, LF_TestTier tier)
	{
		test.SetTier(tier);
		m_aTests.Insert(test);
	}

	//------------------------------------------------------------------------------------------------
	//! A pack test (a pack's Scripts/Game/Tests, registered by its generated
	//! LF_PackTestRegistrations.generated.c). It runs only when named (-Only): a tier run skips it, and
	//! `tools\cli test` names it in the pack leg, which boots under the pack's mission header so its
	//! config overlays apply (docs/architecture/three-worlds.md "Pack test leg").
	void RegisterPack(LF_Test test, LF_TestTier tier)
	{
		test.SetPackTest(true);
		Register(test, tier);
	}

	//------------------------------------------------------------------------------------------------
	//! Empty in the core. A pack's generated registry overrides it (modded class LF_TestManager) and
	//! calls RegisterPack for each of its tests; the core never names a pack test.
	protected void RegisterPackTests()
	{
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
	//! Set by the runner placed in the minimal LOGIC world (Worlds/TestLogic): it has no terrain,
	//! village or annex, so a WORLD or PERSISTENCE test fails by name instead of running, and a
	//! LOGIC test that fails there carries a hint that it may need the WORLD tier.
	void SetLogicWorld(bool on)
	{
		m_bLogicWorld = on;
	}

	//------------------------------------------------------------------------------------------------
	//! Why a test of this tier cannot run in the minimal LOGIC world, or "" when it can.
	static string LogicWorldRefusal(LF_TestTier tier, string name)
	{
		if (tier == LF_TestTier.LOGIC)
			return "";

		return string.Format("%1 is tier=%2 and needs DebugWorld; this run booted the minimal LOGIC world %3 (run it without --world, or with --world Worlds/DebugWorld/DebugWorld.ent)", name, TierName(tier), LOGIC_WORLD);
	}

	//------------------------------------------------------------------------------------------------
	//! The line a LOGIC test that fails in the minimal world prints under its failures.
	static string LogicWorldHint(string name)
	{
		return string.Format("hint: %1 ran in the minimal LOGIC world %2 (no terrain, village or annex); if it needs world content, its tier is WORLD, not LOGIC", name, LOGIC_WORLD);
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
	//! Runs every registered test but the PERF tier (LOGIC only when fast) and fills reportXml
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
	//! PERSISTENCE runs PERSISTENCE-tier tests only, PERF runs PERF-tier tests only,
	//! ALL runs every test except the PERF tier (timing gates run on demand, `--tier perf`).
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
		else if (mode == LF_TestSuiteMode.PERF)
			tierName = "perf";

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
				else if (mode == LF_TestSuiteMode.PERF && test.Tier() != LF_TestTier.PERF)
					skip = true;
				else if (mode == LF_TestSuiteMode.ALL && test.Tier() == LF_TestTier.PERF)
					skip = true;

				if (skip)
				{
					PrintFormat("[LFTEST] SKIP %1 (tier=%2, %3 run)", EscapeXml(test.GetName()), TierName(test.Tier()), tierName);
					continue;
				}

				// A pack test needs the pack's mission header; only the pack leg (which names it) has that.
				if (test.IsPackTest())
				{
					PrintFormat("[LFTEST] SKIP %1 (pack test: runs only in the pack leg, under the pack header)", EscapeXml(test.GetName()));
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
			if (m_bLogicWorld)
			{
				string refusal = LogicWorldRefusal(test.Tier(), name);
				if (refusal != "")
				{
					failures++;
					PrintFormat("[LFTEST] FAIL %1 (wrong world)", name);
					PrintFormat("[LFTEST]   - %1", refusal);
					cases += string.Format("<testcase name=\"%1\" time=\"0\"><failure message=\"wrong world\">%2</failure></testcase>\n", name, refusal);
					continue;
				}
			}

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
				if (m_bLogicWorld)
					PrintFormat("[LFTEST]   - %1", LogicWorldHint(name));
				cases += string.Format("<testcase name=\"%1\" time=\"0\"><failure message=\"assertion failed\">%2</failure></testcase>\n", name, body);
			}
			else if (ctx.AssertionCount() == 0)
			{
				failures++;
				PrintFormat("[LFTEST] FAIL %1 (no assertions executed)", name);
				if (m_bLogicWorld)
					PrintFormat("[LFTEST]   - %1", LogicWorldHint(name));
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
	//! True in a `tools\cli test --tier perf` run (-scrDefine LF_TEST_TIER_PERF): a PERF test then takes
	//! its sharp settings (more repeats and windows, a tighter limit). Under --only, --red or any other
	//! tier it is false, so the same test keeps its lenient settings there.
	static bool PerfRun()
	{
		#ifdef LF_TEST_TIER_PERF
		return true;
		#else
		return false;
		#endif
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

		if (tier == LF_TestTier.PERF)
			return "perf";

		return "logic";
	}

	//------------------------------------------------------------------------------------------------
	//! Two sources while tests move to self-registration (#2610): the machine-generated
	//! LF_TestRegistrations.generated.c (from each unconverted file's `// tier:` comment; `tools\cli
	//! regen-tests`, never by hand) and the LF_TestRegistration subclasses that enqueued themselves at
	//! script load. A class already registered is dropped (logged once).
	//! Order: the self-registered tests are sorted by class name and merged into the generated list,
	//! each before the first generated test whose class name sorts after it. The generated tests keep
	//! their order (some still depend on it), the result does not depend on script load order, and
	//! once every test self-registers the whole list is in class-name order.
	protected void CollectTests()
	{
		m_aTests.Clear();
		LF_TestRegistrations.RegisterAll(this);
		MergeSelfRegistered();
		RegisterPackTests();
	}

	//------------------------------------------------------------------------------------------------
	protected void MergeSelfRegistered()
	{
		if (!s_aSelfRegistered || s_aSelfRegistered.IsEmpty())
			return;

		map<string, ref LF_Test> byName = new map<string, ref LF_Test>();
		map<string, LF_TestTier> tiers = new map<string, LF_TestTier>();
		array<string> names = {};
		foreach (LF_TestRegistration registration : s_aSelfRegistered)
		{
			if (!registration)
				continue;

			LF_Test test = registration.Create();
			if (!test)
			{
				LF_Debug.Error("Tests", string.Format("%1.Create() returned no test; skipped", registration.ClassName()));
				continue;
			}

			string className = test.ClassName();
			if (byName.Contains(className) || HasClass(className))
			{
				LogDuplicate(className);
				continue;
			}

			byName.Insert(className, test);
			tiers.Insert(className, registration.Tier());
			names.Insert(className);
		}

		names.Sort();

		array<ref LF_Test> generated = {};
		foreach (LF_Test t : m_aTests)
		{
			generated.Insert(t);
		}

		m_aTests.Clear();
		int next = 0;
		foreach (LF_Test g : generated)
		{
			while (next < names.Count() && SortsBefore(names[next], g.ClassName()))
			{
				Register(byName.Get(names[next]), tiers.Get(names[next]));
				next++;
			}

			m_aTests.Insert(g);
		}

		while (next < names.Count())
		{
			Register(byName.Get(names[next]), tiers.Get(names[next]));
			next++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True when a comes before b in the order array<string>.Sort() gives.
	static bool SortsBefore(string a, string b)
	{
		if (a == b)
			return false;

		array<string> pair = {a, b};
		pair.Sort();
		return pair[0] == a;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasClass(string className)
	{
		foreach (LF_Test test : m_aTests)
		{
			if (test && test.ClassName() == className)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void LogDuplicate(string className)
	{
		if (!s_aLoggedDuplicates)
			s_aLoggedDuplicates = new array<string>();

		if (s_aLoggedDuplicates.Find(className) != -1)
			return;

		s_aLoggedDuplicates.Insert(className);
		LF_Debug.Warn("Tests", string.Format("%1 is registered twice; kept the first registration", className));
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