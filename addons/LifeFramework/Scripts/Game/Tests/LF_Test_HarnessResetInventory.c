// red-proof: harness/reset-inventory, observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_HarnessResetInventory`:
// commenting out the `LF_StorageSnapshot.Apply(officer, ...Create("kit", "empty"))` line in LF_DutyLocker.DressInKit failed it with
// `[LFTEST]   - the loaded character ends with exactly what a plain reset one has (expected 'cash=0 apples=2 water=1 backpacks=1', got 'cash=237 apples=5 water=2 backpacks=1')`,
// `SUMMARY tier=only passed=0 failed=1 total=1`. Restored; green.

// tier: WORLD
//! 2026-11-14: the harness `resetplayer` clocked a player out and dropped bills but left the
//! inventory as it was, so a proof inherited the last one's items. The server side of it is
//! LF_DutyLocker.ResetToCivilianKit: it empties the body, clothes included (the same step a clock-in
//! makes), and issues the civilian kit. The kit has its own cash, food and a backpack, so the proof
//! is that a loaded character ends up with exactly what a plain one gets, not that it holds nothing.
class LF_Test_HarnessResetInventory : LF_Test
{
	//------------------------------------------------------------------------------------------------
	protected static string Contents(IEntity character)
	{
		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!manager)
			return "no inventory";

		return string.Format("cash=%1 apples=%2 water=%3 backpacks=%4",
			LF_MoneyUtils.GetCash(character),
			LF_InventoryUtils.GetAmount(character, LF_TestCarriedInventoryKit.APPLE_PREFAB),
			LF_InventoryUtils.GetAmount(character, LF_TestCarriedInventoryKit.WATER_PREFAB),
			LF_InventoryUtils.FindItemsByPrefab(manager, LF_TestCarriedInventoryKit.BACKPACK_PREFAB).Count());
	}

	override string GetName()
	{
		return "harness/reset-inventory";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity plain = LF_TestCharacterFixture.Spawn(ctx);
		if (!plain)
			return;

		LF_DutyLocker.ResetToCivilianKit(null, plain);
		string baseline = Contents(plain);
		LF_TestCharacterFixture.Delete(plain);

		IEntity character = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!character || ctx.FailureCount() > 0)
		{
			LF_TestCharacterFixture.Delete(character);
			return;
		}

		ctx.Equal(1, LF_TestCarriedInventoryKit.CountInside(character, LF_TestCarriedInventoryKit.BACKPACK_PREFAB, LF_TestCarriedInventoryKit.WATER_PREFAB), "the water bottle sits in the loaded character's backpack");
		string loaded = Contents(character);
		ctx.True(loaded != baseline, string.Format("the loaded character differs from a reset one before the reset (%1)", loaded));

		int held = LF_DutyLocker.ResetToCivilianKit(null, character);
		ctx.True(held >= 5, string.Format("the reset counted what the body held (%1 item(s))", held));
		ctx.EqualStr(baseline, Contents(character), "the loaded character ends with exactly what a plain reset one has");
		ctx.Equal(0, LF_DutyLocker.ResetToCivilianKit(null, null), "no character, nothing to reset");
		LF_TestCharacterFixture.Delete(character);
	}
}
