// red-proof: RED-OK licence/truck-check-gate, observed 2026-11-24 with `tools/cli test --no-build --red LF_Test_TruckCheckGate`: with Lend skipped no gate sees a lent truck, so the path run never advances (`sitting inside gate 0 ... expected 1, got 0`, `a step through gates 2 and 3 passes the course`). Hook off: green (--only).
//! licence/truck-check-gate (WK-W21, #4695): four gates stand on the course; the lent truck of an open
//! check run reaching them in order gives GATE_OK three times and the truck licence on the fourth; a
//! gate out of order fails the run and ends the loan; the server tick finds a truck driving through a
//! gate by its path (once per entry, several gates in one tick in order). Bodies: a real vehicle
//! prefab spawned near the gate, the gates from the prefab.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_TruckCheckGate : LF_Test
{
	protected static const ResourceName GATE = "{339E4BD26D1C5A04}Prefabs/Props/License/LF_TruckCheckGate.et";
	protected static const ResourceName TRUCK = "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et";
	protected static const string UID_A = "lf-test-truck-gate-a";
	protected static const string UID_B = "lf-test-truck-gate-b";
	protected static const string UID_C = "lf-test-truck-gate-c";
	protected static const int T0 = 1000000;
	protected static const float SPACING = 40;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "licence/truck-check-gate";
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnAt(ResourceName prefab, vector transform[4], vector at)
	{
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
			return null;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixCopy(transform, params.Transform);
		params.Transform[3] = at;
		return GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_TruckCheckGateComponent GateOf(IEntity gate)
	{
		return LF_TruckCheckGateComponent.Cast(gate.FindComponent(LF_TruckCheckGateComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected void Start(LF_TestContext ctx, string uid)
	{
		int left;
		LF_TestCashWallet wallet = LF_TestCashWallet.Create(LF_DrivingCheck.FEE, 100000);
		ctx.Equal(LF_EDrivingCheck.STARTED, LF_DrivingCheck.StartFor(uid, wallet, left), uid + ": the check starts");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_Progression.Forget(UID_A);
		LF_Progression.Forget(UID_B);
		LF_Progression.Forget(UID_C);
		LF_DrivingCheck.ResetStatics();
		LF_TruckCheckLoan.ResetStatics();
		LF_LicenceDesk.SetNowForTest(T0);

		IEntity anchor = LF_TestCharacterFixture.Spawn(ctx);
		if (!anchor)
			return;

		vector base[4];
		anchor.GetTransform(base);
		LF_TestCharacterFixture.Delete(anchor);

		array<IEntity> gates = {};
		for (int order = 0; order < LF_DrivingCheck.GATE_COUNT; order++)
		{
			IEntity gate = SpawnAt(GATE, base, base[3] + Vector(order * SPACING, 0, 60));
			ctx.NotNull(gate, "gate " + order.ToString() + " spawned");
			if (!gate)
			{
				Cleanup(gates, null, null);
				return;
			}

			LF_TruckCheckGateComponent component = GateOf(gate);
			ctx.NotNull(component, "gate " + order.ToString() + " carries the gate component");
			if (!component)
			{
				gates.Insert(gate);
				Cleanup(gates, null, null);
				return;
			}

			component.Configure(order, 6);
			gates.Insert(gate);
			ctx.Equal(order, component.GetOrder(), "gate " + order.ToString() + " holds its order");
			ctx.True(LF_Places.Find(LF_TruckCheckGateComponent.PLACE_KIND, LF_Places.OrderedId(LF_TruckCheckGateComponent.COURSE_ID, order)) == gate, "gate " + order.ToString() + " is placed as TruckCheckGate");
		}

		IEntity truck = SpawnAt(TRUCK, base, base[3] + Vector(0, 0.5, 20));
		ctx.NotNull(truck, "the lent truck spawned");
		if (!truck)
		{
			Cleanup(gates, null, null);
			return;
		}

		// A vehicle nobody lent drives through a gate: nothing happens.
		LF_TruckCheckGateComponent first = GateOf(gates[0]);
		ctx.Equal(LF_EDrivingCheck.NO_RUN, first.ServerReach(truck), "a truck nobody was lent moves no check");

		// Lend needs an open run.
		ctx.False(LF_TruckCheckLoan.Lend(UID_A, truck), "no run open: no loan");

		// The run through the four gates in order: GATE_OK three times, the licence on the fourth.
		Start(ctx, UID_A);
		if (!RedProof())
			ctx.True(LF_TruckCheckLoan.Lend(UID_A, truck), "with a run open the truck is lent");

		ctx.EqualStr(UID_A, LF_TruckCheckLoan.OwnerOf(truck), "the truck is lent to the player");
		ctx.False(LF_TruckCheckLoan.Lend(UID_B, truck), "a truck already lent is not lent again");
		int gate;
		for (gate = 0; gate < LF_DrivingCheck.GATE_COUNT - 1; gate++)
		{
			ctx.Equal(LF_EDrivingCheck.GATE_OK, GateOf(gates[gate]).ServerReach(truck), "gate " + gate.ToString() + " in order moves the run on");
			ctx.False(LF_DrivingCheck.HoldsTruckLicence(UID_A), "no licence before the last gate");
		}

		ctx.Equal(LF_EDrivingCheck.PASSED, GateOf(gates[LF_DrivingCheck.GATE_COUNT - 1]).ServerReach(truck), "the fourth gate passes the course");
		ctx.True(LF_DrivingCheck.HoldsTruckLicence(UID_A), "passing grants the truck licence");
		ctx.False(LF_TruckCheckLoan.HasLoan(UID_A), "the loan ends with the run");
		ctx.EqualStr("", LF_TruckCheckLoan.OwnerOf(truck), "the truck is nobody's after the pass");

		// A gate out of order fails the run, ends the loan and sets the retake wait.
		Start(ctx, UID_B);
		if (!RedProof())
			ctx.True(LF_TruckCheckLoan.Lend(UID_B, truck), "the truck is lent to the next run");

		ctx.Equal(LF_EDrivingCheck.FAILED_COURSE, GateOf(gates[2]).ServerReach(truck), "gate 2 first is out of order");
		ctx.False(LF_DrivingCheck.IsRunning(UID_B), "the failed run is closed");
		ctx.False(LF_DrivingCheck.HoldsTruckLicence(UID_B), "a failed course grants nothing");
		ctx.False(LF_TruckCheckLoan.HasLoan(UID_B), "a failed run ends the loan");

		// The server tick: a truck driving through a gate advances the check once, however many ticks it sits there.
		Start(ctx, UID_C);
		vector g0 = gates[0].GetOrigin();
		vector g1 = gates[1].GetOrigin();
		vector g3 = gates[3].GetOrigin();
		if (!RedProof())
			ctx.True(LF_TruckCheckLoan.Lend(UID_C, truck), "the truck is lent for the tick run");

		LF_TruckCheckLoan.TickLoanAt(UID_C, truck, g0 + "0 0 -30");
		ctx.Equal(0, LF_DrivingCheck.NextGate(UID_C), "outside every gate the run waits at gate 0");
		LF_TruckCheckLoan.TickLoanAt(UID_C, truck, g0);
		ctx.Equal(1, LF_DrivingCheck.NextGate(UID_C), "driving into gate 0 advances the check");
		LF_TruckCheckLoan.TickLoanAt(UID_C, truck, g0 + "1 0 0");
		ctx.Equal(1, LF_DrivingCheck.NextGate(UID_C), "sitting inside gate 0 does not count it again");
		LF_TruckCheckLoan.TickLoanAt(UID_C, truck, g1);
		ctx.Equal(2, LF_DrivingCheck.NextGate(UID_C), "driving into gate 1 advances the check");
		ctx.False(LF_DrivingCheck.HoldsTruckLicence(UID_C), "no licence yet");

		// One long step through gate 2 into gate 3 reaches both, in order, and passes the course.
		LF_TruckCheckLoan.TickLoanAt(UID_C, truck, g3);
		ctx.True(LF_DrivingCheck.HoldsTruckLicence(UID_C), "a step through gates 2 and 3 passes the course");
		ctx.False(LF_TruckCheckLoan.HasLoan(UID_C), "the tick run's loan ends with the pass");

		Cleanup(gates, truck, null);
		LF_Progression.Forget(UID_A);
		LF_Progression.Forget(UID_B);
		LF_Progression.Forget(UID_C);
		LF_DrivingCheck.ResetStatics();
		LF_TruckCheckLoan.ResetStatics();
		LF_LicenceDesk.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(array<IEntity> gates, IEntity truck, IEntity unused)
	{
		foreach (IEntity gate : gates)
		{
			if (gate)
				SCR_EntityHelper.DeleteEntityAndChildren(gate);
		}

		if (truck)
			SCR_EntityHelper.DeleteEntityAndChildren(truck);
	}
}
