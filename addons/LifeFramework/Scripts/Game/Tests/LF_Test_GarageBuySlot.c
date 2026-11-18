// red-proof: observed 2026-11-18 `tools/cli test --no-build --red LF_Test_GarageBuySlot,LF_Test_GarageSlotsPersist`: RED PROOF OK (the hook skips the first paying Buy: "exact cash buys the slot" fails); green with `--only`.
//! Buying a garage slot with carried cash (#2656): enough cash takes the next slot's price from
//! LF_Garage.conf and raises the cap by one; short cash is refused and nothing is taken; at the max
//! the buy is refused and nothing is taken.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_GarageBuySlot : LF_Test
{
	static const string KEY = "test-garage-buy-slot";

	override string GetName()
	{
		return "vehicles/garage-buy-slot";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_GarageSlotStore.Forget(KEY);
		int price;
		ctx.Equal(LF_EGarageSlotBuy.NO_CHARACTER, LF_GarageSlotStore.Buy(null, KEY, price), "no buyer is refused");

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		ctx.True(LF_Utils.IsAliveState(buyer), "the fixture buyer is alive and conscious");
		LF_MoneyUtils.RemoveCash(buyer, LF_MoneyUtils.GetCash(buyer));
		int start = LF_GarageConfig.SlotPriceStart();
		int step = LF_GarageConfig.SlotPriceStep();
		int baseSlots = LF_GarageConfig.BaseSlots();
		int most = LF_GarageConfig.MaxSlots();
		ctx.Equal(baseSlots, LF_GarageSlots.CapOf(KEY), "a new character has the base slots");

		// Short cash: refused, nothing taken.
		LF_MoneyUtils.AddCash(buyer, start - 1);
		ctx.Equal(LF_EGarageSlotBuy.SHORT_CASH, LF_GarageSlotStore.Buy(buyer, KEY, price), "one short of the price is refused");
		ctx.Equal(start, price, "the asked price is the first bought slot's");
		ctx.Equal(start - 1, LF_MoneyUtils.GetCash(buyer), "a refused buy takes nothing");
		ctx.Equal(baseSlots, LF_GarageSlots.CapOf(KEY), "a refused buy leaves the cap");

		// Enough cash: the price is taken and the cap rises by one.
		LF_MoneyUtils.AddCash(buyer, 1);
		LF_EGarageSlotBuy result = LF_EGarageSlotBuy.SHORT_CASH;
		// RedProof(): the buy is skipped, so the cash and cap assertions below fail.
		if (!RedProof())
			result = LF_GarageSlotStore.Buy(buyer, KEY, price);

		ctx.Equal(LF_EGarageSlotBuy.BOUGHT, result, "exact cash buys the slot");
		ctx.Equal(0, LF_MoneyUtils.GetCash(buyer), "the price is taken from carried cash");
		ctx.Equal(baseSlots + 1, LF_GarageSlots.CapOf(KEY), "the cap rises by one");
		ctx.Equal(1, LF_GarageSlotStore.Bought(KEY), "one slot is recorded as bought");
		ctx.Equal(start + step, LF_GarageSlots.NextPriceFor(KEY), "the next slot costs one step more");

		// Buy up to the max, then the max refuses and takes nothing.
		LF_MoneyUtils.AddCash(buyer, 100000);
		int spent = 0;
		for (int slot = baseSlots + 1; slot < most; slot++)
		{
			ctx.Equal(LF_EGarageSlotBuy.BOUGHT, LF_GarageSlotStore.Buy(buyer, KEY, price), "slot " + slot.ToString() + " is bought");
			spent += price;
		}

		ctx.Equal(most, LF_GarageSlots.CapOf(KEY), "the cap reaches the max");
		int cashAtMax = LF_MoneyUtils.GetCash(buyer);
		ctx.Equal(100000 - spent, cashAtMax, "each slot took its own price");
		ctx.Equal(LF_EGarageSlotBuy.AT_MAX, LF_GarageSlotStore.Buy(buyer, KEY, price), "at the max another slot is refused");
		ctx.Equal(-1, price, "no price at the max");
		ctx.Equal(cashAtMax, LF_MoneyUtils.GetCash(buyer), "a refused buy at the max takes nothing");
		ctx.Equal(most, LF_GarageSlots.CapOf(KEY), "the cap stays at the max");

		LF_GarageSlotStore.Forget(KEY);
		LF_TestCharacterFixture.Delete(buyer);
	}
}
