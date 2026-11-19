// red-proof: Observed 2026-11-19 with `--no-build --red LF_Test_ReaperCut`: `RED PROOF OK (all 1 listed test(s) fail with the hook on)`, the tick skipped (one cell cut instead of 20).
//! harvest/reaper-cut (#3289): a reaper hitched to a UAZ469 cuts the field under its bar when the vehicle's
//! physics speed is inside the band and adds cells times yield to its hopper; above the band it cuts nothing,
//! a stopped vehicle cuts nothing, and a full hopper stops cutting. The test deletes the vehicle and the field.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ReaperCut : LF_Test
{
	protected static const ResourceName UAZ = "{C8B16ABCA57679A2}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et";
	protected static const ResourceName FIELD = "{5FF806585DB556B3}Prefabs/Harvest/LF_WheatField.et";
	//! The field's middle: a 60 x 40 m box, so x 10..70 and z 20..60 in 20 x 13 cells.
	protected static const vector SPOT = "40 0 40";
	//! Rows are cells 3 m deep from z 20; a row's middle is z 20 + 3 * row + 1.5.
	protected static const float ROW_Z_BASE = 21.5;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "harvest/reaper-cut";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		vector at = SPOT;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		IEntity fieldEntity = LF_Utils.SpawnEntityPrefab(FIELD, at);
		LF_WheatField field = LF_WheatField.Cast(fieldEntity);
		ctx.NotNull(field, "the wheat field prefab spawns");

		vector carAt = Vector(14.5, 0, ROW_Z_BASE);
		carAt[1] = GetGame().GetWorld().GetSurfaceY(carAt[0], carAt[2]) + 0.3;
		IEntity uaz = LF_Utils.SpawnEntityPrefab(UAZ, carAt, Vector(90, 0, 0));
		ctx.NotNull(uaz, "a UAZ469 spawns");

		if (field && uaz)
			Check(ctx, field, uaz);

		if (uaz)
			SCR_EntityHelper.DeleteEntityAndChildren(uaz);

		if (field)
			field.ClearLocal();

		if (fieldEntity)
			SCR_EntityHelper.DeleteEntityAndChildren(fieldEntity);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, LF_WheatField field, IEntity uaz)
	{
		string reason;
		IEntity reaper = LF_ImplementHitch.Hitch(uaz, reason);
		ctx.NotNull(reaper, "the reaper hitches to the UAZ469 (" + reason + ")");
		LF_ImplementComponent implement = LF_ImplementComponent.Of(reaper);
		ctx.NotNull(implement, "the reaper carries its implement component");
		if (!implement)
			return;

		ctx.Equal(0, implement.GetHopperKg(), "a fresh hopper is empty");
		LF_ReaperCutState state = new LF_ReaperCutState();
		state.m_Reaper = reaper.GetID();

		// Row 0, x from the first cell's middle in 3 m steps, 4 m/s: every cell on the row is cut.
		int steps = 20;
		for (int i = 0; i < steps; i++)
		{
			Drive(uaz, 11.5 + 3 * i, 0, 4);
			if (!RedProof())
				LF_ReaperCutTick.TickOne(state, reaper);
		}

		ctx.Equal(steps, field.CutCount(), "the 20 cells of the row are cut");
		ctx.Equal(steps * LF_WheatFieldRules.CELL_YIELD_KG, implement.GetHopperKg(), "the hopper holds cells times yield");

		// A second pass over the same row finds nothing standing.
		Drive(uaz, 11.5, 0, 4);
		LF_ReaperCutTick.TickOne(state, reaper);
		ctx.Equal(steps, field.CutCount(), "a second pass over cut ground cuts nothing more");

		// Row 1 above 6 m/s: nothing is cut, nothing is added.
		int hopper = implement.GetHopperKg();
		for (int j = 0; j < 5; j++)
		{
			Drive(uaz, 11.5 + 3 * j, 1, 8);
			LF_ReaperCutTick.TickOne(state, reaper);
		}

		ctx.Equal(steps, field.CutCount(), "above 6 m/s nothing is cut");
		ctx.Equal(hopper, implement.GetHopperKg(), "above 6 m/s the hopper does not change");
		ctx.True(state.m_eLast == LF_EHarvestRefusal.TOO_FAST, "the refusal is TOO_FAST");

		// Stopped on a standing cell: nothing is cut.
		Drive(uaz, 11.5, 1, 0);
		LF_ReaperCutTick.TickOne(state, reaper);
		ctx.Equal(steps, field.CutCount(), "a stopped vehicle cuts nothing");
		ctx.True(state.m_eLast == LF_EHarvestRefusal.STOPPED, "the refusal is STOPPED");

		// A full hopper: row 1 standing cells stay standing.
		implement.SetHopperKg(LF_WheatFieldRules.HOPPER_CAP_KG);
		Drive(uaz, 11.5, 1, 4);
		LF_ReaperCutTick.TickOne(state, reaper);
		ctx.Equal(steps, field.CutCount(), "a full hopper stops cutting");
		ctx.Equal(LF_WheatFieldRules.HOPPER_CAP_KG, implement.GetHopperKg(), "a full hopper stays at its cap");
		ctx.True(state.m_eLast == LF_EHarvestRefusal.HOPPER_FULL, "the refusal is HOPPER_FULL");
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the reaper's bar on cell column `x` (world metres, the cell's middle) of `row`, the vehicle heading
	//! east at `speed` m/s. The hitch row puts the reaper 3.4 m behind the vehicle's origin.
	protected void Drive(IEntity uaz, float barX, int row, float speed)
	{
		LF_HitchRow hitch = LF_ImplementHitch.RowFor(uaz);
		float behind = 3;
		if (hitch)
			behind = -hitch.m_vOffset[2];

		vector at = Vector(barX + behind, 0, ROW_Z_BASE + 3 * row);
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]) + 0.3;
		uaz.SetOrigin(at);
		Physics physics = uaz.GetPhysics();
		if (physics)
			physics.SetVelocity(Vector(speed, 0, 0));
	}
}
