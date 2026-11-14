// red-proof: observed 2026-11-14 with `tools\cli test --no-build --local --only LF_Test_ConfigOverlayIds`: making LF_ConfigOverlay.Apply turn a verb on an unknown id into an add failed `- patch of an id no lower layer has is refused, not added`, `- four records refused: unknown patch, unknown remove, add of an existing id, bad verb (expected 4, got 2)` and `- offers: two records refused (expected 2, got 1)`. Restored; green.
// tier: LOGIC
//! #962: the one override model. Each verb (add, replace, patch, remove) on a list section, an id
//! no lower layer has refused while the rest of the overlay applies, an unknown verb refused, and
//! two packs on one id: the later wins and the claim names both. Runs on the crime catalog, the
//! catalogue's offers and the shop rules, the three sections on the shared merge.
class LF_Test_ConfigOverlayIds : LF_Test
{
	protected static const string ROBBERY = "#LF-Offence_Robbery";
	protected static const string MURDER = "#LF-Offence_Murder";
	protected static const string THEFT = "#LF-Offence_VehicleTheft";

	override string GetName()
	{
		return "config/overlay-ids";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_CrimeCatalogConfig MakeCrimes()
	{
		LF_CrimeCatalogConfig config = new LF_CrimeCatalogConfig();
		config.m_aCrimes = {};
		LF_CrimeEntry robbery = LF_CrimeEntry.Make(ROBBERY, 2, 300);
		robbery.m_iSentenceSeconds = 600;
		config.m_aCrimes.Insert(robbery);
		config.m_aCrimes.Insert(LF_CrimeEntry.Make(MURDER, 3, 0));
		config.m_aCrimes.Insert(LF_CrimeEntry.Make(THEFT, 1, 50));
		config.m_aCrimes.Insert(LF_CrimeEntry.Make("test-keep", 2, 0));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! A reader on the `crime` section of one layer's JSON.
	protected LF_ConfigReader Layer(string section, string sectionJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"" + section + "\":" + sectionJson + "}", "test");
		return LF_ConfigService.OpenOwnerSection(section);
	}

	//------------------------------------------------------------------------------------------------
	protected int IndexOf(LF_CrimeCatalogConfig config, string id)
	{
		foreach (int i, LF_CrimeEntry entry : config.m_aCrimes)
		{
			if (entry && entry.m_sCrimeId == id)
				return i;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// One layer, every verb, plus a refused unknown id, a refused add of an existing id and a
		// refused verb. The refusals skip their record only.
		LF_CrimeCatalogConfig crimes = MakeCrimes();
		string json = "{\"crimes\":{";
		json += "\"" + ROBBERY + "\":{\"op\":\"replace\",\"fine\":40},";
		json += "\"" + MURDER + "\":{\"op\":\"patch\",\"fine\":5000},";
		json += "\"" + THEFT + "\":{\"op\":\"remove\"},";
		json += "\"test-new-offence\":{\"op\":\"add\",\"wantedAdd\":4},";
		json += "\"test-missing\":{\"op\":\"patch\",\"fine\":7},";
		json += "\"test-missing-remove\":{\"op\":\"remove\"},";
		json += "\"test-keep\":{\"op\":\"add\",\"wantedAdd\":5},";
		json += "\"test-bad-verb\":{\"op\":\"delete\"},";
		json += "\"test-number-verb\":{\"op\":5,\"fine\":3}";
		json += "}}";
		LF_ConfigReader reader = Layer("crime", json);
		ctx.NotNull(reader, "the crime section opens");
		if (!reader)
			return;

		crimes.ApplyOwner(reader, "test-owner.json");

		LF_CrimeEntry robbery = crimes.Find(ROBBERY);
		ctx.NotNull(robbery, "replace keeps the id");
		if (robbery)
		{
			ctx.Equal(40, robbery.m_iFine, "replace: the field the layer gives applies");
			ctx.Equal(1, robbery.m_iWantedAdd, "replace: a field the layer leaves out is the default, not the core value");
			ctx.Equal(0, robbery.m_iSentenceSeconds, "replace: the core sentence is gone");
		}
		ctx.Equal(0, IndexOf(crimes, ROBBERY), "replace: the entry keeps its place in the list");

		LF_CrimeEntry murder = crimes.Find(MURDER);
		ctx.NotNull(murder, "patch keeps the entry");
		if (murder)
		{
			ctx.Equal(5000, murder.m_iFine, "patch: the field given applies");
			ctx.Equal(3, murder.m_iWantedAdd, "patch: a field left out keeps the lower layer's value");
		}

		ctx.True(crimes.Find(THEFT) == null, "remove: the entry leaves the list");
		LF_CrimeEntry added = crimes.Find("test-new-offence");
		ctx.NotNull(added, "add: a new id is added");
		if (added)
			ctx.Equal(4, added.m_iWantedAdd, "add: its field applies");
		ctx.Equal(3, IndexOf(crimes, "test-new-offence"), "add: the new id is last (three core entries left, then it)");
		LF_CrimeEntry kept = crimes.Find("test-keep");
		if (kept)
			ctx.Equal(2, kept.m_iWantedAdd, "add of an id the list has is refused; the entry keeps its value");
		ctx.True(crimes.Find("test-missing") == null, "patch of an id no lower layer has is refused, not added");
		ctx.True(crimes.Find("test-bad-verb") == null, "an unknown verb is refused");
		ctx.True(crimes.Find("test-number-verb") == null, "a verb that is not a string is refused, not read as no verb");
		ctx.Equal(6, reader.GetRejectedCount(), "five records refused (unknown patch, unknown remove, add of an existing id, bad verb, number verb), the number verb twice: its field and its record");

		// No verb keeps the model before #962: patch an id that exists, add one that does not.
		reader = Layer("crime", "{\"crimes\":{\"" + MURDER + "\":{\"wantedAdd\":4},\"test-upsert\":{\"fine\":9}}}");
		crimes.ApplyOwner(reader, "test-owner.json");
		murder = crimes.Find(MURDER);
		if (murder)
			ctx.Equal(4, murder.m_iWantedAdd, "no verb on an existing id patches it");
		ctx.NotNull(crimes.Find("test-upsert"), "no verb on a new id adds it");

		// Two packs on one id: the later wins and the claim names the earlier one.
		LF_ConfigClaims claims = new LF_ConfigClaims();
		LF_CrimeCatalogConfig packed = MakeCrimes();
		LF_ConfigReader packA = Layer("crime", "{\"crimes\":{\"" + ROBBERY + "\":{\"op\":\"patch\",\"fine\":111}}}");
		LF_ConfigOverlay.ApplyRecords(packA, "crimes", new LF_CrimeList(packed), "PackA.conf", claims);
		LF_ConfigReader packB = Layer("crime", "{\"crimes\":{\"" + ROBBERY + "\":{\"op\":\"patch\",\"fine\":222},\"" + MURDER + "\":{\"op\":\"patch\",\"fine\":1}}}");
		LF_ConfigOverlay.ApplyRecords(packB, "crimes", new LF_CrimeList(packed), "PackB.conf", claims);
		LF_CrimeEntry packedRobbery = packed.Find(ROBBERY);
		if (packedRobbery)
			ctx.Equal(222, packedRobbery.m_iFine, "two packs on one id: the later pack wins");
		ctx.Equal(1, claims.GetConflictCount(), "two packs on one id: one conflict warned; an id one pack touches is none");
		ctx.EqualStr("PackB.conf", claims.Claim("crime.crimes", ROBBERY, "PackC.conf"), "a third pack on the id is warned against the second");

		// The same verbs on the catalogue's offers (keyed outlet/item) and the shop rules.
		LF_ItemCatalogueConfig catalogue = new LF_ItemCatalogueConfig();
		catalogue.m_aItems = {};
		catalogue.m_aOffers = {};
		catalogue.m_aOffers.Insert(LF_OutletOffer.Make("gun_counter", "pistol_m9"));
		catalogue.m_aOffers.Insert(LF_OutletOffer.Make("clothing_rack", "raincoat_green"));
		reader = Layer("catalogue", "{\"offers\":{\"gun_counter/pistol_m9\":{\"op\":\"patch\",\"licence\":\"FIREARM_PERMIT\"},\"clothing_rack/raincoat_green\":{\"op\":\"remove\"},\"clothing_rack/ghost\":{\"op\":\"replace\"},\"nokey\":{}}}");
		catalogue.ApplyOwner(reader, "test-owner.json");
		LF_OutletOffer pistol = catalogue.FindOffer("gun_counter", "pistol_m9");
		if (pistol)
			ctx.EqualStr("FIREARM_PERMIT", pistol.m_sLicence, "offers: patch by outlet/item");
		ctx.True(catalogue.FindOffer("clothing_rack", "raincoat_green") == null, "offers: remove by outlet/item");
		ctx.Equal(1, catalogue.m_aOffers.Count(), "offers: a replace of a missing id and a key that is not outlet/item add nothing");
		ctx.Equal(2, reader.GetRejectedCount(), "offers: two records refused");

		LF_ShopPricesConfig shops = new LF_ShopPricesConfig();
		shops.m_aPriceRules = {};
		shops.m_aPriceRules.Insert(LF_ShopPriceRule.Create("Prefabs/Items/Food/", 0, 2));
		shops.m_aPriceRules.Insert(LF_ShopPriceRule.Create(string.Empty, 64, 2));
		reader = Layer("shops", "{\"priceRules\":{\"type:64\":{\"op\":\"patch\",\"buyPrice\":7},\"Prefabs/Items/Food/\":{\"op\":\"remove\"}}}");
		shops.ApplyOwner(reader, "test-owner.json");
		ctx.Equal(1, shops.m_aPriceRules.Count(), "shops: remove takes the food rule out");
		if (shops.m_aPriceRules.Count() == 1)
			ctx.Equal(7, shops.m_aPriceRules[0].m_iBuyPrice, "shops: type:64 patches the type:64,mode:2 rule");

		LF_ConfigService.Reset();
	}
}
