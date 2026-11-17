// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_LogoutBodyDirty,...`: RED-OK rejoin/logout-body-dirty, with the loot skipped "only the looted body is captured again (expected 1, got 0)" and "the looted body's snapshot lost the bottle (expected 0, got 1)" failed. Green with the fix (--only, 8 passed).
// tier: WORLD
//! #2404: an item taken from one logout body re-captures that body's snapshot only. Before, any item
//! change on any kept body re-captured every kept body (RecaptureAll), so looting one body item by
//! item while dozens were kept serialised all of them per item.
class LF_Test_LogoutBodyDirty : LF_Test
{
	protected static const int BODIES = 3;

	override string GetName()
	{
		return "rejoin/logout-body-dirty";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_LogoutBody.ResetStatics();
		array<IEntity> bodies = {};
		for (int i = 0; i < BODIES; i++)
		{
			IEntity body = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
			if (!body)
			{
				Cleanup(bodies);
				return;
			}

			bodies.Insert(body);
			ctx.True(LF_LogoutBody.KeepBody(Uid(i), CharacterId(i), body, LF_LogoutBody.Now() + 300), "body " + i + " is kept");
		}

		// Loot the middle body: its own listener marks it, nobody else's.
		IEntity looted = bodies[1];
		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(looted);
		array<IEntity> water = LF_InventoryUtils.FindItemsByPrefab(manager, LF_TestCarriedInventoryKit.WATER_PREFAB);
		ctx.Equal(1, water.Count(), "the looted body carries the water bottle");
		int before = LF_LogoutBody.GetCaptureCount();
		if (!water.IsEmpty() && !RedProof())
			ctx.True(manager.TryDeleteItem(water[0]), "the water bottle is taken");

		ctx.Equal(1, LF_LogoutBody.RecaptureDirty(), "only the looted body is captured again");
		ctx.Equal(before + 1, LF_LogoutBody.GetCaptureCount(), "one snapshot written for one item taken");
		ctx.Equal(0, CountInSnapshot(CharacterId(1), LF_TestCarriedInventoryKit.WATER_PREFAB), "the looted body's snapshot lost the bottle");
		ctx.Equal(1, CountInSnapshot(CharacterId(0), LF_TestCarriedInventoryKit.WATER_PREFAB), "an untouched body keeps its bottle");
		ctx.Equal(0, LF_LogoutBody.RecaptureDirty(), "nothing is left to capture");

		// A body that left is not captured when its stale mark comes in.
		LF_LogoutBody.ScheduleRecapture(Uid(2));
		LF_LogoutBody.Release(Uid(2), "test");
		ctx.Equal(0, LF_LogoutBody.RecaptureDirty(), "a released body's mark captures nothing");

		Cleanup(bodies);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(array<IEntity> bodies)
	{
		for (int i = 0; i < BODIES; i++)
		{
			LF_LogoutBody.Release(Uid(i), "test end");
			LF_StorageSnapshot.Clear(CharacterId(i), LF_StorageSnapshot.SLOT_CARRIED);
		}

		foreach (IEntity body : bodies)
		{
			if (body && !body.IsDeleted())
				LF_TestCharacterFixture.Delete(body);
		}

		LF_LogoutBody.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected static string Uid(int i)
	{
		return "logout-dirty-uid-" + i.ToString();
	}

	//------------------------------------------------------------------------------------------------
	protected static string CharacterId(int i)
	{
		return "logout-dirty-character-" + i.ToString();
	}

	//------------------------------------------------------------------------------------------------
	protected int CountInSnapshot(string characterId, ResourceName prefab)
	{
		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Get(characterId, LF_StorageSnapshot.SLOT_CARRIED);
		if (!record || !record.m_aItems)
			return 0;

		int count;
		foreach (LF_StorageItemRecord item : record.m_aItems)
		{
			if (item && LF_Utils.SamePrefab(item.m_sPrefab, prefab))
				count += item.m_iQuantity;
		}

		return count;
	}
}
