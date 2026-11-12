// red-proof: 2026-11-10 `tools/cli test --no-build --only LF_Test_LockpickKeepsPick` without the pick check in LF_Lockpick.Complete failed `- without the pick at the end the door stays locked`. Restored; green.
// tier: WORLD
//! A lockpick attempt needs the pick to the end (#411 review): a picker who drops or hands over the
//! pick while the timer runs does not open the lock. Drives the real attempt on the placed lock-up
//! door through the BeginAs and CompleteNow seams with a fixture character.
class LF_Test_LockpickKeepsPick : LF_Test
{
	override string GetName()
	{
		return "lockpick/keeps-pick";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity door = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.LOCKPICK_DOOR);
		ctx.True(door != null, "Annex_Lockpick_Door is placed");
		if (!door)
			return;

		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(door);
		ctx.NotNull(doorLock, "the door carries LF_LockComponent");
		if (!doorLock)
			return;

		bool wasLocked = doorLock.IsLocked();
		doorLock.SetLocked(true);

		IEntity picker = LF_TestCharacterFixture.Spawn(ctx);
		if (!picker)
		{
			doorLock.SetLocked(wasLocked);
			return;
		}

		picker.SetOrigin(door.GetOrigin() + "1 0 0");
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(picker);
		IEntity pack = LF_Utils.SpawnEntityPrefab("{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et", picker.GetOrigin());
		ctx.True(pack != null && inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "the picker wears a pack");
		int given = LF_InventoryUtils.AddAmount(picker, LF_Lockpick.PICK_PREFAB, 1);
		ctx.Equal(1, given, "the picker carries one lockpick");

		// Every roll opens the lock and the timer never runs out on its own.
		LF_Lockpick.s_fSecondsOverride = 600;
		LF_Lockpick.s_fRollOverride = 0;

		bool started = LF_Lockpick.BeginAs(door, picker, "test-lockpick-uid");
		ctx.True(started, "the attempt starts with the pick in hand");

		// The picker drops the pick while the attempt runs.
		LF_InventoryUtils.RemoveAmount(picker, LF_Lockpick.PICK_PREFAB, 1);
		ctx.False(LF_Lockpick.HasPick(picker), "the pick is gone from the picker");

		bool finished = LF_Lockpick.CompleteNow(door);
		ctx.True(finished, "the attempt was running and finished");
		ctx.True(doorLock.IsLocked(), "without the pick at the end the door stays locked");
		ctx.False(LF_Lockpick.IsBusy(door), "the attempt is gone after it ends");

		LF_Lockpick.s_fSecondsOverride = -1;
		LF_Lockpick.s_fRollOverride = -1;
		doorLock.SetLocked(wasLocked);
		LF_TestCharacterFixture.Delete(picker);
	}
}
