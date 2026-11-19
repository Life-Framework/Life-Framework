// red-proof: RedProof() reads the core garage file without its pack layers, so `Everon: the first bought slot costs a week's wage` (expected 1200, got 500) and `Everon: the tow waits 90 minutes` fail; run `tools\cli test --no-build --red LF_Test_EveronGarage`.
//! #3458: under the Everon 1989 header (the pack leg of `tools\cli test`) the garage reads the pack's
//! overlay (`Configs/Vehicles/LF_Garage_Everon1989.conf`) over the core file: two free slots, the first
//! bought slot costs a week's wage, every later slot costs more, a car left on the street is towed after
//! 90 minutes, and getting one back (impound or tow) costs less than a day's pay.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronGarage : LF_Test
{
	static const ResourceName OVERLAY = "{F43DA1C32A918F6A}Configs/Vehicles/LF_Garage_Everon1989.conf";
	//! A courier day in the flagship: six parcel runs at 40 (LF_Jobs_Everon1989.conf).
	static const int DAY_PAY = 240;
	static const int WEEK_DAYS = 5;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/garage";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the Everon garage overlay");

		LF_GarageConfig config;
		if (!RedProof())
		{
			LF_GarageConfig.Reload();
			config = LF_GarageConfig.Get();
		}
		else
		{
			Resource core = BaseContainerTools.LoadContainer(LF_GarageConfig.CONFIG_PATH);
			if (core && core.GetResource())
				config = LF_GarageConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(core.GetResource().ToBaseContainer()));
		}

		ctx.NotNull(config, "the garage config loads");
		if (!config)
			return;

		ctx.Equal(2, config.m_iBaseSlots, "Everon: two slots are free");
		ctx.Equal(6, config.m_iMaxSlots, "Everon: six slots at most");
		ctx.Equal(DAY_PAY * WEEK_DAYS, config.m_iSlotPriceStart, "Everon: the first bought slot costs a week's wage");
		ctx.Equal(90, config.m_iTowAfterMinutes, "Everon: the tow waits 90 minutes");

		int previous = 0;
		for (int index = config.m_iBaseSlots; index < config.m_iMaxSlots; index++)
		{
			int price = LF_GarageSlots.PriceOfSlot(index, config.m_iBaseSlots, config.m_iSlotPriceStart, config.m_iSlotPriceStep);
			ctx.True(price > previous, string.Format("Everon: slot %1 costs more than the one before (%2 over %3)", index + 1, price, previous));
			previous = price;
		}

		ctx.True(config.m_iImpoundFee > 0, "Everon: the impound desk charges something");
		ctx.True(config.m_iImpoundFee < DAY_PAY, "Everon: the impound fee is under a day's pay");
		ctx.True(config.m_iTowFee < DAY_PAY, "Everon: the tow fee is under a day's pay");

		LF_GarageConfig.Reload();
	}
}
