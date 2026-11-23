// red-proof: Observed 2026-11-19 with `tools\cli test --no-build --red LF_Test_LostAndFound`: `RED PROOF OK` with the lapse step skipped; it failed `the locker holds nothing after the lapse`, `the owner has things waiting` and `the owner's things are untouched by the stranger`.
// red-proof (#4323): 2026-11-23 `test --only LF_Test_LostAndFound` on the unfixed test failed `a stranger reclaims nothing (expected 1, got 3)`: the stranger spawned at <73.90,0.11,108.18>, the box at <76.41,0.11,108.47>, 2.5 m apart, so the reclaim was OUT_OF_REACH. Green with the stranger standing at the owner's position.
// tier: WORLD
//! lockers/lost-and-found (#3457): a locker that lapsed with two items in it is freed and its contents wait in
//! the site's lost-and-found. The owner, short of the fee, is refused and nothing moves; with the fee they take
//! both items back once, and a second try finds nothing. A stranger has nothing waiting there, is refused and
//! loses no cash.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LostAndFound : LF_Test
{
	protected static const string KEY = "lost-and-found-test-owner";
	protected static const string STRANGER_KEY = "lost-and-found-test-stranger";
	protected static const string SITE = "lost-and-found-test-site";
	protected static const ResourceName HANDGUN = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName BOX = "{6C0E8D3B1A5F9247}Prefabs/Props/Lockers/LF_LostAndFoundBox.et";
	protected static const int RENTED_AT = 1700000000;
	protected static const int PERIOD_DAYS = 7;
	protected static const int GRACE_DAYS = 2;
	protected static const int FEE = 50;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "lockers/lost-and-found";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_LockerRentalRow> kept = LF_LockerBank.ExportAll();
		LF_LockerBank.ResetStatics();

		IEntity owner = LF_TestCharacterFixture.Spawn(ctx);
		IEntity stranger = LF_TestCharacterFixture.Spawn(ctx);
		IEntity box;
		if (owner)
			box = LF_Utils.SpawnEntityPrefab(BOX, owner.GetOrigin() + "1 0 0");

		ctx.NotNull(box, "the lost-and-found box spawns");
		if (owner && stranger && box)
			Check(ctx, owner, stranger, box);

		if (box)
			SCR_EntityHelper.DeleteEntityAndChildren(box);
		if (owner)
			LF_TestCharacterFixture.Delete(owner);
		if (stranger)
			LF_TestCharacterFixture.Delete(stranger);

		LF_StorageSnapshot.Clear(KEY, LF_LockerStorageManagerComponent.SlotFor(SITE));
		LF_StorageSnapshot.Clear(KEY, LF_LostAndFound.SlotFor(SITE));
		LF_StorageSnapshot.Clear(KEY, LF_StorageSnapshot.SLOT_CARRIED);
		LF_StorageSnapshot.Clear(STRANGER_KEY, LF_LostAndFound.SlotFor(SITE));
		LF_LockerBank.ApplyAll(kept);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, IEntity owner, IEntity stranger, IEntity box)
	{
		vector at = owner.GetOrigin();
		vector bank = at + "1 0 0";
		float reach = 2.5;
		string lockerSlot = LF_LockerStorageManagerComponent.SlotFor(SITE);
		string waitingSlot = LF_LostAndFound.SlotFor(SITE);

		LF_MoneyUtils.RemoveCash(owner, LF_MoneyUtils.GetCash(owner));
		LF_MoneyUtils.AddCash(owner, 100);
		LF_MoneyUtils.RemoveCash(stranger, LF_MoneyUtils.GetCash(stranger));
		LF_MoneyUtils.AddCash(stranger, 100);
		ctx.Equal(LF_ELockerBankResult.RENTED, LF_LockerBank.RentAt(RENTED_AT, KEY, owner, at, SITE, bank, reach, 100, PERIOD_DAYS), "the owner rents a locker long ago");

		// Two items go into the locker through the real open and close.
		IEntity container;
		ctx.Equal(LF_ELockerOpenResult.OPENED, LF_LockerStorageManagerComponent.OpenAt(owner, KEY, SITE, at, bank, reach, container), "the owner opens their locker");
		if (!container)
			return;

		ctx.Equal(1, LF_InventoryUtils.AddAmount(container, HANDGUN, 1), "a handgun goes into the locker");
		ctx.Equal(1, LF_InventoryUtils.AddAmount(container, APPLE, 1), "an apple goes into the locker");
		LF_LockerStorageManagerComponent manager = LF_LockerStorageManagerComponent.FindOpen(SITE, KEY);
		if (manager)
			manager.Release("test close");

		LF_StorageSnapshotRecord stored = LF_StorageSnapshot.Get(KEY, lockerSlot);
		ctx.NotNull(stored, "the close stored the locker's contents");
		ctx.False(LF_LostAndFound.MoveLapsed(RENTED_AT + 3 * LF_LockerRules.SECONDS_PER_DAY, SITE, KEY, PERIOD_DAYS, GRACE_DAYS), "a locker still paid up is not lapsed");
		ctx.True(LF_LockerBank.IsRenting(SITE, KEY), "the paid-up locker is still rented");

		// The rent lapses: paid until 7 days, grace 2 more. The locker is freed, the contents wait.
		int lapsedNow = RENTED_AT + 10 * LF_LockerRules.SECONDS_PER_DAY;
		if (!RedProof())
			ctx.True(LF_LostAndFound.MoveLapsed(lapsedNow, SITE, KEY, PERIOD_DAYS, GRACE_DAYS), "the lapsed locker is freed");

		ctx.False(LF_LockerBank.IsRenting(SITE, KEY), "a lapsed locker is no longer rented");
		ctx.False(LF_StorageSnapshot.Has(KEY, lockerSlot), "the locker holds nothing after the lapse");
		ctx.True(LF_LostAndFound.HasThings(KEY, SITE),"the owner has things waiting");
		LF_StorageSnapshotRecord waiting = LF_StorageSnapshot.Get(KEY, waitingSlot);
		if (waiting)
		{
			ctx.Equal(1, waiting.CountPrefab(HANDGUN), "the handgun waits in lost-and-found");
			ctx.Equal(1, waiting.CountPrefab(APPLE), "the apple waits in lost-and-found");
		}

		// A stranger sees nothing, is refused and keeps their cash. They stand where the owner stands, in reach of the
		// box: both fixture characters spawn on the gate and the engine shoves them apart by an arbitrary amount, so
		// their own origin could land beyond the 2.5 m reach and the refusal would be OUT_OF_REACH (#4323).
		ctx.False(LF_LostAndFound.HasThings(STRANGER_KEY, SITE), "a stranger has nothing waiting");
		ctx.Equal(LF_ELostAndFoundResult.NOTHING_TO_RECLAIM, LF_LostAndFound.ReclaimAt(lapsedNow, stranger, STRANGER_KEY, SITE, at, box, reach, FEE, PERIOD_DAYS, GRACE_DAYS), "a stranger reclaims nothing");
		ctx.Equal(100, LF_MoneyUtils.GetCash(stranger), "the stranger keeps their cash");
		ctx.True(LF_LostAndFound.HasThings(KEY, SITE), "the owner's things are untouched by the stranger");

		// Short of the fee: refused, nothing taken, nothing moved.
		LF_MoneyUtils.RemoveCash(owner, LF_MoneyUtils.GetCash(owner));
		LF_MoneyUtils.AddCash(owner, 10);
		int appleBefore = Carried(owner, APPLE);
		int gunBefore = Carried(owner, HANDGUN);
		ctx.Equal(LF_ELostAndFoundResult.CANNOT_AFFORD, LF_LostAndFound.ReclaimAt(lapsedNow, owner, KEY, SITE, at, box, reach, FEE, PERIOD_DAYS, GRACE_DAYS), "short of the fee is refused");
		ctx.Equal(10, LF_MoneyUtils.GetCash(owner), "a refused reclaim takes nothing");
		ctx.True(LF_LostAndFound.HasThings(KEY, SITE), "a refused reclaim leaves the things waiting");
		ctx.Equal(appleBefore, Carried(owner, APPLE), "a refused reclaim hands over no apple");

		// Out of reach is refused too.
		LF_MoneyUtils.AddCash(owner, 90);
		ctx.Equal(LF_ELostAndFoundResult.OUT_OF_REACH, LF_LostAndFound.ReclaimAt(lapsedNow, owner, KEY, SITE, at + "40 0 0", box, reach, FEE, PERIOD_DAYS, GRACE_DAYS), "out of reach is refused");

		// With the fee the owner takes both back, once.
		ctx.Equal(LF_ELostAndFoundResult.RECLAIMED, LF_LostAndFound.ReclaimAt(lapsedNow, owner, KEY, SITE, at, box, reach, FEE, PERIOD_DAYS, GRACE_DAYS), "the owner reclaims for the fee");
		ctx.Equal(50, LF_MoneyUtils.GetCash(owner), "the fee was taken");
		ctx.Equal(appleBefore + 1, Carried(owner, APPLE), "the apple is back with the owner");
		ctx.Equal(gunBefore + 1, Carried(owner, HANDGUN), "the handgun is back with the owner");
		ctx.False(LF_LostAndFound.HasThings(KEY, SITE), "nothing waits after the reclaim");
		ctx.Equal(0, LF_Test_LockerRoundtrip.CountItems(box), "the box holds nothing after the reclaim");

		ctx.Equal(LF_ELostAndFoundResult.NOTHING_TO_RECLAIM, LF_LostAndFound.ReclaimAt(lapsedNow, owner, KEY, SITE, at, box, reach, FEE, PERIOD_DAYS, GRACE_DAYS), "a second reclaim finds nothing");
		ctx.Equal(50, LF_MoneyUtils.GetCash(owner), "the second reclaim takes no second fee");
	}

	//------------------------------------------------------------------------------------------------
	//! How many of a prefab the character carries anywhere in their storage tree.
	protected int Carried(IEntity character, ResourceName prefab)
	{
		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Capture(KEY, "lost-and-found-test-probe", character);
		if (!record)
			return -1;

		return record.CountPrefab(prefab);
	}
}
