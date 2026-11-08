[ComponentEditorProps(category: "EveronLife/Tests", description: "Runs the EL_Test suite on boot and closes the game")]
class EL_TestRunnerComponentClass : ScriptComponentClass
{
}

class EL_TestRunnerComponent : ScriptComponent
{
	protected static const int RUN_DELAY_MS = 3000;
	protected static const int CLOSE_DELAY_MS = 5000;
	//! Written by tools/test/test-e2e.ps1 -Only (tools\cli test --only), read only under EL_TEST_ONLY.
	protected static const string ONLY_PATH = "$profile:EL_TestOnly.txt";

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

#ifdef EL_AUTOTEST
		GetGame().GetCallqueue().CallLater(Run, RUN_DELAY_MS, false);
#endif
	}

	//------------------------------------------------------------------------------------------------
	protected void Run()
	{
		string reportXml;
		int failures;
#ifdef EL_TEST_ONLY
		EL_TestManager.GetInstance().SetOnly(ReadOnlyNames());
#endif
#ifdef EL_TEST_TIER_PERSISTENCE
		failures = EL_TestManager.GetInstance().RunSuite(EL_TestSuiteMode.PERSISTENCE, reportXml);
#else
		bool fast = false;
#ifdef EL_TEST_TIER_FAST
		fast = true;
#endif
		failures = EL_TestManager.GetInstance().RunAll(fast, reportXml);
#endif
		string reportFile = EL_TestManager.WriteReport(reportXml);
		PrintFormat("[EL_Tests] runtime tests done, failures=%1, report=%2", failures, reportFile);

		GetGame().GetCallqueue().CallLater(Close, CLOSE_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! One comma-separated line of test class names or GetName() strings. A
	//! missing file logs an error and yields a filter that matches nothing, so
	//! the run fails instead of silently running the whole suite.
	protected array<string> ReadOnlyNames()
	{
		array<string> names = {};
		FileHandle handle = FileIO.OpenFile(ONLY_PATH, FileMode.READ);
		if (!handle)
		{
			Print(string.Format("[EL_Tests] EL_TEST_ONLY set but %1 is missing", ONLY_PATH), LogLevel.ERROR);
			names.Insert("<missing " + ONLY_PATH + ">");
			return names;
		}

		string line;
		handle.ReadLine(line);
		handle.Close();
		line.Split(",", names, true);
		if (names.IsEmpty())
			names.Insert("<empty " + ONLY_PATH + ">");

		return names;
	}

	//------------------------------------------------------------------------------------------------
	protected void Close()
	{
		Print("[EL_Tests] closing game");
		GetGame().RequestClose();
	}
}