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
	//! The fixture spawn point (the gate), what a run found standing there before its first test, how much a test
	//! added to it, and the tests that added it (WatchGate, NoteLeftovers, FixtureLeftovers).
	protected vector m_vGate;
	protected ref LF_TestDebrisProbe m_GateProbe;
	protected int m_iGateLeft;
	protected ref array<string> m_aLeakers = {};
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
	//! Collects now (every LF_Test with its own [LF_TestInfo], in class-name order) and returns the list a run
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
		WatchGate(red);

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
			if (!red)
				NoteLeftovers(name);

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

		// A run leaves no fixture character or loose physics prop at the gate (#2792). `tools\cli test` sends
		// the whole suite as one --only list, so this covers a filtered run too (a red proof keeps its debris on purpose).
		if (!red && m_GateProbe && total > 0)
		{
			total++;
			string leftovers = FixtureLeftovers();
			if (leftovers != "")
			{
				failures++;
				PrintFormat("[LFTEST] FAIL suite/no-leftover-fixtures");
				PrintFormat("[LFTEST]   - a test left characters or loose props at the gate (end the test with LF_TestCharacterFixture.Delete):%1", leftovers);
				foreach (string leaker : m_aLeakers)
					PrintFormat("[LFTEST]   - left by %1", leaker);
				cases += string.Format("<testcase name=\"suite/no-leftover-fixtures\" time=\"0\"><failure message=\"fixtures left at the gate\">%1</failure></testcase>\n", EscapeXml(leftovers));
			}
			else
			{
				passed++;
				PrintFormat("[LFTEST] PASS suite/no-leftover-fixtures (nothing left at the gate)");
				cases += "<testcase name=\"suite/no-leftover-fixtures\" time=\"0\"/>\n";
			}
		}

		PrintFormat("[LFTEST] SUMMARY tier=%1 passed=%2 failed=%3 total=%4 skipped=%5", tierName, passed, failures, total, skippedNeedsPack);
		reportXml = string.Format(
			"<testsuites><testsuite name=\"LifeFramework-%1\" tests=\"%2\" failures=\"%3\" errors=\"0\" time=\"0\">%4</testsuite></testsuites>",
			tierName, total, failures, cases);

		return failures;
	}

	//------------------------------------------------------------------------------------------------
	//! Before the first test: remembers what already stands at the gate (the map's props, the spawn point) so the end-of-run
	//! check names only what a test added. A run with no world or no spawn point has no gate and skips the check.
	protected void WatchGate(bool red)
	{
		m_vGate = vector.Zero;
		m_iGateLeft = 0;
		m_aLeakers.Clear();
		m_GateProbe = null;
		// A red proof keeps its debris on purpose.
		if (red || m_bLogicWorld || !GetGame() || !GetGame().GetWorld())
			return;

		// The e2e pass-1 write test keeps its characters standing on purpose: the shutdown save has to
		// find them (`tools\cli e2e`), so that run has no gate to keep clean (main guard e2e, #2800).
		if (m_aOnly.Find("LF_Test_E2E_SaveStateWrite") != -1)
			return;

		m_vGate = LF_TestDebrisProbe.Gate();
		if (m_vGate == vector.Zero)
			return;

		m_GateProbe = new LF_TestDebrisProbe();
		m_GateProbe.Watch(m_vGate);
	}

	//------------------------------------------------------------------------------------------------
	//! After each test: names it in m_aLeakers when it left more characters or loose props at the gate than were there before.
	//! Only counts (a query); a sweep that deleted them mid-suite crashed the engine (#2792).
	protected void NoteLeftovers(string testName)
	{
		if (!m_GateProbe)
			return;

		m_GateProbe.Leftovers(m_vGate);
		if (m_GateProbe.Count() > m_iGateLeft)
			m_aLeakers.Insert(string.Format("%1 (+%2)", testName, m_GateProbe.Count() - m_iGateLeft));

		m_iGateLeft = m_GateProbe.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Fixture characters and loose props the run added at the gate and nothing deleted, as " | prefab@origin h=height"
	//! entries; "" when the gate is clean (or the run has no gate to look at).
	protected string FixtureLeftovers()
	{
		if (!m_GateProbe)
			return "";

		return m_GateProbe.Leftovers(m_vGate);
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
	//! Every LF_Test descendant that carries its own LF_TestInfo attribute (LF_TestInfo.c), found by
	//! reflection (#2610). Tier and pack flag come from the attribute; the order is a sort by class
	//! name, so it never depends on reflection order or on a shared file. No registry, no generator.
	protected void CollectTests()
	{
		m_aTests.Clear();
		array<typename> types = {};
		typename base = LF_Test;
		base.GetInheritedTypes(types);

		map<string, typename> byName = new map<string, typename>();
		array<string> names = {};
		foreach (typename type : types)
		{
			if (!FindInfo(type))
				continue;

			string className = type.ToString();
			if (byName.Contains(className))
			{
				LogDuplicate(className);
				continue;
			}

			byName.Insert(className, type);
			names.Insert(className);
		}

		names.Sort();
		foreach (string name : names)
		{
			typename testType = byName.Get(name);
			LF_Test test = LF_Test.Cast(testType.Spawn());
			if (!test)
			{
				LF_Debug.Error("Tests", string.Format("%1 has [LF_TestInfo] but Spawn() returned no test; skipped", name));
				continue;
			}

			LF_TestInfo info = FindInfo(testType);
			test.SetPackTest(info.m_bPack);
			Register(test, info.m_eTier);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The class's own LF_TestInfo, or null. Attributes are not inherited, so a subclass of a tagged
	//! test, or a helper base, has none and is not a test.
	static LF_TestInfo FindInfo(typename type)
	{
		array<Class> attributes = {};
		type.GetAttributes(attributes);
		foreach (Class attribute : attributes)
		{
			LF_TestInfo info = LF_TestInfo.Cast(attribute);
			if (info)
				return info;
		}

		return null;
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