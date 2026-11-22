// red-proof: RED-OK everon/starting-bank, observed 2026-11-22 with `tools\cli test --no-build --red LF_Test_EveronStartingBank` (pack leg): with the hook reading the core file without its pack layers the opening bank is the game mode's $1,000 and `Everon: a new character opens with $1,250` fails (expected 1250, got 1000); green without it (pack leg).
//! A new Everon 1989 character opens with $1,250 in the bank (maintainer, 0.2.0 playtest): the cheapest
//! lot car ($700 base, $560 at the flagship's buyPercent 80) + the driving licence ($100) + a pickaxe
//! ($150), an axe ($120) and an entrenching tool ($45) cost $975, which leaves $275 for fuel and food.
//! The figure is the pack overlay's LF_CharacterConfig.m_iStartingBank (LF_Characters_Everon1989.conf);
//! the core and the game mode prefab keep $1,000. Paid once, into an empty account only.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronStartingBank : LF_Test
{
	static const int EXPECTED_BANK = 1250;
	static const int GAME_MODE_DEFAULT = 1000;
	static const ResourceName T601 = "{6303FEB76E4B719A}Prefabs/Vehicles/Wheeled/T601/T601_beige.et";
	//! The pack's basic hardware list: pickaxe, axe, entrenching tool.
	static const int HARDWARE_KIT = 150 + 120 + 45;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/starting-bank";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_CharacterConfig.Reload();
		int opening = GAME_MODE_DEFAULT;
		if (!RedProof())
		{
			opening = LF_CharacterConfig.StartingBank(GAME_MODE_DEFAULT);
		}
		else
		{
			// The core file alone: no pack layer, so the game mode's figure stays.
			Resource core = BaseContainerTools.LoadContainer(LF_CharacterConfig.CONFIG_PATH);
			if (core && core.GetResource())
			{
				LF_CharacterConfig coreConfig = LF_CharacterConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(core.GetResource().ToBaseContainer()));
				if (coreConfig && coreConfig.m_iStartingBank >= 0)
					opening = coreConfig.m_iStartingBank;
			}
		}

		ctx.Equal(EXPECTED_BANK, opening, "Everon: a new character opens with $1,250");

		// The shopping list it was sized for fits with something left over.
		LF_VehiclePricesConfig.Reload();
		int car = LF_VehiclePricesConfig.Lookup(LF_EVehiclePriceAction.BUY, T601);
		ctx.True(car > 0, "the cheapest lot car (the T601) has a buy price");
		int licence = LF_LicencesConfig.DriverLicenceFee();
		int spare = opening - car - licence - HARDWARE_KIT;
		ctx.True(spare > 0, "the opening bank buys the car, the driving licence and the hardware kit with something left");

		// A fresh account is paid the figure once; an account with money keeps its balance.
		LF_ATMManager.Reset();
		string freshUid = "test-everon-starting-bank-fresh";
		ctx.Equal(opening, LF_CharacterCreationManager.PayStartingBank(freshUid, opening), "a fresh account is paid the opening bank");
		ctx.Equal(opening, LF_ATMManager.GetOrCreate(freshUid).GetBalance(), "the fresh account's balance is the opening bank");
		ctx.Equal(0, LF_CharacterCreationManager.PayStartingBank(freshUid, opening), "a second payment into the funded account is refused");
		ctx.Equal(opening, LF_ATMManager.GetOrCreate(freshUid).GetBalance(), "the balance is unchanged by the second payment");
		LF_ATMManager.Reset();
		LF_VehiclePricesConfig.Reload();
		LF_CharacterConfig.Reload();
	}
}
