// red-proof: Observed 2026-11-17 with `tools/cli test --no-build --only LF_Test_PhoneVoiceConnectFail` on the unfixed Connect: failed with `- line A is back on the idle frequency (expected 100000, got 100007)`. With the fix: green.
//------------------------------------------------------------------------------------------------
//! comms/phone-voice-connect-fail (#2625): when the second phone line of a call has no radio,
//! the line transport returns false and the first line is back on the idle frequency with a
//! key that is not the call's. Line B here is the payphone prop itself, which carries no radio.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PhoneVoiceConnectFail : LF_Test
{
	static const ResourceName PAYPHONE = "{8F2C6E1D4B7A5093}Prefabs/Comms/LF_Payphone.et";
	static const ResourceName STATION = "{9BDAE42023AB3357}Prefabs/Comms/LF_DispatchPhone.et";

	override string GetName()
	{
		return "comms/phone-voice-connect-fail";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PhoneVoice.ResetStatics();
		IEntity caller = LF_TestCharacterFixture.Spawn(ctx);
		if (!caller)
			return;

		vector at = caller.GetOrigin();
		IEntity payphoneEntity = LF_Utils.SpawnEntityPrefab(PAYPHONE, at + "0 0 3");
		IEntity stationEntity = LF_Utils.SpawnEntityPrefab(STATION, at + "0 0 9");
		LF_PhoneComponent payphone = LF_PhoneComponent.Of(payphoneEntity);
		LF_PhoneComponent station = LF_PhoneComponent.Of(stationEntity);
		ctx.NotNull(payphone, "the payphone prefab is a telephone");
		ctx.NotNull(station, "the station telephone prefab is a telephone");
		IEntity lineA;
		if (station)
			lineA = station.ServerLineEntity();
		if (station && !lineA && station.ServerLine())
			lineA = station.ServerLineEntity();
		BaseRadioComponent radioA;
		if (lineA)
			radioA = BaseRadioComponent.Cast(lineA.FindComponent(BaseRadioComponent));
		ctx.NotNull(radioA, "line A carries a radio");
		if (!payphone || !station || !radioA)
		{
			Cleanup(caller, payphoneEntity, stationEntity);
			return;
		}

		ctx.True(radioA.GetTransceiver(0).GetFrequency() == LF_PhoneVoice.BAND_MIN, "line A starts on the idle frequency");

		LF_PhoneCall call = new LF_PhoneCall();
		call.m_iId = 7;
		call.m_sFromPhone = station.Key();
		call.m_sToPhone = payphone.Key();
		LF_PhoneVoiceLink link = new LF_PhoneVoiceLink();
		link.m_iCallId = call.m_iId;
		link.m_iFrequency = LF_PhoneVoice.FrequencyFor(call.m_iId);
		link.m_sKey = LF_PhoneVoice.KeyFor(call.m_iId, 123456);

		LF_Test_PhoneVoiceConnectFail_Transport transport = new LF_Test_PhoneVoiceConnectFail_Transport();
		transport.m_LineA = lineA;
		transport.m_NoRadio = payphoneEntity;
		transport.m_sKeyA = station.Key();
		ctx.True(!transport.Connect(call, link), "a call whose second line has no radio does not connect");
		ctx.Equal(LF_PhoneVoice.BAND_MIN, radioA.GetTransceiver(0).GetFrequency(), "line A is back on the idle frequency");
		ctx.True(radioA.GetEncryptionKey() != link.m_sKey, "line A no longer has the call key");

		Cleanup(caller, payphoneEntity, stationEntity);
		LF_PhoneVoice.ResetStatics();
	}

	protected void Cleanup(IEntity caller, IEntity payphone, IEntity station)
	{
		array<IEntity> phones = {payphone, station};
		foreach (IEntity phone : phones)
		{
			LF_PhoneComponent component = LF_PhoneComponent.Of(phone);
			if (component && component.ServerLineEntity())
				SCR_EntityHelper.DeleteEntityAndChildren(component.ServerLineEntity());
			LF_TestLooseSweep.DeleteBuilding(phone);
		}

		LF_TestCharacterFixture.Delete(caller);
	}
}

//------------------------------------------------------------------------------------------------
//! The line transport with line B swapped for an entity that has no radio.
class LF_Test_PhoneVoiceConnectFail_Transport : LF_PhoneLineTransport
{
	IEntity m_LineA;
	IEntity m_NoRadio;
	string m_sKeyA;

	override protected IEntity LineOf(string phoneKey)
	{
		if (phoneKey == m_sKeyA)
			return m_LineA;

		return m_NoRadio;
	}
}
