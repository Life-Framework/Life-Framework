// red-proof: Observed 2026-11-27 in the batched `tools\cli test --no-build --red` run of the six round-5 action tests: `RED PROOF OK (all 6 listed test(s) fail with the hook on)`; here `the fee was taken once (expected 100, got 150)`, `the apple is back with the owner (expected 1, got 0)`. Green with `--only`.
// tier: WORLD
//! lockers/lost-and-found-action (#3457, coverage round 5): the "Reclaim your things (fee)" prompt on the
//! lost-and-found box. The perform finds the box from the acting character's own position and the owner from
//! their own record key: a stranger with nothing waiting is refused and loses no cash (and does not see the
//! prompt on the server); the owner one short of the fee takes nothing and leaves the things waiting; with
//! the fee the owner pays once and gets the item back; a second perform finds nothing and takes no second fee.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LostAndFoundAction : LF_Test
{
	protected static const string KEY = "lost-and-found-action-owner";
	protected static const string STRANGER_KEY = "lost-and-found-action-stranger";
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName BOX = "{6C0E8D3B1A5F9247}Prefabs/Props/Lockers/LF_LostAndFoundBox.et";
	protected static const int RENTED_AT = 1700000000;
	protected static const int PERIOD_DAYS = 7;
	protected static const int GRACE_DAYS = 2;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "lockers/lost-and-found-action";
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
		string site;
		if (owner && stranger && box)
			site = Check(ctx, owner, stranger, box);

		LF_Utils.SetUidForTests(null, "");
		if (box)
			SCR_EntityHelper.DeleteEntityAndChildren(box);
		if (owner)
			LF_TestCharacterFixture.Delete(owner);
		if (stranger)
			LF_TestCharacterFixture.Delete(stranger);

		if (!site.IsEmpty())
		{
			LF_StorageSnapshot.Clear(KEY, LF_LockerStorageManagerComponent.SlotFor(site));
			LF_StorageSnapshot.Clear(KEY, LF_LostAndFound.SlotFor(site));
			LF_StorageSnapshot.Clear(STRANGER_KEY, LF_LostAndFound.SlotFor(site));
		}

		LF_StorageSnapshot.Clear(KEY, LF_StorageSnapshot.SLOT_CARRIED);
		LF_LockerBank.ApplyAll(kept);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The box's site id, for the cleanup.
	protected string Check(LF_TestContext ctx, IEntity owner, IEntity stranger, IEntity box)
	{
		LF_LostAndFoundBoxComponent component = LF_Component<LF_LostAndFoundBoxComponent>.Find(box);
		LF_LostAndFoundReclaimAction action = LF_LostAndFoundReclaimAction.Cast(LF_TestActionFinder.Find(box, LF_LostAndFoundReclaimAction));
		ctx.NotNull(component, "the box carries LF_LostAndFoundBoxComponent");
		ctx.NotNull(action, "the box carries the reclaim action");
		if (!component || !action)
			return "";

		string site = component.GetSiteId();
		vector at = owner.GetOrigin();
		vector bank = at + "1 0 0";
		float reach = 2.5;
		int fee = LF_LockerConfig.ReclaimFee();
		ctx.True(fee > 0, "reclaiming costs something");

		LF_MoneyUtils.RemoveCash(owner, LF_MoneyUtils.GetCash(owner));
		LF_MoneyUtils.AddCash(owner, fee + 100);
		LF_MoneyUtils.RemoveCash(stranger, LF_MoneyUtils.GetCash(stranger));
		LF_MoneyUtils.AddCash(stranger, fee + 100);
		ctx.Equal(LF_ELockerBankResult.RENTED, LF_LockerBank.RentAt(RENTED_AT, KEY, owner, at, site, bank, reach, 100, PERIOD_DAYS), "the owner rents a locker long ago");

		// One apple goes into the locker through the real open and close, then the rent lapses.
		IEntity container;
		ctx.Equal(LF_ELockerOpenResult.OPENED, LF_LockerStorageManagerComponent.OpenAt(owner, KEY, site, at, bank, reach, container), "the owner opens their locker");
		if (!container)
			return site;

		ctx.Equal(1, LF_InventoryUtils.AddAmount(container, APPLE, 1), "an apple goes into the locker");
		LF_LockerStorageManagerComponent manager = LF_LockerStorageManagerComponent.FindOpen(site, KEY);
		if (manager)
			manager.Release("test close");

		int lapsedNow = RENTED_AT + 10 * LF_LockerRules.SECONDS_PER_DAY;
		ctx.True(LF_LostAndFound.MoveLapsed(lapsedNow, site, KEY, PERIOD_DAYS, GRACE_DAYS), "the lapsed locker is freed");
		ctx.True(LF_LostAndFound.HasThings(KEY, site), "the owner has an apple waiting");

		// The stranger stands where the owner stands (inside the box's reach) with nothing waiting.
		stranger.SetOrigin(at);
		LF_Utils.SetUidForTests(stranger, STRANGER_KEY);
		ctx.False(action.CanBeShownScript(stranger), "a stranger with nothing waiting does not see the prompt");
		ctx.True(action.CanBePerformedScript(stranger), "the prompt itself is performable and the server decides");
		action.PerformAction(box, stranger);
		ctx.Equal(fee + 100, LF_MoneyUtils.GetCash(stranger), "a stranger's perform takes no cash");
		ctx.True(LF_LostAndFound.HasThings(KEY, site), "a stranger's perform leaves the owner's things waiting");

		// The owner short of the fee: refused, nothing taken, things still waiting.
		LF_Utils.SetUidForTests(owner, KEY);
		ctx.True(action.CanBeShownScript(owner), "the owner with things waiting sees the prompt");
		LF_MoneyUtils.RemoveCash(owner, LF_MoneyUtils.GetCash(owner));
		LF_MoneyUtils.AddCash(owner, fee - 1);
		int appleBefore = Carried(owner, APPLE);
		action.PerformAction(box, owner);
		ctx.Equal(fee - 1, LF_MoneyUtils.GetCash(owner), "short of the fee takes nothing");
		ctx.True(LF_LostAndFound.HasThings(KEY, site), "short of the fee leaves the things waiting");
		ctx.Equal(appleBefore, Carried(owner, APPLE), "short of the fee hands over no apple");

		// A forged perform with no character moves nothing.
		action.PerformAction(box, null);
		ctx.True(LF_LostAndFound.HasThings(KEY, site), "a perform with no character leaves the things waiting");

		// With the fee the owner takes the apple back, once.
		LF_MoneyUtils.AddCash(owner, 101);
		if (!RedProof())
			action.PerformAction(box, owner);

		ctx.Equal(100, LF_MoneyUtils.GetCash(owner), "the fee was taken once");
		ctx.Equal(appleBefore + 1, Carried(owner, APPLE), "the apple is back with the owner");
		ctx.False(LF_LostAndFound.HasThings(KEY, site), "nothing waits after the reclaim");

		action.PerformAction(box, owner);
		ctx.Equal(100, LF_MoneyUtils.GetCash(owner), "a second perform takes no second fee");
		ctx.Equal(appleBefore + 1, Carried(owner, APPLE), "a second perform hands over nothing more");

		LF_MoneyUtils.RemoveCash(owner, LF_MoneyUtils.GetCash(owner));
		LF_MoneyUtils.RemoveCash(stranger, LF_MoneyUtils.GetCash(stranger));
		return site;
	}

	//------------------------------------------------------------------------------------------------
	//! How many of a prefab the character carries anywhere in their storage tree.
	protected int Carried(IEntity character, ResourceName prefab)
	{
		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Capture(KEY, "lost-and-found-action-probe", character);
		if (!record)
			return -1;

		return record.CountPrefab(prefab);
	}
}
