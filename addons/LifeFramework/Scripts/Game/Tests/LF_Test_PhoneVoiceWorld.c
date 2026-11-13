// red-proof: Observed 2026-11-12 with `tools/cli test --no-build --only LF_Test_PhoneVoiceWorld`: leaving the lines powered in LF_PhoneLineTransport.Disconnect failed it with `- both line radios are off after the call`. Restored; green.
//------------------------------------------------------------------------------------------------
//! comms/phone-voice-world (#147): the voice seam on the shipped prefabs. Picking up connects the
//! two phone lines' radios on the call's frequency with one key; hanging up puts them back on the
//! idle frequency with new idle keys. The handset transport hands a receiver on the private band to the
//! talker and takes it back.
// tier: WORLD
class LF_Test_PhoneVoiceWorld : LF_Test
{
	static const ResourceName PAYPHONE = "{8F2C6E1D4B7A5093}Prefabs/Comms/LF_Payphone.et";
	static const ResourceName STATION = "{9BDAE42023AB3357}Prefabs/Comms/LF_DispatchPhone.et";

	override string GetName()
	{
		return "comms/phone-voice-world";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();
		LF_Payphone.ResetStatics();
		LF_PhoneCalls.ResetStatics();
		LF_PhoneCallBacks.ResetStatics();
		LF_PhoneVoice.ResetStatics();
		LF_PhoneVoice.SetTransport(new LF_PhoneLineTransport());
		LF_PhoneCalls.SetPresence(new LF_PhoneTestPresence());

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
		if (!payphone || !station)
		{
			Cleanup(caller, payphoneEntity, stationEntity);
			return;
		}

		LF_MoneyUtils.AddCash(caller, 5);
		array<ref LF_ServiceCandidate> duty = {};
		duty.Insert(LF_ServiceCandidate.Make(2, true, LF_EJobType.UNEMPLOYED));
		ctx.True(LF_PhoneCalls.DialAt(100, 1, "voice-caller", caller, payphone, "police", duty) == LF_EPhoneRefusal.OK, "the dial rings the station");
		ctx.True(LF_PhoneCalls.AnswerAt(104, 2, "voice-officer", caller, station, duty) == LF_EPhoneRefusal.OK, "the officer picks up");

		LF_PhoneCall call = LF_PhoneCalls.Exchange().CallOfPlayer(1);
		LF_PhoneVoiceLink link;
		if (call)
			link = LF_PhoneVoice.LinkOf(call.m_iId);
		ctx.NotNull(link, "picking up connects the voice");
		BaseRadioComponent a = RadioOf(payphone.ServerLineEntity());
		BaseRadioComponent b = RadioOf(station.ServerLineEntity());
		ctx.True(a != null && b != null, "both phone lines carry a radio");
		if (!link || !a || !b)
		{
			Cleanup(caller, payphoneEntity, stationEntity);
			return;
		}

		ctx.True(a.IsPowered() && b.IsPowered(), "both line radios are powered");
		ctx.Equal(LF_PhoneVoice.FrequencyFor(call.m_iId), a.GetTransceiver(0).GetFrequency(), "the caller line is on the call frequency");
		ctx.Equal(link.m_iFrequency, b.GetTransceiver(0).GetFrequency(), "the answerer line is on the same frequency");
		ctx.EqualStr(link.m_sKey, a.GetEncryptionKey(), "the caller line has the call key");
		ctx.EqualStr(a.GetEncryptionKey(), b.GetEncryptionKey(), "both lines share the key");

		string callKey = link.m_sKey;
		ctx.True(LF_PhoneCalls.HangUpAt(110, 1, payphone) == LF_EPhoneRefusal.OK, "the caller hangs up");
		ctx.True(LF_PhoneVoice.LinkOf(call.m_iId) == null, "hanging up drops the voice");
		ctx.True(a.GetTransceiver(0).GetFrequency() == LF_PhoneVoice.BAND_MIN && b.GetTransceiver(0).GetFrequency() == LF_PhoneVoice.BAND_MIN, "both lines are back on the idle frequency after the call");
		ctx.True(a.GetEncryptionKey() != callKey && b.GetEncryptionKey() != callKey, "the call key is gone from both lines");

		// The alternative: a receiver on the private band in the talker's inventory.
		LF_PhoneHandsetTransport handsets = new LF_PhoneHandsetTransport();
		IEntity handset = handsets.HandTo(caller, 99);
		BaseRadioComponent carried = RadioOf(handset);
		ctx.NotNull(carried, "the handset transport puts a receiver in the inventory");
		if (carried)
		{
			BaseTransceiver t = carried.GetTransceiver(0);
			ctx.True(t.GetMinFrequency() == LF_PhoneVoice.BAND_MIN && t.GetMaxFrequency() == LF_PhoneVoice.BAND_MAX, "the receiver tunes the private band only");
			ctx.True(LF_PhoneVoice.Tune(handset, LF_PhoneVoice.FrequencyFor(99), "LF_call_99_1"), "the receiver tunes to a call");
			LF_PhoneVoiceLink handLink = new LF_PhoneVoiceLink();
			handLink.m_CallerRadio = handset.GetID();
			EntityID handsetId = handset.GetID();
			handsets.Disconnect(handLink);
			ctx.True(LF_PhoneVoice.Find(handsetId) == null, "hanging up takes the receiver back");
		}

		Cleanup(caller, payphoneEntity, stationEntity);
		LF_PhoneCalls.ResetStatics();
		LF_PhoneVoice.ResetStatics();
	}

	protected BaseRadioComponent RadioOf(IEntity entity)
	{
		if (!entity)
			return null;

		return BaseRadioComponent.Cast(entity.FindComponent(BaseRadioComponent));
	}

	protected void Cleanup(IEntity caller, IEntity payphone, IEntity station)
	{
		array<IEntity> phones = {payphone, station};
		foreach (IEntity phone : phones)
		{
			LF_PhoneComponent component = LF_PhoneComponent.Of(phone);
			if (component && component.ServerLineEntity())
				SCR_EntityHelper.DeleteEntityAndChildren(component.ServerLineEntity());
			if (phone)
				SCR_EntityHelper.DeleteEntityAndChildren(phone);
		}

		LF_TestCharacterFixture.Delete(caller);
	}
}
