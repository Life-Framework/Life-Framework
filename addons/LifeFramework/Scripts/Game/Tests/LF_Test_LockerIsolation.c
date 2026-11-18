// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_LockerRoundtrip,LF_Test_LockerIsolation`: `RED PROOF OK` (the first renter's store skipped, so `the first renter's locker holds the handgun` fails). An earlier green run failed `- the second character opens their locker (expected 0, got 2)` when the test used the wrong bank position; fixed in the test.
// tier: WORLD
//! lockers/isolation (#2661): a second character opening the same bank sees nothing of the first's items; a
//! character who rents no locker there is refused and no container is built; out of reach is refused; the
//! engine's access validation refuses another character's inventory and lets the renter's own through.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LockerIsolation : LF_Test
{
	protected static const string KEY_A = "locker-isolation-test-a";
	protected static const string KEY_B = "locker-isolation-test-b";
	protected static const string KEY_C = "locker-isolation-test-c";
	protected static const string SITE = "locker-isolation-test-site";
	protected static const ResourceName HANDGUN = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "lockers/isolation";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_LockerRentalRow> kept = LF_LockerBank.ExportAll();
		LF_LockerBank.ResetStatics();

		IEntity a = LF_TestCharacterFixture.Spawn(ctx);
		IEntity b = LF_TestCharacterFixture.Spawn(ctx);
		if (a && b)
			Check(ctx, a, b);

		if (a)
			LF_TestCharacterFixture.Delete(a);
		if (b)
			LF_TestCharacterFixture.Delete(b);

		string slot = LF_LockerStorageManagerComponent.SlotFor(SITE);
		LF_StorageSnapshot.Clear(KEY_A, slot);
		LF_StorageSnapshot.Clear(KEY_B, slot);
		LF_LockerBank.ApplyAll(kept);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, IEntity a, IEntity b)
	{
		vector at = a.GetOrigin();
		vector bank = at + "1 0 0";
		float reach = 2.5;
		string slot = LF_LockerStorageManagerComponent.SlotFor(SITE);

		LF_MoneyUtils.RemoveCash(a, LF_MoneyUtils.GetCash(a));
		LF_MoneyUtils.AddCash(a, 100);
		LF_MoneyUtils.RemoveCash(b, LF_MoneyUtils.GetCash(b));
		LF_MoneyUtils.AddCash(b, 100);
		ctx.Equal(LF_ELockerBankResult.RENTED, LF_LockerBank.RentAt(1800000000, KEY_A, a, at, SITE, bank, reach, 100, 7), "the first character rents here");

		IEntity containerA;
		ctx.Equal(LF_ELockerOpenResult.OPENED, LF_LockerStorageManagerComponent.OpenAt(a, KEY_A, SITE, at, bank, reach, containerA), "the first character opens their locker");
		if (!containerA)
			return;

		if (!RedProof())
			LF_InventoryUtils.AddAmount(containerA, HANDGUN, 1);

		ctx.Equal(1, LF_Test_LockerRoundtrip.Count(containerA, HANDGUN), "the first renter's locker holds the handgun");

		// While the first locker is open: a character with no rental is refused and nothing is built.
		int openBefore = LF_LockerStorageManagerComponent.OpenCount();
		IEntity refused;
		ctx.Equal(LF_ELockerOpenResult.NOT_RENTER, LF_LockerStorageManagerComponent.OpenAt(b, KEY_B, SITE, at, bank, reach, refused), "a non-renter is refused");
		ctx.True(refused == null, "a refused open builds no container");
		ctx.Equal(openBefore, LF_LockerStorageManagerComponent.OpenCount(), "a refused open leaves no open locker");

		// The first renter walked away from the bank: out of reach is refused.
		IEntity far;
		ctx.Equal(LF_ELockerOpenResult.OUT_OF_REACH, LF_LockerStorageManagerComponent.OpenAt(a, KEY_A, SITE, at + "20 0 0", bank, reach, far), "out of reach is refused");

		// The engine's access gate: the renter's own inventory passes, another character's does not.
		LF_LockerStorageManagerComponent managerA = LF_LockerStorageManagerComponent.FindOpen(SITE, KEY_A);
		ctx.NotNull(managerA, "the first locker is open");
		InventoryStorageManagerComponent inventoryA = LF_InventoryUtils.GetResponsibleStorageManager(a);
		InventoryStorageManagerComponent inventoryB = LF_InventoryUtils.GetResponsibleStorageManager(b);
		if (managerA && inventoryA && inventoryB)
		{
			ctx.True(managerA.IsRenterManager(inventoryA), "the renter's own inventory is the renter's");
			ctx.False(managerA.IsRenterManager(inventoryB), "another character's inventory is not");
		}

		// B rents too and opens the same bank: an empty locker, A's untouched.
		ctx.Equal(LF_ELockerBankResult.RENTED, LF_LockerBank.RentAt(1800000000, KEY_B, b, at, SITE, bank, reach, 100, 7), "the second character rents here");
		LF_LockerStorageManagerComponent.FindOpen(SITE, KEY_A).Release("test close");

		IEntity containerB;
		ctx.Equal(LF_ELockerOpenResult.OPENED, LF_LockerStorageManagerComponent.OpenAt(b, KEY_B, SITE, b.GetOrigin(), b.GetOrigin() + "1 0 0", reach, containerB), "the second character opens their locker");
		if (!containerB)
			return;

		ctx.Equal(0, LF_Test_LockerRoundtrip.CountItems(containerB), "the second character sees nothing of the first's");
		ctx.False(LF_StorageSnapshot.Has(KEY_B, slot) && LF_StorageSnapshot.Get(KEY_B, slot).m_aItems.Count() > 0, "no snapshot of the second character holds the first's items");

		LF_LockerStorageManagerComponent.FindOpen(SITE, KEY_B).Release("test close");
		LF_StorageSnapshotRecord stored = LF_StorageSnapshot.Get(KEY_A, slot);
		ctx.NotNull(stored, "the first character's snapshot is still stored");
		if (stored)
			ctx.Equal(1, stored.CountPrefab(HANDGUN), "the first character's handgun is still theirs");
	}
}
