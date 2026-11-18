// red-proof: RED-OK vehicles/lot-licence, observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_CarLotLicence`
// (the hook skips turning the gate on): `the refusal names the driving licence (expected '#LF-Lot_NeedsDrivingLicence', got
// '#EL-Vehicle_Bought')` and `and every coin in the bank (expected 2000, got 1000)` fail. Hook off: green (--only).
//! vehicles/lot-licence (#3151): with the server's licence gate on, the car lot refuses a buyer
//! without the driving licence with the licence reason, before any cash or bank money moves; with
//! the licence the buy goes through as before; with the gate off (the core default) nobody is asked.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CarLotLicence : LF_Test
{
	protected static const string UID = "lf-test-car-lot-licence";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/lot-licence";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.False(LF_VehiclePricesConfig.LotNeedsDrivingLicence(), "the core vehicles config ships the lot's licence gate off");

		IEntity lotEntity = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.VEHICLE_LOT);
		ctx.NotNull(lotEntity, "the world places the annex vehicle lot");
		if (!lotEntity)
			return;

		LF_VehicleLotComponent lot = LF_Component<LF_VehicleLotComponent>.Find(lotEntity);
		ctx.NotNull(lot, "the lot carries LF_VehicleLotComponent");
		if (!lot)
			return;

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(buyer);
		ctx.NotNull(licences, "the test character has a licence manager");
		LF_BankAccount account = LF_ATMManager.GetOrCreate(UID);
		ctx.NotNull(account, "the buyer has a bank account");
		if (!licences || !account)
		{
			LF_TestCharacterFixture.Delete(buyer);
			return;
		}

		int price = lot.GetPrice(LF_VehicleCatalog.S105);
		account.Deposit(price * 3);
		ctx.False(LF_LicenceDesk.HoldsLicence(buyer, UID, LF_LicenceDesk.DRIVING), "a new character holds no driving licence");

		// Gate off (core default): a buyer with no licence buys as before.
		ctx.True(BuyOnce(lot, buyer), "with the gate off the lot never asks for a licence");

		// Gate on, no licence: refused with the licence reason, nothing moves.
		if (!RedProof())
			LF_VehiclePricesConfig.SetLotNeedsDrivingLicenceForTest(true);

		int cashBefore = LF_MoneyUtils.GetCash(buyer);
		int bankBefore = account.GetBalance();
		string vehicleId;
		int cashCharged;
		string reasonKey;
		bool bought = lot.Buy(UID, LF_VehicleCatalog.S105, buyer, vehicleId, cashCharged, reasonKey);
		ctx.False(bought, "with the gate on a buyer without the driving licence is refused");
		ctx.EqualStr(LF_VehicleLotComponent.KEY_NEEDS_DRIVING, reasonKey, "the refusal names the driving licence");
		ctx.EqualStr(LF_MoneyFormat.FormatMoney(LF_LicencesConfig.DriverLicenceFee()), LF_VehicleLotComponent.ReasonArg(reasonKey), "the refusal carries the licence fee");
		ctx.Equal(0, cashCharged, "a refused buyer is charged nothing");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(buyer), "a refused buyer keeps every coin of cash");
		ctx.Equal(bankBefore, account.GetBalance(), "and every coin in the bank");
		Release(vehicleId);

		// Gate on, with the licence: the buy goes through as today.
		licences.UnlockLicense(LF_LicenceDesk.DRIVING, true);
		ctx.True(LF_LicenceDesk.HoldsLicence(buyer, UID, LF_LicenceDesk.DRIVING), "the buyer now holds the driving licence");
		ctx.True(BuyOnce(lot, buyer), "with the licence the lot sells the car");

		LF_VehiclePricesConfig.Reload();
		LF_TestCharacterFixture.Delete(buyer);
	}

	//------------------------------------------------------------------------------------------------
	//! Buys the S105 and scraps it again so the spawn pad stays free. \return Whether it was sold.
	protected bool BuyOnce(LF_VehicleLotComponent lot, IEntity buyer)
	{
		string vehicleId;
		int cashCharged;
		string reasonKey;
		bool bought = lot.Buy(UID, LF_VehicleCatalog.S105, buyer, vehicleId, cashCharged, reasonKey);
		Release(vehicleId);
		return bought && !vehicleId.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	protected void Release(string vehicleId)
	{
		if (vehicleId.IsEmpty())
			return;

		IEntity vehicle = LF_VehicleOwnership.GetVehicle(vehicleId);
		IEntity key = LF_VehicleOwnership.GetKey(vehicleId);
		LF_VehicleOwnership.Scrap(vehicleId);
		LF_TestCharacterFixture.Delete(vehicle);
		LF_TestCharacterFixture.Delete(key);
	}
}
