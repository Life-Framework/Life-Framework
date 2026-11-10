// red-proof: Observed 2026-11-10 with `tools/cli test --no-build --only LF_Test_CashVan`: spilling one bag too many in LF_BankLoot.SpillBags failed `crime/cash-van` with `- three bags lie behind the van (expected 3, got 4)`, `- the event carries the cash (expected 1200, got 1600)`. Restored; green.
// tier: WORLD
//! The cash van box (#274): opening it with a fence tool pays the bags once and reports once. The
//! cash leaves as carried money bags, never a bank balance; a second try on the open box pays
//! nothing and reports nothing; the next stop stocks it afresh. The route helpers pick the next
//! stop and hold while a player is near.
class LF_Test_CashVan : LF_Test
{
	override string GetName()
	{
		return "crime/cash-van";
	}

	override void Run(LF_TestContext ctx)
	{
		string uid = "cash-van-a";
		string vanId = "cash-van-box";

		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (!robber)
			return;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(robber);
		IEntity pack = LF_Utils.SpawnEntityPrefab("{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et", robber.GetOrigin());
		ctx.True(pack != null && inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "the robber wears a pack");

		LF_Bank.Configure(vanId, 3, 400, 0, 0, 0, "");
		LF_Bank.ConfigureTools(vanId, "drill,charge", 0);
		ResourceName chargePrefab = LF_Breach.CHARGE_PREFAB;
		LF_Bank.ConfigureHeist(vanId, LF_BankLoot.KIND_BAGS, "", LF_WantedReports.OFFENCE_CASH_VAN, false, -1, 0);

		LF_HeistListener listener = new LF_HeistListener();
		LF_EventBus.Subscribe(LF_OnHeistLoot, listener);

		ctx.False(LF_Bank.Rob(vanId, uid, robber, 0.9), "no fence tool: the box stays shut");
		ctx.Equal(0, LF_Bank.GetLooseBagCount(vanId), "no tool: no bags");
		ctx.Equal(0, listener.m_iCount, "no tool: no heist event");

		ctx.Equal(1, LF_InventoryUtils.AddAmount(robber, chargePrefab, 1), "a charge is handed over");
		int calls = LF_Dispatch.CallCount();
		ctx.True(LF_Bank.Rob(vanId, uid, robber, 0.9), "with a charge the box opens");
		ctx.Equal(3, LF_Bank.GetLooseBagCount(vanId), "three bags lie behind the van");
		int total = 0;
		for (int i = 0; i < 3; i++)
			total = total + LF_Bank.ReadWorldBagCash(LF_Bank.GetLooseBag(vanId, i));

		ctx.Equal(1200, total, "the bags carry the configured cash");
		ctx.Equal(0, LF_ATMManager.GetOrCreate(uid).GetBalance(), "the haul is cash, the bank balance stays 0");
		ctx.Equal(1, LF_Breach.GetPoliceNotifyCount(vanId), "the break-in is reported once");
		ctx.Equal(calls + 1, LF_Dispatch.CallCount(), "dispatch took one call");
		ctx.Equal(1, listener.m_iCount, "the heist event was published once");
		ctx.Equal(1200, listener.m_iAmount, "the event carries the cash");
		ctx.EqualStr(vanId, listener.m_sTarget, "the event names the target");

		ctx.Equal(1, LF_InventoryUtils.AddAmount(robber, chargePrefab, 1), "a second charge");
		ctx.False(LF_Bank.Rob(vanId, uid, robber, 0.9), "an open box pays nothing again");
		ctx.Equal(3, LF_Bank.GetLooseBagCount(vanId), "still three bags");
		ctx.Equal(1, LF_Breach.GetPoliceNotifyCount(vanId), "still one report");
		ctx.Equal(1, listener.m_iCount, "still one heist event");


		LF_Bank.Restock(vanId);
		ctx.Equal(0, LF_Bank.GetLooseBagCount(vanId), "the next stop clears what was left lying");
		ctx.False(LF_Breach.IsOpen(vanId), "the next stop locks the box again");

		ctx.Equal(1, LF_CashVanRoute.NextStop(0, 3), "the route goes to the next stop");
		ctx.Equal(0, LF_CashVanRoute.NextStop(2, 3), "the route wraps");
		ctx.True(LF_CashVanRoute.CanMove(0), "the van moves when nobody is near");
		ctx.False(LF_CashVanRoute.CanMove(1), "the van holds while a player is near");

		LF_EventBus.Unsubscribe(LF_OnHeistLoot, listener);

		// Leave the world as found: the robber takes the pack and the spare charge with it. The charge
		// the first rob used is consumed; no drill is used, so no evidence is left lying.
		LF_TestCharacterFixture.Delete(robber);
	}
}

//------------------------------------------------------------------------------------------------
//! Records LF_OnHeistLoot for the heist tests.
class LF_HeistListener : LF_EventSubscriber
{
	int m_iCount;
	int m_iAmount;
	string m_sTarget;
	string m_sKind;
	string m_sSerials;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnHeistLoot loot = LF_OnHeistLoot.Cast(busEvent);
		if (!loot)
			return false;

		m_iCount++;
		m_iAmount = loot.m_iAmount;
		m_sTarget = loot.m_sTargetId;
		m_sKind = loot.m_sKind;
		m_sSerials = loot.m_sSerials;
		return true;
	}
}
