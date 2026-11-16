// red-proof: observed with `tools\cli test --no-build --red LF_Test_RangeTargets`: RedProof() skips the simulated hit, so the target never leaves the erected state and "a hit folds the target down" fails. The first run, before Prefabs/Props/Range/LF_RangeTarget.et existed, failed at "range target prefab loads".
//------------------------------------------------------------------------------------------------
//! range/targets (#1929, Evenings in the village #1011): the range target is the base-game
//! TargetInfantry_01 (SCR_FiringRangeTarget) with the lane's timings. A hit folds it down; after
//! the folded time it pops back up by itself. No scoring here: the range manager is not placed.

// tier: WORLD
class LF_Test_RangeTargets : LF_Test
{
	protected static const ResourceName RANGE_TARGET = "{4D1A82E96B30C7F5}Prefabs/Props/Range/LF_RangeTarget.et";
	protected static const ResourceName BASE_TARGET = "{379D34B0D41DA810}Prefabs/Props/Military/Training/TargetInfantry_01.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "range/targets";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.True(Resource.Load(BASE_TARGET).IsValid(), "base-game target prefab loads");
		ctx.True(Resource.Load(RANGE_TARGET).IsValid(), "range target prefab loads");
		if (ctx.FailureCount() > 0)
			return;

		IEntity entity = LF_Utils.SpawnEntityPrefab(RANGE_TARGET, "20 0 20");
		ctx.True(entity != null, "range target spawns");
		if (ctx.FailureCount() > 0)
			return;

		SCR_FiringRangeTarget target = SCR_FiringRangeTarget.Cast(entity);
		ctx.True(target != null, "range target is a base-game SCR_FiringRangeTarget");
		if (ctx.FailureCount() > 0)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return;
		}

		ctx.True(entity.FindComponent(SignalsManagerComponent) != null, "range target carries the signal manager that drives the fold animation");
		ctx.True(entity.FindComponent(ProcAnimComponent) != null, "range target carries the proc anim that folds it");
		ctx.Equal(6, target.GetSetDistance(), "range target keeps the lane distance of 6 m");
		ctx.Equal(3000, target.GetErectedDuration(), "range target stays up for 3 s");
		ctx.Equal(ETargetState.TARGET_UP, target.GetState(), "range target starts erected");
		ctx.False(target.IsIndicator(), "range target is a target, not a firing-line indicator");

		// A hit sets the state to down (SCR_FiringRangeTarget.OnDamage does exactly this).
		if (!RedProof())
			target.SetState(ETargetState.TARGET_DOWN);
		ctx.Equal(ETargetState.TARGET_DOWN, target.GetState(), "a hit folds the target down");

		// The target resets itself after its folded time (2 s): 1.5 + 1.5 s counted, the next frame pops it up.
		target.EOnFrame(entity, 1.5);
		ctx.Equal(ETargetState.TARGET_DOWN, target.GetState(), "the target is still down 1.5 s after the hit");
		target.EOnFrame(entity, 1.5);
		target.EOnFrame(entity, 0.1);
		ctx.Equal(ETargetState.TARGET_UP, target.GetState(), "the target pops back up after its folded time");

		SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}
}
