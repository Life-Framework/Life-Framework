// red-proof: 2026-11-17 (#2405): with `CreateDefault().m_bCarryAnyone` true, `--only LF_Test_Carry` failed `- by default a bystander may not load a downed player (#2405)`. With the default off, green.
// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_Carry`: LF_Carry.DecideStart ignoring the range failed it with `- out of range is refused (expected 5, got 0)`. Restored; green.
// red-proof: 2026-11-15 (the drag and carry hold cut, Load offered straight on the body): RedProof() skips the Load call; `tools\cli test --no-build --red LF_Test_Carry` names this test RED-OK.
// Why the seat itself is not asserted here: the engine seats a character a frame after the request
// (see LF_Test_JailVehicleArrest) and the suite is synchronous. This test proves the request is
// accepted and the event fires; `session.mjs carry-proof` reads the seat on a client.

//------------------------------------------------------------------------------------------------
class LF_TestCarryRecorder : LF_EventSubscriber
{
	string m_sLog;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPlayerCarried carried = LF_OnPlayerCarried.Cast(busEvent);
		if (carried)
			m_sLog += carried.m_sKind + ";";

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! medic/carry (#261): load a downed body into a vehicle. The pure rules, then the load on two
//! spawned characters beside the annex escort car: a standing body, a body out of reach and a body
//! with no vehicle near are refused and nothing is published; a downed body in reach is accepted,
//! counted and published once as "load".
// tier: WORLD
class LF_Test_Carry : LF_Test
{
	override string GetName()
	{
		return "medic/carry";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		float range = config.m_fCarryRange;
		ctx.Equal(LF_ECarryRefusal.NONE, LF_Carry.DecideLoad(true, false, true, true, 1, range), "a player beside a downed body may load it");
		ctx.Equal(LF_ECarryRefusal.NO_PATIENT, LF_Carry.DecideLoad(false, false, true, true, 1, range), "no patient is refused");
		ctx.Equal(LF_ECarryRefusal.SELF, LF_Carry.DecideLoad(true, true, true, true, 0, range), "loading yourself is refused");
		ctx.Equal(LF_ECarryRefusal.NOT_DOWNED, LF_Carry.DecideLoad(true, false, false, true, 1, range), "a standing player is refused");
		ctx.Equal(LF_ECarryRefusal.NOT_ALLOWED, LF_Carry.DecideLoad(true, false, true, false, 1, range), "a civilian is refused when config says medics only");
		ctx.Equal(LF_ECarryRefusal.TOO_FAR, LF_Carry.DecideLoad(true, false, true, true, range + 1, range), "out of range is refused");
		ctx.True(LF_Carry.MayCarry(true, false), "anyone may when config says anyone");
		ctx.False(LF_Carry.MayCarry(false, false), "a civilian may not when config says medics only");
		ctx.True(LF_Carry.MayCarry(false, true), "an on-duty medic may when config says medics only");
		ctx.True(LF_Carry.MayCarry(false, false, true), "on-duty police may when config says medics only");
		ctx.False(LF_Carry.MayCarry(false, false, false), "a civilian may not when nobody on duty is asking");
		ctx.True(LF_Carry.MayUseVehicle(false, false), "a civilian vehicle is open to police and anyone who may load");
		ctx.False(LF_Carry.MayUseVehicle(true, false), "a stretcher vehicle is not open to a non-medic");
		ctx.True(LF_Carry.MayUseVehicle(true, true), "a stretcher vehicle is open to an on-duty medic");
		ctx.False(LF_MedicConfig.CreateDefault().m_bCarryAnyone, "by default a bystander may not load a downed player (#2405)");

		bool wasEnabled = config.m_bDownedEnabled;
		bool wasAnyone = config.m_bCarryAnyone;
		config.m_bDownedEnabled = true;
		config.m_bCarryAnyone = true;
		LF_Downed.ResetStatics();
		LF_Downed.SetTestAllEligible(true);
		LF_Carry.ResetStatics();
		LF_EventBus.ResetStatics();
		LF_TestCarryRecorder recorder = new LF_TestCarryRecorder();
		LF_EventBus.Subscribe(LF_OnPlayerCarried, recorder);

		IEntity carrier = LF_TestCharacterFixture.Spawn(ctx);
		IEntity patient = LF_TestCharacterFixture.Spawn(ctx);
		if (carrier && patient)
			RunLoad(ctx, recorder, carrier, patient);

		LF_TestCharacterFixture.Delete(carrier);
		LF_TestCharacterFixture.Delete(patient);
		config.m_bDownedEnabled = wasEnabled;
		config.m_bCarryAnyone = wasAnyone;
		LF_Downed.SetTestAllEligible(false);
		LF_Downed.ResetStatics();
		LF_Carry.ResetStatics();
		LF_EventBus.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunLoad(LF_TestContext ctx, LF_TestCarryRecorder recorder, IEntity carrier, IEntity patient)
	{
		SCR_ChimeraCharacter body = SCR_ChimeraCharacter.Cast(patient);
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(patient.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!body || !damage)
		{
			ctx.Fail("the patient lacks SCR_ChimeraCharacter or the character damage manager");
			return;
		}

		IEntity car = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.ESCORT_CAR);
		ctx.NotNull(car, "DebugWorld has Annex_Escort_Car");
		if (!car)
			return;

		carrier.SetOrigin(car.GetOrigin() + "3 0 0");
		patient.SetOrigin(car.GetOrigin() + "4 0 0");

		ctx.Equal(LF_ECarryRefusal.NOT_DOWNED, LF_Carry.Load(carrier, patient), "a standing body is refused");
		ctx.False(LF_Carry.CanOfferLoad(carrier, patient), "Load is not offered on a standing body");

		damage.Kill(Instigator.CreateInstigator(null));
		ctx.True(body.LF_IsDowned(), "the patient is downed");

		// #2405: a bystander with no duty cannot load a downed player when config is at its default.
		LF_MedicConfig.Get().m_bCarryAnyone = false;
		ctx.False(LF_Carry.CanOfferLoad(carrier, patient), "Load is not offered to a civilian bystander");
		ctx.Equal(LF_ECarryRefusal.NOT_ALLOWED, LF_Carry.Load(carrier, patient), "the server refuses a civilian bystander");
		LF_MedicConfig.Get().m_bCarryAnyone = true;

		// Too far, then in reach.
		patient.SetOrigin(carrier.GetOrigin() + "30 0 0");
		ctx.Equal(LF_ECarryRefusal.TOO_FAR, LF_Carry.Load(carrier, patient), "a body out of reach is refused");
		patient.SetOrigin(carrier.GetOrigin() + "1 0 0");

		// No vehicle near: both walk off the car, then come back.
		carrier.SetOrigin(car.GetOrigin() + "60 0 0");
		patient.SetOrigin(carrier.GetOrigin() + "1 0 0");
		ctx.True(LF_Carry.FindSeatVehicle(carrier, patient) == null, "no vehicle in load range out there");
		ctx.Equal(LF_ECarryRefusal.NO_SEAT, LF_Carry.Load(carrier, patient), "a body with no vehicle near is refused");
		ctx.EqualStr("", recorder.m_sLog, "a refused load publishes nothing");
		carrier.SetOrigin(car.GetOrigin() + "3 0 0");
		patient.SetOrigin(carrier.GetOrigin() + "1 0 0");

		ctx.True(LF_Carry.FindSeatVehicle(carrier, patient) != null, "the escort car has a free passenger seat in reach");
		ctx.True(LF_Carry.CanOfferLoad(carrier, patient), "Load is offered on the downed body beside the car");
		LF_ECarryRefusal result = LF_ECarryRefusal.NO_SEAT;
		if (!RedProof())
			result = LF_Carry.Load(carrier, patient);
		ctx.Equal(LF_ECarryRefusal.NONE, result, "the load into the car is accepted");
		ctx.Equal(1, LF_Carry.GetLoadedCount(), "one load counted");
		ctx.EqualStr("load;", recorder.m_sLog, "the bus saw one load");
	}
}
