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
		LF_Garage.AppendRow(skipped, "missing", "#EL-Vehicle_S105", 1, "");
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
	}
}
