// red-proof: 2026-11-21 `node tools/cli.mjs test --no-build --only LF_Test_CashVanMoveBounds` on the unfixed MoveVan (no Update()): FAIL crime/cash-van-move-bounds "no query at the old stop finds the van", "a query at the new stop finds the van". Update() added: green.
//! crime/cash-van-move-bounds: after the cash van jumps to its next stop, a vehicle query along the
//! road it skipped finds nothing. On the island (2026-11-21 release walk) the van's stale bounds
//! stretched from its old stop to its new one, across the car lot's pads, and the second player's
//! car purchase was refused with "no free pad for roadmap-lot".
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CashVanMoveBounds : LF_Test
{
	protected static const ResourceName VAN = "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et";

	override string GetName()
	{
		return "crime/cash-van-move-bounds";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity anchor = LF_TestCharacterFixture.Spawn(ctx);
		if (!anchor)
			return;

		vector transform[4];
		anchor.GetTransform(transform);
		LF_TestCharacterFixture.Delete(anchor);

		Resource res = Resource.Load(VAN);
		ctx.True(res && res.IsValid(), "the van prefab resolves");
		if (!res || !res.IsValid())
			return;

		vector start = transform[3] + "0 0.5 40";
		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixCopy(transform, params.Transform);
		params.Transform[3] = start;
		IEntity van = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(van, "the van spawned");
		if (!van)
			return;

		vector stop[4];
		Math3D.MatrixCopy(transform, stop);
		stop[3] = start + "200 0 0";
		LF_CashVanRoute.MoveVan(van, stop);

		ctx.True(vector.Distance(van.GetOrigin(), stop[3]) < 0.5, "the van is at the new stop");
		ctx.False(Finds(start + "100 0 0", van), "no query on the road between the stops finds the van");
		ctx.False(Finds(start, van), "no query at the old stop finds the van");
		ctx.True(Finds(stop[3], van), "a query at the new stop finds the van");

		SCR_EntityHelper.DeleteEntityAndChildren(van);
	}

	//------------------------------------------------------------------------------------------------
	protected bool Finds(vector at, IEntity van)
	{
		array<IEntity> nearby = LF_Utils.GetNearbyVehicles(at, 3);
		return nearby && nearby.Find(van) >= 0;
	}
}
