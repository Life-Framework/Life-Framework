// red-proof: Observed 2026-11-16 with `toolsi test --no-build --red LF_Test_LogoutBody`: RED PROOF OK, with KeepBody skipped `the window closes (expected 1, got 0)`, `the body left the world` and `the rejoin still gets what the body held` fail. Green after. The real disconnect is `toolsi session logout-body-proof` (PASS 2026-11-16 on lf-testbox).
// tier: WORLD
//! Acceptance of #1973 (`rejoin/logout-body`): after a simulated disconnect the body stays, its
//! inventory can be taken and the account snapshot follows it (no dupe on the rejoin), it
//! despawns when the window closes or the player returns, and a killed body drops the snapshot.
//! The real disconnect (two clients) and the restart are manual and wt e2e.
class LF_Test_LogoutBody : LF_Test
{
	protected static const string UID = "logout-body-uid";
	protected static const string CHARACTER_ID = "logout-body-character";

	override string GetName()
	{
		return "rejoin/logout-body";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_LogoutBody.ResetStatics();
		LF_StorageSnapshot.Clear(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.Equal(300, LF_LogoutBody.WindowFor(false, 300, 600), "a standing logout keeps 5 minutes");
		ctx.Equal(600, LF_LogoutBody.WindowFor(true, 300, 600), "a downed logout keeps longer");

		// 1. Logout: the body stays and the snapshot is the body's.
		IEntity body = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		ctx.NotNull(body, "the character spawns loaded");
		if (!body)
			return;

		EntityID bodyId = body.GetID();
		float endsAt = LF_LogoutBody.Now() + 300;
		if (!RedProof())
			LF_LogoutBody.KeepBody(UID, CHARACTER_ID, body, endsAt);

		ctx.True(LF_LogoutBody.HasBody(UID), "the logout leaves a body");
		ctx.True(LF_LogoutBody.IsLoggedOut(body), "the body carries the Logged out tag");
		ctx.Equal(1, CountInSnapshot(LF_TestCarriedInventoryKit.WATER_PREFAB), "the snapshot holds the water bottle");

		// 2. Loot: the water bottle leaves the body, and the snapshot with it.
		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(body);
		array<IEntity> water = LF_InventoryUtils.FindItemsByPrefab(manager, LF_TestCarriedInventoryKit.WATER_PREFAB);
		ctx.Equal(1, water.Count(), "the body carries the water bottle");
		if (!water.IsEmpty())
			ctx.True(manager.TryDeleteItem(water[0]), "the water bottle is taken");

		LF_LogoutBody.RecaptureAll();
		ctx.Equal(0, CountInSnapshot(LF_TestCarriedInventoryKit.WATER_PREFAB), "the taken bottle is not given back on the rejoin");
		ctx.True(CountInSnapshot(LF_TestCarriedInventoryKit.APPLE_PREFAB) > 0, "what was left on the body still is");

		// 3. The window: still there before it closes, gone after.
		ctx.Equal(0, LF_LogoutBody.Sweep(endsAt - 1), "the body stays inside the window");
		ctx.True(LF_LogoutBody.HasBody(UID), "still kept before the window closes");
		ctx.Equal(1, LF_LogoutBody.Sweep(endsAt + 1), "the window closes");
		ctx.False(LF_LogoutBody.HasBody(UID), "the body is no longer tracked");
		ctx.True(IsGone(bodyId), "the body left the world");
		ctx.True(LF_StorageSnapshot.Has(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED), "the rejoin still gets what the body held");

		// 4. The player returns inside the window: the body leaves at once.
		IEntity second = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (second)
		{
			EntityID secondId = second.GetID();
			LF_LogoutBody.KeepBody(UID, CHARACTER_ID, second, LF_LogoutBody.Now() + 300);
			ctx.True(LF_LogoutBody.Release(UID, "rejoined"), "the rejoin ends the body");
			ctx.True(IsGone(secondId), "the returned player's body left the world");
		}

		// 5. Killed while logged out: an ordinary death, the snapshot drops (the loot is on the corpse).
		IEntity third = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (third)
		{
			LF_LogoutBody.KeepBody(UID, CHARACTER_ID, third, LF_LogoutBody.Now() + 300);
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(third.FindComponent(SCR_CharacterDamageManagerComponent));
			ctx.NotNull(damage, "the body has a damage manager");
			if (damage)
				damage.Kill(Instigator.CreateInstigator(null));

			LF_LogoutBody.Sweep(LF_LogoutBody.Now());
			ctx.False(LF_LogoutBody.HasBody(UID), "a killed body is no longer a logout body");
			ctx.False(LF_StorageSnapshot.Has(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED), "the dead body's items are not given back on the rejoin");
			LF_TestCharacterFixture.Delete(third);
		}

		LF_StorageSnapshot.Clear(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
		LF_LogoutBody.ResetStatics();
	}

	protected int CountInSnapshot(ResourceName prefab)
	{
		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Get(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
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

	protected bool IsGone(EntityID id)
	{
		IEntity entity = GetGame().GetWorld().FindEntityByID(id);
		return !entity || entity.IsDeleted();
	}
}
