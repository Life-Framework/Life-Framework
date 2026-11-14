// red-proof: 2026-11-14, with LF_Bank.FillVaultBag back on FindEntityByName(vaultId),
//   cli test --no-build --only LF_Test_PlacesBankBag:
//   [LFTEST]   - the vault fills the bag it found by component (expected 250, got 0)
// Restored; green.
// tier: WORLD
//! places/bank-bag: the vault money bag is found by component under the vault id, never by entity
//! name (#1045). The annex bag is found under the annex vault id; an unnamed bag placed under
//! another id is filled with the vault cash and reads back; a vault with no bag placed fills
//! nothing and does not fail. Under the old FindEntityByName lookup an unnamed bag was never filled.
class LF_Test_PlacesBankBag : LF_Test
{
	protected static const string VAULT_ID = "places-bank-bag-vault";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "places/bank-bag";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity annexBag = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.BANK_BAG);
		ctx.NotNull(annexBag, "the annex bag is placed");
		ctx.True(LF_Places.Find(LF_Bank.BAG_PLACE_KIND, LF_TestAnnex.BANK_VAULT) == annexBag, "the annex bag is found under its vault id");

		IEntity vault = LF_Places.Find(LF_Bank.PLACE_KIND, LF_TestAnnex.BANK_VAULT);
		if (!vault)
		{
			ctx.NotNull(vault, "the annex vault is placed");
			return;
		}

		LF_Bank.FillVaultBag(VAULT_ID, 250);
		ctx.True(LF_Places.Find(LF_Bank.BAG_PLACE_KIND, VAULT_ID) == null, "a vault with no bag placed has none");

		IEntity bag = LF_Utils.SpawnEntityPrefab(LF_Bank.BagPrefab(), vault.GetOrigin() + "0 -20 0", "0 0 0", false);
		ctx.NotNull(bag, "an unnamed bag spawns");
		if (!bag)
			return;

		ctx.EqualStr("", bag.GetName(), "the bag has no name");
		ctx.True(LF_Places.Register(LF_Bank.BAG_PLACE_KIND, VAULT_ID, bag), "the unnamed bag is placed under the vault id");
		ctx.Equal(0, LF_Bank.ReadWorldBagCash(bag), "the bag starts empty");

		LF_Bank.FillVaultBag(VAULT_ID, 250);
		ctx.Equal(250, LF_Bank.ReadWorldBagCash(bag), "the vault fills the bag it found by component");

		LF_Bank.FillVaultBag(VAULT_ID, 400);
		ctx.Equal(400, LF_Bank.ReadWorldBagCash(bag), "the next haul sets the cash of the bag again");

		SCR_EntityHelper.DeleteEntityAndChildren(bag);
		ctx.True(LF_Places.Find(LF_Bank.BAG_PLACE_KIND, VAULT_ID) == null, "a deleted bag is not found");
	}
}
