// red-proof: hideout/stash, observed 2026-11-08 with `tools/cli test --no-build --only
// EL_Test_HideoutStash,EL_Test_HideoutStashPersistence`: making
// LF_HideoutStashStorageManagerComponent.AllowsAccessFrom return true and dropping the AllowsUser
// check in LF_HideoutStashStorageComponent.CanStoreItem failed it with `[ELTEST] FAIL hideout/stash`,
// `- a non-member's inventory may not take from the stash`, `- a non-member cannot put an item in`,
// `- no water bottle in the stash (expected 0, got 1)`. Restored; green.
// red-proof: hideout/stash-save, same run: commenting out LF_StorageSnapshot.Set(record) in
// LF_PersistentStorageComponent.Capture failed it with `- a snapshot is stored under hideout:<id>`,
// `- the load restores the three items once (expected 3, got 0)`. Restored; green.

//------------------------------------------------------------------------------------------------
//! Shared helpers for the hideout stash tests and the restart e2e.
class EL_TestHideoutStashKit
{
	static const string STASH_NAME = "Roadmap_Hideout_Stash";
	static const string STASH_KEY = "hideout:Roadmap_Hideout";
	static const ResourceName MONEY_PREFAB = "{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et";
	static const ResourceName APPLE_PREFAB = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const ResourceName WATER_PREFAB = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";
	static const int SEEDED_CASH = 151;

	//------------------------------------------------------------------------------------------------
	static IEntity FindStash()
	{
		return GetGame().GetWorld().FindEntityByName(STASH_NAME);
	}

	//------------------------------------------------------------------------------------------------
	static BaseInventoryStorageComponent StorageOf(IEntity stash)
	{
		return EL_Component<LF_HideoutStashStorageComponent>.Find(stash);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Items directly in the stash storage.
	static int ItemCount(IEntity stash)
	{
		BaseInventoryStorageComponent storage = StorageOf(stash);
		if (!storage)
			return -1;

		array<IEntity> items = {};
		storage.GetAll(items, false);
		return items.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Sum of the prefab's quantity directly in the stash storage.
	static int Count(IEntity stash, ResourceName prefab)
	{
		BaseInventoryStorageComponent storage = StorageOf(stash);
		if (!storage)
			return -1;

		array<IEntity> items = {};
		storage.GetAll(items, false);
		int total;
		foreach (IEntity item : items)
		{
			if (EL_Utils.GetPrefabName(item) != prefab)
				continue;

			EL_QuantityComponent quantity = EL_QuantityComponent.Cast(item.FindComponent(EL_QuantityComponent));
			if (quantity)
				total += quantity.GetQuantity();
			else
				total++;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The first item of the prefab directly in the stash storage.
	static IEntity FindInStash(IEntity stash, ResourceName prefab)
	{
		BaseInventoryStorageComponent storage = StorageOf(stash);
		if (!storage)
			return null;

		array<IEntity> items = {};
		storage.GetAll(items, false);
		foreach (IEntity item : items)
		{
			if (EL_Utils.GetPrefabName(item) == prefab)
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static void Empty(IEntity stash)
	{
		LF_StorageSnapshot.Apply(stash, LF_StorageSnapshotRecord.Create("stash-test-empty", "empty"));
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the stash and puts three items in it: 151 cash, one apple, one water bottle.
	static void SeedThree(EL_TestContext ctx, IEntity stash)
	{
		Empty(stash);
		ctx.Equal(0, ItemCount(stash), "the stash is empty before seeding");
		ctx.Equal(SEEDED_CASH, EL_InventoryUtils.AddAmount(stash, MONEY_PREFAB, SEEDED_CASH), "151 cash goes into the stash");
		ctx.Equal(1, EL_InventoryUtils.AddAmount(stash, APPLE_PREFAB, 1), "an apple goes into the stash");
		ctx.Equal(1, EL_InventoryUtils.AddAmount(stash, WATER_PREFAB, 1), "a water bottle goes into the stash");
		ctx.Equal(3, ItemCount(stash), "the stash holds three items");
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the first item of the prefab from the character's inventory into the stash.
	static bool StoreFrom(IEntity character, IEntity stash, ResourceName prefab)
	{
		InventoryStorageManagerComponent manager = EL_InventoryUtils.GetResponsibleStorageManager(character);
		if (!manager)
			return false;

		array<IEntity> items = EL_InventoryUtils.FindItemsByPrefab(manager, prefab);
		if (items.IsEmpty())
			return false;

		return manager.TryMoveItemToStorage(items[0], StorageOf(stash));
	}

	//------------------------------------------------------------------------------------------------
	//! The character's inventory asks to take the first item of the prefab out of the stash.
	static bool TakeInto(IEntity character, IEntity stash, ResourceName prefab)
	{
		InventoryStorageManagerComponent manager = EL_InventoryUtils.GetResponsibleStorageManager(character);
		IEntity item = FindInStash(stash, prefab);
		if (!manager || !item)
			return false;

		BaseInventoryStorageComponent target = manager.FindStorageForItem(item, EStoragePurpose.PURPOSE_DEPOSIT);
		if (!target)
			return false;

		return manager.TryMoveItemToStorage(item, target);
	}
}

//------------------------------------------------------------------------------------------------
// tier: WORLD
class EL_Test_HideoutStash : EL_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "hideout/stash";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		IEntity stash = EL_TestHideoutStashKit.FindStash();
		ctx.NotNull(stash, "Roadmap_Hideout_Stash is placed");
		if (!stash)
			return;

		LF_HideoutStashStorageManagerComponent stashManager = EL_Component<LF_HideoutStashStorageManagerComponent>.Find(stash);
		LF_PersistentStorageComponent persistent = EL_Component<LF_PersistentStorageComponent>.Find(stash);
		ctx.NotNull(EL_TestHideoutStashKit.StorageOf(stash), "the stash is a real container (LF_HideoutStashStorageComponent)");
		ctx.NotNull(stashManager, "the stash carries LF_HideoutStashStorageManagerComponent");
		ctx.NotNull(persistent, "the stash carries LF_PersistentStorageComponent");
		ctx.True(stash.FindComponent(ActionsManagerComponent) != null, "the stash has an open action");
		if (ctx.FailureCount() > 0)
			return;

		ctx.EqualStr(LF_Hideout.HIDEOUT_ID, stashManager.GetHideoutId(), "the stash belongs to the roadmap hideout");
		ctx.EqualStr(EL_TestHideoutStashKit.STASH_KEY, persistent.GetStorageKey(), "the stash persists under hideout:<id>, not an entity id");

		LF_StorageSnapshotRecord before = LF_StorageSnapshot.Capture("stash-test", "before", stash);
		EL_OrganizationManager.Reset();
		LF_Hideout.Reset();

		string founder = "hideout-stash-founder";
		string member = "hideout-stash-member";
		string outsider = "hideout-stash-outsider";
		EL_Organization org = EL_OrganizationManager.Create(founder, "Stash Outfit");
		ctx.NotNull(org, "the owning organization exists");
		if (!org)
			return;

		ctx.True(EL_OrganizationManager.Invite(org.GetId(), founder, member), "founder adds a member");
		ctx.False(LF_Hideout.AllowsStash(member, LF_Hideout.HIDEOUT_ID, true), "an unowned hideout's stash opens for nobody");
		ctx.True(LF_Hideout.Assign(LF_Hideout.HIDEOUT_ID, org.GetId()), "the organization owns the hideout");

		ctx.False(LF_Hideout.AllowsStash(outsider, LF_Hideout.HIDEOUT_ID, true), "a non-member is refused at the stash");
		ctx.EqualStr("stash refused: not a member", LF_Hideout.GetLastRefusal(), "the stash refusal is logged");
		ctx.True(LF_Hideout.AllowsStash(member, LF_Hideout.HIDEOUT_ID, true), "a member opens the stash");
		ctx.True(LF_Hideout.TryUnlock(member, LF_Hideout.HIDEOUT_ID), "the hideout door stands open");
		ctx.False(LF_Hideout.AllowsStash(outsider, LF_Hideout.HIDEOUT_ID, true), "an open (or breached) door does not open the stash");

		IEntity memberCharacter = EL_TestCharacterFixture.Spawn(ctx);
		IEntity outsiderCharacter = EL_TestCharacterFixture.Spawn(ctx);
		if (memberCharacter && outsiderCharacter)
		{
			LF_Hideout.BindCharacter(memberCharacter, member);
			LF_Hideout.BindCharacter(outsiderCharacter, outsider);
			EL_TestHideoutStashKit.Empty(stash);
			ctx.Equal(0, EL_TestHideoutStashKit.ItemCount(stash), "the stash starts empty");

			// Member stores: the apple leaves the member's pockets and sits in the crate.
			ctx.Equal(1, EL_InventoryUtils.AddAmount(memberCharacter, EL_TestHideoutStashKit.APPLE_PREFAB, 1), "the member carries an apple");
			ctx.True(EL_TestHideoutStashKit.StoreFrom(memberCharacter, stash, EL_TestHideoutStashKit.APPLE_PREFAB), "the member stores the apple");
			ctx.Equal(1, EL_TestHideoutStashKit.Count(stash, EL_TestHideoutStashKit.APPLE_PREFAB), "the apple is in the stash");
			ctx.Equal(0, EL_InventoryUtils.GetAmount(memberCharacter, EL_TestHideoutStashKit.APPLE_PREFAB), "the apple left the member's pockets");

			// Non-member is refused both ways by the server. Taking out is the engine's access check on
			// a client inventory request; the decision it calls is checked here with the real managers.
			InventoryStorageManagerComponent outsiderManager = EL_InventoryUtils.GetResponsibleStorageManager(outsiderCharacter);
			InventoryStorageManagerComponent memberManager = EL_InventoryUtils.GetResponsibleStorageManager(memberCharacter);
			ctx.False(stashManager.AllowsAccessFrom(outsiderManager), "a non-member's inventory may not take from the stash");
			ctx.True(stashManager.AllowsAccessFrom(memberManager), "a member's inventory may take from the stash");
			ctx.Equal(1, EL_InventoryUtils.AddAmount(outsiderCharacter, EL_TestHideoutStashKit.WATER_PREFAB, 1), "the non-member carries a water bottle");
			ctx.False(EL_TestHideoutStashKit.StoreFrom(outsiderCharacter, stash, EL_TestHideoutStashKit.WATER_PREFAB), "a non-member cannot put an item in");
			ctx.Equal(0, EL_TestHideoutStashKit.Count(stash, EL_TestHideoutStashKit.WATER_PREFAB), "no water bottle in the stash");

			// Member retrieves.
			ctx.True(EL_TestHideoutStashKit.TakeInto(memberCharacter, stash, EL_TestHideoutStashKit.APPLE_PREFAB), "the member takes the apple back");
			ctx.Equal(0, EL_TestHideoutStashKit.Count(stash, EL_TestHideoutStashKit.APPLE_PREFAB), "the stash is empty again");
			ctx.Equal(1, EL_InventoryUtils.GetAmount(memberCharacter, EL_TestHideoutStashKit.APPLE_PREFAB), "the member carries the apple again");
		}

		EL_TestCharacterFixture.Delete(memberCharacter);
		EL_TestCharacterFixture.Delete(outsiderCharacter);
		if (before)
			LF_StorageSnapshot.Apply(stash, before);

		EL_OrganizationManager.Reset();
		LF_Hideout.Reset();
	}
}

//------------------------------------------------------------------------------------------------
// tier: PERSISTENCE
class EL_Test_HideoutStashPersistence : EL_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "hideout/stash-save";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		IEntity stash = EL_TestHideoutStashKit.FindStash();
		LF_PersistentStorageComponent persistent = EL_Component<LF_PersistentStorageComponent>.Find(stash);
		ctx.NotNull(persistent, "the placed stash carries LF_PersistentStorageComponent");
		if (!persistent)
			return;

		ctx.True(LF_PersistentStorageComponent.Resolve(EL_TestHideoutStashKit.STASH_KEY) == stash, "the stash registered under its key");
		LF_StorageSnapshotRecord before = LF_StorageSnapshot.Capture("stash-test", "before", stash);
		array<ref LF_StorageSnapshotRecord> storeBefore = LF_StorageSnapshot.ExportAll();

		EL_TestHideoutStashKit.SeedThree(ctx, stash);
		ctx.True(persistent.Capture(), "the stash contents are captured");
		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Get(EL_TestHideoutStashKit.STASH_KEY, LF_PersistentStorageComponent.SLOT_PLACED);
		ctx.NotNull(record, "a snapshot is stored under hideout:<id>");
		if (record)
			ctx.Equal(3, record.m_aItems.Count(), "the snapshot lists the three items");

		array<ref LF_StorageSnapshotRecord> saved = LF_StorageSnapshot.ExportAll();
		EL_TestHideoutStashKit.Empty(stash);
		ctx.Equal(0, EL_TestHideoutStashKit.ItemCount(stash), "the live stash differs from the save");

		LF_StorageSnapshot.ApplyAll(saved);
		ctx.Equal(3, LF_PersistentStorageComponent.RestoreAllPlaced(), "the load restores the three items once");
		ctx.Equal(3, EL_TestHideoutStashKit.ItemCount(stash), "three items are back in the stash");
		ctx.Equal(EL_TestHideoutStashKit.SEEDED_CASH, EL_TestHideoutStashKit.Count(stash, EL_TestHideoutStashKit.MONEY_PREFAB), "151 cash is back in the stash");
		ctx.Equal(1, EL_TestHideoutStashKit.Count(stash, EL_TestHideoutStashKit.APPLE_PREFAB), "the apple is back in the stash");
		ctx.Equal(1, EL_TestHideoutStashKit.Count(stash, EL_TestHideoutStashKit.WATER_PREFAB), "the water bottle is back in the stash");

		ctx.Equal(0, LF_PersistentStorageComponent.RestoreAllPlaced(), "a second restore spawns nothing");
		ctx.Equal(3, EL_TestHideoutStashKit.ItemCount(stash), "still three items after the second restore");
		ctx.Equal(EL_TestHideoutStashKit.SEEDED_CASH, EL_TestHideoutStashKit.Count(stash, EL_TestHideoutStashKit.MONEY_PREFAB), "the cash is not doubled");

		LF_StorageSnapshot.ApplyAll(storeBefore);
		if (before)
			LF_StorageSnapshot.Apply(stash, before);
		persistent.Capture();
	}
}
