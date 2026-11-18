// red-proof: 2026-11-17 `tools\cli test --no-build --red LF_Test_EveronStoreBoxSafe`: RED PROOF OK (RedProof() skips LF_Bank.Rob); failed "one bag spills (expected 1, got 0)" and "the bag holds the shop's takings (expected 120, got 0)". Green without the hook (pack leg).
// tier: WORLD
//! crime/store-box (#2331): a shop till has a `store_box` safe beside it, set up with the safe-types
//! PR's type. Under the Everon 1989 header (the pack leg) the pack grid cell `store-box` places a till
//! and a LF_StoreBox next to it. The box is a lockable heist target of the store_box type, it is
//! configured under its entity name with the pack's takings (120 cash a bag), one bag, no tool and a
//! 10 s fuse; a script-driven robbery arms it, the fuse runs out and exactly that haul spills.
//! Lives in the pack because it names a pack prefab and the pack's overlay (the card's core test
//! path cannot: validate-addon-boundary).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronStoreBoxSafe : LF_Test
{
	static const ResourceName STORE_BOX = "{FDF16EF9B78A92F7}Prefabs/Props/Safes/LF_StoreBox.et";
	static const string TILL_NAME = "Grid_store_box_R0_1";
	static const string BOX_NAME = "Grid_store_box_R0_2";
	static const float BESIDE_METRES = 2;
	static const int PACK_HAUL = 120;
	static const float STORE_FUSE = 10;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/store-box-safe";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_Test_SafeTypes.SafeTypeOf(STORE_BOX) == LF_SafeTypeConfig.TYPE_STORE_BOX, "the pack store box prefab names the store_box type");

		IEntity till = GetGame().GetWorld().FindEntityByName(TILL_NAME);
		IEntity box = GetGame().GetWorld().FindEntityByName(BOX_NAME);
		ctx.NotNull(till, "the store-box grid cell places a till");
		ctx.NotNull(box, "the store-box grid cell places the store box");
		if (!till || !box)
			return;

		ctx.True(vector.Distance(till.GetOrigin(), box.GetOrigin()) < BESIDE_METRES, "the box stands beside the till");
		RunBox(ctx, box);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunBox(LF_TestContext ctx, IEntity box)
	{
		ctx.NotNull(LF_Component<LF_LockComponent>.Find(box), "the box has the lock a breach and a robbery need");
		LF_HeistVaultComponent vault = LF_Component<LF_HeistVaultComponent>.Find(box);
		ctx.NotNull(vault, "the box is a heist vault target");
		if (!vault)
			return;

		string vaultId = vault.GetVaultId();
		ctx.True(vaultId == BOX_NAME, "the box is registered under its entity name");
		ctx.True(vault.GetActionName() == "#LF-Safe_BoxRob", "the type gives it its own action name");

		// What the type says it holds and how it cracks.
		ctx.Equal(1, LF_Bank.GetConfiguredBagCount(vaultId), "it holds one bag");
		ctx.Equal(PACK_HAUL, LF_Bank.GetCashPerBag(vaultId), "the pack's 1989 takings: 120 a bag");
		ctx.EqualFloat(10, LF_Bank.GetDrillSeconds(vaultId), 0.001, "the drill time is the type's");

		// The robbery: nobody needs a tool or police, the fuse opens it, the haul spills once.
		string robber = "store-box-test-robber";
		bool armed = false;
		if (!RedProof())
			armed = LF_Bank.Rob(vaultId, robber);

		ctx.True(armed, "a robbery with no tool and no police on duty arms the box");
		ctx.Equal(0, LF_Bank.GetLooseBagCount(vaultId), "an armed box has spilled nothing yet");
		bool open = false;
		if (armed)
			open = LF_Bank.Advance(vaultId, STORE_FUSE);

		ctx.True(open, "the box opens when its 10 s fuse runs out");
		ctx.Equal(1, LF_Bank.GetLooseBagCount(vaultId), "one bag spills");
		ctx.Equal(PACK_HAUL, LF_Bank.GetBagCash(vaultId, 0), "the bag holds the shop's takings");

		for (int i = LF_Bank.GetLooseBagCount(vaultId) - 1; i >= 0; i--)
			SCR_EntityHelper.DeleteEntityAndChildren(LF_Bank.GetLooseBag(vaultId, i));
	}
}
