// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_Carry`: LF_Carry.DecideStart ignoring the range failed it with `- out of range is refused (expected 5, got 0)`. Restored; green.
// Why the seat itself is not asserted here: the engine seats a character a frame after the request
// (see LF_Test_JailVehicleArrest) and the suite is synchronous. This test proves the request is
// accepted, the hold ends and the event fires; `session.mjs carry-proof` reads the seat on a client.

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
//! medic/carry (#261): drag, drop, carry and load of a downed body. The pure rules, then the
//! hold on two spawned characters: refusals change nothing, a hold marks the body, a drop frees it,
//! a load beside the DebugWorld escort car is accepted and ends the hold.
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
		ctx.Equal(LF_ECarryRefusal.NONE, LF_Carry.DecideStart(true, false, true, false, false, true, 1, range), "a player beside a downed body may carry it");
		ctx.Equal(LF_ECarryRefusal.NO_PATIENT, LF_Carry.DecideStart(false, false, true, false, false, true, 1, range), "no patient is refused");
		ctx.Equal(LF_ECarryRefusal.SELF, LF_Carry.DecideStart(true, true, true, false, false, true, 0, range), "carrying yourself is refused");
		ctx.Equal(LF_ECarryRefusal.NOT_DOWNED, LF_Carry.DecideStart(true, false, false, false, false, true, 1, range), "a standing player is refused");
		ctx.Equal(LF_ECarryRefusal.ALREADY_HELD, LF_Carry.DecideStart(true, false, true, true, false, true, 1, range), "a body someone holds is refused");
		ctx.Equal(LF_ECarryRefusal.BUSY, LF_Carry.DecideStart(true, false, true, false, true, true, 1, range), "a carrier with a body already is refused");
		ctx.Equal(LF_ECarryRefusal.NOT_ALLOWED, LF_Carry.DecideStart(true, false, true, false, false, false, 1, range), "a civilian is refused when config says medics only");
		ctx.Equal(LF_ECarryRefusal.TOO_FAR, LF_Carry.DecideStart(true, false, true, false, false, true, range + 1, range), "out of range is refused");
		ctx.True(LF_Carry.MayCarry(true, false), "anyone may when config says anyone");
		ctx.False(LF_Carry.MayCarry(false, false), "a civilian may not when config says medics only");
		ctx.True(LF_Carry.MayCarry(false, true), "an on-duty medic may when config says medics only");

		vector behind = LF_Carry.FollowPoint("10 0 10", "0 0 1", 1.5);
		ctx.EqualFloat(10, behind[0], 0.01, "the body trails on the carrier's x");
		ctx.EqualFloat(8.5, behind[2], 0.01, "the body trails 1.5 m behind a carrier facing +z");
		ctx.EqualStr("", LF_Carry.EndReason(true, false, true, true, 2, 8), "a healthy hold is kept");
		ctx.True(!LF_Carry.EndReason(false, false, true, true, 2, 8).IsEmpty(), "a downed carrier drops the body");
		ctx.True(!LF_Carry.EndReason(true, true, true, true, 2, 8).IsEmpty(), "a carrier in a vehicle drops the body");
		ctx.True(!LF_Carry.EndReason(true, false, true, false, 2, 8).IsEmpty(), "a revived body is released");
		ctx.True(!LF_Carry.EndReason(true, false, true, true, 9, 8).IsEmpty(), "a body that fell behind is released");

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
			RunCarry(ctx, recorder, carrier, patient);

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
	protected void RunCarry(LF_TestContext ctx, LF_TestCarryRecorder recorder, IEntity carrier, IEntity patient)
	{
		SCR_ChimeraCharacter body = SCR_ChimeraCharacter.Cast(patient);
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(patient.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!body || !damage)
		{
			ctx.Fail("the patient lacks SCR_ChimeraCharacter or the character damage manager");
			return;
		}

		IEntity car = GetGame().GetWorld().FindEntityByName("Roadmap_Escort_Car");
		ctx.NotNull(car, "DebugWorld has Roadmap_Escort_Car");
		if (!car)
			return;

		carrier.SetOrigin(car.GetOrigin() + "3 0 0");
		patient.SetOrigin(car.GetOrigin() + "4 0 0");

		ctx.Equal(LF_ECarryRefusal.NOT_DOWNED, LF_Carry.Start(carrier, patient, LF_ECarryMode.DRAG), "a standing body is refused");
		ctx.Equal(0, body.LF_GetCarrierPlayerId(), "a refused start marks nothing");

		damage.Kill(Instigator.CreateInstigator(null));
		ctx.True(body.LF_IsDowned(), "the patient is downed");

		// Too far, then in reach.
		patient.SetOrigin(carrier.GetOrigin() + "30 0 0");
		ctx.Equal(LF_ECarryRefusal.TOO_FAR, LF_Carry.Start(carrier, patient, LF_ECarryMode.DRAG), "a body out of reach is refused");
		patient.SetOrigin(carrier.GetOrigin() + "1 0 0");
		ctx.Equal(LF_ECarryRefusal.NONE, LF_Carry.Start(carrier, patient, LF_ECarryMode.DRAG), "a player in reach drags the downed body");
		ctx.True(body.LF_GetCarrierPlayerId() != 0, "the body is marked held (replicated)");
		ctx.Equal(LF_ECarryMode.DRAG, body.LF_GetCarryMode(), "in drag mode");
		ctx.Equal(1, LF_Carry.GetHoldCount(), "one hold");
		ctx.Equal(LF_ECarryRefusal.ALREADY_HELD, LF_Carry.Start(carrier, patient, LF_ECarryMode.CARRY), "a held body cannot be taken again");

		ctx.Equal(LF_ECarryRefusal.NONE, LF_Carry.Drop(carrier), "the carrier drops the body");
		ctx.Equal(0, body.LF_GetCarrierPlayerId(), "the body is free");
		ctx.Equal(0, LF_Carry.GetHoldCount(), "no hold left");
		ctx.Equal(LF_ECarryRefusal.NOT_HELD, LF_Carry.Drop(carrier), "dropping nothing is refused");

		// Carry, then load beside the car.
		ctx.Equal(LF_ECarryRefusal.NONE, LF_Carry.Start(carrier, patient, LF_ECarryMode.CARRY), "the body is carried");
		ctx.Equal(LF_ECarryMode.CARRY, body.LF_GetCarryMode(), "in carry mode");
		ctx.True(LF_Carry.FindSeatVehicle(carrier, patient) != null, "the escort car has a free passenger seat in reach");
		ctx.Equal(LF_ECarryRefusal.NONE, LF_Carry.Load(carrier), "the load into the car is accepted");
		ctx.Equal(0, body.LF_GetCarrierPlayerId(), "a loaded body is no longer held");
		ctx.Equal(1, LF_Carry.GetLoadedCount(), "one load counted");
		ctx.EqualStr("drag;drop;carry;load;", recorder.m_sLog, "the bus saw drag, drop, carry, load");
	}
}
