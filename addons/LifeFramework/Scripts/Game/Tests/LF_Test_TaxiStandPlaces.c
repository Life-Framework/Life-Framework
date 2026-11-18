// red-proof: 2026-11-17, `--red LF_Test_TaxiStandPlaces` skips spawning the stands: RED-OK jobs/taxi-stand-places (fails with the hook on, 12 assertions: "two stands registered as kind taxi (expected 2, got 0)", "Nearest finds a stand beside A"); `--only` green. `--tier all` then failed "expected 2, got 3" once the grid cell's own stand existed; the count is now relative to the stands already placed.
//! jobs/taxi-stand-places: a taxi stand registers in LF_Places as kind `taxi`; Nearest picks the closer
//! of two stands and names it. Stand A stands beside a vanilla map location and takes its name; stand B
//! is far from every location and falls back to its place id. The fixtures sit north of the annex, clear
//! of the grid lattice (same spot as LF_Test_DispatchNamesPlace, which deletes its own fixtures).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_TaxiStandPlaces : LF_Test
{
	protected static const ResourceName STAND = "{2FD0DE951C3DC94E}Prefabs/Jobs/LF_TaxiStand.et";
	protected static const ResourceName LOCATION = "{A99A1ADAF98F2D48}Prefabs/World/Locations/Arland/Arleville.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/taxi-stand-places";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		float groundA = GetGame().GetWorld().GetSurfaceY(340, 320);
		float groundB = GetGame().GetWorld().GetSurfaceY(340, 2000);
		vector posA = Vector(345, groundA, 325);
		vector posB = Vector(340, groundB, 2000);

		IEntity location = LF_Utils.SpawnEntityPrefab(LOCATION, Vector(340, groundA, 320));
		ctx.NotNull(location, "the location label spawned");
		string locationName;
		if (location)
		{
			SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(location.FindComponent(SCR_EditableEntityComponent));
			if (editable)
				locationName = editable.GetDisplayName();
		}
		ctx.False(locationName.IsEmpty(), "the location has a name");

		// The grid's own taxi stand (the look cell) is already placed; count only the two spawned here.
		array<IEntity> before = {};
		LF_Places.FindAll(LF_TaxiStand.PLACE_KIND, before);
		int baseline = before.Count();

		IEntity standA;
		IEntity standB;
		if (!RedProof())
		{
			standA = LF_Utils.SpawnEntityPrefab(STAND, posA);
			standB = LF_Utils.SpawnEntityPrefab(STAND, posB);
		}

		ctx.NotNull(standA, "stand A spawned");
		ctx.NotNull(standB, "stand B spawned");

		array<IEntity> placed = {};
		LF_Places.FindAll(LF_TaxiStand.PLACE_KIND, placed);
		ctx.Equal(baseline + 2, placed.Count(), "two more stands registered as kind taxi");

		LF_TaxiStandComponent nearA = LF_TaxiStandComponent.Nearest(Vector(posA[0] + 10, groundA, posA[2]));
		ctx.NotNull(nearA, "Nearest finds a stand beside A");
		if (nearA)
		{
			ctx.True(nearA.GetOwner() == standA, "beside A, the closer stand is A");
			ctx.EqualStr(locationName, nearA.GetLabel(), "stand A is named after the location beside it");
		}

		LF_TaxiStandComponent nearB = LF_TaxiStandComponent.Nearest(Vector(posB[0] - 10, groundB, posB[2]));
		ctx.NotNull(nearB, "Nearest finds a stand beside B");
		if (nearB)
		{
			ctx.True(nearB.GetOwner() == standB, "beside B, the closer stand is B");
			ctx.False(nearB.GetLabel().IsEmpty(), "stand B still has a label");
			ctx.False(nearB.GetLabel() == locationName, "stand B is not named after the far location");
		}

		if (standA)
			SCR_EntityHelper.DeleteEntityAndChildren(standA);

		if (standB)
			SCR_EntityHelper.DeleteEntityAndChildren(standB);

		if (location)
			SCR_EntityHelper.DeleteEntityAndChildren(location);
	}
}
