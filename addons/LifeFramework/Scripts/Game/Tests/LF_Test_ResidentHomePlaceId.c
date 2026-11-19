// red-proof: 2026-11-19, with the id ignored (RedProof() resolves by nearest only) the far house is
//   not chosen and the run fails: cli test --no-build --only LF_Test_ResidentHomePlaceId
//! residents/home-place-id: a resident's home can be one named house (#3231). Two `House` places: a
//! resident naming the far one resolves to it although the near one is closer to the player; an
//! empty id still picks the nearest; an unknown id falls back to the nearest and warns once.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentHomePlaceId : LF_Test
{
	protected static const string KIND = "test_residents_house";
	protected static const ResourceName PLACE_PREFAB = "{25C8B889A777399D}Prefabs/Props/Infrastructure/ConeTraffic/ConeTraffic_01_red.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/home-place-id";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity vault = LF_Places.Find(LF_Bank.PLACE_KIND, LF_TestAnnex.BANK_VAULT);
		ctx.NotNull(vault, "the annex vault is placed, to spawn the stand-ins near");
		if (!vault)
			return;

		vector nearAt = vault.GetOrigin() + "0 -20 0";
		vector farAt = nearAt + "400 0 0";
		IEntity houseNear = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, nearAt, "0 0 0", false);
		IEntity houseFar = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, farAt, "0 0 0", false);
		ctx.True(houseNear && houseFar, "two test houses spawn");
		if (!houseNear || !houseFar)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(houseNear);
			SCR_EntityHelper.DeleteEntityAndChildren(houseFar);
			return;
		}

		ctx.True(LF_Places.Register(KIND, "house_near", houseNear), "the near house is placed");
		ctx.True(LF_Places.Register(KIND, "house_far", houseFar), "the far house is placed");

		// A resident whose home is the far house, with the player standing at the near one.
		string homeId = "house_far";
		if (RedProof())
			homeId = string.Empty;

		LF_ResidentDef def = LF_ResidentDef.Make("test_home_id", KIND, "");
		def.m_sHomePlaceId = homeId;
		LF_ResidentSpot spot = new LF_ResidentSpot();
		int warnsBefore = LF_ResidentAnchors.WarnCount();

		ctx.True(LF_ResidentAnchors.ResolveIn(KIND, def.m_sHomePlaceId, "", nearAt, spot), "a named home resolves");
		ctx.EqualFloat(farAt[0], spot.m_vOrigin[0], 0.05, "the named far house answers, not the nearest");

		ctx.True(LF_ResidentAnchors.ResolveIn(KIND, "", "", nearAt, spot), "an empty id resolves");
		ctx.EqualFloat(nearAt[0], spot.m_vOrigin[0], 0.05, "an empty id is the nearest house");

		ctx.True(LF_ResidentAnchors.Resolve(KIND, "", nearAt, spot), "the old Resolve still resolves");
		ctx.EqualFloat(nearAt[0], spot.m_vOrigin[0], 0.05, "the old Resolve is the nearest house");
		ctx.Equal(warnsBefore, LF_ResidentAnchors.WarnCount(), "a named and an empty home do not warn");

		ctx.True(LF_ResidentAnchors.ResolveIn(KIND, "house_nowhere", "", nearAt, spot), "an unknown id still resolves");
		ctx.EqualFloat(nearAt[0], spot.m_vOrigin[0], 0.05, "an unknown id falls back to the nearest house");
		ctx.Equal(warnsBefore + 1, LF_ResidentAnchors.WarnCount(), "an unknown id warns");
		LF_ResidentAnchors.ResolveIn(KIND, "house_nowhere", "", nearAt, spot);
		ctx.Equal(warnsBefore + 1, LF_ResidentAnchors.WarnCount(), "the same unknown id warns once");

		// The id reaches the timetable line: a home line carries it, a work line does not.
		def.m_sWorkKind = KIND;
		def.Add("08:00", "home").Add("12:00", "work");
		LF_ResidentTimetable day = LF_ResidentTimetable.Build(def);
		ctx.NotNull(day, "the day builds");
		if (day)
		{
			ctx.EqualStr(homeId, day.At(9 * 60).m_sPlaceId, "the home line carries the home place id");
			ctx.EqualStr(string.Empty, day.At(13 * 60).m_sPlaceId, "the work line names no house");
		}

		LF_Places.Unregister(KIND, "house_near");
		LF_Places.Unregister(KIND, "house_far");
		SCR_EntityHelper.DeleteEntityAndChildren(houseNear);
		SCR_EntityHelper.DeleteEntityAndChildren(houseFar);
	}
}
