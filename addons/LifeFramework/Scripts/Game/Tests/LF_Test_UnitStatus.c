// red-proof: 2026-11-23 `tools\cli test --no-build --red LF_Test_UnitStatus` printed RED PROOF OK: RedProof() skips the radio presses, so the status, board-row and board-line assertions fail. The first --only run was also red on "clocking out clears the unit status" (the OnDutyChangedAt-only hook), green after the OnDutyChanged hook.

//! PO-D3 (#4204): an officer on duty sets "available" or "busy" from the police car's radio rows; the
//! server keeps it, every officer's dispatch board shows the unit row, and clocking out clears it.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_UnitStatus : LF_Test
{
	protected static const ResourceName MARKER = "{5D2B8E41A7C3906F}Prefabs/Systems/Vehicles/LF_PoliceCarMarker.et";
	protected static const string OFFICER = "unit-status-officer";
	protected static const string CIVILIAN = "unit-status-civilian";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/unit-status";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_UnitStatus.ResetStatics();
		RunPure(ctx);

		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer)
			return;

		IEntity markerEntity = LF_Utils.SpawnEntityPrefab(MARKER, officer.GetOrigin() + "4 0.5 0");
		LF_ParkedCarMarkerComponent marker;
		if (markerEntity)
			marker = LF_Component<LF_ParkedCarMarkerComponent>.Find(markerEntity);

		IEntity car;
		if (marker)
			car = marker.SpawnCar("test");

		ctx.True(car != null, "the police marker puts a car out");
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(OFFICER);
		if (car && account)
		{
			officer.SetOrigin(car.GetOrigin() + "2 0.1 0");
			LF_Utils.SetUidForTests(officer, OFFICER);
			RunCar(ctx, car, officer, account);
		}

		if (account)
		{
			account.SetOnDuty(false);
			account.SetFaction(LF_Faction.CIVILIAN);
		}

		LF_Utils.SetUidForTests(null, "");
		LF_UnitStatus.ResetStatics();
		LF_TestCharacterFixture.Delete(car);
		LF_TestCharacterFixture.Delete(markerEntity);
		LF_TestCharacterFixture.Delete(officer);
	}

	//------------------------------------------------------------------------------------------------
	//! The store and the board line, no world.
	protected void RunPure(LF_TestContext ctx)
	{
		ctx.False(LF_UnitStatus.Set(CIVILIAN, "Civ", LF_EUnitStatus.BUSY, false), "an officer off duty sets no status");
		ctx.Equal(LF_EUnitStatus.NONE, LF_UnitStatus.Get(CIVILIAN), "the refused status is not kept");
		ctx.False(LF_UnitStatus.Set(CIVILIAN, "Civ", LF_EUnitStatus.NONE, true), "NONE is not a status to set");

		string line = LF_UnitStatus.BoardLine(LF_UnitStatus.BoardEntry("Dupont", LF_EUnitStatus.BUSY));
		ctx.True(line.Contains("Dupont"), "the board line names the unit: " + line);
		ctx.True(LF_UnitStatus.IsBoardEntry(LF_UnitStatus.BoardEntry("Dupont", LF_EUnitStatus.AVAILABLE)), "a unit entry reads as one");
		ctx.False(LF_UnitStatus.IsBoardEntry("21:14|robbery|0|1|Fuel Station"), "a dispatch entry is not a unit entry");
		ctx.True(LF_UnitStatus.BoardLine("unit|broken").IsEmpty(), "a malformed unit entry reads as nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCar(LF_TestContext ctx, IEntity car, IEntity officer, LF_PlayerAccount account)
	{
		LF_UnitStatusAction busy = FindAction(car, true);
		LF_UnitStatusAction available = FindAction(car, false);
		ctx.NotNull(busy, "the police car has the radio 'busy' row");
		ctx.NotNull(available, "the police car has the radio 'available' row");
		if (!busy || !available)
			return;

		// Off duty: the radio refuses and the board stays empty.
		account.SetFaction(LF_Faction.CIVILIAN);
		account.SetOnDuty(false);
		busy.PerformAction(car, officer);
		ctx.Equal(LF_EUnitStatus.NONE, LF_UnitStatus.Get(OFFICER), "off duty, the radio sets nothing");

		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		// RedProof() skips the radio press, so the status and board assertions fail.
		if (!RedProof())
			busy.PerformAction(car, officer);

		ctx.Equal(LF_EUnitStatus.BUSY, LF_UnitStatus.Get(OFFICER), "the radio sets the unit busy on the server");
		array<string> entries = {};
		LF_UnitStatus.BuildBoardEntries(entries);
		ctx.Equal(1, entries.Count(), "the board carries one unit row");
		if (!entries.IsEmpty())
			ctx.EqualStr(LF_UnitStatus.BoardEntry(LF_UnitStatus.NameOf(OFFICER), LF_EUnitStatus.BUSY), entries[0], "the row says busy");

		// The row reaches an officer's board answer and becomes a board line.
		if (!RedProof())
			available.PerformAction(car, officer);

		ctx.Equal(LF_EUnitStatus.AVAILABLE, LF_UnitStatus.Get(OFFICER), "the radio sets the unit available");
		LF_UnitStatus.BuildBoardEntries(entries);
		bool updated = !entries.IsEmpty() && entries[0] == LF_UnitStatus.BoardEntry(LF_UnitStatus.NameOf(OFFICER), LF_EUnitStatus.AVAILABLE);
		ctx.True(updated, "the board row updates to available");

		array<string> dispatches = {};
		dispatches.InsertAll(entries);
		array<ref LF_UIPart> parts = LF_WantedBoard.BuildParts(true, {}, {}, {}, {}, {}, {}, dispatches);
		bool shown;
		foreach (LF_UIPart part : parts)
		{
			if (part && !entries.IsEmpty() && part.GetText() == LF_UnitStatus.BoardLine(entries[0]))
				shown = true;
		}

		ctx.True(shown, "an officer's board shows the unit row");

		// Clocking out clears it.
		account.SetOnDuty(false);
		LF_PoliceRanks.OnDutyChanged(account, false);
		ctx.Equal(LF_EUnitStatus.NONE, LF_UnitStatus.Get(OFFICER), "clocking out clears the unit status");
		LF_UnitStatus.BuildBoardEntries(entries);
		ctx.Equal(0, entries.Count(), "the board drops the row");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_UnitStatusAction FindAction(IEntity car, bool busy)
	{
		ActionsManagerComponent actions = ActionsManagerComponent.Cast(car.FindComponent(ActionsManagerComponent));
		if (!actions)
			return null;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		foreach (BaseUserAction base : list)
		{
			LF_UnitStatusAction action = LF_UnitStatusAction.Cast(base);
			if (action && action.IsBusyRow() == busy)
				return action;
		}

		return null;
	}
}
