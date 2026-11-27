// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_ParcelDescription`
// and the LF_ParcelComponent removed from LF_Parcel.et:
// FAIL the description names the destination: #LF-Item_Parcel_Desc (raw key, no destination)

// red-proof: observed 2026-11-27 (playtest round 2) with the string still reading "...east along the street to the porch past the
// brick-walled lock-up" and no %1: FAIL the run-start toast names the deposit (see the run log in the PR body).

//! A spawned courier parcel describes where it goes; the run-start toast says where the drop is and what the deposit is.
[LF_TestInfo(LF_TestTier.WORLD)]
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
			ctx.False(dest.Contains("lock-up"), "the destination is neutral core text, not a place on one map: " + dest);
		}

		// The run-start toast points at the marked stop (the drop is on a map in every pack, not "east along the street")
		// and says what the cash deposit taken at the counter is (#playtest-1127-r2: $100 became $75 with no word why).
		string started = WidgetManager.Translate("#LF-Job_RunStarted_CourierDeposit", "25");
		ctx.True(started.Contains("25"), "the run-start toast names the deposit: " + started);
		ctx.False(started.Contains("east along"), "the run-start toast names no direction or place of one map: " + started);
		SCR_EntityHelper.DeleteEntityAndChildren(parcel);
	}
}
