// red-proof: observed 2026-11-10 with `toolsi test --no-build --only LF_Test_ServiceRadio`: tuning through BaseRadioComponent.SetTransceiverFrequency on the server left the radio on its factory channel and failed it with `- the police kit radio is tuned to the police frequency (expected 41000, got 32000)`. Switched to BaseTransceiver.SetFrequency; green.
//------------------------------------------------------------------------------------------------
//! comms/radio-preset (#252): clocking in as police or medic gives a vanilla handheld radio whose
//! first transceiver sits on the service frequency from LF_ServiceCalls.conf; a radio that is not
//! on duty stays on its factory channel; the frequencies differ per service.
// tier: WORLD
class LF_Test_ServiceRadio : LF_Test
{
	protected static const string UID = "service-radio";

	override string GetName()
	{
		return "comms/radio-preset";
	}

	override void Run(LF_TestContext ctx)
	{
		int policeFreq = LF_ServiceCallsConfig.Frequency("police");
		int medicFreq = LF_ServiceCallsConfig.Frequency("medic");
		ctx.True(policeFreq > 0 && medicFreq > 0 && policeFreq != medicFreq, "police and medic have distinct configured frequencies");

		LF_PlayerAccount account = LF_TestPoliceKit.Account(UID, true);
		string characterId = account.GetActiveCharacter().GetId();
		IEntity body = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!body || ctx.FailureCount() > 0)
		{
			Release(characterId);
			LF_TestCharacterFixture.Delete(body);
			return;
		}

		ctx.EqualStr("", LF_DutyLocker.ClockIn(account, body, LF_Faction.POLICE), "the officer clocks in");
		ctx.Equal(policeFreq, FirstFrequency(ctx, body), "the police kit radio is tuned to the police frequency");
		ctx.EqualStr("", LF_DutyLocker.ClockOut(account, body, LF_Faction.POLICE), "the officer clocks out");

		ctx.EqualStr("", LF_DutyLocker.ClockIn(account, body, LF_Faction.MEDIC), "the same player clocks in as a medic");
		ctx.Equal(medicFreq, FirstFrequency(ctx, body), "the medic kit radio is tuned to the medic frequency");
		LF_DutyLocker.ClockOut(account, body, LF_Faction.MEDIC);

		// Off duty a radio is left alone.
		IEntity radioEntity = LF_Utils.SpawnEntityPrefab(LF_ServiceRadio.RADIO_PREFAB, vector.Zero);
		ctx.NotNull(radioEntity, "the vanilla handheld radio spawns");
		if (radioEntity)
		{
			BaseRadioComponent radio = BaseRadioComponent.Cast(radioEntity.FindComponent(BaseRadioComponent));
			ctx.NotNull(radio, "the radio prefab carries BaseRadioComponent");
			if (radio)
			{
				ctx.True(LF_ServiceRadio.Tune(radio, medicFreq), "a radio tunes to a configured frequency");
				ctx.Equal(medicFreq, radio.GetTransceiver(0).GetFrequency(), "the transceiver reports the tuned frequency");
				ctx.False(LF_ServiceRadio.Tune(radio, 1000), "a frequency off the radio's range is refused");
				ctx.Equal(medicFreq, radio.GetTransceiver(0).GetFrequency(), "a refused tune leaves the frequency alone");
			}
			SCR_EntityHelper.DeleteEntityAndChildren(radioEntity);
		}

		Release(characterId);
		LF_TestCharacterFixture.Delete(body);
	}

	protected int FirstFrequency(LF_TestContext ctx, IEntity body)
	{
		array<BaseRadioComponent> radios = {};
		LF_ServiceRadio.FindCarried(body, radios);
		ctx.True(radios.Count() > 0, "the kit carries a radio");
		if (radios.Count() == 0 || !radios[0].GetTransceiver(0))
			return -1;

		return radios[0].GetTransceiver(0).GetFrequency();
	}

	protected void Release(string characterId)
	{
		LF_StorageSnapshot.Clear(characterId, LF_DutyLocker.SLOT_MEDIC);
		LF_TestPoliceKit.Release(UID);
	}
}
