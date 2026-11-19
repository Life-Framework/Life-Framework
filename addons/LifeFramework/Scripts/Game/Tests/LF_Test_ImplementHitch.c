// red-proof: Observed 2026-11-19 with `--no-build --red LF_Test_ImplementHitch`: `RED-OK harvest/implement-hitch (fails with the hook on, 5 assertions)` (the hitch skipped, no reaper).
//! harvest/implement-hitch (#3284): a reaper hitches to a UAZ469 as a rigid child at the config row's local
//! offset, a vehicle that is not on the hitch list is refused with a reason (and a log line), a second hitch on
//! the same vehicle is refused, and unhitching deletes the reaper. The test deletes both vehicles.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ImplementHitch : LF_Test
{
	protected static const ResourceName UAZ = "{C8B16ABCA57679A2}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et";
	protected static const ResourceName NOT_LISTED = "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et";
	protected static const vector SPOT = "70 0 40";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "harvest/implement-hitch";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity uaz = SpawnAt(UAZ, SPOT);
		IEntity car = SpawnAt(NOT_LISTED, SPOT + "12 0 0");
		ctx.NotNull(uaz, "a UAZ469 spawns");
		ctx.NotNull(car, "an S105 spawns");
		if (uaz && car)
			Check(ctx, uaz, car);

		if (uaz)
			SCR_EntityHelper.DeleteEntityAndChildren(uaz);

		if (car)
			SCR_EntityHelper.DeleteEntityAndChildren(car);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, IEntity uaz, IEntity car)
	{
		vector mins, maxs;
		uaz.GetBounds(mins, maxs);
		LF_Debug.Info("Harvest", string.Format("test: UAZ469 bounds %1 .. %2", mins, maxs));

		LF_HitchRow row = LF_ImplementHitch.RowFor(uaz);
		ctx.NotNull(row, "the UAZ469 variant finds its hitch row through its prefab ancestry");
		if (!row)
			return;

		string reason;
		IEntity reaper;
		if (!RedProof())
			reaper = LF_ImplementHitch.Hitch(uaz, reason);

		ctx.NotNull(reaper, "the reaper hitches to the UAZ469 (reason: " + reason + ")");
		if (!reaper)
			return;

		ctx.True(reaper.GetParent() == uaz, "the reaper's parent is the vehicle");
		ctx.True(LF_ImplementHitch.FindHitched(uaz) == reaper, "FindHitched returns the reaper");
		vector local[4];
		reaper.GetLocalTransform(local);
		ctx.EqualFloat(row.m_vOffset[0], local[3][0], 0.01, "local offset X matches the config row");
		ctx.EqualFloat(row.m_vOffset[1], local[3][1], 0.01, "local offset Y matches the config row");
		ctx.EqualFloat(row.m_vOffset[2], local[3][2], 0.01, "local offset Z matches the config row");
		ctx.True(row.m_vOffset[2] < mins[2], "the hitch point is behind the vehicle's rear bound");
		ctx.True(reaper.GetPhysics() == null, "the reaper has no physics body, so it never drags the vehicle");

		string again;
		ctx.True(LF_ImplementHitch.Hitch(uaz, again) == null, "a second hitch on the same vehicle is refused");
		ctx.True(again == "already towing an implement", "the second hitch says why: " + again);

		string refused;
		IEntity none = LF_ImplementHitch.Hitch(car, refused);
		ctx.True(none == null, "an S105 is not on the hitch list and is refused");
		ctx.True(refused == "not on the hitch list", "the refusal says why (logged on [LFDebug:Harvest]): " + refused);
		ctx.True(LF_ImplementHitch.FindHitched(car) == null, "nothing hangs off the refused car");

		ctx.True(LF_ImplementHitch.Unhitch(uaz), "unhitch succeeds");
		ctx.True(LF_ImplementHitch.FindHitched(uaz) == null, "after unhitching the vehicle tows nothing");
		ctx.False(LF_ImplementHitch.Unhitch(uaz), "a second unhitch is refused");
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnAt(ResourceName prefab, vector at)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		return LF_Utils.SpawnEntityPrefab(prefab, at);
	}
}
