// red-proof: 2026-11-17 `tools/cli test --no-build --only LF_Test_HideoutStashPutCarrier` on the unfixed
// CanStoreItem failed `- a non-member cannot move a car's apple into the stash` and `- a non-member
// cannot move a body's item into the stash`; green with the fix (with LF_Test_HideoutStash and -Persistence).
// tier: WORLD
//! hideout/stash-put-carrier (#2574): the stash took any item whose carrier was not a character, so
//! anyone with the stash in an inventory view filled it from a vehicle's storage. Now an item from
//! a vehicle or from a body is refused; a living member's own inventory and the save restore still
//! fill it. Items on the ground (no carrier, like a server spawn) are a follow-up card.
class LF_Test_HideoutStashPutCarrier : LF_Test
{
	protected static const ResourceName CAR = "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et";
	protected static const string FOUNDER = "hideout-put-carrier-founder";
	protected static const string MEMBER = "hideout-put-carrier-member";
	protected static const string OUTSIDER = "hideout-put-carrier-outsider";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "hideout/stash-put-carrier";
	}

	//------------------------------------------------------------------------------------------------
	//! The mover's inventory asks to move the first item of the prefab held by the carrier into the stash.
	protected static bool MoveFrom(IEntity mover, IEntity carrier, IEntity stash, ResourceName prefab)
	{
		InventoryStorageManagerComponent moverManager = LF_InventoryUtils.GetResponsibleStorageManager(mover);
		InventoryStorageManagerComponent carrierManager = LF_InventoryUtils.GetResponsibleStorageManager(carrier);
		if (!moverManager || !carrierManager)
			return false;

		array<IEntity> items = LF_InventoryUtils.FindItemsByPrefab(carrierManager, prefab);
		if (items.IsEmpty())
			return false;

		return moverManager.TryMoveItemToStorage(items[0], LF_TestHideoutStashKit.StorageOf(stash));
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity stash = LF_TestHideoutStashKit.FindStash();
		ctx.NotNull(stash, "Annex_Hideout_Stash is placed");
		LF_PersistentStorageComponent persistent = LF_Component<LF_PersistentStorageComponent>.Find(stash);
		ctx.NotNull(persistent, "the stash carries LF_PersistentStorageComponent");
		if (!stash || !persistent)
			return;

		LF_StorageSnapshotRecord before = LF_StorageSnapshot.Capture("stash-put-carrier", "before", stash);
		array<ref LF_StorageSnapshotRecord> storeBefore = LF_StorageSnapshot.ExportAll();
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();

		IEntity memberCharacter = LF_TestCharacterFixture.Spawn(ctx);
		IEntity outsiderCharacter = LF_TestCharacterFixture.Spawn(ctx);
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		IEntity car;
		if (memberCharacter)
			car = LF_Utils.SpawnEntityPrefab(CAR, memberCharacter.GetOrigin() + "0 30 6");

		LF_Organization org = LF_OrganizationManager.Create(FOUNDER, "Put Carrier Crew");
		ctx.NotNull(org, "the owning organization exists");
		ctx.NotNull(car, "a car is spawned");
		if (org && car && memberCharacter && outsiderCharacter && body)
		{
			ctx.True(LF_OrganizationManager.Invite(org.GetId(), FOUNDER, MEMBER), "founder adds a member");
			ctx.True(LF_Hideout.Assign(LF_TestAnnex.HIDEOUT_ID, org.GetId()), "the organization owns the hideout");
			LF_Hideout.BindCharacter(memberCharacter, MEMBER);
			LF_Hideout.BindCharacter(outsiderCharacter, OUTSIDER);
			LF_Hideout.BindCharacter(body, MEMBER);
			LF_TestHideoutStashKit.Empty(stash);

			// A vehicle's storage is open to anyone: its item never goes straight into the stash.
			ctx.Equal(1, LF_InventoryUtils.AddAmount(car, LF_TestHideoutStashKit.APPLE_PREFAB, 1), "the car's storage holds an apple");
			ctx.False(MoveFrom(outsiderCharacter, car, stash, LF_TestHideoutStashKit.APPLE_PREFAB), "a non-member cannot move a car's apple into the stash");
			ctx.Equal(0, LF_TestHideoutStashKit.Count(stash, LF_TestHideoutStashKit.APPLE_PREFAB), "no apple from the car is in the stash");

			// A body is open to anyone too, even a member's.
			ctx.Equal(1, LF_InventoryUtils.AddAmount(body, LF_TestHideoutStashKit.WATER_PREFAB, 1), "the member's body carries a water bottle");
			SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(body.FindComponent(SCR_CharacterDamageManagerComponent));
			if (damage)
				damage.Kill(Instigator.CreateInstigator(null));
			ctx.False(LF_Utils.IsAliveState(body), "the body is not alive");
			ctx.False(MoveFrom(outsiderCharacter, body, stash, LF_TestHideoutStashKit.WATER_PREFAB), "a non-member cannot move a body's item into the stash");
			ctx.Equal(0, LF_TestHideoutStashKit.Count(stash, LF_TestHideoutStashKit.WATER_PREFAB), "no water bottle from the body is in the stash");

			// A living member's own inventory still fills the stash.
			ctx.Equal(1, LF_InventoryUtils.AddAmount(memberCharacter, LF_TestHideoutStashKit.APPLE_PREFAB, 1), "the member carries an apple");
			ctx.True(LF_TestHideoutStashKit.StoreFrom(memberCharacter, stash, LF_TestHideoutStashKit.APPLE_PREFAB), "a member stores an apple from their own pockets");
			ctx.Equal(1, LF_TestHideoutStashKit.Count(stash, LF_TestHideoutStashKit.APPLE_PREFAB), "the member's apple is in the stash");

			// The save restore spawns items with no carrier and still fills the stash.
			ctx.True(persistent.Capture(), "the stash contents are captured");
			array<ref LF_StorageSnapshotRecord> saved = LF_StorageSnapshot.ExportAll();
			LF_TestHideoutStashKit.Empty(stash);
			ctx.Equal(0, LF_TestHideoutStashKit.ItemCount(stash), "the live stash is emptied");
			LF_StorageSnapshot.ApplyAll(saved);
			ctx.Equal(1, LF_PersistentStorageComponent.RestoreAllPlaced(), "the restore puts the apple back");
			ctx.Equal(1, LF_TestHideoutStashKit.Count(stash, LF_TestHideoutStashKit.APPLE_PREFAB), "the apple is back in the stash");
		}

		LF_TestCharacterFixture.Delete(memberCharacter);
		LF_TestCharacterFixture.Delete(outsiderCharacter);
		LF_TestCharacterFixture.Delete(body);
		if (car)
			SCR_EntityHelper.DeleteEntityAndChildren(car);

		LF_StorageSnapshot.ApplyAll(storeBefore);
		if (before)
			LF_StorageSnapshot.Apply(stash, before);
		persistent.Capture();
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
	}
}
