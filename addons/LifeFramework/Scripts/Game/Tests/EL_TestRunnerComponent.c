[ComponentEditorProps(category: "EveronLife/Tests", description: "Runs the EL_Test suite on boot and closes the game")]
class EL_TestRunnerComponentClass : ScriptComponentClass
{
}

class EL_TestRunnerComponent : ScriptComponent
{
	protected static const int RUN_DELAY_MS = 3000;
	protected static const int CLOSE_DELAY_MS = 5000;

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
		bool fast = false;
#ifdef EL_TEST_TIER_FAST
		fast = true;
#endif

		string reportXml;
		int failures = EL_TestManager.GetInstance().RunAll(fast, reportXml);
		string reportFile = EL_TestManager.WriteReport(reportXml);
		PrintFormat("[EL_Tests] runtime tests done, failures=%1, report=%2", failures, reportFile);

		GetGame().GetCallqueue().CallLater(Close, CLOSE_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void Close()
	{
		Print("[EL_Tests] closing game");
		GetGame().RequestClose();
	}
}