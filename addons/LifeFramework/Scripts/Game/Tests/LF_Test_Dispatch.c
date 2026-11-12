// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only LF_Test_Dispatch,LF_Test_DispatchAlarm`.
// LF_Dispatch.IsLoud returning `officersOnDuty < 0` (0 on duty read as normal) failed dispatch/once with
// `[LFTEST]   - nobody on duty: the loud alarm` and
// `- the unanswered call says the loud alarm sounded: 09:36  Armed robbery at an unknown place: 0 on duty, radioed`.
// Restored; green.

//------------------------------------------------------------------------------------------------
//! dispatch/once (M1-20): every accepted crime reports once through LF_Dispatch; a refused one
//! reports nothing. An officer on duty means the normal alarm; nobody on duty means the loud
//! one. The per-crime police minimum stays a framework gate. The radio line and the board line
//! carry the place and the description, never a name.
// tier: LOGIC
class LF_Test_Dispatch : LF_Test
{
	protected static const string ROBBER = "dispatch-once-robber";

	override string GetName()
	{
		return "dispatch/once";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Dispatch.ResetStatics();
		LF_WantedReports.ResetStatics();
		LF_RobAction.ResetSession();

		LF_PlayerAccount robber = LF_PlayerAccountManager.GetOrCreate(ROBBER);
		ctx.NotNull(robber, "the robber has an account");
		if (!robber)
			return;

		if (!robber.GetActiveCharacter())
			robber.AddCharacter(LF_PlayerCharacter.Create(LF_TestCharacterFixture.CHARACTER_PREFAB, "Rob", "Tilly", 25), true);

		robber.SetFaction(LF_Faction.CIVILIAN);
		robber.SetWantedLevel(0);

		const float COOLDOWN_S = 60;
		float now = 100000;
		float next = 0;

		// One officer on duty: one dispatch, the normal alarm.
		int paid = LF_RobAction.CommitRobbery(ROBBER, null, 100, 1, 0, COOLDOWN_S, 0, now, next);
		ctx.Equal(100, paid, "the till robbery is accepted");
		ctx.Equal(1, LF_Dispatch.CallCount(), "one accepted robbery is one dispatch");
		ctx.Equal(1, LF_WantedReports.CountFor(ROBBER), "one accepted robbery is one crime report");
		LF_DispatchRecord first = LF_Dispatch.Last();
		ctx.NotNull(first, "the dispatch is logged");
		if (first)
		{
			ctx.EqualStr(LF_WantedReports.OFFENCE_ROBBERY, first.m_sOffence, "the dispatch names the offence");
			ctx.Equal(1, first.m_iOfficers, "the dispatch records one officer on duty");
			ctx.False(first.m_bLoud, "an officer on duty: the normal alarm");
		}

		// Inside the cooldown the till refuses: no dispatch, no report.
		float last = next;
		ctx.Equal(0, LF_RobAction.CommitRobbery(ROBBER, null, 100, 1, 0, COOLDOWN_S, last, now + 1000, next), "the till refuses inside the cooldown");
		ctx.Equal(1, LF_Dispatch.CallCount(), "a refused robbery dispatches nothing");
		ctx.Equal(1, LF_WantedReports.CountFor(ROBBER), "a refused robbery reports nothing");

		// A server that sets a minimum still refuses below it (framework gate kept).
		ctx.Equal(0, LF_RobAction.CommitRobbery(ROBBER, null, 100, 0, 1, COOLDOWN_S, last, now + COOLDOWN_S * 1000 + 1, next), "a configured minimum of one refuses with nobody on duty");
		ctx.Equal(1, LF_Dispatch.CallCount(), "the minimum refusal dispatches nothing");

		// The flagship minimum is 0: nobody on duty still robs, and the alarm is the loud one.
		paid = LF_RobAction.CommitRobbery(ROBBER, null, 100, 0, 0, COOLDOWN_S, last, now + COOLDOWN_S * 1000 + 1, next);
		ctx.Equal(100, paid, "with nobody on duty the robbery still happens");
		ctx.Equal(2, LF_Dispatch.CallCount(), "the second accepted robbery is the second dispatch");
		LF_DispatchRecord second = LF_Dispatch.Last();
		if (second)
		{
			ctx.True(second.m_bLoud, "nobody on duty: the loud alarm");
			ctx.Equal(0, second.m_iRadioed, "nobody on duty: nobody is radioed");
		}

		ctx.True(LF_PoliceConfig.AlarmSeconds(true) > LF_PoliceConfig.AlarmSeconds(false), "the loud alarm sounds longer than the normal one");
		ctx.True(LF_PoliceConfig.AlarmPrefab(true) != LF_PoliceConfig.AlarmPrefab(false), "the loud alarm is its own prefab");

		// The radio line: offence, place, description; the robber's name is nowhere in it.
		LF_Dispatch.Record(LF_WantedReports.OFFENCE_ROBBERY, "#AR-Jacket_M88", "", "Fuel station", 1);
		string radio = LF_Dispatch.RadioLine(LF_WantedReports.OFFENCE_ROBBERY, "#AR-Jacket_M88", "", "Fuel station");
		ctx.True(radio.Contains("Fuel station"), "the radio line names the place: " + radio);
		ctx.True(radio.Contains(WidgetManager.Translate("#LF-Wanted_OnFoot")), "the radio line says the suspect is on foot: " + radio);
		ctx.True(radio.Contains(WidgetManager.Translate(LF_WantedReports.OFFENCE_ROBBERY)), "the radio line names the offence: " + radio);
		ctx.False(radio.Contains("Tilly"), "the radio line carries no name: " + radio);

		// The board: newest first, the call's outcome on each line.
		array<string> entries = {};
		LF_Dispatch.BuildBoardEntries(2, entries);
		ctx.Equal(2, entries.Count(), "the board shows the asked number of dispatch lines");
		if (entries.Count() == 2)
		{
			string newest = LF_Dispatch.BoardLine(entries[0]);
			ctx.True(newest.Contains("Fuel station"), "the newest board line is the newest call: " + newest);
			ctx.True(newest.Contains(WidgetManager.Translate("#LF-Dispatch_BoardRadioed", "1")), "an answered call says it was radioed: " + newest);
			ctx.False(newest.Contains("Tilly"), "the board line carries no name: " + newest);
			string loud = LF_Dispatch.BoardLine(entries[1]);
			ctx.True(loud.Contains(WidgetManager.Translate("#LF-Dispatch_BoardLoud")), "the unanswered call says the loud alarm sounded: " + loud);
		}

		for (int i = 0; i < 10; i++)
		{
			LF_Dispatch.Record(LF_WantedReports.OFFENCE_BREAK_IN, "", "", "", 0);
		}

		LF_Dispatch.BuildBoardEntries(0, entries);
		ctx.Equal(LF_PoliceConfig.DispatchLogLines(), entries.Count(), "the dispatch log keeps only the configured number of lines");

		robber.SetWantedLevel(0);
		LF_Dispatch.ResetStatics();
		LF_WantedReports.ResetStatics();
		LF_RobAction.ResetSession();
	}
}
