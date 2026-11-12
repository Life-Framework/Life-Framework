// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only LF_Test_CarryMeshes` and
// LF_CivilianJob.PARCEL_PREFAB pointed back at the bare HandCarryItem_Base.et (the old cargo):
// [LFTEST] FAIL items/carry-meshes
// [LFTEST]   - the courier parcel has a mesh
// Restored to LF_Parcel.et; green.

// tier: WORLD
//! Cargo a player carries in their hands has a mesh. The bare HandCarryItem_Base has none,
//! so the parcel and the bank bag were invisible in the hands (#155).
class LF_Test_CarryMeshes : LF_Test
{
	override string GetName()
	{
		return "items/carry-meshes";
	}

	override void Run(LF_TestContext ctx)
	{
		ExpectMesh(ctx, LF_CivilianJob.PARCEL_PREFAB, "the courier parcel");
		ExpectMesh(ctx, LF_CivilianJob.FUEL_PREFAB, "the fuel can");
		ExpectMesh(ctx, LF_Bank.BAG_PREFAB, "the bank bag");

		IEntity placed = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.BANK_BAG);
		ctx.True(placed != null, "Annex_Bank_Bag is placed");
		if (placed)
		{
			ctx.True(placed.GetVObject() != null, "the placed bank bag has a mesh");
			ctx.NotNull(LF_Component<LF_HandCarryComponent>.Find(placed), "the placed bank bag is carried in the hands");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ExpectMesh(LF_TestContext ctx, ResourceName prefab, string label)
	{
		Resource resource = Resource.Load(prefab);
		ctx.True(resource.IsValid(), label + " prefab loads");
		if (!resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity item = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		ctx.True(item != null, label + " spawns");
		if (!item)
			return;

		ctx.True(item.GetVObject() != null, label + " has a mesh");
		SCR_EntityHelper.DeleteEntityAndChildren(item);
	}
}
