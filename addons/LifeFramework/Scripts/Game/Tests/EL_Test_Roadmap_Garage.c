// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: dropping the empty-spawn-prefab fallback in
// LF_Garage.AppendRow failed it with `[ELTEST] FAIL roadmap/garage`, `- an empty spawn prefab falls
// back to the preview prefab (expected '{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et',
// got '')`. Restored; green.
// tier: LOGIC
class EL_Test_Roadmap_Garage : EL_Test
{
	override string GetName()
	{
		return "roadmap/garage";
	}

	override void Run(EL_TestContext ctx)
	{
		ref array<ref LF_GarageCatalogRow> rows = LF_Garage.Catalog();
		ctx.True(rows.Count() > 1, "the catalog lists more than one vehicle");
		if (rows.Count() < 2)
			return;

		// Invariants the purchase, preview and spawn paths depend on, not the price list itself.
		array<string> ids = {};
		foreach (LF_GarageCatalogRow row : rows)
		{
			ctx.NotNull(row, "every catalog row is present");
			if (!row)
				continue;

			ctx.False(row.m_sId.IsEmpty(), "every catalog row has an id");
			ctx.False(ids.Contains(row.m_sId), "catalog id " + row.m_sId + " is unique");
			ids.Insert(row.m_sId);
			ctx.True(row.m_sNameKey.IndexOf("#") == 0, "catalog row " + row.m_sId + " names itself with a localization key");
			ctx.True(row.m_iPrice > 0, "catalog row " + row.m_sId + " has a positive price");
			ctx.False(row.m_sPrefab.IsEmpty(), "catalog row " + row.m_sId + " has a preview prefab");
			ctx.False(row.m_sSpawnPrefab.IsEmpty(), "catalog row " + row.m_sId + " has a spawn prefab");
		}

		ref array<ref LF_GarageCatalogRow> skipped = {};
		LF_Garage.AppendRow(skipped, "missing", "#EL-Vehicle_S105", 1, "", "");
		ctx.Equal(0, skipped.Count(), "a missing prefab is skipped");

		ref array<ref LF_GarageCatalogRow> fallback = {};
		LF_Garage.AppendRow(fallback, "plain", "#EL-Vehicle_S105", 1, EL_VehicleCatalog.S105, "");
		ctx.Equal(1, fallback.Count(), "a row with only a preview prefab is kept");
		if (fallback.Count() == 1)
			ctx.EqualStr(EL_VehicleCatalog.S105, fallback.Get(0).m_sSpawnPrefab, "an empty spawn prefab falls back to the preview prefab");

		array<ref LF_UIPart> parts = LF_GarageScreen.GetParts();
		ctx.True(parts.Count() > 0, "the garage screen has parts");
		if (parts.Count() == 0)
			return;

		ctx.EqualStr("VehicleTitle", parts.Get(0).GetName(), "the screen opens with the garage title");
		ctx.EqualStr("#LF-Garage_Title", parts.Get(0).GetText(), "the title is the garage title");

		// The screen lists the catalog rows in order with their names and prices.
		int seen = 0;
		foreach (LF_UIPart part : parts)
		{
			if (!part || part.GetKind() != LF_EUIPartKind.STATUS)
				continue;

			if (seen < rows.Count())
			{
				LF_GarageCatalogRow expected = rows.Get(seen);
				ctx.EqualStr(expected.m_sId, part.GetName(), "screen row " + seen.ToString() + " is catalog row " + expected.m_sId);
				ctx.EqualStr(expected.m_sNameKey, part.GetText(), "screen row " + seen.ToString() + " shows the catalog name");
				ctx.EqualStr(expected.m_iPrice.ToString(), part.GetStatus(), "screen row " + seen.ToString() + " shows the catalog price");
			}

			seen++;
		}

		ctx.Equal(rows.Count(), seen, "the screen lists every catalog row");

		string vehicleId = "garage-logic-1";
		string owner = "garage-owner";
		string other = "garage-other";
		LF_Garage garage = new LF_Garage();
		ctx.True(garage.Place(LF_GarageRecord.Create(vehicleId, EL_VehicleCatalog.S105, owner, "key-logic")), "the vehicle starts in the world");
		ctx.True(garage.IsInWorld(vehicleId), "the world record is present");
		ctx.False(garage.Place(LF_GarageRecord.Create("garage-missing", "", owner, "key-logic")), "a record with no prefab is skipped");

		ctx.False(garage.Store(vehicleId, other), "a stranger cannot store the vehicle");
		ctx.True(garage.IsInWorld(vehicleId), "a refused store leaves the world record");
		ctx.True(garage.Store(vehicleId, owner), "the owner can store the vehicle");
		ctx.False(garage.IsInWorld(vehicleId), "store removes the world record");
		ctx.True(garage.IsStored(vehicleId, owner), "the stored record is the owner's");

		string prefab;
		string keyId;
		ctx.False(garage.Retrieve(vehicleId, other, prefab, keyId), "a stranger cannot retrieve the vehicle");
		ctx.False(garage.IsInWorld(vehicleId), "a refused retrieve leaves it stored");
		ctx.True(garage.Retrieve(vehicleId, owner, prefab, keyId), "the owner can retrieve the vehicle");
		ctx.EqualStr(EL_VehicleCatalog.S105, prefab, "retrieve returns the same prefab");
		ctx.EqualStr("key-logic", keyId, "retrieve returns the same key");
		ctx.True(garage.IsInWorld(vehicleId), "retrieve puts the vehicle back in the world");

		int cashDebit;
		int bankDebit;
		ctx.True(LF_Garage.Quote(100, 100, 0, cashDebit, bankDebit), "cash covers a price at the ceiling");
		ctx.Equal(100, cashDebit, "a small price debits cash");
		ctx.Equal(0, bankDebit, "a small price does not touch the bank");
		ctx.False(LF_Garage.Quote(1000, 1000, 0, cashDebit, bankDebit), "cash does not pay a price above the ceiling");
		ctx.True(LF_Garage.Quote(1000, 0, 1000, cashDebit, bankDebit), "the bank covers the S105 price");
		ctx.Equal(0, cashDebit, "the S105 price does not take cash");
		ctx.Equal(1000, bankDebit, "the S105 price debits the bank");
		ctx.EqualStr("#EL-Vehicle_CannotAfford", LF_GarageScreen.BuyLabel(1000, 0, 0), "a short balance disables the buy");
		ctx.EqualStr("#EL-Vehicle_Buy", LF_GarageScreen.BuyLabel(1000, 0, 1000), "a covered price keeps the buy");

		ctx.True(garage.AcceptPurchase(0, "garage-bought", owner, "key-bought", 0, 1000, 300, -4, 10, cashDebit, bankDebit), "the owner can buy the S105");
		int red;
		int green;
		int blue;
		ctx.True(garage.GetPaint("garage-bought", red, green, blue), "the purchase keeps its paint");
		ctx.Equal(255, red, "red is clamped to 255");
		ctx.Equal(0, green, "green is clamped to 0");
		ctx.Equal(10, blue, "blue is kept");
		ctx.Equal(0, LF_Garage.Channel(-1), "a channel below 0 is 0");
		ctx.Equal(255, LF_Garage.Channel(300), "a channel above 255 is 255");

		ctx.False(garage.Park(vehicleId, other, false, true), "a stranger cannot park the vehicle");
		ctx.True(garage.IsInWorld(vehicleId), "a refused park leaves the world record");
		ctx.False(garage.Park(vehicleId, owner, true, true), "park refuses a seated player");
		ctx.False(garage.Park(vehicleId, owner, false, false), "park refuses when no garage is nearby");
		ctx.True(garage.Park(vehicleId, owner, false, true), "the owner on foot can park");
		ctx.False(garage.IsInWorld(vehicleId), "park stores the vehicle");

		ctx.True(LF_Garage.PadFree(0), "an empty pad is free");
		ctx.False(LF_Garage.PadFree(1), "an occupied pad is refused");
		ctx.False(LF_Garage.PreviewReady("", EL_VehicleCatalog.S105), "a preview without a building is skipped");
		ctx.False(LF_Garage.CameraReady(""), "a camera without a prefab is skipped");
		ctx.EqualStr("Prefabs/Editor/Camera/ManualCameraBase.et", LF_Garage.CAMERA_PREFAB, "the shop camera is the editor camera already in the repo");

		float engine;
		float handling;
		float braking;
		float storage;
		LF_Garage.ReadStats("", engine, handling, braking, storage);
		ctx.EqualFloat(0, engine, 0.001, "missing stats leave the engine at 0");
		ctx.EqualFloat(0, handling, 0.001, "missing stats leave the handling at 0");
		ctx.EqualFloat(0, braking, 0.001, "missing stats leave the braking at 0");
		ctx.EqualFloat(0, storage, 0.001, "missing stats leave the storage at 0");
		ctx.EqualStr("#LF-Garage_Engine", LF_Garage.StatLine(0, "#LF-Garage_Engine"), "a zero stat keeps its label");

		LF_GarageScreen shop = new LF_GarageScreen();
		shop.Select(-1);
		ctx.Equal(0, shop.GetIndex(), "previous stops on the first vehicle");
		shop.Select(1);
		ctx.Equal(1, shop.GetIndex(), "next moves to the second vehicle");
		ctx.EqualStr(rows.Get(1).m_iPrice.ToString(), shop.PriceLine(), "the price follows the selected vehicle");
		for (int step = 0; step < rows.Count() + 1; step++)
			shop.Select(1);
		ctx.Equal(rows.Count() - 1, shop.GetIndex(), "next stops on the last vehicle");
	}
}
