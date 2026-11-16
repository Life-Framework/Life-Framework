// red-proof: Observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_LogoutBodyRejoin`: RED PROOF OK, with TakeBack skipped `the rejoin takes the body back`, `it is the same body`, `it is no longer a logout body`, `the save holds what the body holds (expected 0, got 1)` fail. Green after (`--only`, with LF_Test_LogoutBody).
// tier: WORLD
//! Acceptance of #1975: a rejoin inside the window takes back the same body with what is left of
//! its inventory (and nothing restored on top); after the window there is no body and the normal
//! rejoin rule spawns; the hospital comes first and ends the body; a body killed while its player
//! was away takes the ordinary death path. The real two-client rejoin is manual and a session proof.
class LF_Test_LogoutBodyRejoin : LF_Test
{
	protected static const string UID = "logout-rejoin-uid";
	protected static const string CHARACTER_ID = "logout-rejoin-character";

	override string GetName()
	{
		return "rejoin/logout-body-takeback";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_LogoutBody.ResetStatics();
		LF_HospitalRespawn.Reset();
		LF_StorageSnapshot.Clear(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);

		ctx.True(LF_LogoutBodyRejoin.Applies(true, false, false), "a clean rejoin takes the body back");
		ctx.False(LF_LogoutBodyRejoin.Applies(true, true, false), "jail comes first");
		ctx.False(LF_LogoutBodyRejoin.Applies(true, false, true), "hospital comes first");
		ctx.False(LF_LogoutBodyRejoin.Applies(false, false, false), "no body, no take-back");

		// 1. Inside the window: the same body, robbed, comes back as it is.
		IEntity body = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		ctx.NotNull(body, "the character spawns loaded");
		if (!body)
			return;

		EntityID bodyId = body.GetID();
		LF_LogoutBody.KeepBody(UID, CHARACTER_ID, body, LF_LogoutBody.Now() + 300);
		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(body);
		array<IEntity> water = LF_InventoryUtils.FindItemsByPrefab(manager, LF_TestCarriedInventoryKit.WATER_PREFAB);
		if (!water.IsEmpty())
			ctx.True(manager.TryDeleteItem(water[0]), "the water bottle is looted");

		IEntity taken;
		if (!RedProof())
			taken = LF_LogoutBodyRejoin.TakeBack(UID);

		ctx.NotNull(taken, "the rejoin takes the body back");
		ctx.True(taken && taken.GetID() == bodyId, "it is the same body");
		ctx.False(IsGone(bodyId), "the body was not deleted");
		ctx.False(LF_LogoutBody.HasBody(UID), "it is no longer a logout body");
		ctx.Equal(0, LF_InventoryUtils.FindItemsByPrefab(manager, LF_TestCarriedInventoryKit.WATER_PREFAB).Count(), "the looted bottle stays gone");
		ctx.True(LF_InventoryUtils.FindItemsByPrefab(manager, LF_TestCarriedInventoryKit.APPLE_PREFAB).Count() > 0, "what was left is still on it");
		ctx.Equal(0, CountInSnapshot(LF_TestCarriedInventoryKit.WATER_PREFAB), "the save holds what the body holds");
		ctx.True(!LF_LogoutBodyRejoin.TakeBack(UID), "a second take-back finds nothing (no second body)");
		LF_TestCharacterFixture.Delete(body);

		// 2. After the window: no body, the normal rejoin rule spawns with the snapshot.
		IEntity second = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (second)
		{
			float endsAt = LF_LogoutBody.Now() + 300;
			LF_LogoutBody.KeepBody(UID, CHARACTER_ID, second, endsAt);
			LF_LogoutBody.Sweep(endsAt + 1);
			ctx.True(!LF_LogoutBodyRejoin.TakeBack(UID), "after the window there is no body to take back");
			ctx.True(LF_StorageSnapshot.Has(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED), "the normal rejoin restores the snapshot");
		}

		// 3. The hospital comes first: the body ends, the snapshot is all the rejoin gets.
		IEntity third = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (third)
		{
			EntityID thirdId = third.GetID();
			LF_LogoutBody.KeepBody(UID, CHARACTER_ID, third, LF_LogoutBody.Now() + 300);
			LF_HospitalRespawn.MarkDied(UID);
			ctx.True(!LF_LogoutBodyRejoin.TakeBack(UID), "a pending hospital wake does not take the body back");
			ctx.True(IsGone(thirdId), "the body ended, so there is never a second one");
			ctx.True(LF_StorageSnapshot.Has(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED), "the hospital spawn restores what the body held");
			LF_HospitalRespawn.Reset();
		}

		// 4. Killed while away: the ordinary death path (the next spawn wakes in hospital).
		IEntity fourth = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (fourth)
		{
			LF_LogoutBody.KeepBody(UID, CHARACTER_ID, fourth, LF_LogoutBody.Now() + 300);
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(fourth.FindComponent(SCR_CharacterDamageManagerComponent));
			ctx.NotNull(damage, "the body has a damage manager");
			if (damage)
				damage.Kill(Instigator.CreateInstigator(null));

			ctx.True(LF_HospitalRespawn.HasPendingWake(UID), "a body killed while away wakes its player in hospital");
			LF_LogoutBody.Sweep(LF_LogoutBody.Now());
			ctx.True(!LF_LogoutBodyRejoin.TakeBack(UID), "a dead body is never taken back");
			ctx.False(LF_StorageSnapshot.Has(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED), "its items are loot, not given back");
			LF_TestCharacterFixture.Delete(fourth);
		}

		LF_HospitalRespawn.Reset();
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
