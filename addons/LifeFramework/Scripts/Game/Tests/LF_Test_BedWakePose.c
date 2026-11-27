// red-proof: observed 2026-11-19 with `tools/cli test --no-build --only LF_Test_BedWakePose` on the unfixed beds: "the first-spawn wake point is on the floor (y 0.3, floor 0)", "... beside the mattress, not on it (bed x 0 ...)" and both stand points at the bed height failed; green with LF_BedWakePose. #5153 (2026-11-27): the laid-patient check reads 0 m above the bed origin on the old GetWakeTransform (bed origin + a zero offset), 0.52 m on the fixed one.
//! The arrival bed float (2026-11-19): a body that wakes in a first-spawn or hospital bed is spawned
//! with its feet on the floor at the bed's free side, clear of the mattress, and steps off onto the
//! floor; never at the bed origin, which physics pushed up onto the mattress so the bed wake
//! animation played sitting and standing in the air above the bed. The beds float 0.3 m above the
//! ground here, so a point that kept the bed's height fails.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BedWakePose : LF_Test
{
	static const ResourceName FIRST_BED_PREFAB = "{15468371DAC524FC}Prefabs/Medic/LF_FirstSpawnBed.et";
	static const ResourceName HOSPITAL_BED_PREFAB = "{B643FBB194A8C8D9}Prefabs/Medic/LF_HospitalBed.et";
	static const float FLOAT = 0.3;
	static const float TOLERANCE = 0.03;

	override string GetName()
	{
		return "spawn/bed-wake-pose";
	}

	override void Run(LF_TestContext ctx)
	{
		BaseWorld world = GetGame().GetWorld();
		vector spot;
		ctx.True(FindOpenGround(spot), "fixture: open ground for the beds");
		IEntity firstBed = SpawnFloating(FIRST_BED_PREFAB, spot);
		IEntity hospitalBed = SpawnFloating(HOSPITAL_BED_PREFAB, spot + "4 0 0");
		ctx.True(firstBed != null, "fixture: a first-spawn bed spawns");
		ctx.True(hospitalBed != null, "fixture: a hospital bed spawns");
		if (!firstBed || !hospitalBed)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(firstBed);
			SCR_EntityHelper.DeleteEntityAndChildren(hospitalBed);
			return;
		}

		LF_FirstSpawnBedComponent first = LF_FirstSpawnBedComponent.Cast(firstBed.FindComponent(LF_FirstSpawnBedComponent));
		LF_HospitalBedComponent hospital = LF_HospitalBedComponent.Cast(hospitalBed.FindComponent(LF_HospitalBedComponent));

		// The first-spawn wake: feet on the ground, beside the mattress.
		vector wake[4];
		first.GetWakeTransform(wake);
		float ground = world.GetSurfaceY(wake[3][0], wake[3][2]);
		ctx.True(Math.AbsFloat(wake[3][1] - ground) <= TOLERANCE, string.Format("the first-spawn wake point is on the floor (y %1, floor %2)", wake[3][1], ground));
		vector mins, maxs;
		firstBed.GetBounds(mins, maxs);
		vector local = firstBed.CoordToLocal(wake[3]);
		ctx.True(Math.AbsFloat(local[0]) > maxs[0], string.Format("the first-spawn wake point is beside the mattress, not on it (bed x %1, half width %2)", local[0], maxs[0]));

		vector stand = first.GetStandPosition();
		float standGround = world.GetSurfaceY(stand[0], stand[2]);
		ctx.True(Math.AbsFloat(stand[1] - standGround) <= TOLERANCE, string.Format("the first-spawn stand point is on the floor (y %1, floor %2)", stand[1], standGround));

		// The hospital bed: its stand point too.
		vector hospitalStand = hospital.GetStandPosition();
		float hospitalGround = world.GetSurfaceY(hospitalStand[0], hospitalStand[2]);
		ctx.True(Math.AbsFloat(hospitalStand[1] - hospitalGround) <= TOLERANCE, string.Format("the hospital stand point is on the floor (y %1, floor %2)", hospitalStand[1], hospitalGround));

		// The hospital respawn wakes at the bed edge too, facing the mattress it sits up onto.
		vector sit[4];
		hospital.GetSitTransform(sit);
		float sitGround = world.GetSurfaceY(sit[3][0], sit[3][2]);
		ctx.True(Math.AbsFloat(sit[3][1] - sitGround) <= TOLERANCE, string.Format("the hospital wake point is on the floor (y %1, floor %2)", sit[3][1], sitGround));
		vector hospitalLocal = hospitalBed.CoordToLocal(sit[3]);
		ctx.True(Math.AbsFloat(hospitalLocal[0]) > maxs[0], "the hospital wake point is beside the mattress");
		vector away = sit[3] - hospitalBed.GetOrigin();
		away[1] = 0;
		away.Normalize();
		ctx.True(vector.Dot(sit[2], away) < -0.9, "the waking body faces the bed");

		// A patient laid in the hospital bed lies on the mattress top, never at the bed origin on the
		// floor under the frame (#5153), and has a floor spot to step down to beside the bed.
		vector lay[4];
		hospital.GetWakeTransform(lay);
		vector hospitalMins, hospitalMaxs;
		hospitalBed.GetBounds(hospitalMins, hospitalMaxs);
		float layHeight = lay[3][1] - hospitalBed.GetOrigin()[1];
		ctx.True(layHeight >= hospitalMaxs[1] * 0.5 && layHeight <= hospitalMaxs[1] + 0.2, string.Format("a patient laid in the hospital bed lies on the mattress (%1 m above the bed origin, bed top %2)", layHeight, hospitalMaxs[1]));
		vector stepSpot;
		ctx.True(hospital.FindStandSpot(null, stepSpot), "the hospital bed has a floor spot to step down to");
		ctx.True(Math.AbsFloat(stepSpot[1] - world.GetSurfaceY(stepSpot[0], stepSpot[2])) <= TOLERANCE, "the step-down spot is on the floor");

		// The sit point in bed space: the side of the stand offset, kept off the bed ends.
		vector bedMins = "-0.58 0 -1.25";
		vector bedMaxs = "0.58 0.66 1.25";
		vector left = LF_BedWakePose.SitLocal(bedMins, bedMaxs, "-0.85 0 0");
		ctx.True(Math.AbsFloat(left[0] + 0.58 + LF_BedWakePose.SIT_OUT) < 0.001 && Math.AbsFloat(left[2]) < 0.001, string.Format("a left stand offset sits up on the left side (%1)", left));
		vector corner = LF_BedWakePose.SitLocal(bedMins, bedMaxs, "0.9 0 -1");
		ctx.True(Math.AbsFloat(corner[0] - 0.58 - LF_BedWakePose.SIT_OUT) < 0.001 && Math.AbsFloat(corner[2] + 0.8) < 0.001, string.Format("a corner stand offset sits up on the right side, off the foot end (%1)", corner));

		// The step down: a body standing on the floor beside the bed stays; one left on the mattress
		// (the old wake, 0.5 m up) or carried away by the get-up (5.7 m in a real session) is moved.
		vector floorSpot = "10 2 10";
		ctx.False(LF_HospitalWake.NeedsStepDown("10.3 2.02 10.2", floorSpot), "a body on the floor beside the bed stays where it got up");
		ctx.True(LF_HospitalWake.NeedsStepDown("10 2.52 10.5", floorSpot), "a body standing on the mattress steps down");
		ctx.True(LF_HospitalWake.NeedsStepDown("10 2.86 15.7", floorSpot), "a body carried off by the get-up is put back beside the bed");

		SCR_EntityHelper.DeleteEntityAndChildren(firstBed);
		SCR_EntityHelper.DeleteEntityAndChildren(hospitalBed);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity SpawnFloating(ResourceName prefab, vector at)
	{
		float y = GetGame().GetWorld().GetSurfaceY(at[0], at[2]) + FLOAT;
		return LF_Utils.SpawnEntityPrefab(prefab, Vector(at[0], y, at[2]));
	}

	//------------------------------------------------------------------------------------------------
	//! A spot with terrain and nothing standing on it across both fixture beds (no town coordinates:
	//! open DebugWorld ground, the first free candidate).
	protected static bool FindOpenGround(out vector spot)
	{
		BaseWorld world = GetGame().GetWorld();
		array<vector> candidates = {"420 0 420", "380 0 460", "460 0 380", "200 0 330", "330 0 60"};
		foreach (vector candidate : candidates)
		{
			bool open = true;
			for (float dx = -1.5; dx <= 5.5; dx += 0.5)
			{
				for (float dz = -2; dz <= 2; dz += 1)
				{
					float x = candidate[0] + dx;
					float z = candidate[2] + dz;
					float ground = world.GetSurfaceY(x, z);
					IEntity hit;
					float d = LF_PlacementAudit.SurfaceTrace(Vector(x, ground + 3, z), Vector(x, ground - 1, z), null, hit);
					if (ground < -100 || Math.AbsFloat(ground + 3 - d - ground) > 0.02)
						open = false;
				}
			}

			if (open)
			{
				spot = candidate;
				return true;
			}
		}

		return false;
	}
}
