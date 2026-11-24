// red-proof: observed 2026-11-24 (#4703): the run before the catalogue row existed failed `the gun counter sells the gang radio`, then passed with the row. RedProof() points the lookup at an outlet that does not sell it.
//! shop/gang-radio-sale (#4703, GA-C8): a crew buys gang radios in town for $60. The Saint-Philippe gun counter
//! (Place_saint-philippe_gun-counter.layer places the `LF_GunCounter` prefab with the `gun_counter` outlet) sells
//! the pack's `LF_GangRadio` at $60 to anyone: no licence, job, rank or right on the offer. Tuning to the gang
//! band is already gated by membership (#4221), so the sale itself stays open.
//! Lives in the pack because it reads the pack's catalogue (pack leg).
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronGangRadioSale : LF_Test
{
	protected static const string OUTLET = "gun_counter";
	protected static const ResourceName GANG_RADIO = "{42646EECFBC540D9}Prefabs/Items/Comms/LF_GangRadio.et";
	protected static const int PRICE = 60;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/gang-radio-sale";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(catalogue, "the item catalogue loads");
		if (!catalogue)
			return;

		string outlet = OUTLET;
		if (RedProof())
			outlet = "clothing_rack";

		LF_CatalogueItem item;
		LF_OutletOffer offer = LF_ItemCatalogue.OfferFor(catalogue, outlet, GANG_RADIO, item);
		ctx.NotNull(offer, "the gun counter sells the gang radio");
		if (!offer || !item)
			return;

		ctx.Equal(PRICE, item.m_iPrice, "the gang radio costs $60");
		ctx.True(Resource.Load(item.m_Prefab).IsValid(), "the gang radio prefab loads");

		// Anyone may buy it: a buyer with no licence, no job and no rank is served.
		LF_Test_CatalogueBuyer stranger = new LF_Test_CatalogueBuyer();
		string missing;
		ctx.True(LF_ItemCatalogue.Check(offer, stranger, missing) == LF_EOutletGate.OPEN, "a stranger with no licence or job buys the gang radio");
	}
}
