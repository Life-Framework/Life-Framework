// red-proof: RedProof() leaves the missing prefab as the only car in the list, so `a list with a missing prefab still yields a car` fails (run `tools\cli test --no-build --red LF_Test_ParkedCarFallback`).

//! #3676: a parked-car marker whose car prefab does not spawn (the Z7211 on a server without the Rally content)
//! tries the next car in the list and remembers the failing prefab for the session. Every car and the marker
//! are deleted at the end.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ParkedCarFallback : LF_Test
{
	protected static const ResourceName MARKER = "{C48202E5BB6761C0}Prefabs/Systems/Vehicles/LF_ParkedCarMarker.et";
	protected static const ResourceName MISSING = "{0123456789ABCDEF}Prefabs/Vehicles/NoSuchCar_RallyOnly.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/parked-car-fallback";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ParkedCarDef Row(ResourceName prefab)
	{
		LF_ParkedCarDef def = new LF_ParkedCarDef();
		def.m_sPrefab = prefab;
		return def;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ParkedCars.ResetStatics();
		LF_ParkedCarConfig.Reload();
		LF_ParkedCarConfig config = LF_ParkedCarConfig.Get();
		ctx.True(config != null && config.m_aCars.Count() > 0, "the core parked car list has cars");

		IEntity desk = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.IMPOUND_DESK);
		ctx.True(desk != null, "Annex_Impound_Desk is placed");
		if (!config || !desk || config.m_aCars.Count() == 0)
			return;

		ResourceName real = config.m_aCars[0].m_sPrefab;
		vector spot = desk.GetOrigin() + "0 0 60";
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		IEntity markerEntity = LF_Utils.SpawnEntityPrefab(MARKER, spot);
		ctx.True(markerEntity != null, "the marker prefab spawns");
		array<IEntity> cars = {};
		LF_ParkedCarMarkerComponent marker;
		if (markerEntity)
			marker = LF_Component<LF_ParkedCarMarkerComponent>.Find(markerEntity);

		ctx.NotNull(marker, "the marker carries its component");
		if (marker)
			RunMarker(ctx, config, marker, real, cars);

		// A test deletes what it spawns.
		foreach (IEntity car : cars)
		{
			LF_TestCharacterFixture.Delete(car);
		}

		if (markerEntity)
			LF_TestCharacterFixture.Delete(markerEntity);

		LF_ParkedCars.ResetStatics();
		LF_ParkedCarConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunMarker(LF_TestContext ctx, LF_ParkedCarConfig config, LF_ParkedCarMarkerComponent marker, ResourceName real, array<IEntity> cars)
	{
		string missingId = LF_ParkedCarList.CarId(MISSING);
		string realId = LF_ParkedCarList.CarId(real);

		// 1. A list of only the missing prefab puts nothing out, and the prefab is remembered once.
		config.m_aCars = {};
		config.m_aCars.Insert(Row(MISSING));
		ctx.True(marker.SpawnCar("test") == null, "a list of only a missing prefab puts no car out");
		ctx.Equal(0, marker.SpawnCount(), "no car counted");
		ctx.True(LF_ParkedCars.FailedCars().Find(missingId) >= 0, "the missing prefab is remembered as failed");
		ctx.True(marker.SpawnCar("test") == null, "and it is not retried");
		ctx.Equal(1, LF_ParkedCars.FailedCars().Count(), "one failed prefab, not one per attempt");

		// 2. The pick skips failed prefabs.
		config.m_aCars = {};
		config.m_aCars.Insert(Row(MISSING));
		config.m_aCars.Insert(Row(real));
		array<string> failed = {};
		failed.Insert(missingId);
		bool onlyReal = true;
		for (int i = 0; i < 20; i++)
		{
			if (LF_ParkedCarList.CarId(config.PickPrefabAvoiding(failed)) != realId)
				onlyReal = false;
		}

		ctx.True(onlyReal, "the pick skips a failed prefab");
		failed.Insert(realId);
		ctx.True(config.PickPrefabAvoiding(failed).IsEmpty(), "with every car failed the pick is empty");

		// 3. With a fresh session, a list holding a missing prefab and a real one still yields a car, whichever
		// is drawn first; the missing one is recorded the moment it is drawn.
		LF_ParkedCars.ResetStatics();
		// RedProof(): the missing prefab is left alone in the list, so no car can come out.
		if (RedProof())
		{
			config.m_aCars = {};
			config.m_aCars.Insert(Row(MISSING));
		}

		int attempts = 0;
		bool everyCar = true;
		while (attempts < 12 && LF_ParkedCars.FailedCars().Count() == 0)
		{
			IEntity car = marker.SpawnCar("test");
			if (!car)
			{
				everyCar = false;
				break;
			}

			cars.Insert(car);
			attempts++;
		}

		ctx.True(everyCar, "a list with a missing prefab still yields a car");
		ctx.True(LF_ParkedCars.FailedCars().Find(missingId) >= 0, "the missing prefab was drawn and remembered");
		ctx.True(LF_ParkedCars.FailedCars().Find(realId) < 0, "the real car is not marked failed");
	}
}
