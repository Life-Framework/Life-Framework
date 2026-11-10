[ComponentEditorProps(category: "EveronLife/Tests", description: "Runs the LF_Test suite on boot and closes the game")]
class LF_TestRunnerComponentClass : ScriptComponentClass
{
}

class LF_TestRunnerComponent : ScriptComponent
{
	protected static const int RUN_DELAY_MS = 3000;
	protected static const int CLOSE_DELAY_MS = 5000;
	//! Written by tools/test/test-e2e.ps1 -Only (tools\cli test --only), read only under LF_TEST_ONLY.
	protected static const string ONLY_PATH = "$profile:LF_TestOnly.txt";
	//! Written by tools/test/test-e2e.ps1 under LF_TEST_HANG=run: the suite never starts, so the pool's runtime budget (tools/remote/pool.mjs) has a hang to kill.
	protected static const string HANG_PATH = "$profile:LF_TestHang.txt";

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

#ifdef LF_AUTOTEST
		GetGame().GetCallqueue().CallLater(Run, RUN_DELAY_MS, false);
#endif
	}

	//------------------------------------------------------------------------------------------------
	protected void Run()
	{
		if (FileIO.FileExists(HANG_PATH))
		{
			Print("[LF_Tests] LF_TestHang.txt present: holding the suite (runtime budget proof)", LogLevel.WARNING);
			return;
		}

		string reportXml;
		int failures;
#ifdef LF_TEST_ONLY
		LF_TestManager.GetInstance().SetOnly(ReadOnlyNames());
#endif
#ifdef LF_TEST_TIER_PERSISTENCE
		failures = LF_TestManager.GetInstance().RunSuite(LF_TestSuiteMode.PERSISTENCE, reportXml);
#else
		bool fast = false;
#ifdef LF_TEST_TIER_FAST
		fast = true;
#endif
		failures = LF_TestManager.GetInstance().RunAll(fast, reportXml);
#endif
		string reportFile = LF_TestManager.WriteReport(reportXml);
		PrintFormat("[LF_Tests] runtime tests done, failures=%1, report=%2", failures, reportFile);

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
			Print(string.Format("[LF_Tests] LF_TEST_ONLY set but %1 is missing", ONLY_PATH), LogLevel.ERROR);
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
		Print("[LF_Tests] closing game");
		GetGame().RequestClose();
	}
}