// red-proof: observed 2026-11-16 with `tools\cli test --no-build --only LF_Test_ParcelHandTool` on the unfixed LF_Parcel.et (HandCarryItem_Base gadget):
// FAIL - the parcel carries LF_HandToolComponent; - it is no longer a hand-carry gadget on the hands storage; - the carried parcel is a hand tool with id parcel. Fixed prefab; green.

// tier: WORLD
//! The courier parcel is a core hand tool (#1846): it takes the whole hand like the vault drill
//! (weapon quick slot, held by the wrench graph), not a hand-carry gadget on the hands storage.
//! The job gives it to a character, it goes into a 1-0 quick slot and the hands, and it still
//! reads as in a slot (not on the ground) wherever it is carried.
class LF_Test_ParcelHandTool : LF_Test
{
	override string GetName()
	{
		return "jobs/parcel-hand-tool";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource resource = Resource.Load(LF_CivilianJob.ParcelPrefab());
		ctx.True(resource.IsValid(), "the parcel prefab loads");
		if (!resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity bare = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		ctx.True(bare != null, "the parcel spawns");
		if (!bare)
			return;

		LF_HandToolComponent bareTool = LF_HandTool.Of(bare);
		ctx.NotNull(bareTool, "the parcel carries LF_HandToolComponent");
		ctx.True(LF_Component<LF_HandCarryComponent>.Find(bare) == null, "it is no longer a hand-carry gadget on the hands storage");
		ctx.NotNull(LF_Component<LF_ParcelComponent>.Find(bare), "it still carries the marker and description component");
		if (bareTool)
		{
			ctx.True(bareTool.GetToolId() == LF_ParcelComponent.TOOL_ID, "its tool id is parcel");
			ctx.True(bareTool.CanBeHeld(), "it can be held in the hands");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(bare);

		IEntity courier = LF_TestCharacterFixture.Spawn(ctx);
		if (!courier)
			return;

		ctx.True(LF_CivilianJob.GiveCargo(courier, LF_EJobType.PARCEL), "the job hands the parcel to a character in a jacket");
		IEntity parcel = LF_HandTool.Carried(courier, LF_ParcelComponent.TOOL_ID);
		ctx.NotNull(parcel, "the carried parcel is a hand tool with id parcel");
		if (parcel)
		{
			ctx.True(LF_ParcelComponent.InSlot(parcel), "the carried parcel is in a slot, not on the ground");
			ctx.True(LF_HandTool.QuickSlot(courier, parcel) >= 0, "it goes into a 1-0 quick slot");
			ctx.False(LF_HandTool.Holds(courier, LF_ParcelComponent.TOOL_ID), "in the pack it is not held");
			ctx.True(LF_HandTool.Equip(courier, parcel), "the quick slot equip is accepted");

			if (!RedProof())
				LF_TestVaultDrillFixture.Hold(courier, parcel, true);

			ctx.True(LF_HandTool.Holds(courier, LF_ParcelComponent.TOOL_ID), "in the hands it is held");
			ctx.False(LF_HandTool.Holds(courier, "drill"), "another tool id is not held");
		}

		LF_TestCharacterFixture.Delete(courier);
	}
}
