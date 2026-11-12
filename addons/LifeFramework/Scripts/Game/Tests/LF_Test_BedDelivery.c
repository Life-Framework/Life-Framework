// red-proof: observed 2026-11-10 with `toolsi test --no-build --only LF_Test_BedDelivery`: skipping LF_Carry.EndForDelivery in LF_BedDelivery.Deliver failed it with `- the carry ended (expected 0, got -1)`. Restored; green.
//------------------------------------------------------------------------------------------------
class LF_TestBedDeliveryRecorder : LF_EventSubscriber
{
	string m_sLog;

	override bool Handle(notnull LF_Event busEvent)
	{
		if (LF_OnPlayerRevived.Cast(busEvent))
			m_sLog += "revived;";

		if (LF_OnPatientDelivered.Cast(busEvent))
			m_sLog += "delivered;";

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! medic/bed-delivery (#262): the pure rule; the ambulance is a stretcher vehicle and the escort car
//! is not (a seated patient only bleeds out in the car); a medic who holds a downed body puts it in
//! a hospital bed: the body stands on the mattress awake, the carry ends, revived then delivered
//! reach the bus once. A civilian, a standing patient and a medic far from the bed change nothing.
//! The reduced bill itself needs a real player (test bodies have none): `session.mjs bed-proof`.
// tier: WORLD
class LF_Test_BedDelivery : LF_Test
{
	override string GetName()
	{
		return "medic/bed-delivery";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		float range = config.m_fBedRange;
		ctx.Equal(LF_EDeliveryRefusal.NONE, LF_BedDelivery.Decide(true, true, true, 1, range), "a medic at the bed with a downed patient may deliver");
		ctx.Equal(LF_EDeliveryRefusal.NOT_MEDIC, LF_BedDelivery.Decide(true, true, false, 1, range), "a civilian is refused");
		ctx.Equal(LF_EDeliveryRefusal.NO_PATIENT, LF_BedDelivery.Decide(false, false, true, 1, range), "no patient is refused");
		ctx.Equal(LF_EDeliveryRefusal.NOT_DOWNED, LF_BedDelivery.Decide(true, false, true, 1, range), "a standing patient is refused");
		ctx.Equal(LF_EDeliveryRefusal.TOO_FAR, LF_BedDelivery.Decide(true, true, true, range + 1, range), "a medic far from the bed is refused");
		ctx.True(LF_Utils.SamePrefab("{1111111111111111}Prefabs/a.et", "{1111111111111111}Prefabs/Vehicles/b.et"), "the GUID decides");
		ctx.False(LF_Utils.SamePrefab("{1111111111111112}Prefabs/a.et", "{1111111111111111}Prefabs/a.et"), "another GUID is another prefab");

		IEntity ambulance = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HOSPITAL_AMBULANCE);
		IEntity car = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.ESCORT_CAR);
		IEntity bed = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HOSPITAL_BED_1);
		ctx.NotNull(ambulance, "DebugWorld has the hospital ambulance");
		ctx.NotNull(car, "DebugWorld has the escort car");
		ctx.NotNull(bed, "DebugWorld has Annex_Hospital_Bed_1");
		if (!ambulance || !car || !bed)
			return;

		ctx.True(LF_BedDelivery.IsStretcherVehicle(ambulance), "the ambulance is a stretcher vehicle");
		ctx.False(LF_BedDelivery.IsStretcherVehicle(car), "the escort car is not");

		bool wasEnabled = config.m_bDownedEnabled;
		config.m_bDownedEnabled = true;
		LF_Downed.ResetStatics();
		LF_Downed.SetTestAllEligible(true);
		LF_Carry.ResetStatics();
		LF_BedDelivery.ResetStatics();
		LF_EventBus.ResetStatics();
		LF_TestBedDeliveryRecorder recorder = new LF_TestBedDeliveryRecorder();
		LF_EventBus.Subscribe(LF_OnPlayerRevived, recorder);
		LF_EventBus.Subscribe(LF_OnPatientDelivered, recorder);

		IEntity medic = LF_TestCharacterFixture.Spawn(ctx);
		IEntity patient = LF_TestCharacterFixture.Spawn(ctx);
		if (medic && patient)
			RunDelivery(ctx, recorder, bed, medic, patient);

		LF_TestCharacterFixture.Delete(medic);
		LF_TestCharacterFixture.Delete(patient);
		config.m_bDownedEnabled = wasEnabled;
		LF_Downed.SetTestAllEligible(false);
		LF_Downed.ResetStatics();
		LF_Carry.ResetStatics();
		LF_BedDelivery.ResetStatics();
		LF_EventBus.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDelivery(LF_TestContext ctx, LF_TestBedDeliveryRecorder recorder, IEntity bed, IEntity medic, IEntity patient)
	{
		SCR_ChimeraCharacter body = SCR_ChimeraCharacter.Cast(patient);
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(patient.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!body || !damage)
		{
			ctx.Fail("the patient lacks SCR_ChimeraCharacter or the character damage manager");
			return;
		}

		medic.SetOrigin(bed.GetOrigin() + "1.2 0 0");
		patient.SetOrigin(medic.GetOrigin() + "1 0 0");

		ctx.Equal(LF_EDeliveryRefusal.NOT_DOWNED, LF_BedDelivery.Deliver(medic, bed, patient, true), "a standing patient is refused");
		damage.Kill(Instigator.CreateInstigator(null));
		ctx.True(body.LF_IsDowned(), "the patient is downed");
		ctx.Equal(LF_EDeliveryRefusal.NOT_MEDIC, LF_BedDelivery.Deliver(medic, bed, patient, false), "a civilian is refused");
		ctx.True(body.LF_IsDowned(), "a refused delivery leaves the patient downed");

		ctx.Equal(LF_ECarryRefusal.NONE, LF_Carry.Start(medic, patient, LF_ECarryMode.CARRY), "the medic carries the patient");
		ctx.True(LF_Carry.HeldPatient(medic) == patient, "the medic's held patient is the body");

		medic.SetOrigin(bed.GetOrigin() + "30 0 0");
		ctx.Equal(LF_EDeliveryRefusal.TOO_FAR, LF_BedDelivery.Deliver(medic, bed, patient, true), "a medic far from the bed is refused");
		ctx.True(body.LF_IsDowned(), "still downed after the far refusal");

		medic.SetOrigin(bed.GetOrigin() + "1.2 0 0");
		ctx.Equal(LF_EDeliveryRefusal.NONE, LF_BedDelivery.Deliver(medic, bed, patient, true), "the delivery is accepted");
		ctx.False(body.LF_IsDowned(), "the patient is revived");
		ctx.Equal(0, body.LF_GetCarrierPlayerId(), "the carry ended");
		ctx.True(LF_Carry.HeldPatient(medic) == null, "the medic holds nobody");
		ctx.True(vector.Distance(patient.GetOrigin(), bed.GetOrigin()) < 1.5, "the patient lies on the bed");
		ctx.Equal(1, LF_BedDelivery.GetDeliveredCount(), "one delivery");

		ctx.Equal(LF_EDeliveryRefusal.NOT_DOWNED, LF_BedDelivery.Deliver(medic, bed, patient, true), "a second delivery is refused");
		ctx.Equal(1, LF_BedDelivery.GetDeliveredCount(), "still one delivery");
		ctx.EqualStr("revived;delivered;", recorder.m_sLog, "the bus saw revived then delivered once");
	}
}
