// red-proof: observed 2026-11-12 with `toolsi test --no-build --only LF_Test_ConfigConstantsBatch1`: expecting a founding fee of 251 failed `- founding fee (expected 251, got 250)` and `SUMMARY tier=only passed=0 failed=1`; restored, green.

// tier: WORLD
//! Config-constants batch 1 (#284): the tunables moved out of script keep today's values. Each
//! accessor reads the shipped .conf and must return the number the constant had before.
class LF_Test_ConfigConstantsBatch1 : LF_Test
{
	override string GetName()
	{
		return "config/constantsBatch1";
	}

	override void Run(LF_TestContext ctx)
	{
		// Police
		ctx.Equal(3, LF_PoliceConfig.TicketMaxOpen(), "ticket open cap");
		ctx.Equal(3, LF_PoliceConfig.TicketKeepPaid(), "ticket paid kept");
		ctx.Equal(5, LF_PoliceConfig.ArrestReach(), "arrest reach");
		ctx.Equal(8, LF_PoliceConfig.JailArrivalRadius(), "jail arrival radius");
		ctx.Equal(6, LF_PoliceConfig.ApplyDeskRange(), "police desk range");

		// Incident and zones
		ctx.Equal(400, LF_IncidentConfig.GmRadius(), "GM incident radius");
		ctx.Equal(3600, LF_IncidentConfig.GmWindowSeconds(), "GM incident window");
		ctx.Equal(8, LF_IncidentConfig.ZoneNoticeSeconds(), "zone notice interval");

		// Medic
		LF_MedicConfig medic = LF_MedicConfig.Get();
		ctx.Equal(5, medic.m_iBillMaxOpenPerPatient, "bill open cap");
		ctx.Equal(3, medic.m_iBillKeepPaidPerPatient, "bill paid kept");
		ctx.Equal(60, medic.m_iDebtSweepSeconds, "debt sweep seconds");

		// Garage
		ctx.Equal(5, LF_GarageConfig.DeskReach(), "garage desk reach");
		ctx.Equal(15, LF_GarageConfig.GarageRadius(), "garage radius");
		ctx.Equal(3, LF_GarageConfig.PadOccupiedRadius(), "pad occupied radius");

		// ATM and registration
		ctx.Equal(1000000, LF_LedgerConfig.AtmMaxTransaction(), "ATM transaction cap");
		ctx.Equal(5, LF_LedgerConfig.AtmRange(), "ATM range");
		ctx.Equal(18, LF_CharacterConfig.MinAge(), "minimum age");
		ctx.Equal(80, LF_CharacterConfig.MaxAge(), "maximum age");

		// Organizations
		ctx.Equal(250, LF_OrgConfig.FoundingFee(), "founding fee");
		ctx.Equal(3, LF_OrgConfig.NameMin(), "name min");
		ctx.Equal(32, LF_OrgConfig.NameMax(), "name max");
		ctx.Equal(120000, LF_OrgConfig.OfferMs(), "offer lifetime");
		ctx.Equal(1000000, LF_OrgConfig.MaxCashMove(), "fund cash move cap");
		ctx.Equal(500, LF_OrgConfig.PriceExtraStorage(), "extra storage price");
		ctx.Equal(250, LF_OrgConfig.PriceProcessingSlot(), "processing slot price");
		ctx.Equal(750, LF_OrgConfig.PriceParkedVehicle(), "parked vehicle price");
		ctx.Equal(5, LF_OrgConfig.DeskRange(), "org desk range");

		// Shop
		ctx.Equal(3.5, LF_ShopLimitsConfig.TillReach(), "till reach");
		ctx.Equal(30, LF_ShopLimitsConfig.ShopDistance(), "shop distance");
		ctx.Equal(99, LF_ShopLimitsConfig.OrderMaxQuantity(), "order quantity cap");
		ctx.Equal(4, LF_ShopLimitsConfig.BackRoomReach(), "back room reach");
		ctx.Equal(600000, LF_ShopLimitsConfig.BackRoomShutMs(), "back room shut time");
	}
}
