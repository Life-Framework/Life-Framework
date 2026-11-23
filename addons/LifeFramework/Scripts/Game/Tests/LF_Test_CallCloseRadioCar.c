// red-proof: 2026-11-23 RED PROOF OK, batched with LF_Test_CallCloseRadio: `tools\cli test --no-build --red LF_Test_CallCloseRadioCar` (RedProof() skips the press on the car's row, so the held call stays open and the close checks fail).
//! The police car carries the three close rows (#4585): resolved, no one found and unfounded. An on-duty
//! officer at the car, parked at the scene, presses "resolved" and the call they took closes resolved.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CallCloseRadioCar : LF_Test
{
	protected static const ResourceName MARKER = "{5D2B8E41A7C3906F}Prefabs/Systems/Vehicles/LF_PoliceCarMarker.et";
	protected static const string OFFICER = "close-radio-officer";
	protected static const string SUSPECT = "close-radio-suspect";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/call-close-radio-car";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PoliceCalls.ResetStatics();
		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer)
			return;

		IEntity markerEntity = LF_Utils.SpawnEntityPrefab(MARKER, officer.GetOrigin() + "4 0.5 0");
		LF_ParkedCarMarkerComponent marker;
		if (markerEntity)
			marker = LF_Component<LF_ParkedCarMarkerComponent>.Find(markerEntity);

		IEntity car;
		if (marker)
			car = marker.SpawnCar("test");

		ctx.True(car != null, "the police marker puts a car out");
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(OFFICER);
		if (car && account)
		{
			officer.SetOrigin(car.GetOrigin() + "2 0.1 0");
			LF_Utils.SetUidForTests(officer, OFFICER);
			RunCar(ctx, car, officer, account);
		}

		if (account)
		{
			account.SetOnDuty(false);
			account.SetFaction(LF_Faction.CIVILIAN);
		}

		LF_Utils.SetUidForTests(null, "");
		LF_PoliceCalls.ResetStatics();
		LF_PoliceCalls.Install();
		LF_TestCharacterFixture.Delete(car);
		LF_TestCharacterFixture.Delete(markerEntity);
		LF_TestCharacterFixture.Delete(officer);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCar(LF_TestContext ctx, IEntity car, IEntity officer, LF_PlayerAccount account)
	{
		LF_CallCloseAction resolved = FindAction(car, LF_CallOutcomeKind.RESOLVED);
		ctx.NotNull(resolved, "the police car has the 'call resolved' row");
		ctx.NotNull(FindAction(car, LF_CallOutcomeKind.NO_ONE_FOUND), "the police car has the 'no one found' row");
		ctx.NotNull(FindAction(car, LF_CallOutcomeKind.UNFOUNDED), "the police car has the 'call unfounded' row");
		if (!resolved)
			return;

		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		int state = LF_AccountStateBits.FromAccount(account);
		LF_PoliceCall call = LF_PoliceCalls.Post(LF_PoliceCalls.KIND_FINE, SUSPECT, "Square", LF_AccountStateBits.GetPoliceRank(state), true, car.GetOrigin());
		ctx.True(call != null, "the call posts at the car's spot");
		if (!call)
			return;

		ctx.True(LF_PoliceCalls.Take(call.m_iId, OFFICER, state), "the officer takes the call");
		if (!RedProof())
			resolved.PerformAction(car, officer);

		ctx.True(call.m_bClosed, "the car's 'call resolved' row closes the held call");
		ctx.Equal(LF_CallOutcomeKind.RESOLVED, call.m_iOutcome, "the call closed resolved");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_CallCloseAction FindAction(IEntity car, LF_CallOutcomeKind outcome)
	{
		ActionsManagerComponent actions = ActionsManagerComponent.Cast(car.FindComponent(ActionsManagerComponent));
		if (!actions)
			return null;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		foreach (BaseUserAction base : list)
		{
			LF_CallCloseAction action = LF_CallCloseAction.Cast(base);
			if (action && action.Outcome() == outcome)
				return action;
		}

		return null;
	}
}
