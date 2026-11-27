// red-proof: with `RedProof()` on the alias table is not asked, so no saved place id is rewritten and every "renamed" check
// fails (the hideout, held ground, house, vehicle shop and both store rows).

//! persist/save-ids-place-rows: a place that was renamed between releases must still load its saved rows (LF_SaveIds). The
//! hideout, held-ground, house-lock, vehicle and store rows each hold a place id; every one is rewritten through the alias
//! table on load, the other fields on the row (lock state, owner, stock, held points) are left exactly as saved, a place no
//! alias names loads as itself, and a place alias never renames a crop or job id of the same text. A null list or a null
//! row never stops a load.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_SaveIdsPlaceRows : LF_Test
{
	override string GetName()
	{
		return "persist/save-ids-place-rows";
	}

	override void Run(LF_TestContext ctx)
	{
		string hideoutNew = "-";
		string hideoutKept = "-";
		string hideoutOrg = "-";
		string heldNew = "-";
		int heldPoints = -1;
		string houseNew = "-";
		string houseKept = "-";
		string houseOwner = "-";
		bool houseLocked = false;
		string vehicleShop = "-";
		string vehicleOwner = "-";
		string stockStore = "-";
		string stockKept = "-";
		int stockCount = -1;
		string unpaidStore = "-";
		int unpaidPrice = -1;
		string unpaidCarrier = "-";
		string cropSamePlaceText = "-";
		string jobSamePlaceText = "-";
		int hideoutCount = -1;
		bool survived = false;
		if (!RedProof())
		{
			LF_SaveIds.ResetStatics();
			LF_SaveIds.Add(LF_SaveIds.KIND_PLACE, "old_hideout", "new_hideout");
			LF_SaveIds.Add(LF_SaveIds.KIND_PLACE, "old_house", "new_house");
			LF_SaveIds.Add(LF_SaveIds.KIND_PLACE, "old_shop", "new_shop");
			LF_SaveIds.Add(LF_SaveIds.KIND_PLACE, "old_store", "new_store");

			array<ref LF_HideoutRecord> hideouts = {};
			LF_HideoutRecord moved = new LF_HideoutRecord();
			moved.m_sHideoutId = "old_hideout";
			moved.m_sOrganizationId = "org-1";
			hideouts.Insert(moved);
			hideouts.Insert(null);
			LF_HideoutRecord stayed = new LF_HideoutRecord();
			stayed.m_sHideoutId = "kept_hideout";
			hideouts.Insert(stayed);
			LF_SaveIds.ResolveHideouts(hideouts);
			hideoutNew = moved.m_sHideoutId;
			hideoutKept = stayed.m_sHideoutId;
			hideoutOrg = moved.m_sOrganizationId;
			hideoutCount = hideouts.Count();
			LF_SaveIds.ResolveHideouts(null);

			array<ref LF_HeldGroundRow> held = {};
			LF_HeldGroundRow heldRow = new LF_HeldGroundRow();
			heldRow.m_sHideoutId = "old_hideout";
			heldRow.m_iHoldPoints = 7;
			held.Insert(heldRow);
			held.Insert(null);
			LF_SaveIds.ResolveHeldGround(held);
			heldNew = heldRow.m_sHideoutId;
			heldPoints = heldRow.m_iHoldPoints;
			LF_SaveIds.ResolveHeldGround(null);

			array<ref LF_HouseLockRecord> houses = {};
			houses.Insert(LF_HouseLockRecord.Create("old_house", true, "char-1"));
			houses.Insert(LF_HouseLockRecord.Create("kept_house", false));
			houses.Insert(null);
			LF_SaveIds.ResolveHouses(houses);
			houseNew = houses[0].m_sHouseIdentifier;
			houseOwner = houses[0].m_sOwnerKey;
			houseLocked = houses[0].m_bLocked;
			houseKept = houses[1].m_sHouseIdentifier;
			LF_SaveIds.ResolveHouses(null);

			array<ref LF_VehicleRecord> vehicles = {};
			LF_VehicleRecord car = new LF_VehicleRecord();
			car.m_sShopId = "old_shop";
			car.m_sOwnerUid = "uid-9";
			vehicles.Insert(car);
			vehicles.Insert(null);
			LF_SaveIds.ResolveVehicles(vehicles);
			vehicleShop = car.m_sShopId;
			vehicleOwner = car.m_sOwnerUid;
			LF_SaveIds.ResolveVehicles(null);

			array<ref LF_StoreStockRow> stock = {};
			LF_StoreStockRow stockRow = new LF_StoreStockRow();
			stockRow.m_sStoreKey = "old_store";
			stockRow.m_iStock = 12;
			stock.Insert(stockRow);
			LF_StoreStockRow stockOther = new LF_StoreStockRow();
			stockOther.m_sStoreKey = "other_store";
			stock.Insert(stockOther);
			stock.Insert(null);
			LF_SaveIds.ResolveStoreStock(stock);
			stockStore = stockRow.m_sStoreKey;
			stockCount = stockRow.m_iStock;
			stockKept = stockOther.m_sStoreKey;
			LF_SaveIds.ResolveStoreStock(null);

			array<ref LF_StoreUnpaidRow> unpaid = {};
			LF_StoreUnpaidRow unpaidRow = new LF_StoreUnpaidRow();
			unpaidRow.m_sStoreKey = "old_store";
			unpaidRow.m_iPrice = 40;
			unpaidRow.m_sCarrierKey = "char-2";
			unpaid.Insert(unpaidRow);
			unpaid.Insert(null);
			LF_SaveIds.ResolveStoreUnpaid(unpaid);
			unpaidStore = unpaidRow.m_sStoreKey;
			unpaidPrice = unpaidRow.m_iPrice;
			unpaidCarrier = unpaidRow.m_sCarrierKey;
			LF_SaveIds.ResolveStoreUnpaid(null);

			// A place alias is for places: a crop or a job with the same text stays itself.
			array<ref LF_FarmPlotRow> plots = {};
			LF_FarmPlotRow plot = new LF_FarmPlotRow();
			plot.m_sPlotKey = "kept_plot";
			plot.m_sCropId = "old_hideout";
			plots.Insert(plot);
			LF_SaveIds.ResolveFarmPlots(plots);
			cropSamePlaceText = plot.m_sCropId;

			array<ref LF_JobXpRow> xp = {};
			LF_JobXpRow xpRow = LF_JobXpRow.Create("uid-1", "old_house", 100);
			xp.Insert(xpRow);
			LF_SaveIds.ResolveJobXp(xp);
			jobSamePlaceText = xpRow.m_sJob;

			survived = true;
			LF_SaveIds.ResetStatics();
		}

		ctx.EqualStr("new_hideout", hideoutNew, "a renamed hideout loads under its new id");
		ctx.EqualStr("kept_hideout", hideoutKept, "a hideout nothing renamed loads as itself");
		ctx.EqualStr("org-1", hideoutOrg, "the hideout's owning organization is not touched by a place alias");
		ctx.Equal(3, hideoutCount, "the hideout list keeps its rows (a null row is skipped, not removed)");
		ctx.EqualStr("new_hideout", heldNew, "held ground is keyed by the same place id and follows the rename");
		ctx.Equal(7, heldPoints, "the held-ground points are saved data and stay as saved");
		ctx.EqualStr("new_house", houseNew, "a renamed house keeps its lock row under the new id");
		ctx.EqualStr("kept_house", houseKept, "a house nothing renamed loads as itself");
		ctx.EqualStr("char-1", houseOwner, "the house owner key is a character id and is never aliased");
		ctx.True(houseLocked, "the saved lock state survives the rewrite");
		ctx.EqualStr("new_shop", vehicleShop, "a car bought at a renamed shop still points at that shop");
		ctx.EqualStr("uid-9", vehicleOwner, "the car's owner is untouched");
		ctx.EqualStr("new_store", stockStore, "a renamed store keeps its saved stock row");
		ctx.Equal(12, stockCount, "the saved stock count stays as saved");
		ctx.EqualStr("other_store", stockKept, "a store nothing renamed loads as itself");
		ctx.EqualStr("new_store", unpaidStore, "the unpaid-item debt follows its renamed store");
		ctx.Equal(40, unpaidPrice, "the unpaid price is untouched");
		ctx.EqualStr("char-2", unpaidCarrier, "the carrier of the unpaid item is untouched");
		ctx.EqualStr("old_hideout", cropSamePlaceText, "a place alias does not rename a crop of the same text");
		ctx.EqualStr("old_house", jobSamePlaceText, "a place alias does not rename a job of the same text");
		ctx.True(survived, "a null list and a null row never stop a load");
	}
}
