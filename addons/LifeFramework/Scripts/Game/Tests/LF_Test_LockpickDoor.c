// red-proof: 2026-11-10 `tools/cli test --no-build --only LF_Test_LockpickDoor` with the Wear(1, OPENED) expectation changed 0 to 1 failed `- the last use opens the lock and the pick is gone (expected 1, got 0)`. Restored; green.
// tier: LOGIC
//! Lockpicking rules (#411): skill shortens the attempt, a worn pick breaks, a non-pickable or open
//! door refuses, a plain failure wears nothing, and only a heard attempt files a report.
class LF_Test_LockpickDoor : LF_Test
{
	override string GetName()
	{
		return "lockpick/door";
	}

	override void Run(LF_TestContext ctx)
	{
		// Skill comes from the level and is clamped.
		ctx.Equal(0, LF_LockpickRules.SkillFromLevel(1), "a level 1 character has skill 0");
		ctx.Equal(4, LF_LockpickRules.SkillFromLevel(5), "level 5 is skill 4");
		ctx.Equal(LF_LockpickRules.MAX_SKILL, LF_LockpickRules.SkillFromLevel(99), "skill is capped");
		ctx.Equal(0, LF_LockpickRules.SkillFromLevel(-3), "a bad level is skill 0");

		// Time falls with skill.
		ctx.EqualFloat(LF_LockpickRules.BASE_SECONDS, LF_LockpickRules.Seconds(0), 0.001, "skill 0 takes the base time");
		ctx.EqualFloat(LF_LockpickRules.MIN_SECONDS, LF_LockpickRules.Seconds(LF_LockpickRules.MAX_SKILL), 0.001, "top skill takes the minimum time");
		ctx.True(LF_LockpickRules.Seconds(5) < LF_LockpickRules.Seconds(2), "a better skill is faster");
		ctx.True(LF_LockpickRules.SuccessChance(8) > LF_LockpickRules.SuccessChance(1), "a better skill succeeds more often");
		ctx.True(LF_LockpickRules.SuccessChance(99) <= LF_LockpickRules.MAX_SUCCESS, "success is capped below certain");
		ctx.True(LF_LockpickRules.SnapChance(8) < LF_LockpickRules.SnapChance(1), "a better skill snaps fewer picks");
		ctx.True(LF_LockpickRules.NoiseChance(8) < LF_LockpickRules.NoiseChance(1), "a better skill is quieter");

		// The roll decides the result.
		ctx.Equal(LF_ELockpickResult.OPENED, LF_LockpickRules.Resolve(0, 0.1, 0.9), "a low roll opens the lock");
		ctx.Equal(LF_ELockpickResult.FAILED, LF_LockpickRules.Resolve(0, 0.99, 0.99), "a high roll fails and the pick holds");
		ctx.Equal(LF_ELockpickResult.SNAPPED, LF_LockpickRules.Resolve(0, 0.99, 0.01), "a failure with a low snap roll snaps the pick");

		// A worn pick breaks.
		ctx.Equal(0, LF_LockpickRules.Wear(1, LF_ELockpickResult.OPENED), "the last use opens the lock and the pick is gone");
		ctx.Equal(2, LF_LockpickRules.Wear(3, LF_ELockpickResult.SNAPPED), "a snap wears one use");
		ctx.Equal(3, LF_LockpickRules.Wear(3, LF_ELockpickResult.FAILED), "a plain failure wears nothing");
		ctx.Equal(0, LF_LockpickRules.Wear(0, LF_ELockpickResult.SNAPPED), "wear never goes below zero");

		// Noise: a success is always heard, a failure only on a low noise roll.
		ctx.True(LF_LockpickRules.IsHeard(LF_ELockpickResult.OPENED, 10, 0.99), "an opened lock is always reported");
		ctx.True(LF_LockpickRules.IsHeard(LF_ELockpickResult.FAILED, 0, 0.01), "a loud failure is reported");
		ctx.False(LF_LockpickRules.IsHeard(LF_ELockpickResult.FAILED, 10, 0.99), "a quiet failure is not");

		// The gate.
		ctx.Equal(LF_ELockpickCheck.NONE, LF_LockpickRules.Check(true, true, true, false), "a locked pickable door with a pick is allowed");
		ctx.Equal(LF_ELockpickCheck.NOT_PICKABLE, LF_LockpickRules.Check(false, true, true, false), "a non-pickable target refuses");
		ctx.Equal(LF_ELockpickCheck.NOT_LOCKED, LF_LockpickRules.Check(true, false, true, false), "an open door refuses");
		ctx.Equal(LF_ELockpickCheck.NO_PICK, LF_LockpickRules.Check(true, true, false, false), "no pick refuses");
		ctx.Equal(LF_ELockpickCheck.BUSY, LF_LockpickRules.Check(true, true, true, true), "a door being picked refuses a second attempt");
		ctx.Equal(LF_ELockpickCheck.NOT_PICKABLE, LF_LockpickRules.Check(false, false, false, true), "not pickable wins over every other reason");
		ctx.True(LF_LockpickRules.RefusalKey(LF_ELockpickCheck.NOT_PICKABLE) != "", "every refusal has a line to print");
		ctx.EqualStr("", LF_LockpickRules.RefusalKey(LF_ELockpickCheck.NONE), "an allowed attempt prints no refusal");
	}
}

// red-proof: world wiring is asserted against the placed door; dropping m_bDebugLocked from the placed door failed `- without a pick the locked door says it needs one (expected 3, got 2)` on 2026-11-10 (first run, before the fix).
// tier: WORLD
//! The playtest lock-up door is wired (#411): locked, pickable, the pick action on it, the jail-style
//! breach door refuses a pick. Spawns nothing. The held attempt itself is the two-client session proof.
class LF_Test_LockpickDoorWorld : LF_Test
{
	override string GetName()
	{
		return "e2e/lockpick-door";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource pick = Resource.Load(LF_Lockpick.PICK_PREFAB);
		ctx.True(pick.IsValid(), "the lockpick prefab loads");

		IEntity door = GetGame().GetWorld().FindEntityByName("Roadmap_Lockpick_Door");
		ctx.True(door != null, "Roadmap_Lockpick_Door is placed");
		if (ctx.FailureCount() > 0)
			return;

		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(door);
		ctx.NotNull(doorLock, "the door carries LF_LockComponent");
		LF_LockpickTargetComponent target = LF_Component<LF_LockpickTargetComponent>.Find(door);
		ctx.NotNull(target, "the door carries LF_LockpickTargetComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(doorLock.IsLocked(), "the lock-up door starts locked");
		ctx.True(target.IsPickable(), "the lock-up door is pickable");
		ctx.Equal(LF_ELockpickCheck.NO_PICK, LF_Lockpick.CheckDoor(door, null), "without a pick the locked door says it needs one");
		ctx.False(LF_Lockpick.IsBusy(door), "nobody is picking it");

		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(door);
		ctx.NotNull(actions, "the door carries ActionsManagerComponent");
		if (ctx.FailureCount() > 0)
			return;

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		bool wired = false;
		foreach (BaseUserAction action : actionList)
		{
			if (LF_LockpickAction.Cast(action))
				wired = true;
		}

		ctx.True(wired, "the lockpick action is on the door");

		IEntity breachDoor = GetGame().GetWorld().FindEntityByName("Roadmap_Breach_Door");
		ctx.True(breachDoor != null, "Roadmap_Breach_Door is placed");
		if (breachDoor)
			ctx.Equal(LF_ELockpickCheck.NOT_PICKABLE, LF_Lockpick.CheckDoor(breachDoor, null), "a door with no pick target refuses a pick");
	}
}
