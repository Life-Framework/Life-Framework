// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_ParcelCarry` and CarrierIsDown(null) returning false:
// FAIL no carrier counts as down. Restored; green.

// tier: WORLD
//! A carried parcel is kept in the hands of a living carrier; a downed or dead one lets go.
//! A dropped parcel (not in any slot) is told apart from a carried one.
class LF_Test_ParcelCarry : LF_Test
{
	override string GetName()
	{
		return "jobs/parcel-carry";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_ParcelComponent.CarrierIsDown(null), "no carrier counts as down (nothing to keep it for)");

		Resource resource = Resource.Load(LF_CivilianJob.PARCEL_PREFAB);
		ctx.True(resource.IsValid(), "the parcel prefab loads");
		if (!resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity parcel = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		ctx.True(parcel != null, "the parcel spawns");
		if (!parcel)
			return;

		ctx.False(LF_ParcelComponent.InSlot(parcel), "a parcel on the ground is in no slot");
		ctx.NotNull(LF_Component<LF_ParcelComponent>.Find(parcel), "the parcel has the marker/settle component");
		SCR_EntityHelper.DeleteEntityAndChildren(parcel);
	}
}
