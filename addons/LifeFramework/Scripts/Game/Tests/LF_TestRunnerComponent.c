[ComponentEditorProps(category: "EveronLife/Tests", description: "Runs the LF_Test suite on boot and closes the game")]
class LF_TestRunnerComponentClass : ScriptComponentClass
{
}

class LF_TestRunnerComponent : ScriptComponent
{
	//! The suite starts SETTLE_MS after the game mode is running (SCR_BaseGameMode.IsRunning(): state
	//! GAME, which StartGameMode sets on the first frame after pregame), checked every READY_POLL_MS.
	//! The settle is for what the first frames still spawn (NPC loadouts, vehicle cargo crates are in
	//! the world untraceable for a moment; debugworld/entity-collision fails on frame 1). A world
	//! without an SCR_BaseGameMode, or one that never gets there, starts at RUN_FALLBACK_MS, the fixed
	//! delay every run used before.
	protected static const int READY_POLL_MS = 100;
	protected static const int SETTLE_MS = 500;
	protected static const int RUN_FALLBACK_MS = 3000;
	protected static const int CLOSE_DELAY_MS = 5000;
	protected int m_iWaitedMs;
	protected bool m_bStarted;
	//! Written by tools/test/test-e2e.ps1 -Only (tools\cli test --only, and --red, which adds LF_TEST_RED), read only under LF_TEST_ONLY.
	protected static const string ONLY_PATH = "$profile:LF_TestOnly.txt";
	//! Written by tools/test/test-e2e.ps1 under LF_TEST_HANG=run: the suite never starts, so the pool's runtime budget (tools/remote/pool.mjs) has a hang to kill.
	protected static const string HANG_PATH = "$profile:LF_TestHang.txt";

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

#ifdef LF_AUTOTEST
		GetGame().GetCallqueue().CallLater(WaitForGameMode, READY_POLL_MS, false);
#endif
	}

	//------------------------------------------------------------------------------------------------
	protected void WaitForGameMode()
	{
		if (m_bStarted)
			return;

		m_iWaitedMs += READY_POLL_MS;
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		bool running = gameMode && gameMode.IsRunning();
		if (!running && m_iWaitedMs < RUN_FALLBACK_MS)
		{
			GetGame().GetCallqueue().CallLater(WaitForGameMode, READY_POLL_MS, false);
			return;
		}

		m_bStarted = true;
		if (!running)
		{
			Print(string.Format("[LF_Tests] game mode not running after %1 ms; starting the suite anyway", m_iWaitedMs), LogLevel.WARNING);
			Run();
			return;
		}

		PrintFormat("[LF_Tests] game mode running after %1 ms; starting the suite in %2 ms", m_iWaitedMs, SETTLE_MS);
		GetGame().GetCallqueue().CallLater(Run, SETTLE_MS, false);
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
#ifdef LF_TEST_RED
		// tools\cli test --red: the --only tests run with RedProof() on and must all fail.
		LF_TestManager.GetInstance().SetRed(true);
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

#ifdef LF_E2E_HOST_STOP
		// wt e2e pass 1 (#830): the harness closes this boot through the host stop file, the path
		// tools\cli host stop takes, so the engine's SHUTDOWN save is what the reload boot loads.
		Print("[LF_Tests] leaving the close to the host stop file (LF_E2E_HOST_STOP)");
#else
		GetGame().GetCallqueue().CallLater(Close, CLOSE_DELAY_MS, false);
#endif
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