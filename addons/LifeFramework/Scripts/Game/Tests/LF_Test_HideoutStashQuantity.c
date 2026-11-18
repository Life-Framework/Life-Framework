// red-proof: hideout/stash-quantity (#2585), observed 2026-11-17 with `tools/cli test --no-build
// --only LF_Test_HideoutStashQuantity` before the stash override of LF_AllowsRestrictedStorage
// existed: `[LFTEST] FAIL hideout/stash-quantity`, `- a non-member cannot transfer cash out of a
// stash stack`, `- the stash still holds 100 cash (expected 100, got 50)`, `- a non-member cannot
// transfer cash into a stash stack`. Green with LF_HideoutStashQuantityGate.c.

//------------------------------------------------------------------------------------------------
// tier: WORLD
//! A quantity transfer (split/merge RPC) changes stacks in place, so the stash's own storage checks
//! never run. The quantity gate must apply the stash's member rule to both sides.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HideoutStashQuantity : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "hideout/stash-quantity";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_QuantityComponent CashOf(IEntity character)
	{
		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!manager)
			return null;

		array<IEntity> items = LF_InventoryUtils.FindItemsByPrefab(manager, LF_TestHideoutStashKit.MONEY_PREFAB);
		if (items.IsEmpty())
			return null;

		return LF_Component<LF_QuantityComponent>.Find(items[0]);
	}

	//------------------------------------------------------------------------------------------------
	protected static ScriptedInventoryStorageManagerComponent ManagerOf(IEntity character)
	{
		return ScriptedInventoryStorageManagerComponent.Cast(LF_InventoryUtils.GetResponsibleStorageManager(character));
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity stash = LF_TestHideoutStashKit.FindStash();
		ctx.NotNull(stash, "Annex_Hideout_Stash is placed");
		if (!stash)
			return;

		LF_StorageSnapshotRecord before = LF_StorageSnapshot.Capture("stash-quantity-test", "before", stash);
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();

		string founder = "stash-quantity-founder";
		string member = "stash-quantity-member";
		string outsider = "stash-quantity-outsider";
		LF_Organization org = LF_OrganizationManager.Create(founder, "Stash Quantity Outfit");
		ctx.NotNull(org, "the owning organization exists");
		IEntity memberCharacter = LF_TestCharacterFixture.Spawn(ctx);
		IEntity outsiderCharacter = LF_TestCharacterFixture.Spawn(ctx);
		if (org && memberCharacter && outsiderCharacter)
		{
			ctx.True(LF_OrganizationManager.Invite(org.GetId(), founder, member), "founder adds a member");
			ctx.True(LF_Hideout.Assign(LF_TestAnnex.HIDEOUT_ID, org.GetId()), "the organization owns the hideout");
			LF_Hideout.BindCharacter(memberCharacter, member);
			LF_Hideout.BindCharacter(outsiderCharacter, outsider);

			// Both stand next to the stash, inside the 10 m manipulation range.
			// Moved before the cash spawns, so the carried stacks sit there too.
			memberCharacter.SetOrigin(stash.GetOrigin() + "1 0 0");
			memberCharacter.Update();
			outsiderCharacter.SetOrigin(stash.GetOrigin() + "0 0 1");
			outsiderCharacter.Update();

			LF_TestHideoutStashKit.Empty(stash);
			ctx.Equal(100, LF_InventoryUtils.AddAmount(stash, LF_TestHideoutStashKit.MONEY_PREFAB, 100), "100 cash goes into the stash");
			ctx.Equal(10, LF_InventoryUtils.AddAmount(memberCharacter, LF_TestHideoutStashKit.MONEY_PREFAB, 10), "the member carries 10 cash");
			ctx.Equal(10, LF_InventoryUtils.AddAmount(outsiderCharacter, LF_TestHideoutStashKit.MONEY_PREFAB, 10), "the non-member carries 10 cash");

			IEntity stashCashEntity = LF_TestHideoutStashKit.FindInStash(stash, LF_TestHideoutStashKit.MONEY_PREFAB);
			LF_QuantityComponent stashCash = LF_Component<LF_QuantityComponent>.Find(stashCashEntity);
			LF_QuantityComponent memberCash = CashOf(memberCharacter);
			LF_QuantityComponent outsiderCash = CashOf(outsiderCharacter);
			ScriptedInventoryStorageManagerComponent memberManager = ManagerOf(memberCharacter);
			ScriptedInventoryStorageManagerComponent outsiderManager = ManagerOf(outsiderCharacter);
			ctx.NotNull(stashCash, "the stash holds a cash stack");
			ctx.NotNull(memberCash, "the member holds a cash stack");
			ctx.NotNull(outsiderCash, "the non-member holds a cash stack");
			ctx.NotNull(memberManager, "the member's inventory manager resolves");
			ctx.NotNull(outsiderManager, "the non-member's inventory manager resolves");

			if (stashCash && memberCash && outsiderCash && memberManager && outsiderManager)
			{
				// A teleported test character's pockets keep the spawn-point transform; put the carried
				// stacks next to their carriers so only the access rule decides.
				memberCash.GetOwner().SetOrigin(memberCharacter.GetOrigin());
				outsiderCash.GetOwner().SetOrigin(outsiderCharacter.GetOrigin());


				// Non-member, out of the stash: theft.
				ctx.False(outsiderManager.LF_ServerQuantityTransfer(stashCash, outsiderCash, 50), "a non-member cannot transfer cash out of a stash stack");
				ctx.Equal(100, LF_TestHideoutStashKit.Count(stash, LF_TestHideoutStashKit.MONEY_PREFAB), "the stash still holds 100 cash");
				ctx.Equal(10, LF_InventoryUtils.GetAmount(outsiderCharacter, LF_TestHideoutStashKit.MONEY_PREFAB), "the non-member still carries 10 cash");

				// Non-member, into the stash.
				ctx.False(outsiderManager.LF_ServerQuantityTransfer(outsiderCash, stashCash, 5), "a non-member cannot transfer cash into a stash stack");
				ctx.Equal(100, LF_TestHideoutStashKit.Count(stash, LF_TestHideoutStashKit.MONEY_PREFAB), "the stash still holds 100 cash after the push");

				// A member uses the stash both ways.
				ctx.True(memberManager.LF_ServerQuantityTransfer(stashCash, memberCash, 50), "a member transfers cash out of the stash");
				ctx.Equal(50, LF_TestHideoutStashKit.Count(stash, LF_TestHideoutStashKit.MONEY_PREFAB), "the stash holds 50 cash");
				ctx.Equal(60, LF_InventoryUtils.GetAmount(memberCharacter, LF_TestHideoutStashKit.MONEY_PREFAB), "the member carries 60 cash");
				ctx.True(memberManager.LF_ServerQuantityTransfer(memberCash, stashCash, 20), "a member transfers cash into the stash");
				ctx.Equal(70, LF_TestHideoutStashKit.Count(stash, LF_TestHideoutStashKit.MONEY_PREFAB), "the stash holds 70 cash");
			}
		}

		LF_TestCharacterFixture.Delete(memberCharacter);
		LF_TestCharacterFixture.Delete(outsiderCharacter);
		if (before)
			LF_StorageSnapshot.Apply(stash, before);

		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
	}
}
