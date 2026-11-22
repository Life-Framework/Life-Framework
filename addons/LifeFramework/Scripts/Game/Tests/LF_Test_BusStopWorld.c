// red-proof: 2026-11-22 `node tools/cli.mjs test --no-build --red LF_Test_BusStop,LF_Test_BusStopWorld,LF_Test_BusStopActions`: RED-OK travel/bus-stop-world (fails with the hook on, 32 assertions)
//! Bus stops in the world: a real character at a stop buys a ticket with cash, walks off and gets
//! the fare back; cancels a ticket ("Cancel trip") and gets it back; one with too little cash is
//! refused and keeps it; a second ticket rides and the
//! character is set down at the far stop with its feet on the ground (within 5 cm), not on the road
//! and not inside a prop. Stand-in stops are taxi stand signs registered under the BusStop kind.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BusStopWorld : LF_Test
{
	static const ResourceName STOP_PREFAB = "{2FD0DE951C3DC94E}Prefabs/Jobs/LF_TaxiStand.et";
	static const string FROM_ID = "test_bus_from";
	static const string TO_ID = "test_bus_to";
	static const string KEY = "test-bus-passenger";

	override string GetName()
	{
		return "travel/bus-stop-world";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_BusStop.ResetStatics();
		LF_BusArrival.ResetStatics();
		LF_BusStopConfig config = LF_BusStopConfig.CreateDefault();
		config.m_fFadeSeconds = 0;
		config.m_fAskCooldown = 0;
		config.m_fTripCooldown = 0;
		LF_BusStopConfig.UseForTest(config);

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
		{
			LF_BusStopConfig.Reload();
			return;
		}

		vector origin = character.GetOrigin();
		IEntity from = LF_Utils.SpawnEntityPrefab(STOP_PREFAB, origin + "2 0 0");
		IEntity to = LF_Utils.SpawnEntityPrefab(STOP_PREFAB, origin + "40 0 25");
		if (!from || !to)
		{
			ctx.Fail("stand-in stop prefab did not spawn: " + STOP_PREFAB);
			Cleanup(character, from, to);
			return;
		}

		LF_Places.Register(LF_BusStop.PLACE_KIND, FROM_ID, from);
		LF_Places.Register(LF_BusStop.PLACE_KIND, TO_ID, to);
		int fare = LF_BusStop.Fare(config, vector.Distance(from.GetOrigin(), to.GetOrigin()));

		// Too little cash: refused, nothing taken.
		LF_MoneyUtils.RemoveCash(character, LF_MoneyUtils.GetCash(character), "test");
		LF_MoneyUtils.AddCash(character, fare - 1, "test");
		ctx.True(LF_BusStop.Board(0, character, KEY, FROM_ID, TO_ID) == LF_EBusRefusal.NO_CASH, "a passenger short of the fare is refused");
		ctx.Equal(fare - 1, LF_MoneyUtils.GetCash(character), "a refused ticket takes no cash");
		ctx.True(LF_BusStop.Board(0, character, KEY, FROM_ID, FROM_ID) != LF_EBusRefusal.NONE, "a ticket to the same stop is refused");
		ctx.True(LF_BusStop.Board(0, character, KEY, FROM_ID, "no_such_stop") == LF_EBusRefusal.NO_STOP, "a ticket to an unplaced stop is refused");

		// Paid, then walked off: the fare comes back.
		LF_MoneyUtils.AddCash(character, 100, "test");
		int before = LF_MoneyUtils.GetCash(character);
		ctx.True(LF_BusStop.Board(0, character, KEY, FROM_ID, TO_ID) == LF_EBusRefusal.NONE, "a passenger at the stop with the fare buys a ticket");
		ctx.Equal(before - fare, LF_MoneyUtils.GetCash(character), "the ticket takes the fare in cash");
		ctx.True(LF_BusStop.Board(0, character, KEY, FROM_ID, TO_ID) == LF_EBusRefusal.BUSY, "a second ticket while one waits is refused");

		character.SetOrigin(origin + "-6 0 0");
		if (!RedProof())
			LF_BusStop.Tick();

		ctx.Equal(0, LF_BusStop.CountTrips(), "walking off the stop cancels the wait");
		ctx.Equal(before, LF_MoneyUtils.GetCash(character), "a cancelled wait refunds the fare");

		// Paid, then "Cancel trip": the fare comes back and the stop is free again.
		character.SetOrigin(origin);
		ctx.True(LF_BusStop.Board(0, character, KEY, FROM_ID, TO_ID) == LF_EBusRefusal.NONE, "the passenger buys a ticket to cancel");
		bool cancelled = false;
		if (!RedProof())
			cancelled = LF_BusStop.CancelAsked(0, KEY);

		ctx.True(cancelled, "a waiting ticket can be cancelled");
		ctx.Equal(0, LF_BusStop.CountTrips(), "a cancelled ticket no longer waits");
		ctx.Equal(before, LF_MoneyUtils.GetCash(character), "cancelling the trip refunds the fare");
		ctx.False(LF_BusStop.CancelAsked(0, KEY), "with no ticket waiting there is nothing to cancel");

		// Paid and waited: set down at the far stop on the ground.
		character.SetOrigin(origin);
		ctx.True(LF_BusStop.Board(0, character, KEY, FROM_ID, TO_ID) == LF_EBusRefusal.NONE, "the passenger buys a second ticket");
		LF_BusStop.DepartNowForTest(KEY);
		if (!RedProof())
			LF_BusStop.Tick();

		vector wanted = LF_BusArrival.WantedAt(to);
		vector at = character.GetOrigin();
		ctx.Equal(0, LF_BusStop.CountTrips(), "the trip is over once the bus arrives");
		ctx.True(vector.DistanceXZ(at, wanted) <= (LF_BusArrival.RINGS - 1) * LF_BusArrival.RING_STEP + 0.01, string.Format("the passenger is at the far stop (%1 m from its spot)", vector.DistanceXZ(at, wanted)));
		ctx.Equal(before - fare, LF_MoneyUtils.GetCash(character), "a ride keeps the fare");

		float ground;
		bool onGround = LF_BusArrival.GroundAt(character, at, ground);
		ctx.True(onGround, "there is ground under the arrival spot");
		if (onGround)
			ctx.True(Math.AbsFloat(at[1] - ground) <= 0.05, string.Format("the feet are within 5 cm of the ground (%1 m)", at[1] - ground));

		vector spot;
		ctx.True(LF_BusArrival.Find(to, character, spot), "the far stop has a clear arrival spot");
		ctx.False(LF_PlacementZones.IsOnRoad(spot), "the arrival spot is off the road");

		Cleanup(character, from, to);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity character, IEntity from, IEntity to)
	{
		LF_Places.Unregister(LF_BusStop.PLACE_KIND, FROM_ID);
		LF_Places.Unregister(LF_BusStop.PLACE_KIND, TO_ID);
		LF_BusStop.ResetStatics();
		LF_BusArrival.ResetStatics();
		LF_BusStopConfig.Reload();
		LF_TestCharacterFixture.Delete(from);
		LF_TestCharacterFixture.Delete(to);
		LF_TestCharacterFixture.Delete(character);
	}
}
