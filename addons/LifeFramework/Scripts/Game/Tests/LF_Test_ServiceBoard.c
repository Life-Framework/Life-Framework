// red-proof: observed 2026-11-09 with `toolsi test --no-build --only LF_Test_ServiceBoard`: LF_ServiceBoard.BuildEntries listing every service (the `m_sService == service` filter replaced by `true`) failed it with `- the medic call is not on the police board (expected 0, got 1)`. Restored; green.
//------------------------------------------------------------------------------------------------
//! comms/board (#250): a medic call shows on the medic board and not on the police board; only the
//! service's own on-duty staff may read it; a call leaves the board when it expires; a crime report
//! and a payphone call both land on the police board.
// tier: WORLD
class LF_Test_ServiceBoard : LF_Test
{
	override string GetName()
	{
		return "comms/board";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();
		LF_Dispatch.ResetStatics();
		LF_ServiceBoard.ResetStatics();
		LF_ServiceCallDispatch.ResetStatics();
		LF_ServiceCallDispatch.Install();
		LF_ServiceBoard.Install();

		LF_ServiceCandidate medic = LF_ServiceCandidate.Make(1, false, LF_EJobType.MEDIC);
		LF_ServiceCandidate officer = LF_ServiceCandidate.Make(2, true, LF_EJobType.UNEMPLOYED);
		LF_ServiceCandidate civilian = LF_ServiceCandidate.Make(3, false, LF_EJobType.UNEMPLOYED);
		array<ref LF_ServiceCandidate> everyone = {};
		everyone.Insert(medic);
		everyone.Insert(officer);
		everyone.Insert(civilian);

		int now = System.GetTickCount();
		array<string> entries = {};

		LF_ServiceRoute route = LF_ServiceCalls.Place("caller-uid", "medic", "Village payphone", "#LF-Payphone_Summary_Emergency", "96.5 0 263", everyone);
		ctx.True(route && route.IsAnswered(), "the medic call is answered");
		ctx.Equal(1, LF_ServiceBoard.BuildEntries("medic", now, 10, entries), "the medic call is on the medic board");
		ctx.Equal(0, LF_ServiceBoard.BuildEntries("police", now, 10, entries), "the medic call is not on the police board");
		ctx.Equal(0, LF_Dispatch.CallCount(), "a medic call is not a police dispatch");

		array<ref LF_ServiceDef> services = LF_ServiceCallsConfig.Services();
		ctx.True(LF_ServiceBoard.MayRead("medic", services, medic), "a medic reads the medic board");
		ctx.False(LF_ServiceBoard.MayRead("medic", services, officer), "an officer does not read the medic board");
		ctx.False(LF_ServiceBoard.MayRead("police", services, civilian), "a civilian gets a closed police board");
		ctx.True(LF_ServiceBoard.MayRead("police", services, officer), "an on-duty officer reads the police board");

		LF_ServiceBoard.BuildEntries("medic", now, 10, entries);
		array<ref LF_UIPart> open = LF_ServiceBoard.BuildParts("medic", true, entries);
		array<ref LF_UIPart> closed = LF_ServiceBoard.BuildParts("medic", false, entries);
		ctx.Equal(3, open.Count(), "an open board is a title, the call and a close button");
		ctx.Equal(3, closed.Count(), "a closed board is a title, one line and a close button, with no call details");

		int expiry = LF_ServiceCallsConfig.BoardExpirySeconds();
		ctx.Equal(0, LF_ServiceBoard.BuildEntries("medic", now + (expiry + 1) * 1000, 10, entries), "an expired call leaves the board");

		// A payphone call to the police reaches the police board and the radio log.
		LF_ServiceCalls.Place("caller-uid", "police", "Village payphone", "#LF-Payphone_Summary_Crime", "96.5 0 263", everyone);
		ctx.Equal(1, LF_ServiceBoard.BuildEntries("police", now, 10, entries), "the payphone call is on the police board");
		ctx.Equal(1, LF_Dispatch.CallCount(), "the payphone call is also a police dispatch");

		// A crime report is a police service call too.
		LF_WantedReports.ResetStatics();
		LF_PlayerAccount robber = LF_PlayerAccountManager.GetOrCreate("board-test-robber");
		if (robber && !robber.GetActiveCharacter())
			robber.AddCharacter(LF_PlayerCharacter.Create(LF_TestCharacterFixture.CHARACTER_PREFAB, "Rob", "Tilly", 25), true);

		LF_DispatchRecord record = LF_Dispatch.Report("board-test-robber", null, LF_WantedReports.OFFENCE_ROBBERY, vector.Zero, 1);
		ctx.NotNull(record, "the crime report reaches dispatch through the bus");
		ctx.Equal(2, LF_ServiceBoard.BuildEntries("police", now, 10, entries), "the crime report is on the police board");
		ctx.Equal(0, LF_ServiceBoard.BuildEntries("medic", now, 10, entries), "the crime report is not on the medic board");

		LF_ServiceBoard.ResetStatics();
		LF_EventBus.ResetStatics();
		LF_Dispatch.ResetStatics();
		LF_WantedReports.ResetStatics();
	}
}
