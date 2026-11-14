// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_ParcelDescription`
// and the LF_ParcelComponent removed from LF_Parcel.et:
// FAIL the description names the destination: #LF-Item_Parcel_Desc (raw key, no destination)

// tier: WORLD
//! A spawned courier parcel describes where it goes.
class LF_Test_ParcelDescription : LF_Test
{
	override string GetName()
	{
		return "jobs/parcel-description";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource resource = Resource.Load(LF_CivilianJob.ParcelPrefab());
		ctx.True(resource.IsValid(), "the parcel prefab loads");
		if (!resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity parcel = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		ctx.True(parcel != null, "the parcel spawns");
		if (!parcel)
			return;

		UIInfo info = LF_UIInfoUtils.GetInfo(parcel);
		ctx.NotNull(info, "the parcel has UI info");
		if (info)
		{
			string dest = WidgetManager.Translate("#LF-Job_ParcelDest");
			string text = info.GetDescription();
			ctx.True(text.Contains(dest), "the description names the destination: " + text);
		}
		SCR_EntityHelper.DeleteEntityAndChildren(parcel);
	}
}
