// red-proof: RedProof() skips loading the catalogue, so the catalogue assertions fail (RED-OK). Also red on main before this change: 50 rows, 15 hats, 2 boots, 1 backpack and 1 pair of gloves; and 4 backpacks (not 5) once the veshmeshok row moved to the pack canvas prefab (#4245).
//! shop/clothing-catalogue: the island clothing outlets (clothing_rack, workwear_counter, surplus_stall) carry
//! every civilian vanilla wearable the game allows us to sell: each catalogue row they sell has a real prefab
//! and a price, hats, footwear, gloves and backpacks each have a range, every backpack is priced by its
//! capacity, and each colour variant of the raincoat is its own row.
//! Lives in the pack because it reads the pack's catalogue (pack leg).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronClothingCatalogue : LF_Test
{
	protected static const int MIN_HATS = 29;
	protected static const int MIN_FOOTWEAR = 3;
	protected static const int MIN_GLOVES = 2;
	protected static const int MIN_BACKPACKS = 5;
	protected static const int MIN_ROWS = 74; // was 75 while the rack listed the green raincoat twice (core and pack offer)

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/clothing-catalogue";
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a clothing outlet sells the item with this id.
	protected static bool SoldByClothingOutlet(LF_ItemCatalogueConfig catalogue, string id)
	{
		array<string> outlets = {"clothing_rack", "workwear_counter", "surplus_stall"};
		foreach (LF_OutletOffer offer : catalogue.m_aOffers)
		{
			if (!offer || offer.m_bRemoved || offer.m_sItem != id || offer.IsLicenceRow())
				continue;

			if (outlets.Find(offer.m_sOutlet) != -1)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static int PriceOf(LF_ItemCatalogueConfig catalogue, string id)
	{
		foreach (LF_CatalogueItem item : catalogue.m_aItems)
		{
			if (item && !item.m_bRemoved && item.m_sId == id)
				return item.m_iPrice;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig catalogue;
		if (!RedProof())
			catalogue = LF_ItemCatalogueConfig.Get();

		ctx.NotNull(catalogue, "the item catalogue loads");
		if (!catalogue || !catalogue.m_aItems)
			return;

		int rows;
		int hats;
		int footwear;
		int gloves;
		int backpacks;
		foreach (LF_CatalogueItem item : catalogue.m_aItems)
		{
			if (!item || item.m_bRemoved || !SoldByClothingOutlet(catalogue, item.m_sId))
				continue;

			string path = item.m_Prefab;
			bool real = path.Length() > 0 && Resource.Load(item.m_Prefab).IsValid();
			ctx.True(real, "the prefab loads: " + item.m_sId);
			ctx.True(item.m_iPrice > 0 || item.m_sId.StartsWith("police_") || item.m_sId == "doctor_coat", "a sold row has a price: " + item.m_sId);

			rows++;
			if (path.Contains("/HeadGear/"))
				hats++;
			else if (path.Contains("/Footwear/"))
				footwear++;
			else if (path.Contains("/Handwear/"))
				gloves++;
			else if (path.Contains("/Backpacks/") || item.m_sId.StartsWith("backpack_"))
				backpacks++; // by id too: backpack_veshmeshok is the pack's canvas prefab, not under /Backpacks/
		}

		ctx.True(rows >= MIN_ROWS, string.Format("clothing outlets sell at least %1 rows (%2 do)", MIN_ROWS, rows));
		ctx.True(hats >= MIN_HATS, string.Format("at least %1 headgear rows (%2)", MIN_HATS, hats));
		ctx.True(footwear >= MIN_FOOTWEAR, string.Format("at least %1 footwear rows (%2)", MIN_FOOTWEAR, footwear));
		ctx.True(gloves >= MIN_GLOVES, string.Format("at least %1 gloves rows (%2)", MIN_GLOVES, gloves));
		ctx.True(backpacks >= MIN_BACKPACKS, string.Format("at least %1 backpack rows (%2)", MIN_BACKPACKS, backpacks));

		// A bigger bag costs more: the rucksacks are priced by capacity (6000, 8000+, 15000+ ml).
		int sack = PriceOf(catalogue, "backpack_veshmeshok");
		int kolobok = PriceOf(catalogue, "backpack_kolobok");
		int alice = PriceOf(catalogue, "backpack_alice_medium");
		int field = PriceOf(catalogue, "backpack_iifs_fieldpack");
		ctx.True(sack > 0 && sack < kolobok && kolobok < alice && alice < field, string.Format("backpack prices rise with capacity (%1 < %2 < %3 < %4)", sack, kolobok, alice, field));

		// Each raincoat colour is its own row at the clothing rack (the green one is the core's raincoat_green offer, which conceals the build;
		// the pack's own jacket_raincoat_green item stays for the mannequin but is not offered a second time).
		array<string> coats = {"raincoat_green", "jacket_raincoat_orange", "jacket_raincoat_yellow"};
		foreach (string coat : coats)
			ctx.True(PriceOf(catalogue, coat) > 0 && SoldByClothingOutlet(catalogue, coat), "the raincoat colour is on sale: " + coat);
	}
}
