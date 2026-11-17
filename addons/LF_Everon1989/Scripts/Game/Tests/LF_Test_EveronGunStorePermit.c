// red-proof: RED-OK everon/gun-store-permit, observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_LicenceCounter,LF_Test_EveronGunStorePermit`:
// looking the row up on another outlet, `- the gun store counter offers the firearm permit row`. Hook off: green.
// tier: WORLD
//! Maintainer 2026-11-17: the firearm permit is got at the gun store. Under the Everon 1989 header (the
//! pack leg of `tools\cli test`), the gun store's `gun_counter` outlet carries the firearm permit licence
//! row (LF_Catalogue_GunStore.conf), its fee is the licence office's, and the licence desk therefore no
//! longer takes permit applications.
class LF_Test_EveronGunStorePermit : LF_Test
{
	protected static const string COUNTER = "gun_counter";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/gun-store-permit";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The leg boots under the pack header; layer the catalogue again so the gun store overlay applies.
		LF_ItemCatalogueConfig.Reload();
		LF_ItemCatalogueConfig config = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(config, "the item catalogue loads under the pack header");
		if (!config)
			return;

		string outlet = COUNTER;
		if (RedProof())
			outlet = "clothing_rack";

		LF_OutletOffer row = LF_ItemCatalogue.LicenceRowFor(config, outlet, LF_ELicenseType.FIREARM_PERMIT);
		ctx.NotNull(row, "the gun store counter offers the firearm permit row");
		if (row)
			ctx.Equal(LF_LicencesConfig.PermitFee(), row.LicenceFee(), "the row charges the licence office's permit fee");

		ctx.False(LF_LicenceDesk.DeskIssuesPermit(), "the licence desk leaves the permit to the gun store");
		LF_ItemCatalogueConfig.Reload();
	}
}
