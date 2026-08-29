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
	//! \return the number of failed tests.
	int RunAll(bool fast, out string reportXml)
	{
		CollectTests();

		string tierName = "all";
		if (fast)
			tierName = "fast";

		int failures = 0;
		int total = 0;
		int passed = 0;
		string cases = "";

		foreach (EL_Test test : m_aTests)
		{
			if (fast && test.Tier() != EL_TestTier.LOGIC)
				continue;

			total++;
			string name = EscapeXml(test.GetName());
			PrintFormat("[ELTEST] START %1", name);

			EL_TestContext ctx = new EL_TestContext();
			test.Run(ctx);

			if (ctx.FailureCount() == 0)
			{
				passed++;
				PrintFormat("[ELTEST] PASS %1 (%2 assertions)", name, ctx.AssertionCount());
				cases += string.Format("<testcase name=\"%1\" time=\"0\"/>\n", name);
			}
			else
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
		}

		PrintFormat("[ELTEST] SUMMARY tier=%1 passed=%2 failed=%3 total=%4", tierName, passed, failures, total);
		reportXml = string.Format(
			"<testsuites><testsuite name=\"LifeFramework-%1\" tests=\"%2\" failures=\"%3\" errors=\"0\" time=\"0\">%4</testsuite></testsuites>",
			tierName, total, failures, cases);

		return failures;
	}

	//------------------------------------------------------------------------------------------------
	//! Register new test classes here. Tier by setup cost: LOGIC for pure
	//! logic, WORLD for anything that loads a resource or touches the world.
	protected void CollectTests()
	{
		m_aTests.Clear();
		Register(new EL_Test_WorldLoaded(), EL_TestTier.WORLD);
		Register(new EL_Test_MoneyStackPrefab(), EL_TestTier.WORLD);
		Register(new EL_Test_MathStringSanity(), EL_TestTier.LOGIC);
		Register(new EL_Test_ContextSelfTest(), EL_TestTier.LOGIC);
		Register(new EL_Test_Survival(), EL_TestTier.LOGIC);
		Register(new EL_Test_SurvivalSaveData(), EL_TestTier.LOGIC);
		Register(new EL_Test_AccountWantedClamp(), EL_TestTier.LOGIC);
		Register(new EL_Test_AccountCharacterRoster(), EL_TestTier.LOGIC);
		Register(new EL_Test_AccountSaveRoundtrip(), EL_TestTier.LOGIC);
		Register(new EL_Test_AccountManagerCache(), EL_TestTier.LOGIC);
		Register(new EL_Test_BankAccountMath(), EL_TestTier.LOGIC);
		Register(new EL_Test_BankSaveRoundtrip(), EL_TestTier.LOGIC);
		Register(new EL_Test_ATMManagerRegistry(), EL_TestTier.LOGIC);
		Register(new EL_Test_FormatAbbreviate(), EL_TestTier.LOGIC);
		Register(new EL_Test_UtilsMinMax(), EL_TestTier.LOGIC);
		Register(new EL_Test_LicensePlateFormat(), EL_TestTier.LOGIC);
		Register(new EL_Test_Prefabs(), EL_TestTier.WORLD);
		Register(new EL_Test_Data(), EL_TestTier.WORLD);
		Register(new EL_Test_MoneyCash(), EL_TestTier.WORLD);
		Register(new EL_Test_QuantityStack(), EL_TestTier.WORLD);
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
		value.Replace("&", "&amp;");
		value.Replace("<", "&lt;");
		value.Replace(">", "&gt;");
		value.Replace("\"", "&quot;");
		return value;
	}
}
