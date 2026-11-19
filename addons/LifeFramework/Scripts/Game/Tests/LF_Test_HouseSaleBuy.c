// red-proof: 2026-11-19 `tools\cli test --no-build --red LF_Test_HouseSaleBuy` printed RED PROOF OK: with the hook on every Buy call is skipped, so `a buyer 6 m from the sign is refused (expected 1, got -1)`, `a cuffed buyer is refused (expected 6, got -1)`. `--only` then passed with the buy on.
//! property/house-sale-buy (#3207): a player at a house's "For sale" sign buys the house with carried cash.
//! Enough cash makes the buyer the owner (the book, the purchase time), takes exactly the price, puts a key
//! bound to the house identifier in their inventory and marks the sign sold. A second buyer is refused
//! (owned) and keeps their cash; a buyer short of cash loses nothing; a buyer 6 m from the sign, a cuffed
//! buyer and a downed buyer are refused too. The config prices houses (m_aHouses) and keeps a house id out
//! of the plot list.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HouseSaleBuy : LF_Test
{
	protected static const ResourceName SIGN = "{2F815241C89039BC}Prefabs/Props/Property/LF_HouseSaleSign.et";
	protected static const ResourceName KEY_PREFAB = "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et";
	protected static const string HOUSE = "sale-house-a";
	protected static const string OTHER_HOUSE = "sale-house-b";
	protected static const string BUYER_UID = "lf-test-house-buyer";
	protected static const string SECOND_UID = "lf-test-house-second";
	protected static const string POOR_UID = "lf-test-house-poor";
	protected static const string FAR_UID = "lf-test-house-far";
	protected static const string CUFFED_UID = "lf-test-house-cuffed";
	protected static const string DOWNED_UID = "lf-test-house-downed";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "property/house-sale-buy";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PropertyConfig.Reload();
		LF_Properties.ResetStatics();
		int price = LF_PropertyConfig.HousePrice(HOUSE);
		int otherPrice = LF_PropertyConfig.HousePrice(OTHER_HOUSE);
		ctx.True(price > 0, "the example house has a price");
		ctx.True(otherPrice > 0, "the second example house has a price");
		ctx.True(!LF_PropertyConfig.HouseTierLabel(HOUSE).IsEmpty(), "the example house has a tier label");
		ctx.Equal(0, LF_PropertyConfig.PlotPrice(HOUSE), "a house id is no plot key");

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity second = LF_TestCharacterFixture.Spawn(ctx);
		IEntity poor = LF_TestCharacterFixture.Spawn(ctx);
		IEntity far = LF_TestCharacterFixture.Spawn(ctx);
		IEntity cuffed = LF_TestCharacterFixture.Spawn(ctx);
		IEntity downed = LF_TestCharacterFixture.Spawn(ctx);
		IEntity signEntity;
		if (buyer)
			signEntity = LF_Utils.SpawnEntityPrefab(SIGN, buyer.GetOrigin() + "1 0 0");

		ctx.NotNull(signEntity, "the house sale sign prefab spawns");
		LF_HouseSaleComponent sign;
		if (signEntity)
			sign = LF_Component<LF_HouseSaleComponent>.Find(signEntity);

		ctx.NotNull(sign, "the house sale sign carries LF_HouseSaleComponent");
		if (price <= 0 || otherPrice <= 0 || !buyer || !second || !poor || !far || !cuffed || !downed || !sign)
		{
			Cleanup(buyer, second, poor, far, cuffed, downed, signEntity);
			return;
		}

		sign.SetHouseId(HOUSE);
		ctx.True(sign.IsForSale(), "a priced house nobody owns is for sale");
		ctx.False(sign.IsOwned(), "and not owned");
		string houseId;

		// Enough cash: the buyer owns the house, exactly the price is taken, the key is theirs.
		SetCash(ctx, buyer, price + 500);
		ctx.Equal(LF_EHouseBuy.BOUGHT, Buy(buyer, BUYER_UID, houseId, HOUSE), "a buyer with the price buys the house");
		ctx.EqualStr(HOUSE, houseId, "the server sold the house of the sign used");
		ctx.Equal(500, LF_MoneyUtils.GetCash(buyer), "exactly the price was taken");
		ctx.EqualStr(BUYER_UID, LF_Properties.GetOwnerKey(HOUSE), "the buyer owns the house in LF_Properties");
		LF_PropertyRecord row = LF_Properties.Get(HOUSE);
		ctx.True(row && row.m_iPurchasedAt > 0, "the row records when it was bought");
		ctx.True(LF_BuildSiteComponent.UserHoldsKey(buyer, HOUSE), "the buyer carries a key bound to the house");
		array<IEntity> keys = LF_InventoryUtils.FindItemsByPrefab(LF_InventoryUtils.GetResponsibleStorageManager(buyer), KEY_PREFAB);
		ctx.Equal(1, keys.Count(), "exactly one house key was made");
		ctx.True(sign.IsOwned(), "the sign flips to sold (the replicated owned flag)");
		ctx.False(sign.IsForSale(), "a sold house is no longer for sale");

		// An owned house: a second buyer at the sign is refused and keeps their cash and no key.
		signEntity.SetOrigin(second.GetOrigin() + "1 0 0");
		SetCash(ctx, second, price * 2);
		ctx.Equal(LF_EHouseBuy.REFUSED_OWNED, Buy(second, SECOND_UID, houseId, HOUSE), "an owned house is refused for a second buyer");
		ctx.Equal(price * 2, LF_MoneyUtils.GetCash(second), "the second buyer keeps their cash");
		ctx.EqualStr(BUYER_UID, LF_Properties.GetOwnerKey(HOUSE), "the first buyer still owns it");
		ctx.False(LF_BuildSiteComponent.UserHoldsKey(second, HOUSE), "the second buyer got no key");

		// Short cash: refused, nothing taken, nobody owns it, no key.
		sign.SetHouseId(OTHER_HOUSE);
		signEntity.SetOrigin(poor.GetOrigin() + "1 0 0");
		SetCash(ctx, poor, otherPrice - 1);
		ctx.Equal(LF_EHouseBuy.REFUSED_NO_CASH, Buy(poor, POOR_UID, houseId, OTHER_HOUSE), "a buyer one short of the price is refused");
		ctx.Equal(otherPrice - 1, LF_MoneyUtils.GetCash(poor), "and nothing is taken");
		ctx.EqualStr("", LF_Properties.GetOwnerKey(OTHER_HOUSE), "and nobody owns the house");
		ctx.False(LF_BuildSiteComponent.UserHoldsKey(poor, OTHER_HOUSE), "and no key was made");
		ctx.True(sign.IsForSale(), "the sign still sells it");

		// Out of reach: a free house, but the sign is 6 m away.
		signEntity.SetOrigin(far.GetOrigin() + "6 0 0");
		SetCash(ctx, far, otherPrice * 2);
		ctx.Equal(LF_EHouseBuy.REFUSED_NO_SIGN, Buy(far, FAR_UID, houseId, OTHER_HOUSE), "a buyer 6 m from the sign is refused");
		ctx.Equal(otherPrice * 2, LF_MoneyUtils.GetCash(far), "the far buyer keeps their cash");
		ctx.EqualStr("", LF_Properties.GetOwnerKey(OTHER_HOUSE), "and nobody owns the house");

		// A cuffed buyer at the sign: refused, nothing taken.
		signEntity.SetOrigin(cuffed.GetOrigin() + "1 0 0");
		SetCash(ctx, cuffed, otherPrice * 2);
		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(cuffed);
		ctx.NotNull(chimera, "the cuffed fixture is a character");
		if (chimera)
			chimera.LF_SetRestrained(true);

		ctx.Equal(LF_EHouseBuy.REFUSED_NOT_ABLE, Buy(cuffed, CUFFED_UID, houseId, OTHER_HOUSE), "a cuffed buyer is refused");
		ctx.Equal(otherPrice * 2, LF_MoneyUtils.GetCash(cuffed), "the cuffed buyer keeps their cash");
		ctx.EqualStr("", LF_Properties.GetOwnerKey(OTHER_HOUSE), "and nobody owns the house");
		if (chimera)
			chimera.LF_SetRestrained(false);

		// A downed buyer at the sign: refused, nothing taken.
		signEntity.SetOrigin(downed.GetOrigin() + "1 0 0");
		SetCash(ctx, downed, otherPrice * 2);
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(downed.FindComponent(SCR_CharacterDamageManagerComponent));
		if (damage)
			damage.Kill(Instigator.CreateInstigator(null));

		ctx.False(LF_Utils.IsAliveState(downed), "the downed fixture is not alive and conscious");
		ctx.Equal(LF_EHouseBuy.REFUSED_NOT_ABLE, Buy(downed, DOWNED_UID, houseId, OTHER_HOUSE), "a dead or downed buyer is refused");
		ctx.Equal(otherPrice * 2, LF_MoneyUtils.GetCash(downed), "the downed buyer keeps their cash");
		ctx.EqualStr("", LF_Properties.GetOwnerKey(OTHER_HOUSE), "and nobody owns the house");

		// The server clears the owner (an eviction or a wipe): the sign sells again.
		LF_Properties.SetOwner(HOUSE, "");
		sign.SetHouseId(HOUSE);
		ctx.False(sign.IsOwned(), "a house with its owner cleared is for sale again");

		LF_Properties.ResetStatics();
		Cleanup(buyer, second, poor, far, cuffed, downed, signEntity);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EHouseBuy Buy(IEntity buyer, string uid, out string houseId, string askedId)
	{
		houseId = "";
		if (RedProof())
			return -1;

		return LF_HouseSaleComponent.Buy(buyer, uid, houseId, askedId);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetCash(LF_TestContext ctx, IEntity character, int amount)
	{
		int carried = LF_MoneyUtils.GetCash(character);
		if (carried > 0)
			LF_MoneyUtils.RemoveCash(character, carried);

		if (amount > 0)
			LF_MoneyUtils.AddCash(character, amount);

		ctx.Equal(amount, LF_MoneyUtils.GetCash(character), string.Format("the character carries %1 to start", amount));
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity buyer, IEntity second, IEntity poor, IEntity far, IEntity cuffed, IEntity downed, IEntity sign)
	{
		LF_TestCharacterFixture.Delete(buyer);
		LF_TestCharacterFixture.Delete(second);
		LF_TestCharacterFixture.Delete(poor);
		LF_TestCharacterFixture.Delete(far);
		LF_TestCharacterFixture.Delete(cuffed);
		LF_TestCharacterFixture.Delete(downed);
		LF_TestCharacterFixture.Delete(sign);
	}
}
