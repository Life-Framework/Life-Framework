// red-proof: RED-OK residents/work-place-id (2026-11-23, `test --red LF_Test_ResidentWorkPlaceId`: with the work place ids dropped the plate finds the other keeper and keeper b stands at shop a)
//! residents/work-place-id: a resident's workplace can be one named place of the work kind (#4560), so
//! four StoreCounter keepers each stand at their own shop. Two `StoreCounter` places six metres apart and
//! two keepers: each keeper's work line carries their shop's id and resolves to it though the player stands
//! beside the other shop; the opening-hours plate beside one shop finds that shop's keeper, not the other
//! keeper of the same kind; a home line never takes the work id.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentWorkPlaceId : LF_Test
{
	protected static const string KIND = "test_residents_store";
	protected static const ResourceName PLACE_PREFAB = "{25C8B889A777399D}Prefabs/Props/Infrastructure/ConeTraffic/ConeTraffic_01_red.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/work-place-id";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ResidentDef Keeper(string id, string shopId)
	{
		LF_ResidentDef def = LF_ResidentDef.Make(id, "t_home", KIND);
		def.m_sWorkPlaceId = shopId;
		if (RedProof())
			def.m_sWorkPlaceId = string.Empty;

		def.Add("00:00", "home", "bench").Add("09:00", "work", "counter", "serve").Add("17:00", "home", "bench");
		return def;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity vault = LF_Places.Find(LF_Bank.PLACE_KIND, LF_TestAnnex.BANK_VAULT);
		ctx.NotNull(vault, "the annex vault is placed, to spawn the stand-ins near");
		if (!vault)
			return;

		vector shopAAt = vault.GetOrigin() + "0 -20 0";
		vector shopBAt = shopAAt + "6 0 0";
		vector signAt = shopAAt + "0 0 2";
		IEntity shopA = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, shopAAt, "0 0 0", false);
		IEntity shopB = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, shopBAt, "0 0 0", false);
		IEntity sign = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, signAt, "0 0 0", false);
		ctx.True(shopA && shopB && sign, "two shops and a sign spawn");
		if (!shopA || !shopB || !sign)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(shopA);
			SCR_EntityHelper.DeleteEntityAndChildren(shopB);
			SCR_EntityHelper.DeleteEntityAndChildren(sign);
			return;
		}

		ctx.True(LF_Places.Register(KIND, "shop_a", shopA), "shop a is placed");
		ctx.True(LF_Places.Register(KIND, "shop_b", shopB), "shop b is placed");

		LF_ResidentsConfig config = new LF_ResidentsConfig();
		config.m_aResidents = {};
		config.m_aResidents.Insert(Keeper("wp_keeper_a", "shop_a"));
		config.m_aResidents.Insert(Keeper("wp_keeper_b", "shop_b"));
		LF_ResidentTimetable.Load(config, new LF_Test_AnyKinds());

		LF_ResidentTimetable dayA = LF_ResidentTimetable.Find("wp_keeper_a");
		LF_ResidentTimetable dayB = LF_ResidentTimetable.Find("wp_keeper_b");
		ctx.True(dayA && dayB, "both keepers have a day");
		if (dayA && dayB)
		{
			ctx.EqualStr("shop_a", dayA.At(10 * 60).m_sPlaceId, "keeper a's work line carries shop a");
			ctx.EqualStr("shop_b", dayB.At(10 * 60).m_sPlaceId, "keeper b's work line carries shop b");
			ctx.EqualStr(string.Empty, dayA.At(20 * 60).m_sPlaceId, "a home line never takes the work place id");
			ctx.EqualStr("shop_a", LF_HoursPlate.WorkPlaceId(dayA), "the plate reads the keeper's work place id from the day");

			// The player stands beside shop a; keeper b still resolves to shop b.
			LF_ResidentSpot spot = new LF_ResidentSpot();
			LF_ResidentSlot slotB = dayB.At(10 * 60);
			ctx.True(LF_ResidentAnchors.ResolveIn(slotB.m_sKind, slotB.m_sPlaceId, slotB.m_sAnchor, shopAAt, spot), "keeper b's workplace resolves");
			ctx.EqualFloat(shopBAt[0], spot.m_vOrigin[0], 0.05, "keeper b stands at shop b although a player stands beside shop a");
		}

		// The hours plate beside shop a prints keeper a, not keeper b (both work a place of this kind within 15 m).
		ctx.EqualStr("wp_keeper_a", LF_HoursPlate.KeeperNear(sign), "the plate beside shop a finds shop a's keeper");

		// A keeper whose shop id no place holds counts no place: the plate has no keeper to print.
		LF_ResidentsConfig lost = new LF_ResidentsConfig();
		lost.m_aResidents = {};
		lost.m_aResidents.Insert(Keeper("wp_keeper_lost", "shop_nowhere"));
		LF_ResidentTimetable.Load(lost, new LF_Test_AnyKinds());
		if (!RedProof())
			ctx.EqualStr("", LF_HoursPlate.KeeperNear(sign), "a keeper whose shop id no place holds is no keeper of a nearby shop");

		LF_ResidentTimetable.ResetStatics();
		LF_Places.Unregister(KIND, "shop_a");
		LF_Places.Unregister(KIND, "shop_b");
		SCR_EntityHelper.DeleteEntityAndChildren(shopA);
		SCR_EntityHelper.DeleteEntityAndChildren(shopB);
		SCR_EntityHelper.DeleteEntityAndChildren(sign);
	}
}
