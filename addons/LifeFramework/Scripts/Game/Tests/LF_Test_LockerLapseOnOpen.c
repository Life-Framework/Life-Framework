// red-proof: Observed 2026-11-19 with `tools\cli test --no-build --only LF_Test_LockerLapseOnOpen` before the fix (OpenAtTime without MoveLapsed, no ArmSweep in LF_LockerBankComponent): it failed `a lapsed renter is refused at the locker`, `no container opens for a lapsed renter`, `the lapsed locker's apple waits in lost-and-found` and `a locker bank with no box beside it starts the lapse sweep`; green after the fix.
//! lockers/lapse-on-open (#3457 review): a renter whose rent lapsed before the 5-minute sweep reached it is
//! refused at the locker and their things wait in the site's lost-and-found, instead of opening it for free.
//! A locker bank alone (no lost-and-found box placed) starts the lapse sweep.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LockerLapseOnOpen : LF_Test
{
	protected static const string KEY = "locker-lapse-open-test-owner";
	protected static const string SITE = "locker-lapse-open-test-site";
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName BANK = "{271A5DA917F4DF47}Prefabs/Props/Lockers/LF_LockerBank.et";
	protected static const int RENTED_AT = 1700000000;
	protected static const int PERIOD_DAYS = 7;
	protected static const int GRACE_DAYS = 2;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "lockers/lapse-on-open";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_LockerRentalRow> kept = LF_LockerBank.ExportAll();
		LF_LockerBank.ResetStatics();

		IEntity owner = LF_TestCharacterFixture.Spawn(ctx);
		if (owner)
		{
			CheckLapseOnOpen(ctx, owner);
			CheckBankArmsSweep(ctx, owner.GetOrigin() + "3 0 0");
			LF_TestCharacterFixture.Delete(owner);
		}

		LF_StorageSnapshot.Clear(KEY, LF_LockerStorageManagerComponent.SlotFor(SITE));
		LF_StorageSnapshot.Clear(KEY, LF_LostAndFound.SlotFor(SITE));
		LF_StorageSnapshot.Clear(KEY, LF_StorageSnapshot.SLOT_CARRIED);
		LF_LockerBank.ApplyAll(kept);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckLapseOnOpen(LF_TestContext ctx, IEntity owner)
	{
		vector at = owner.GetOrigin();
		vector bank = at + "1 0 0";
		float reach = 2.5;
		LF_MoneyUtils.AddCash(owner, 100);
		ctx.Equal(LF_ELockerBankResult.RENTED, LF_LockerBank.RentAt(RENTED_AT, KEY, owner, at, SITE, bank, reach, 100, PERIOD_DAYS), "the owner rents a locker long ago");

		IEntity container;
		ctx.Equal(LF_ELockerOpenResult.OPENED, LF_LockerStorageManagerComponent.OpenAtTime(RENTED_AT + LF_LockerRules.SECONDS_PER_DAY, owner, KEY, SITE, at, bank, reach, PERIOD_DAYS, GRACE_DAYS, container), "the paid-up owner opens their locker");
		if (!container)
			return;

		ctx.Equal(1, LF_InventoryUtils.AddAmount(container, APPLE, 1), "an apple goes into the locker");
		LF_LockerStorageManagerComponent manager = LF_LockerStorageManagerComponent.FindOpen(SITE, KEY);
		if (manager)
			manager.Release("test close");

		// Lapsed (7 days paid, 2 grace) and the sweep has not run: the open itself lapses it.
		IEntity lapsed;
		int lapsedNow = RENTED_AT + 10 * LF_LockerRules.SECONDS_PER_DAY;
		ctx.Equal(LF_ELockerOpenResult.NOT_RENTER, LF_LockerStorageManagerComponent.OpenAtTime(lapsedNow, owner, KEY, SITE, at, bank, reach, PERIOD_DAYS, GRACE_DAYS, lapsed), "a lapsed renter is refused at the locker");
		ctx.True(lapsed == null, "no container opens for a lapsed renter");
		if (lapsed)
			SCR_EntityHelper.DeleteEntityAndChildren(lapsed);

		ctx.True(LF_LostAndFound.HasThings(KEY, SITE), "the lapsed locker's apple waits in lost-and-found");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBankArmsSweep(LF_TestContext ctx, vector at)
	{
		LF_LostAndFound.ResetStatics();
		ctx.False(LF_LostAndFound.IsSweepArmed(), "the sweep is off after the reset");
		IEntity bank = LF_Utils.SpawnEntityPrefab(BANK, at);
		ctx.NotNull(bank, "a locker bank spawns");
		ctx.True(LF_LostAndFound.IsSweepArmed(), "a locker bank with no box beside it starts the lapse sweep");
		if (bank)
			SCR_EntityHelper.DeleteEntityAndChildren(bank);

		// Leave the sweep running for the rest of the world, as the placed banks and boxes armed it.
		LF_LostAndFound.ArmSweep();
	}
}
