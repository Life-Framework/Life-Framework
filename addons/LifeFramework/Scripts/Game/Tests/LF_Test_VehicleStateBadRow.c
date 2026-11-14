// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_VehicleStateBadRow,...`:
// LF_VehicleOwnership.WriteState accepting any tow state above NONE failed `[LFTEST] FAIL vehicles/state-bad-row`,
// `- an unknown tow state reads as none (apply 1) (expected 0, got 7)`. Restored; green.
// tier: LOGIC
//! #752a: LF_VehicleOwnership.ApplyState corrects what a damaged "vehiclestate" row holds. A row
//! for a vehicle that is not loaded is skipped; an unknown tow state reads as none, a negative stamp
//! as never, a compartment level outside 0-3 is clamped; the other rows still apply. A second apply
//! gives the same records, and an empty apply puts every vehicle back to the default.
class LF_Test_VehicleStateBadRow : LF_Test
{
	protected static const string OWNER = "vehicle-state-bad-owner";
	protected static const string BROKEN = "vehicle-state-bad-broken";
	protected static const string GOOD = "vehicle-state-bad-good";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/state-bad-row";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		LF_VehicleOwnership.Register(BROKEN, OWNER, BROKEN, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", "VS BA 1001", "#EL-Vehicle_S105", true);
		LF_VehicleOwnership.Register(GOOD, OWNER, GOOD, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", "VS BA 1002", "#EL-Vehicle_S105", true);

		array<ref LF_VehicleStateRow> rows = {};
		rows.Insert(LF_VehicleStateRow.Create("vehicle-state-bad-gone", LF_EVehicleTowState.TOWED, 50, 60, 1));
		rows.Insert(LF_VehicleStateRow.Create(BROKEN, 7, -5, 100, 9));
		rows.Insert(LF_VehicleStateRow.Create(GOOD, LF_EVehicleTowState.POLICE_IMPOUND, 200, 300, 2));

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (apply %1)", pass);
			ctx.Equal(2, LF_VehicleOwnership.ApplyState(rows), "the unknown vehicle's row is skipped, the other two apply" + tag);

			LF_VehicleStateRow broken = LF_VehicleOwnership.StateOf(BROKEN);
			ctx.NotNull(broken, "the broken row's vehicle is owned" + tag);
			if (broken)
			{
				ctx.Equal(LF_EVehicleTowState.NONE, broken.m_iTowState, "an unknown tow state reads as none" + tag);
				ctx.Equal(0, broken.m_iTowedAt, "a negative towed-at reads as never" + tag);
				ctx.Equal(100, broken.m_iInspectedAt, "the good stamp on the broken row is kept" + tag);
				ctx.Equal(LF_VehicleOwnership.COMPARTMENT_MAX, broken.m_iCompartment, "a compartment above 3 is clamped to 3" + tag);
			}

			LF_VehicleStateRow good = LF_VehicleOwnership.StateOf(GOOD);
			ctx.NotNull(good, "the good row's vehicle is owned" + tag);
			if (good)
			{
				ctx.Equal(LF_EVehicleTowState.POLICE_IMPOUND, good.m_iTowState, "the good row's tow state applies" + tag);
				ctx.Equal(200, good.m_iTowedAt, "the good row's towed-at applies" + tag);
				ctx.Equal(300, good.m_iInspectedAt, "the good row's inspected-at applies" + tag);
				ctx.Equal(2, good.m_iCompartment, "the good row's compartment applies" + tag);
			}
		}

		ctx.True(LF_VehicleOwnership.SetState(BROKEN, LF_EVehicleTowState.TOWED, 10, 20, -1), "a set on an owned vehicle is accepted");
		LF_VehicleStateRow clamped = LF_VehicleOwnership.StateOf(BROKEN);
		if (clamped)
			ctx.Equal(0, clamped.m_iCompartment, "a negative compartment level is clamped to none");

		ctx.False(LF_VehicleOwnership.SetState("vehicle-state-bad-gone", LF_EVehicleTowState.TOWED, 1, 1, 1), "a set on an unknown vehicle is refused");
		ctx.True(LF_VehicleOwnership.StateOf("vehicle-state-bad-gone") == null, "an unknown vehicle has no state");

		array<ref LF_VehicleStateRow> none = {};
		ctx.Equal(0, LF_VehicleOwnership.ApplyState(none), "an empty apply applies nothing");
		LF_VehicleStateRow reset = LF_VehicleOwnership.StateOf(GOOD);
		if (reset)
		{
			ctx.Equal(LF_EVehicleTowState.NONE, reset.m_iTowState, "a vehicle with no row is back to untowed");
			ctx.Equal(0, reset.m_iCompartment, "a vehicle with no row has no compartment");
		}

		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release("VS BA 1001");
		LF_LicensePlateRegistry.Release("VS BA 1002");
	}
}
