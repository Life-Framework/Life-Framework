// red-proof: the server was not booted. tools\cli test was not run.
// tier: LOGIC
class EL_Test_Roadmap_Garage : EL_Test
{
	override string GetName()
	{
		return "roadmap/garage";
	}

	override void Run(EL_TestContext ctx)
	{
		array<ref LF_GarageCatalogRow> rows = LF_Garage.Catalog();
		ctx.Equal(5, rows.Count(), "the catalog lists the civilian vehicles");
		if (rows.Count() < 5)
			return;

		ctx.EqualStr("s105", rows.Get(0).m_sId, "the catalog opens with the S105");
		ctx.EqualStr("#EL-Vehicle_S105", rows.Get(0).m_sNameKey, "the S105 row shows its name");
		ctx.Equal(1000, rows.Get(0).m_iPrice, "the S105 cash price is the lot price");
		ctx.EqualStr(EL_VehicleCatalog.S105, rows.Get(0).m_sPrefab, "the S105 preview slot names that prefab");
		ctx.EqualStr(EL_VehicleCatalog.S105_LOCKED, rows.Get(0).m_sSpawnPrefab, "the S105 spawns the locked child");
		ctx.EqualStr("", rows.Get(0).m_sTexture, "the S105 preview texture is empty");

		ctx.EqualStr("uaz469", rows.Get(1).m_sId, "the covered UAZ follows the S105");
		ctx.EqualStr(EL_VehicleCatalog.UAZ469, rows.Get(1).m_sPrefab, "the UAZ preview slot names that prefab");
		ctx.Equal(2500, rows.Get(1).m_iPrice, "the UAZ cash price is the lot price");

		ctx.EqualStr("s1203", rows.Get(2).m_sId, "the S1203 follows the UAZ");
		ctx.EqualStr(EL_VehicleCatalog.S1203, rows.Get(2).m_sPrefab, "the S1203 preview slot names that prefab");
		ctx.Equal(4000, rows.Get(2).m_iPrice, "the S1203 cash price is the lot price");

		ctx.EqualStr("uaz452", rows.Get(3).m_sId, "the UAZ452 follows the S1203");
		ctx.EqualStr(EL_VehicleCatalog.UAZ452, rows.Get(3).m_sPrefab, "the UAZ452 preview slot names that prefab");
		ctx.Equal(3500, rows.Get(3).m_iPrice, "the UAZ452 cash price is the lot price");

		ctx.EqualStr("m923", rows.Get(4).m_sId, "the catalog ends with the M923 transport");
		ctx.EqualStr(EL_VehicleCatalog.M923, rows.Get(4).m_sPrefab, "the M923 preview slot names that prefab");
		ctx.Equal(8000, rows.Get(4).m_iPrice, "the M923 cash price is the lot price");
		ctx.EqualStr("", rows.Get(4).m_sTexture, "the M923 preview texture is empty");

		ref array<ref LF_GarageCatalogRow> skipped = {};
		LF_Garage.AppendRow(skipped, "missing", "#EL-Vehicle_S105", 1, "", "");
		ctx.Equal(0, skipped.Count(), "a missing prefab is skipped");

		array<ref LF_UIPart> parts = LF_GarageScreen.GetParts();
		ctx.True(parts.Count() > 0, "the garage screen has parts");
		if (parts.Count() == 0)
			return;

		ctx.EqualStr("VehicleTitle", parts.Get(0).GetName(), "the screen opens with the garage title");
		ctx.EqualStr("#LF-Garage_Title", parts.Get(0).GetText(), "the title is the garage title");

		int seen = 0;
		foreach (LF_UIPart part : parts)
		{
			if (!part || part.GetKind() != LF_EUIPartKind.STATUS)
				continue;

			if (seen == 0)
			{
				ctx.EqualStr("s105", part.GetName(), "the first screen row is the S105");
				ctx.EqualStr("#EL-Vehicle_S105", part.GetText(), "the first screen row shows the S105 name");
				ctx.EqualStr("1000", part.GetStatus(), "the first screen row shows the cash price");
			}

			seen++;
		}

		ctx.Equal(5, seen, "the screen lists every catalog row");

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
		ctx.Equal(1, shop.GetIndex(), "next moves to the UAZ");
		ctx.EqualStr("2500", shop.PriceLine(), "the price follows the selected vehicle");
		shop.Select(1);
		shop.Select(1);
		shop.Select(1);
		shop.Select(1);
		ctx.Equal(4, shop.GetIndex(), "next stops on the last vehicle");
	}
}
