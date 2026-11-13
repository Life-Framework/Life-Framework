// red-proof: Observed 2026-11-12 with `tools/cli test --no-build --only LF_Test_PhoneCallBackWorld`: skipping the duty check in LF_PhoneCalls.CallBackAt failed it with `- an off-duty player cannot ring back` (and, with the length check also skipped, `- the server refuses a message over the limit`). Restored; green.
//------------------------------------------------------------------------------------------------
//! comms/phone-callback-world (#147): the call-back flow on the shipped prefabs. Nobody on duty
//! offers the caller a request at that payphone (its line carries the offer); a too-long message
//! is refused; the request is left; an off-duty player cannot ring it back; the officer rings the
//! payphone back, anyone there picks up and the request leaves the pad; a rung-out call offers
//! too; the bus hears left, rung and answered.
// tier: WORLD
class LF_Test_PhoneCallBackWorld : LF_Test
{
	static const ResourceName PAYPHONE = "{8F2C6E1D4B7A5093}Prefabs/Comms/LF_Payphone.et";
	static const ResourceName STATION = "{9BDAE42023AB3357}Prefabs/Comms/LF_DispatchPhone.et";

	override string GetName()
	{
		return "comms/phone-callback-world";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();
		LF_Payphone.ResetStatics();
		LF_PhoneCalls.ResetStatics();
		LF_PhoneCallBacks.ResetStatics();
		LF_PhoneTestCallBackCounter counter = new LF_PhoneTestCallBackCounter();
		LF_EventBus.Subscribe(LF_OnPhoneCallBack, counter);
		LF_PhoneTestPresence presence = new LF_PhoneTestPresence();
		LF_PhoneCalls.SetPresence(presence);

		IEntity caller = LF_TestCharacterFixture.Spawn(ctx);
		if (!caller)
			return;

		vector at = caller.GetOrigin();
		IEntity payphoneEntity = LF_Utils.SpawnEntityPrefab(PAYPHONE, at + "-3 0 0");
		IEntity stationEntity = LF_Utils.SpawnEntityPrefab(STATION, at + "-9 0 0");
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
		array<ref LF_ServiceCandidate> nobody = {};
		array<ref LF_ServiceCandidate> duty = {};
		duty.Insert(LF_ServiceCandidate.Make(2, true, LF_EJobType.UNEMPLOYED));
		duty.Insert(LF_ServiceCandidate.Make(3, false, LF_EJobType.UNEMPLOYED));

		// Nobody on duty: the payphone's line carries the caller's offer.
		ctx.True(LF_PhoneCalls.DialAt(100, 1, "cb-caller", caller, payphone, "police", nobody) == LF_EPhoneRefusal.NOBODY_ON_DUTY, "nobody on duty: missed");
		LF_PhoneLineComponent payLine = payphone.Line();
		ctx.NotNull(payLine, "the payphone got a line for the offer");
		if (payLine)
			ctx.Equal(1, payLine.GetOfferId(), "the line shows the offer to the caller");

		string tooLong = "";
		for (int i = 0; i <= LF_PhoneConfig.MessageMaxChars(); i++)
		{
			tooLong += "x";
		}
		ctx.True(LF_PhoneCallBacks.LeaveAt(101, 1, "cb-caller", caller, payphone, tooLong, duty) == LF_EPhoneRequestRefusal.TOO_LONG, "the server refuses a message over the limit");
		ctx.True(LF_PhoneCallBacks.LeaveAt(101, 4, "cb-other", caller, payphone, "hello", duty) == LF_EPhoneRequestRefusal.NO_OFFER, "someone else cannot use the caller's offer");
		ctx.True(LF_PhoneCallBacks.LeaveAt(102, 1, "cb-caller", caller, payphone, "  saw a <b>robbery</b> | ", duty) == LF_EPhoneRequestRefusal.OK, "the caller leaves a request");
		if (payLine)
			ctx.Equal(0, payLine.GetOfferId(), "the offer leaves the line once used");

		array<ref LF_PhoneCallBackRequest> requests = LF_PhoneCallBacks.Book().Requests();
		ctx.Equal(1, requests.Count(), "one request on the pad");
		if (requests.Count() != 1)
		{
			Cleanup(caller, payphoneEntity, stationEntity);
			return;
		}

		LF_PhoneCallBackRequest request = requests[0];
		ctx.EqualStr("saw a b robbery /b", request.m_sMessage, "the stored message is cleaned");
		ctx.EqualStr(payphone.Key(), request.m_sPhoneKey, "the request names the payphone");

		// Ring-back: duty is checked, then the payphone rings and anyone there picks up.
		ctx.True(LF_PhoneCalls.CallBackAt(110, 3, "cb-civilian", caller, station, request.m_iId, duty) == LF_EPhoneRefusal.NOT_ON_DUTY, "an off-duty player cannot ring back");
		ctx.True(LF_PhoneCalls.CallBackAt(111, 2, "cb-officer", caller, station, request.m_iId, duty) == LF_EPhoneRefusal.OK, "the officer rings the payphone back");
		ctx.True(payLine != null && payLine.GetState() == LF_EPhoneLineState.RINGING, "the payphone rings");
		ctx.True(LF_PhoneCalls.CallBackAt(112, 2, "cb-officer", caller, station, request.m_iId, duty) == LF_EPhoneRefusal.REQUEST_CALLING, "a request being rung back cannot be rung twice");
		ctx.True(LF_PhoneCalls.AnswerAt(115, 5, "cb-passerby", caller, payphone, duty) == LF_EPhoneRefusal.OK, "anyone at the payphone picks up");
		ctx.Equal(0, LF_PhoneCallBacks.Book().Requests().Count(), "an answered ring-back leaves the pad");
		ctx.True(LF_PhoneCalls.HangUpAt(120, 5, payphone) == LF_EPhoneRefusal.OK, "the passer-by hangs up");

		// A call that rings out offers too.
		ctx.True(LF_PhoneCalls.DialAt(1000, 1, "cb-caller-2", caller, payphone, "police", duty) == LF_EPhoneRefusal.OK, "a second call rings the station");
		LF_PhoneCalls.TickAt(1000 + LF_PhoneConfig.RingSeconds());
		if (payLine)
			ctx.Equal(1, payLine.GetOfferId(), "a rung-out call offers a request");

		ctx.EqualStr("left,rung,answered", counter.Kinds(), "the bus heard left, rung and answered");

		Cleanup(caller, payphoneEntity, stationEntity);
		LF_EventBus.Unsubscribe(LF_OnPhoneCallBack, counter);
		LF_PhoneCalls.ResetStatics();
		LF_PhoneCallBacks.ResetStatics();
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

//------------------------------------------------------------------------------------------------
class LF_PhoneTestCallBackCounter : LF_EventSubscriber
{
	ref array<string> m_aKinds = {};

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPhoneCallBack e = LF_OnPhoneCallBack.Cast(busEvent);
		if (!e)
			return false;

		m_aKinds.Insert(e.m_sKind);
		return true;
	}

	string Kinds()
	{
		string text;
		foreach (int i, string kind : m_aKinds)
		{
			if (i > 0)
				text += ",";
			text += kind;
		}
		return text;
	}
}
