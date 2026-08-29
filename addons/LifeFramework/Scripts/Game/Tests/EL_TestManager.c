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
	void Register(EL_Test test)
	{
		m_aTests.Insert(test);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs every registered test and fills reportXml with a JUnit-style report.
	//! \return the number of failed tests.
	int RunAll(out string reportXml)
	{
		CollectTests();

		int failures = 0;
		string cases = "";

		foreach (EL_Test test : m_aTests)
		{
			EL_TestContext ctx = new EL_TestContext();
			test.Run(ctx);

			string name = EscapeXml(test.GetName());

			if (ctx.FailureCount() == 0)
			{
				cases += string.Format("<testcase name=\"%1\" time=\"0\"/>\n", name);
			}
			else
			{
				failures++;
				string body = "";
				foreach (string failure : ctx.GetFailures())
				{
					body += string.Format("%1\n", EscapeXml(failure));
				}
				cases += string.Format("<testcase name=\"%1\" time=\"0\"><failure message=\"assertion failed\">%2</failure></testcase>\n", name, body);
			}
		}

		reportXml = string.Format(
			"<testsuites><testsuite name=\"LifeFramework\" tests=\"%1\" failures=\"%2\" errors=\"0\" time=\"0\">%3</testsuite></testsuites>",
			m_aTests.Count(), failures, cases);

		return failures;
	}

	//------------------------------------------------------------------------------------------------
	//! Register new test classes here (or via EL_TestManager.GetInstance().Register()
	//! from a component when EL_AUTOTEST is defined).
	protected void CollectTests()
	{
		m_aTests.Clear();
		m_aTests.Insert(new EL_Test_WorldLoaded());
		m_aTests.Insert(new EL_Test_MoneyStackPrefab());
		m_aTests.Insert(new EL_Test_MathStringSanity());
		m_aTests.Insert(new EL_Test_ContextSelfTest());
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
