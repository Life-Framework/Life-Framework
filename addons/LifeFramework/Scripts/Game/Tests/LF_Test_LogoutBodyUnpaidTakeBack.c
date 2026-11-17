// red-proof: observed 2026-11-17 on the unfixed LF_LogoutBodyRejoin.TakeBack (no ReattachUnpaid), run beside LF_Test_PersistOwnersStaticReset: `the taken-back body still owes the store (expected 7, got 0)`. Green with ReattachUnpaid in TakeBack (`tools\cli test --no-build --only`).
// tier: WORLD
//! A shopper who logs out carrying unpaid goods has the tags parked under their character key
//! (LF_Stores.ParkCarrier). Rejoining inside the logout window takes the same body back, and the
//! parked tags must go back on it: before the fix only a fresh spawn reattached them, so a quick
//! logout and rejoin made the goods free.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LogoutBodyUnpaidTakeBack : LF_Test
{
	protected static const string UID = "logout-unpaid-uid";
	protected static const string CHARACTER_ID = "logout-unpaid-character";
	protected static const int PRICE = 7;

	override string GetName()
	{
		return "rejoin/logout-body-unpaid-tags";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity counter = GetGame().GetWorld().FindEntityByName(LF_Test_StoreShelf.GRID_STORE);
		ctx.NotNull(counter, "the grid store counter resolves");
		if (!counter)
			return;

		LF_StoreComponent store = LF_Component<LF_StoreComponent>.Find(counter);
		ctx.NotNull(store, "the counter carries LF_StoreComponent");
		if (!store)
			return;

		LF_LogoutBody.ResetStatics();
		LF_HospitalRespawn.Reset();
		array<ref LF_StoreUnpaidRow> parkedBefore = LF_Stores.CopyParked();

		IEntity body = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		ctx.NotNull(body, "the shopper spawns");
		if (!body)
		{
			LF_Stores.SetParked(parkedBefore);
			return;
		}

		// The shopper ate an apple they did not pay for, then logged out: the tag waits under their key.
		LF_StoreUnpaidRow row = new LF_StoreUnpaidRow();
		row.m_sStoreKey = store.GetStoreKey();
		row.m_sPrefab = LF_Test_StoreShelf.APPLE;
		row.m_iPrice = PRICE;
		row.m_sCarrierKey = UID;
		row.m_bConsumed = true;
		array<ref LF_StoreUnpaidRow> parked = {row};
		LF_Stores.SetParked(parked);
		LF_LogoutBody.KeepBody(UID, CHARACTER_ID, body, LF_LogoutBody.Now() + 300);

		IEntity taken = LF_LogoutBodyRejoin.TakeBack(UID);
		ctx.True(taken && taken == body, "the rejoin takes the same body back");
		ctx.Equal(0, LF_Stores.ParkedCount(), "the parked tag does not wait any more");
		ctx.Equal(PRICE, LF_Stores.OwedBy(store.GetStoreKey(), body), "the taken-back body still owes the store");

		LF_Stores.ReattachUnpaid(UID, body);
		LF_TestCharacterFixture.Delete(body);
		LF_Stores.SetParked(parkedBefore);
		LF_HospitalRespawn.Reset();
		LF_StorageSnapshot.Clear(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
		LF_LogoutBody.ResetStatics();
	}
}
