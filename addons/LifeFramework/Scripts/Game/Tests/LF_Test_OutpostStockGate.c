// red-proof: `tools\cli test --no-build --red LF_Test_OutpostStockGate`; RedProof() skips the pistol Buy and the
// outpost zone registration, so the paid, cash, carried-pistol and officer-refused assertions go red.

//! outposts/stock-gate (#4223, O2, N1, N29): the quartermaster's crate sells by underworld tier. Tier 1 buys a pistol
//! ($400 to $600) and the server takes the cash; a tier 2 item refuses at tier 1 in words and moves nothing; the gang
//! radio sells for $60; an on-duty officer inside the outpost zone is refused before any cash moves.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_OutpostStockGate : LF_Test
{
	protected static const string BUYER_UID = "outpost-stock-buyer";
	protected static const string OFFICER_UID = "outpost-stock-officer";
	protected static const string ZONE_NAME = "outpost:stock-gate-test";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "outposts/stock-gate";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_OutpostStockItem Row(notnull array<ref LF_OutpostStockItem> stock, string id)
	{
		foreach (LF_OutpostStockItem item : stock)
		{
			if (item.m_sId == id)
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_OutpostStockItem> stock = {};
		LF_OutpostStock.DefaultStock(stock);
		LF_OutpostStockItem pistol = Row(stock, "pistol");
		LF_OutpostStockItem rifle = Row(stock, "rifle");
		LF_OutpostStockItem radio = Row(stock, "gang_radio");
		ctx.True(pistol && rifle && radio, "the default crate stocks a pistol, a rifle and the gang radio");
		if (!pistol || !rifle || !radio)
			return;

		// N29 prices and tiers; the radio at $60 (#4221).
		ctx.True(pistol.m_iPrice >= 400 && pistol.m_iPrice <= 600 && pistol.m_iMinTier == 1, "the pistol is $400 to $600 at tier 1");
		ctx.True(rifle.m_iPrice >= 1500 && rifle.m_iPrice <= 2500 && rifle.m_iMinTier == 2, "the rifle is $1,500 to $2,500 at tier 2");
		ctx.Equal(60, radio.m_iPrice, "the gang radio is $60");
		ctx.EqualStr(LF_OutpostStock.GANG_RADIO_SLOT, radio.m_sPrefabSlot, "the radio comes through its prefab slot, so the pack hands out its own");

		// The pure gate.
		ctx.True(LF_OutpostStock.Gate(pistol, 1) == LF_EOutpostBuy.OK, "tier 1 may buy the pistol");
		ctx.True(LF_OutpostStock.Gate(pistol, 0) == LF_EOutpostBuy.NOT_KNOWN, "tier 0 may not buy the pistol");
		ctx.True(LF_OutpostStock.Gate(rifle, 1) == LF_EOutpostBuy.NOT_KNOWN, "tier 1 may not buy the rifle");
		ctx.True(LF_OutpostStock.Gate(rifle, 2) == LF_EOutpostBuy.OK, "tier 2 may buy the rifle");

		// The refusal is words: its own key, and the text names no number (N1).
		string refusalKey = LF_OutpostStock.RefusalKey(LF_EOutpostBuy.NOT_KNOWN);
		ctx.EqualStr("#LF-OutpostStock_NotKnown", refusalKey, "a tier refusal has its own words");
		string refusal = WidgetManager.Translate(refusalKey);
		bool hasDigit = false;
		for (int i = 0; i < refusal.Length(); i++)
		{
			if (refusal.Substring(i, 1).ToInt().ToString() == refusal.Substring(i, 1))
				hasDigit = true;
		}

		ctx.False(hasDigit, "the tier refusal names no number");

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		LF_MoneyUtils.SetUidForTests(buyer, BUYER_UID);
		LF_MoneyUtils.AddCash(buyer, 3000, "test", "", false);
		int before = LF_MoneyUtils.GetCash(buyer);
		vector crate = buyer.GetOrigin();

		// Tier 1 buys the pistol; the server takes its price.
		int paid;
		LF_EOutpostBuy bought = LF_EOutpostBuy.BAD_INPUT;
		if (!RedProof())
			bought = LF_OutpostStock.Buy(buyer, pistol, crate, 1, paid);

		ctx.True(bought == LF_EOutpostBuy.OK, "tier 1 buys the pistol");
		ctx.Equal(pistol.m_iPrice, paid, "the pistol's price is paid");
		ctx.Equal(before - pistol.m_iPrice, LF_MoneyUtils.GetCash(buyer), "the cash leaves the buyer on the server");
		ctx.True(LF_TestCharacterFixture.HasItem(buyer, pistol.m_Prefab), "the buyer carries the pistol");

		// A tier 2 item refuses at tier 1 and moves nothing.
		int afterPistol = LF_MoneyUtils.GetCash(buyer);
		int rifPaid;
		ctx.True(LF_OutpostStock.Buy(buyer, rifle, crate, 1, rifPaid) == LF_EOutpostBuy.NOT_KNOWN, "the rifle refuses at tier 1");
		ctx.Equal(0, rifPaid, "nothing is paid for a refused rifle");
		ctx.Equal(afterPistol, LF_MoneyUtils.GetCash(buyer), "a refused rifle takes no cash");
		ctx.False(LF_TestCharacterFixture.HasItem(buyer, rifle.m_Prefab), "a refused rifle is not handed over");

		// The radio: $60, from its slot.
		int radioPaid;
		ctx.True(LF_OutpostStock.Buy(buyer, radio, crate, 0, radioPaid) == LF_EOutpostBuy.OK, "anyone buys the gang radio");
		ctx.Equal(60, radioPaid, "the radio costs $60");
		ctx.Equal(afterPistol - 60, LF_MoneyUtils.GetCash(buyer), "the radio's $60 leaves the buyer");
		ctx.True(LF_TestCharacterFixture.HasItem(buyer, LF_PrefabSlots.Get(LF_OutpostStock.GANG_RADIO_SLOT)), "the buyer carries the radio");

		// Short of cash: refused, refunded, nothing handed over.
		LF_MoneyUtils.RemoveAmount(buyer, LF_MoneyUtils.GetCash(buyer), "test", "");
		LF_MoneyUtils.AddCash(buyer, 100, "test", "", false);
		int shortPaid;
		ctx.True(LF_OutpostStock.Buy(buyer, pistol, crate, 3, shortPaid) == LF_EOutpostBuy.CASH_SHORT, "a buyer short of cash is refused");
		ctx.Equal(100, LF_MoneyUtils.GetCash(buyer), "a short buyer keeps what they had");

		LF_MoneyUtils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(buyer);

		// An on-duty officer inside the outpost zone is refused before any cash moves (#4217).
		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer)
			return;

		LF_PlayerAccount account = LF_TestPoliceKit.Account(OFFICER_UID, true);
		account.SetFaction(LF_Faction.POLICE);
		account.SetPoliceRank(LF_PoliceRanks.PATROL);
		account.SetOnDuty(true);
		LF_PoliceRanks.BindOfficer(officer, OFFICER_UID);
		LF_MoneyUtils.SetUidForTests(officer, OFFICER_UID);
		LF_MoneyUtils.AddCash(officer, 1000, "test", "", false);
		int officerCash = LF_MoneyUtils.GetCash(officer);
		vector post = officer.GetOrigin();
		if (!RedProof())
			LF_Zones.RegisterCircle(ZONE_NAME, LF_ZoneRule.SAFE, post, 40);

		int officerPaid;
		ctx.True(LF_OutpostStock.Buy(officer, pistol, post, 3, officerPaid) == LF_EOutpostBuy.REFUSED_OFFICER, "an on-duty officer is refused inside the outpost");
		ctx.Equal(officerCash, LF_MoneyUtils.GetCash(officer), "the refused officer keeps their cash");
		ctx.False(LF_TestCharacterFixture.HasItem(officer, pistol.m_Prefab), "the refused officer gets no pistol");

		// Off duty, the same officer is served.
		account.SetOnDuty(false);
		ctx.True(LF_OutpostStock.Buy(officer, radio, post, 0, officerPaid) == LF_EOutpostBuy.OK, "an off-duty officer is served");

		LF_Zones.Unregister(ZONE_NAME);
		LF_MoneyUtils.SetUidForTests(null, "");
		LF_TestPoliceKit.Release(OFFICER_UID);
		LF_TestCharacterFixture.Delete(officer);
	}
}
