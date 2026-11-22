// red-proof: 2026-11-22 `node tools/cli.mjs test --no-build --red LF_Test_BusStop,LF_Test_BusStopWorld`: RED-OK travel/bus-stop (fails with the hook on, 48 assertions)
//! Bus stops, the rules with no world: the fare and the wait grow with distance between a base and a
//! cap, every body state that may not ride is refused (dead, downed, cuffed or jailed, in a vehicle,
//! in a fight, wanted with an officer close), the two cooldowns hold, a waiting trip cancels when the
//! passenger walks off, is hit or hurt, and leaves on time, and the arrival search starts at the
//! wanted spot and widens round it.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_BusStop : LF_Test
{
	override string GetName()
	{
		return "travel/bus-stop";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_CharacterSwitchState Fine()
	{
		LF_CharacterSwitchState state = new LF_CharacterSwitchState();
		state.m_bHasCharacter = true;
		state.m_bAlive = true;
		return state;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_BusStopConfig config = LF_BusStopConfig.CreateDefault();
		ctx.Equal(10, config.m_iBaseFare, "the shipped base fare is 10");
		ctx.Equal(40, config.m_iMaxFare, "the shipped fare cap is 40");
		ctx.True(config.IsUsable(), "the shipped numbers can run a bus");

		// Fares: base + per km, capped. The act is skipped under --red, so every fare reads -1.
		int nearFare = -1;
		int midFare = -1;
		int farFare = -1;
		float nearWait = -1;
		float farWait = -1;
		if (!RedProof())
		{
			nearFare = LF_BusStop.Fare(config, 80);
			midFare = LF_BusStop.Fare(config, 3000);
			farFare = LF_BusStop.Fare(config, 20000);
			nearWait = LF_BusStop.WaitSeconds(config, 80);
			farWait = LF_BusStop.WaitSeconds(config, 20000);
		}

		ctx.Equal(10, nearFare, "an 80 m hop pays the base fare");
		ctx.Equal(25, midFare, "3 km pays the base plus 5 a km");
		ctx.Equal(40, farFare, "20 km pays the cap");
		ctx.EqualFloat(20.16, nearWait, 0.01, "a short hop waits the base 20 s plus 2 s a km");
		ctx.EqualFloat(30, farWait, 0.01, "a long trip waits no more than 30 s");
		ctx.Equal(10, LF_BusStop.Fare(config, -5), "a negative distance pays the base");

		int last = 0;
		bool rising = true;
		for (int i = 0; i < 25; i++)
		{
			int fare = LF_BusStop.Fare(config, i * 1000);
			if (fare < last)
				rising = false;

			last = fare;
		}

		ctx.True(rising, "a longer trip never costs less");

		LF_BusStopConfig broken = LF_BusStopConfig.CreateDefault();
		broken.m_iMaxFare = 5;
		ctx.False(broken.IsUsable(), "a cap below the base fare is unusable");

		// Refusals.
		LF_CharacterSwitchState state = Fine();
		ctx.True(LF_BusStop.RefuseFor(state, false) == LF_EBusRefusal.NONE, "a fit, free passenger may ride");
		ctx.True(LF_BusStop.RefuseFor(null, false) == LF_EBusRefusal.NO_CHARACTER, "no state is refused");

		state = Fine();
		state.m_bAlive = false;
		ctx.True(LF_BusStop.RefuseFor(state, false) == LF_EBusRefusal.DEAD, "a dead body is refused");

		state = Fine();
		state.m_bDowned = true;
		ctx.True(LF_BusStop.RefuseFor(state, false) == LF_EBusRefusal.DOWNED, "a downed body is refused");

		state = Fine();
		state.m_bCuffed = true;
		ctx.True(LF_BusStop.RefuseFor(state, false) == LF_EBusRefusal.CUFFED, "a cuffed body is refused");

		state = Fine();
		state.m_bJailed = true;
		ctx.True(LF_BusStop.RefuseFor(state, false) == LF_EBusRefusal.CUFFED, "a prisoner is refused");

		state = Fine();
		state.m_bInVehicle = true;
		ctx.True(LF_BusStop.RefuseFor(state, false) == LF_EBusRefusal.DRIVING, "a driver or passenger in a vehicle is refused");

		state = Fine();
		state.m_bInCombat = true;
		ctx.True(LF_BusStop.RefuseFor(state, false) == LF_EBusRefusal.COMBAT, "a player in a fight is refused");

		state = Fine();
		state.m_iWanted = 2;
		ctx.True(LF_BusStop.RefuseFor(state, true) == LF_EBusRefusal.PURSUIT, "wanted with an officer close is refused");
		ctx.True(LF_BusStop.RefuseFor(state, false) == LF_EBusRefusal.NONE, "wanted with no officer near may ride");

		// Cooldowns.
		ctx.False(LF_BusStop.IsTooSoon(config, -1, -1, 100), "a first request is never too soon");
		ctx.True(LF_BusStop.IsTooSoon(config, 99, -1, 100), "a second request inside 3 s is too soon");
		ctx.False(LF_BusStop.IsTooSoon(config, 90, -1, 100), "a request after the ask cooldown passes");
		ctx.True(LF_BusStop.IsTooSoon(config, -1, 70, 100), "a ride 30 s after an arrival is too soon");
		ctx.False(LF_BusStop.IsTooSoon(config, -1, 30, 100), "a ride 70 s after an arrival passes");

		// A waiting trip.
		LF_BusTrip trip = new LF_BusTrip();
		trip.m_fBoughtAt = 100;
		trip.m_fDepartAt = 125;
		trip.m_vStandAt = Vector(500, 10, 500);
		trip.m_fHealthAtStart = 100;
		vector here = trip.m_vStandAt + Vector(1, 0, 1);
		ctx.True(LF_BusStop.Step(trip, config, 110, here, Fine(), -1, 100) == LF_EBusStep.WAIT, "a passenger still at the stop waits");
		ctx.True(LF_BusStop.Step(trip, config, 125, here, Fine(), -1, 100) == LF_EBusStep.DEPART, "the bus leaves when the wait is over");
		ctx.True(LF_BusStop.Step(trip, config, 110, trip.m_vStandAt + Vector(5, 0, 0), Fine(), -1, 100) == LF_EBusStep.CANCEL_MOVED, "walking 5 m off cancels the wait");
		ctx.True(LF_BusStop.Step(trip, config, 110, here, Fine(), 105, 100) == LF_EBusStep.CANCEL_HURT, "a player-on-player hit after the ticket cancels it");
		ctx.True(LF_BusStop.Step(trip, config, 110, here, Fine(), 50, 100) == LF_EBusStep.WAIT, "a hit before the ticket does not");
		ctx.True(LF_BusStop.Step(trip, config, 110, here, Fine(), -1, 80) == LF_EBusStep.CANCEL_HURT, "losing health while waiting cancels it");
		LF_CharacterSwitchState downed = Fine();
		downed.m_bDowned = true;
		ctx.True(LF_BusStop.Step(trip, config, 110, here, downed, -1, 100) == LF_EBusStep.CANCEL_STATE, "going down while waiting cancels it");

		// Every refusal and cancel has its own text.
		ctx.EqualStr("#LF-BusStop_NoCash", LF_BusStop.RefusalKey(LF_EBusRefusal.NO_CASH), "no cash has its text");
		ctx.EqualStr("#LF-BusStop_CancelMoved", LF_BusStop.CancelKey(LF_EBusStep.CANCEL_MOVED), "walking off has its text");

		// The arrival search: the wanted spot first, then rings round it.
		array<vector> spots = {};
		vector wanted = Vector(1000, 20, 1000);
		LF_BusArrival.Candidates(wanted, spots);
		ctx.Equal(1 + (LF_BusArrival.RINGS - 1) * LF_BusArrival.DIRECTIONS, spots.Count(), "one wanted spot and eight per ring");
		if (spots.Count() > 1)
		{
			ctx.EqualFloat(0, vector.Distance(spots[0], wanted), 0.001, "the wanted spot is tried first");
			ctx.EqualFloat(LF_BusArrival.RING_STEP, vector.DistanceXZ(spots[1], wanted), 0.001, "the first ring is half a stride out");
			ctx.EqualFloat(wanted[1], spots[spots.Count() - 1][1], 0.001, "the search keeps the wanted height for the ground probe");
		}

		// A spot handed out is taken for a while, so two passengers on one bus never share it.
		LF_BusArrival.ResetStatics();
		ctx.False(LF_BusArrival.IsReserved(wanted, 100), "a fresh spot is free");
		LF_BusArrival.Reserve(wanted, 100);
		ctx.True(LF_BusArrival.IsReserved(wanted + "0.3 0 0", 105), "a spot beside one just handed out is taken");
		ctx.False(LF_BusArrival.IsReserved(wanted + "2 0 0", 105), "a spot two metres off is free");
		ctx.False(LF_BusArrival.IsReserved(wanted, 100 + LF_BusArrival.RESERVE_SECONDS + 1), "the reservation lapses");
		LF_BusArrival.ResetStatics();
	}
}
